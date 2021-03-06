//@HEADER
// ************************************************************************
// 
//               miniXyce: A simple circuit simulation benchmark code
//                 Copyright (2011) Sandia Corporation
// 
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
// 
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//  
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//  
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
// Questions? Contact Michael A. Heroux (maherou@sandia.gov) 
// 
// ************************************************************************
//@HEADER

// Author : Karthik V Aadithya
// Mentor : Heidi K Thornquist
// Date : July 2010

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include "mX_parser.h"
#include "mX_source.h"
#include "mX_sparse_matrix.h"
#include "mX_linear_DAE.h"
#include "mX_parms.h"
#include "mX_timer.h"
#include "YAML_Element.hpp"
#include "YAML_Doc.hpp"
#include "RHT.h"
#include <time.h>

#ifdef HAVE_MPI
#include "mpi.h"
#endif

using namespace mX_parse_utils;
using namespace mX_source_utils;
using namespace mX_linear_DAE_utils;
using namespace mX_parms_utils;

//mpirun -np 1 miniXyce.x -c tests/cir7.net

//-D CMAKE_C_COMPILER=/usr/bin/gcc-7 -D CMAKE_CXX_COMPILER=/usr/bin/g++-7
//-D CMAKE_C_COMPILER=/usr/bin/mpicc -D CMAKE_CXX_COMPILER=/usr/bin/mpicxx

typedef struct {
    int n, p, pid, argc, executionCore;
    char** argv;
} ConsumerParams;

void consumer_thread_func(void * args);

double main_execution_replicated(int p, int pid, int n, int argc, char* argv[]);

double main_execution(int p, int pid, int n, int argc, char* argv[]);

//perl RC_ladder.pl 5000 > cirHuge.net

int main(int argc, char* argv[]) {

    //stressTest_queue();

    // this is of course, the actual transient simulator
    int p = 1, pid = 0, n = 0, replicated = 0;
    int producerCore = 0, consumerCore = 2, numThreads = 2;
    char * basePath;
    int numRuns = 1;

#ifdef HAVE_MPI
    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &p);
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
#endif

    // params for either normal or replicated execution
    if(argc > 4) {
        basePath = argv[3];
        numRuns = atoi(argv[4]);
        if(pid == 0) printf("\n-------- Will execute %d times the ", numRuns);
    }

    //params just for replicated execution
    if(argc > 6) {
        replicated = 1;
        numThreads = atoi(argv[5]);

        producerCore = atoi(argv[pid * 2 + 6]);
        consumerCore = atoi(argv[pid * 2 + 7]);

        argc -= (numThreads + 3); // basepath, numRuns, numThreads and the thread list
        if(pid == 0) printf("Replicated");
    }else{
        argc -= 2; // basepath, numRuns
        if(pid == 0) printf("Non-Replicated");
    }

    if(pid == 0 && replicated) {
        printf("  version with %d ranks --- Approach: ", numThreads/2);
#if APPROACH_USING_POINTERS == 1
        printf("Enq/Deq Pointers");
#elif APPROACH_ALREADY_CONSUMED == 1
        printf("AlreadyConsumed value");
#elif APPROACH_NEW_LIMIT == 1
        printf("New limit");
#elif APPROACH_WRITE_INVERTED_NEW_LIMIT == 1
        printf("New limit write inverted");
#elif APPROACH_WANG == 1
        printf("Wang");
#endif

#if VAR_GROUPING == 1
        printf(" -- With var grouping of %d: ", GROUP_GRANULARITY);
#endif
    }


#if DPRINT_OUTPUT == 0
    if(pid == 0) printf("\nThe print output has been disabled, check CMakeList.txt to switch on/off options\n\n");
#else
    if(pid == 0) printf("\nThe print output is enabled, check CMakeList.txt to switch on/off options\n\n");
#endif

    // calculates the path of defaultParams and lastUsedParams files
    sprintf(DEFAULT_PARAMS_FILE_PATH, "%s/%s", basePath, DEFAULT_PARAMS_FILE_NAME);
    sprintf(LAST_USED_PARAMS_FILE_PATH, "%s/%s", basePath, LAST_USED_PARAMS_FILE_NAME);

    // initialize the simulation parameters
    double times = 0, currentElapsed;

    if (!replicated) {
        for (int iterator = 0; iterator < numRuns; iterator++) {

            currentElapsed = main_execution(p, pid, n, argc, argv);

            if(pid == 0) {
                printf("Actual Walltime[%d] in seconds: %f \n", iterator, currentElapsed);
                times += currentElapsed;
            }
        }

        if(pid == 0) {
            printf("\n -------- Summary Baseline ----------- \n");
            printf("Mean time in seconds: %f \n\n", times / numRuns);
        }

    } else {
        printf("Rank %d using cores: %d, %d\n", pid, producerCore, consumerCore);
        SetThreadAffinity(producerCore);
        ConsumerParams *consumerParams;

        for (int iterator = 0; iterator < numRuns; iterator++) {
            RHT_Replication_Init();
            consumerParams = new ConsumerParams();

            consumerParams->pid = pid;
            consumerParams->n = n;
            consumerParams->p = p;
            consumerParams->executionCore = consumerCore;
            consumerParams->argc = argc;
            consumerParams->argv = argv;

            pthread_t myThread;

            int err = pthread_create(&myThread, NULL, (void *(*)(void *)) consumer_thread_func, (void *) consumerParams);

            if (err) {
                fprintf(stderr, "Fail creating thread %d\n", 1);
                exit(1);
            }

            currentElapsed = main_execution_replicated(p, pid, n, argc, argv);
            pthread_join(myThread, NULL);

            if (pid == 0) {
                printf("Actual Walltime[%d] in seconds: %f, pCount: %ld vs cCount: %ld \n",
                       iterator,  currentElapsed, wangQueue.producerCount, wangQueue.consumerCount);
                times += currentElapsed;
            }

            // params that need to be reset each time
            if (consumerParams)
                delete consumerParams;

            RHT_Replication_Finish();
        }

        if(pid == 0) {
            printf("\n -------- Summary Replicated ----------- \n");
            printf("Mean time in seconds: %f \n\n", times / numRuns);
        }

#ifdef HAVE_MPI
        MPI_Finalize();
#endif
        return 0;
    }

#ifdef HAVE_MPI
    MPI_Finalize();
#endif

    return 0;
}

double main_execution(int p, int pid, int n, int argc, char* argv[]) {
    double sim_start = mX_timer(), elapsedExe = 0;
    struct timespec startExe, endExe, startAll;

    // initialize the simulation parameters
    std::string ckt_netlist_filename;
    double t_start, t_step, t_stop, tol, res;
    int k, iters, restarts;

    std::vector<double> init_cond;
    bool init_cond_specified;
    double tstart, tend;
    int num_internal_nodes, num_voltage_sources, num_inductors;
    int num_current_sources = 0, num_resistors = 0, num_capacitors = 0;

    // initialize YAML doc
    YAML_Doc doc("miniXyce", "1.0");

    tstart = mX_timer();
    get_parms(argc, argv, ckt_netlist_filename, t_start, t_step, t_stop, tol, k, init_cond, init_cond_specified, p,
              pid);
    tend = mX_timer() - tstart;
    doc.add("Parameter_parsing_time", tend);

    // build the DAE from the circuit netlist
    tstart = mX_timer();

    mX_linear_DAE *dae = parse_netlist(ckt_netlist_filename, p, pid, n, num_internal_nodes, num_voltage_sources,
                        num_current_sources, num_resistors, num_capacitors, num_inductors);

    //dperez, here is where the timer of the actual replicated execution starts...
    if(pid == 0) {
        clock_gettime(CLOCK_MONOTONIC, &startExe);
    }

    tend = mX_timer() - tstart;
    doc.add("Netlist_parsing_time", tend);


// document circuit and matrix attributes

    doc.add("Netlist_file", ckt_netlist_filename.c_str());

    int total_devices = num_voltage_sources + num_current_sources + num_resistors + num_capacitors + num_inductors;
    doc.add("Circuit_attributes", "");
    doc.get("Circuit_attributes")->add("Number_of_devices", total_devices);
    if (num_resistors > 0)
        doc.get("Circuit_attributes")->add("Resistors_(R)", num_resistors);
    if (num_inductors > 0)
        doc.get("Circuit_attributes")->add("Inductors_(L)", num_inductors);
    if (num_capacitors > 0)
        doc.get("Circuit_attributes")->add("Capacitors_(C)", num_capacitors);
    if (num_voltage_sources > 0)
        doc.get("Circuit_attributes")->add("Voltage_sources_(V)", num_voltage_sources);
    if (num_current_sources > 0)
        doc.get("Circuit_attributes")->add("Current_sources_(I)", num_current_sources);

    int num_my_rows = dae->A->end_row - dae->A->start_row + 1;
    int num_my_nnz = dae->A->local_nnz, sum_nnz = dae->A->local_nnz;
    int min_nnz = num_my_nnz, max_nnz = num_my_nnz;
    int min_rows = num_my_rows, max_rows = num_my_rows, sum_rows = num_my_rows;

#ifdef HAVE_MPI
    MPI_Allreduce(&num_my_nnz, &sum_nnz, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&num_my_nnz, &min_nnz, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&num_my_nnz, &max_nnz, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    MPI_Allreduce(&num_my_rows, &sum_rows, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&num_my_rows, &min_rows, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&num_my_rows, &max_rows, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
#endif

    doc.add("Matrix_attributes", "");
    doc.get("Matrix_attributes")->add("Global_rows", sum_rows);
    doc.get("Matrix_attributes")->add("Rows_per_proc_MIN", min_rows);
    doc.get("Matrix_attributes")->add("Rows_per_proc_MAX", max_rows);
    doc.get("Matrix_attributes")->add("Rows_per_proc_AVG", (double) sum_rows / p);
    doc.get("Matrix_attributes")->add("Global_NNZ", sum_nnz);
    doc.get("Matrix_attributes")->add("NNZ_per_proc_MIN", min_nnz);
    doc.get("Matrix_attributes")->add("NNZ_per_proc_MAX", max_nnz);
    doc.get("Matrix_attributes")->add("NNZ_per_proc_AVG", (double) sum_nnz / p);

// compute the initial condition if not specified by user
    int start_row = dae->A->start_row;
    int end_row = dae->A->end_row;
    tstart = mX_timer();

    if (!init_cond_specified) {
        std::vector<double> init_cond_guess;

        for (int i = 0; i < num_my_rows; i++) {
            init_cond_guess.push_back((double) (0));
        }

        std::vector<double> init_RHS = evaluate_b(t_start, dae);

        gmres(dae->A, init_RHS, init_cond_guess, tol, res, k, init_cond, iters, restarts);

        doc.add("DCOP Calculation", "");
        doc.get("DCOP Calculation")->add("Init_cond_specified", false);
        doc.get("DCOP Calculation")->add("GMRES_tolerance", tol);
        doc.get("DCOP Calculation")->add("GMRES_subspace_dim", k);
        doc.get("DCOP Calculation")->add("GMRES_iterations", iters);
        doc.get("DCOP Calculation")->add("GMRES_restarts", restarts);
        doc.get("DCOP Calculation")->add("GMRES_native_residual", res);
    } else {
        doc.add("DCOP Calculation", "");
        doc.get("DCOP Calculation")->add("Init_cond_specified", true);
    }
    tend = mX_timer() - tstart;
    doc.get("DCOP Calculation")->add("DCOP_calculation_time", tend);

// write the headers and computed initial condition to file
    tstart = mX_timer();
    int dot_position = ckt_netlist_filename.find_first_of('.');

    std::__cxx11::string out_filename = ckt_netlist_filename.substr(0, dot_position) + "_tran_results.prn";
    std::ofstream *outfile = 0;

#ifdef HAVE_MPI
// Prepare rcounts and displs for a contiguous gather of the full solution vector.
    std::vector<int> rcounts(p, 0), displs(p, 0);
    MPI_Gather(&num_my_rows, 1, MPI_INT, &rcounts[0], 1, MPI_INT, 0, MPI_COMM_WORLD);
    for (int i = 1; i < p; i++) displs[i] = displs[i - 1] + rcounts[i - 1];

    std::vector<double> fullX(sum_rows, 0.0);
    MPI_Gatherv(&init_cond[0], num_my_rows, MPI_DOUBLE, &fullX[0], &rcounts[0], &displs[0], MPI_DOUBLE, 0,
                MPI_COMM_WORLD);
#endif

    if (pid == 0) {
        outfile = new std::ofstream(out_filename.data(), std::ios_base::out);

        *outfile << std::setw(18) << "TIME";

        for (int i = 0; i < sum_rows; i++) {
            std::stringstream ss2;
            if (i < num_internal_nodes)
                ss2 << "V" << i + 1;
            else
                ss2 << "I" << i + 1 - num_internal_nodes;
            *outfile << std::setw(18) << ss2.str();
        }

        *outfile << std::setw(20) << "num_GMRES_iters" << std::setw(20) << "num_GMRES_restarts" << std::endl;

        outfile->precision(8);

        *outfile << std::scientific << std::setw(18) << t_start;

        for (int i = 0; i < sum_rows; i++) {
#ifdef HAVE_MPI
            *outfile << std::setw(18) << fullX[i];
#else
            *outfile << std::setw(18) << init_cond[i];
#endif
        }

        *outfile << std::fixed << std::setw(20) << iters << std::setw(20) << restarts << std::endl;
    }

    double io_tend = mX_timer() - tstart;

// from now you won't be needing any more Ax = b solves
// but you will be needing many (A + B/t_step)x = b solves
// so change A to (A + B/t_step) right now
// so you won't have to compute it at each time step

    tstart = mX_timer();
    distributed_sparse_matrix *A = dae->A;
    distributed_sparse_matrix *B = dae->B;

    std::vector<distributed_sparse_matrix_entry *>::iterator it1;
    int row_idx = start_row - 1;

    for (it1 = B->row_headers.begin(); it1 != B->row_headers.end(); it1++) {
        row_idx++;
        distributed_sparse_matrix_entry *curr = *it1;

        while (curr) {
            int col_idx = curr->column;
            double value = (curr->value) / t_step;

            distributed_sparse_matrix_add_to(A, row_idx, col_idx, value, n, p);

            curr = curr->next_in_row;
        }
    }
    double matrix_setup_tend = mX_timer() - tstart;

    // this is where the actual transient simulation starts

    tstart = mX_timer();
    double t = t_start + t_step;
    double total_gmres_res = 0.0;
    int total_gmres_iters = 0;
    int trans_steps = 0;

    while (t < t_stop) {
        trans_steps++;

        // new time point t => new value for b(t)

        std::vector<double> b = evaluate_b(t, dae);

// build the linear system Ax = b that needs to be solved at this time point
// Backward Euler is used at every iteration

        std::vector<double> RHS;
        sparse_matrix_vector_product(B, init_cond, RHS);

        for (int i = 0; i < num_my_rows; i++) {
            RHS[i] /= t_step;
            RHS[i] += b[i];
        }

// now solve the linear system just built using GMRES(k)
        gmres(A, RHS, init_cond, tol, res, k, init_cond, iters, restarts);
        total_gmres_iters += iters;
        total_gmres_res += res;

// write the results to file
        double io_tstart = mX_timer();
#ifdef HAVE_MPI
        MPI_Gatherv(&init_cond[0], num_my_rows, MPI_DOUBLE, &fullX[0], &rcounts[0], &displs[0], MPI_DOUBLE, 0,
                    MPI_COMM_WORLD);
#endif
        if (pid == 0) {
            outfile->precision(8);
            *outfile << std::scientific << std::setw(18) << t;

            for (int i = 0; i < sum_rows; i++) {
#ifdef HAVE_MPI
                *outfile << std::setw(18) << fullX[i];
#else
                *outfile << std::setw(18) << init_cond[i];
#endif
            }

            *outfile << std::fixed << std::setw(20) << iters << std::setw(20) << restarts << std::endl;
        }

        io_tend += (mX_timer() - io_tstart);

// increment t

        t += t_step;
    }

    // Hurray, the transient simulation is done!
    if (pid == 0) {
        outfile->close();
        delete outfile;
    }

    tend = mX_timer();
    double sim_end = tend - sim_start;
    doc.add("Transient Calculation", "");
    doc.get("Transient Calculation")->add("Number_of_time_steps", trans_steps);
    doc.get("Transient Calculation")->add("Time_start", t_start);
    doc.get("Transient Calculation")->add("Time_end", t_stop);
    doc.get("Transient Calculation")->add("Time_step", t_step);
    doc.get("Transient Calculation")->add("GMRES_tolerance", tol);
    doc.get("Transient Calculation")->add("GMRES_subspace_dim", k);
    doc.get("Transient Calculation")->add("GMRES_average_iters", total_gmres_iters / trans_steps);
    doc.get("Transient Calculation")->add("GMRES_average_res", total_gmres_res / trans_steps);
    doc.get("Transient Calculation")->add("Matrix_setup_time", matrix_setup_tend);
    doc.get("Transient Calculation")->add("Transient_calculation_time", tend - tstart);
    doc.add("I/O File Time", io_tend);
    doc.add("Total Simulation Time", sim_end);

    if (pid == 0) { // Only PE 0 needs to compute and report timing results
        std::__cxx11::string yaml = doc.generateYAML();
#if PRINT_OUTPUT == 1
        std::cout << yaml;
#endif
    }

    //dperez, this is where the replicated execution ends
    if(pid == 0) {
        clock_gettime(CLOCK_MONOTONIC, &endExe);

        elapsedExe = (endExe.tv_sec - startExe.tv_sec);
        elapsedExe += (endExe.tv_nsec - startExe.tv_nsec) / 1000000000.0;
    }

    // Clean up
    mX_linear_DAE_utils::destroy(dae);

    return elapsedExe;
}

double main_execution_replicated(int p, int pid, int n, int argc, char* argv[]) {
    double sim_start = mX_timer(), elapsedExe = 0;
    struct timespec startExe, endExe;

#if PERCENTAGE_OF_REPLICATION == 1
    double elapsedAll = 0;
    struct timespec startAll, endAll;

    //dperez, here is where the timer of the all execution starts...
    if(pid == 0) {
        clock_gettime(CLOCK_MONOTONIC, &startAll);
    }
#endif

    // initialize the simulation parameters
    std::string ckt_netlist_filename;
    double t_start, t_step, t_stop, tol, res;
    int k, iters, restarts;

    std::vector<double> init_cond;
    bool init_cond_specified;
    double tstart, tend;
    int num_internal_nodes, num_voltage_sources, num_inductors;
    int num_current_sources = 0, num_resistors = 0, num_capacitors = 0;

    YAML_Doc doc("miniXyce", "1.0");
    double tempVar, tempVar2;

    tstart = mX_timer();
    get_parms(argc, argv, ckt_netlist_filename, t_start, t_step, t_stop, tol, k, init_cond, init_cond_specified, p, pid);
    tend = mX_timer() - tstart;
    doc.add("Parameter_parsing_time", tend);

    // build the DAE from the circuit netlist
    tstart = mX_timer();

    mX_linear_DAE *dae = parse_netlist(ckt_netlist_filename, p, pid, n, num_internal_nodes, num_voltage_sources,
                                       num_current_sources, num_resistors, num_capacitors, num_inductors);

    //dperez, here is where the timer of the actual replicated execution starts...
    if(pid == 0) {
        clock_gettime(CLOCK_MONOTONIC, &startExe);
    }

    tend = mX_timer() - tstart;
    doc.add("Netlist_parsing_time", tend);

    // document circuit and matrix attributes
    doc.add("Netlist_file", ckt_netlist_filename.c_str());

    int total_devices = num_voltage_sources + num_current_sources + num_resistors + num_capacitors + num_inductors;
#if JUST_VOLATILES == 1
    /*-- RHT -- */ RHT_Produce_Volatile(total_devices);
    /*-- RHT -- */ RHT_Produce_Volatile(num_resistors);
    /*-- RHT -- */ RHT_Produce_Volatile(num_inductors);
    /*-- RHT -- */ RHT_Produce_Volatile(num_capacitors);
    /*-- RHT -- */ RHT_Produce_Volatile(num_voltage_sources);
#else
    /*-- RHT -- */ RHT_Produce(total_devices);
    /*-- RHT -- */ RHT_Produce(num_resistors);
    /*-- RHT -- */ RHT_Produce(num_inductors);
    /*-- RHT -- */ RHT_Produce(num_capacitors);
    /*-- RHT -- */ RHT_Produce(num_voltage_sources);
#endif
    /*-- RHT -- */ RHT_Produce_Volatile(num_current_sources);

    doc.add("Circuit_attributes", "");
    doc.get("Circuit_attributes")->add("Number_of_devices", total_devices);
    if (num_resistors > 0)
        doc.get("Circuit_attributes")->add("Resistors_(R)", num_resistors);
    if (num_inductors > 0)
        doc.get("Circuit_attributes")->add("Inductors_(L)", num_inductors);
    if (num_capacitors > 0)
        doc.get("Circuit_attributes")->add("Capacitors_(C)", num_capacitors);
    if (num_voltage_sources > 0)
        doc.get("Circuit_attributes")->add("Voltage_sources_(V)", num_voltage_sources);
    if (num_current_sources > 0)
        doc.get("Circuit_attributes")->add("Current_sources_(I)", num_current_sources);

    int num_my_rows = dae->A->end_row - dae->A->start_row + 1;
    int num_my_nnz = dae->A->local_nnz, sum_nnz = dae->A->local_nnz;
    int min_nnz = num_my_nnz, max_nnz = num_my_nnz;
    int min_rows = num_my_rows, max_rows = num_my_rows, sum_rows = num_my_rows;

#if JUST_VOLATILES == 1
    /*-- RHT -- */ RHT_Produce(sum_nnz);
    /*-- RHT -- */ RHT_Produce(min_nnz);
    /*-- RHT -- */ RHT_Produce(max_nnz);
    /*-- RHT -- */ RHT_Produce(sum_rows);
    /*-- RHT -- */ RHT_Produce(min_rows);
#else
    /*-- RHT -- */ RHT_Produce_Volatile(sum_nnz);
    /*-- RHT -- */ RHT_Produce_Volatile(min_nnz);
    /*-- RHT -- */ RHT_Produce_Volatile(max_nnz);
    /*-- RHT -- */ RHT_Produce_Volatile(sum_rows);
    /*-- RHT -- */ RHT_Produce_Volatile(min_rows);
#endif
    /*-- RHT -- */ RHT_Produce_Volatile(max_rows);

#ifdef HAVE_MPI
    MPI_Allreduce(&num_my_nnz, &sum_nnz, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&num_my_nnz, &min_nnz, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&num_my_nnz, &max_nnz, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    MPI_Allreduce(&num_my_rows, &sum_rows, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&num_my_rows, &min_rows, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&num_my_rows, &max_rows, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    RHT_Produce_NoCheck(sum_nnz);
    RHT_Produce_NoCheck(sum_rows);
#endif

    tempVar = (double) sum_rows / p;
    tempVar2 = (double) sum_nnz / p;
    /*-- RHT -- */ RHT_Produce_Volatile(tempVar);
    /*-- RHT -- */ RHT_Produce_Volatile(tempVar2);

    doc.add("Matrix_attributes", "");
    doc.get("Matrix_attributes")->add("Global_rows", sum_rows);
    doc.get("Matrix_attributes")->add("Rows_per_proc_MIN", min_rows);
    doc.get("Matrix_attributes")->add("Rows_per_proc_MAX", max_rows);
    doc.get("Matrix_attributes")->add("Rows_per_proc_AVG", tempVar);
    doc.get("Matrix_attributes")->add("Global_NNZ", sum_nnz);
    doc.get("Matrix_attributes")->add("NNZ_per_proc_MIN", min_nnz);
    doc.get("Matrix_attributes")->add("NNZ_per_proc_MAX", max_nnz);
    doc.get("Matrix_attributes")->add("NNZ_per_proc_AVG", tempVar2);

    // compute the initial condition if not specified by user
    int start_row = dae->A->start_row;
    int end_row = dae->A->end_row;
    tstart = mX_timer();

    if (!init_cond_specified) {
        std::vector<double> init_cond_guess;

        /*-- RHT -- */ RHT_Produce_NoCheck(num_my_rows);
        for (int i = 0; i < num_my_rows; i++) {
            init_cond_guess.push_back((double) (0));
        }

        /*-- RHT -- */ RHT_Produce_NoCheck(t_start);
        /*-- RHT -- */ std::vector<double> init_RHS = evaluate_b_producer(t_start, dae);
        /*-- RHT -- */ gmres_producer(dae->A, init_RHS, init_cond_guess, tol, res, k, init_cond, iters, restarts);

#if JUST_VOLATILES == 1
        /*-- RHT -- */ RHT_Produce_Volatile(tol);
        /*-- RHT -- */ RHT_Produce_Volatile(k);
        /*-- RHT -- */ RHT_Produce_Volatile(iters);
        /*-- RHT -- */ RHT_Produce_Volatile(restarts);
#else
        /*-- RHT -- */ RHT_Produce(tol);
        /*-- RHT -- */ RHT_Produce(k);
        /*-- RHT -- */ RHT_Produce(iters);
        /*-- RHT -- */ RHT_Produce(restarts);
#endif
        /*-- RHT -- */ RHT_Produce_Volatile(res);

        doc.add("DCOP Calculation", "");
        doc.get("DCOP Calculation")->add("Init_cond_specified", false);
        doc.get("DCOP Calculation")->add("GMRES_tolerance", tol);
        doc.get("DCOP Calculation")->add("GMRES_subspace_dim", k);
        doc.get("DCOP Calculation")->add("GMRES_iterations", iters);
        doc.get("DCOP Calculation")->add("GMRES_restarts", restarts);
        doc.get("DCOP Calculation")->add("GMRES_native_residual", res);
    } else {
        doc.add("DCOP Calculation", "");
        doc.get("DCOP Calculation")->add("Init_cond_specified", true);
    }

    tend = mX_timer() - tstart;
    doc.get("DCOP Calculation")->add("DCOP_calculation_time", tend);

    // write the headers and computed initial condition to file
    tstart = mX_timer();
    int dot_position = ckt_netlist_filename.find_first_of('.');

    std::string out_filename = ckt_netlist_filename.substr(0, dot_position) + "_tran_results.prn";
    std::ofstream *outfile = 0;

#ifdef HAVE_MPI
    // Prepare rcounts and displs for a contiguous gather of the full solution vector.
    std::vector<int> rcounts(p, 0), displs(p, 0);
    /*-- RHT -- */ RHT_Produce_Volatile(num_my_rows);
    MPI_Gather(&num_my_rows, 1, MPI_INT, &rcounts[0], 1, MPI_INT, 0, MPI_COMM_WORLD);
    int i, length = rcounts.size();
    for(i = 0; i < length; i++){
        /*-- RHT -- */ RHT_Produce_NoCheck(rcounts[i]);
    }

    for (i = 1; i < p; i++) {
        displs[i] = displs[i - 1] + rcounts[i - 1];
        /*-- RHT -- */ RHT_Produce(displs[i]);
    }

    std::vector<double> fullX(sum_rows, 0.0);
    MPI_Gatherv(&init_cond[0], num_my_rows, MPI_DOUBLE, &fullX[0], &rcounts[0], &displs[0], MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

    if (pid == 0) {
        outfile = new std::ofstream(out_filename.data(), std::ios::out);

        *outfile << std::setw(18) << "TIME";

        for (int i = 0; i < sum_rows; i++) {
            std::stringstream ss2;
            if (i < num_internal_nodes) {
                tempVar = i + 1;
                ss2 << "V";
            }
            else {
                tempVar = i + 1 - num_internal_nodes;
                ss2 << "I";
            }

            /*-- RHT -- */ RHT_Produce_Volatile(tempVar);
            ss2 << tempVar;

            *outfile << std::setw(18) << ss2.str();
        }

        *outfile << std::setw(20) << "num_GMRES_iters" << std::setw(20) << "num_GMRES_restarts" << std::endl;

        outfile->precision(8);

        *outfile << std::scientific << std::setw(18) << t_start;

        for (int i = 0; i < sum_rows; i++) {
#ifdef HAVE_MPI
            // not volatiled-checked because it was just sent to consumer side
            *outfile << std::setw(18) << fullX[i];
#else
            RHT_Produce_Volatile(init_cond[i]);
            *outfile << std::setw(18) << init_cond[i];
#endif
        }

        *outfile << std::fixed << std::setw(20) << iters << std::setw(20) << restarts << std::endl;
    }

    double io_tend = mX_timer() - tstart;

    // from now you won't be needing any more Ax = b solves
    // but you will be needing many (A + B/t_step)x = b solves
    // so change A to (A + B/t_step) right now
    // so you won't have to compute it at each time step

    tstart = mX_timer();
    distributed_sparse_matrix *A = dae->A;
    distributed_sparse_matrix *B = dae->B;

    std::vector<distributed_sparse_matrix_entry *>::iterator it1;
    int row_idx = start_row - 1;

    for (it1 = B->row_headers.begin(); it1 != B->row_headers.end(); it1++) {
        row_idx++;
        distributed_sparse_matrix_entry *curr = *it1;

        while (curr) {
            int col_idx = curr->column;
            double value = (curr->value) / t_step;
            /*-- RHT -- */ RHT_Produce(value);
            /*-- RHT -- */ distributed_sparse_matrix_add_to_producer(A, row_idx, col_idx, value, n, p);

            curr = curr->next_in_row;
        }
    }

    double matrix_setup_tend = mX_timer() - tstart;
    // this is where the actual transient simulation starts

    tstart = mX_timer();
    double t = t_start + t_step;
    double total_gmres_res = 0.0;
    int total_gmres_iters = 0;
    int trans_steps = 0;
    /*-- RHT -- */ RHT_Produce(t);
    /*-- RHT -- */ RHT_Produce(total_gmres_res);
    /*-- RHT -- */ RHT_Produce(total_gmres_iters);
    /*-- RHT -- */ RHT_Produce(trans_steps);

    while (t < t_stop) {
        trans_steps++;
        /*-- RHT -- */ RHT_Produce(trans_steps);

        // new time point t => new value for b(t)

        /*-- RHT -- */ std::vector<double> b = evaluate_b_producer(t, dae);

        // build the linear system Ax = b that needs to be solved at this time point
        // Backward Euler is used at every iteration

        std::vector<double> RHS;
        /*-- RHT -- */ sparse_matrix_vector_product_producer(B, init_cond, RHS);

        for (int i = 0; i < num_my_rows; i++) {
            RHS[i] /= t_step;
            RHS[i] += b[i];
            /*-- RHT -- */ RHT_Produce(RHS[i]);
        }

        // now solve the linear system just built using GMRES(k)
        /*-- RHT -- */ gmres_producer(A, RHS, init_cond, tol, res, k, init_cond, iters, restarts);

        total_gmres_iters += iters;
        total_gmres_res += res;
        /*-- RHT -- */ RHT_Produce(total_gmres_iters);
        /*-- RHT -- */ RHT_Produce(total_gmres_res);

        // write the results to file
        double io_tstart = mX_timer();
#ifdef HAVE_MPI
        MPI_Gatherv(&init_cond[0], num_my_rows, MPI_DOUBLE, &fullX[0], &rcounts[0], &displs[0], MPI_DOUBLE, 0,
                    MPI_COMM_WORLD);
#endif
        // write results to output file
        if (pid == 0) {
            outfile->precision(8);
            *outfile << std::scientific << std::setw(18) << t;

            for (int i = 0; i < sum_rows; i++) {
#ifdef HAVE_MPI
                *outfile << std::setw(18) << fullX[i];
#else
                *outfile << std::setw(18) << init_cond[i];
#endif
            }

            *outfile << std::fixed << std::setw(20) << iters << std::setw(20) << restarts << std::endl;
        }

        io_tend += (mX_timer() - io_tstart);

        // increment t
        t += t_step;
        /*-- RHT -- */ RHT_Produce(t);
    }

    // Hurray, the transient simulation is done!
    if (pid == 0) {
        outfile->close();
        delete outfile;
    }

    // Document transient simulation performance

    tend = mX_timer();
    double sim_end = tend - sim_start;


#if JUST_VOLATILES == 1
    /*-- RHT -- */ RHT_Produce_Volatile(trans_steps);
    /*-- RHT -- */ RHT_Produce_Volatile(tol);
    /*-- RHT -- */ RHT_Produce_Volatile(k);
    tempVar = total_gmres_iters / trans_steps;
    /*-- RHT -- */ RHT_Produce_Volatile(tempVar);
#else
    /*-- RHT -- */ RHT_Produce(trans_steps);
    /*-- RHT -- */ RHT_Produce(tol);
    /*-- RHT -- */ RHT_Produce(k);
    tempVar = total_gmres_iters / trans_steps;
    /*-- RHT -- */ RHT_Produce(tempVar);
#endif

    tempVar2 = total_gmres_res / trans_steps;
    /*-- RHT -- */ RHT_Produce_Volatile(tempVar2);

    doc.add("Transient Calculation", "");
    doc.get("Transient Calculation")->add("Number_of_time_steps", trans_steps);
    doc.get("Transient Calculation")->add("Time_start", t_start);
    doc.get("Transient Calculation")->add("Time_end", t_stop);
    doc.get("Transient Calculation")->add("Time_step", t_step);
    doc.get("Transient Calculation")->add("GMRES_tolerance", tol);
    doc.get("Transient Calculation")->add("GMRES_subspace_dim", k);
    doc.get("Transient Calculation")->add("GMRES_average_iters", tempVar);
    doc.get("Transient Calculation")->add("GMRES_average_res", tempVar2);
    doc.get("Transient Calculation")->add("Matrix_setup_time", matrix_setup_tend);
    doc.get("Transient Calculation")->add("Transient_calculation_time", tend - tstart);
    doc.add("I/O File Time", io_tend);
    doc.add("Total Simulation Time", sim_end);

    if (pid == 0) { // Only PE 0 needs to compute and report timing results
        std::string yaml = doc.generateYAML();
#if PRINT_OUTPUT == 1
        std::cout << yaml;
#endif
    }

    //dperez, this is where the replicated execution ends
    if(pid == 0) {
        clock_gettime(CLOCK_MONOTONIC, &endExe);

        elapsedExe = (endExe.tv_sec - startExe.tv_sec);
        elapsedExe += (endExe.tv_nsec - startExe.tv_nsec) / 1000000000.0;
    }

    // Clean up
    mX_linear_DAE_utils::destroy(dae);

#if APPROACH_WANG == 1
    // done replication but UNIT might not have been reached
    wangQueue.enqPtr = wangQueue.enqPtrLocal;
#endif

#if PERCENTAGE_OF_REPLICATION == 1
    //dperez, this is where all execution ends
    if(pid == 0) {
        clock_gettime(CLOCK_MONOTONIC, &endAll);

        elapsedAll = (endAll.tv_sec - startAll.tv_sec);
        elapsedAll += (endAll.tv_nsec - startAll.tv_nsec) / 1000000000.0;

        printf("The replication is %.5f %% of all the execution\n", elapsedExe / elapsedAll);
    }
#endif
    return elapsedExe;
}

void consumer_thread_func(void *args) {
    ConsumerParams *params = (ConsumerParams *) args;

    bool init_cond_specified;
    double tol, res, t_step, t_stop, t_start, tempVar, tempVar2;
    int n, k, iters, p, pid, restarts;
    int num_internal_nodes, num_voltage_sources, num_inductors;
    int num_current_sources, num_resistors, num_capacitors;
    std::vector<double> init_cond;
    std::string ckt_netlist_filename;

    SetThreadAffinity(params->executionCore);

    pid = params->pid;
    n = params->n;
    p = params->p;

    get_parms(params->argc, params->argv, ckt_netlist_filename, t_start, t_step, t_stop, tol, k, init_cond,
              init_cond_specified, p, pid);
    mX_linear_DAE *dae = parse_netlist(ckt_netlist_filename, p, pid, n, num_internal_nodes,
                                       num_voltage_sources,
                                       num_current_sources, num_resistors, num_capacitors, num_inductors);

    // document circuit and matrix attributes
    int total_devices = num_voltage_sources + num_current_sources + num_resistors + num_capacitors + num_inductors;

#if JUST_VOLATILES == 1
    RHT_Consume_Volatile((double)total_devices);
    RHT_Consume_Volatile((double)num_resistors);
    RHT_Consume_Volatile((double)num_inductors);
    RHT_Consume_Volatile((double)num_capacitors);
    RHT_Consume_Volatile((double)num_voltage_sources);
#else
    RHT_Consume_Check((double) total_devices);
    RHT_Consume_Check((double) num_resistors);
    RHT_Consume_Check((double) num_inductors);
    RHT_Consume_Check((double) num_capacitors);
    RHT_Consume_Check((double) num_voltage_sources);
#endif
    RHT_Consume_Volatile((double) num_current_sources);

    int num_my_rows = dae->A->end_row - dae->A->start_row + 1;
    /*-- RHT -- */ RHT_Consume_Check((double) num_my_rows);

    int num_my_nnz = dae->A->local_nnz, sum_nnz = dae->A->local_nnz;
    int min_nnz = num_my_nnz, max_nnz = num_my_nnz;
    int min_rows = num_my_rows, max_rows = num_my_rows, sum_rows = num_my_rows;

#if JUST_VOLATILES == 1
    /*-- RHT -- */ RHT_Consume_Check(sum_nnz);
    /*-- RHT -- */ RHT_Consume_Check(min_nnz);
    /*-- RHT -- */ RHT_Consume_Check(max_nnz);
    /*-- RHT -- */ RHT_Consume_Check(sum_rows);
    /*-- RHT -- */ RHT_Consume_Check(min_rows);
#else
    /*-- RHT -- */ RHT_Consume_Volatile(sum_nnz);
    /*-- RHT -- */ RHT_Consume_Volatile(min_nnz);
    /*-- RHT -- */ RHT_Consume_Volatile(max_nnz);
    /*-- RHT -- */ RHT_Consume_Volatile(sum_rows);
    /*-- RHT -- */ RHT_Consume_Volatile(min_rows);
#endif
    /*-- RHT -- */ RHT_Consume_Volatile(max_rows);

#ifdef HAVE_MPI
    //-- RHT - Not replicated -- MPI_Allreduce(&num_my_nnz, &sum_nnz, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    //-- RHT - Not replicated -- MPI_Allreduce(&num_my_nnz, &min_nnz, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    //-- RHT - Not replicated -- MPI_Allreduce(&num_my_nnz, &max_nnz, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    //-- RHT - Not replicated -- MPI_Allreduce(&num_my_rows, &sum_rows, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    //-- RHT - Not replicated -- MPI_Allreduce(&num_my_rows, &min_rows, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    //-- RHT - Not replicated -- MPI_Allreduce(&num_my_rows, &max_rows, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    sum_nnz = (int) RHT_Consume();
    sum_rows = (int) RHT_Consume();
#endif

    tempVar = (double) sum_rows / p;
    tempVar2 = (double) sum_nnz / p;
    /*-- RHT -- */ RHT_Consume_Volatile(tempVar);
    /*-- RHT -- */ RHT_Consume_Volatile(tempVar2);

    //doc.add("Matrix_attributes", "");
    //doc.get("Matrix_attributes")->add("Global_rows", sum_rows);
    //doc.get("Matrix_attributes")->add("Rows_per_proc_MIN", min_rows);
    //doc.get("Matrix_attributes")->add("Rows_per_proc_MAX", max_rows);
    //doc.get("Matrix_attributes")->add("Rows_per_proc_AVG", tempVar);
    //doc.get("Matrix_attributes")->add("Global_NNZ", sum_nnz);
    //doc.get("Matrix_attributes")->add("NNZ_per_proc_MIN", min_nnz);
    //doc.get("Matrix_attributes")->add("NNZ_per_proc_MAX", max_nnz);
    //doc.get("Matrix_attributes")->add("NNZ_per_proc_AVG", tempVar2);

    // compute the initial condition if not specified by user
    int start_row = dae->A->start_row;
    int end_row = dae->A->end_row;

    if (!init_cond_specified) {
        std::vector<double> init_cond_guess;

        /*-- RHT -- */ num_my_rows = RHT_Consume();
        for (int i = 0; i < num_my_rows; i++) {
            init_cond_guess.push_back((double) (0));
        }

        /*-- RHT -- */ t_start = RHT_Consume();
        /*-- RHT -- */ std::vector<double> init_RHS = evaluate_b_consumer(t_start, dae);
        /*-- RHT -- */ gmres_consumer(dae->A, init_RHS, init_cond_guess, tol, res, k, init_cond, iters, restarts);

#if JUST_VOLATILES == 1
        /*-- RHT -- */ RHT_Consume_Volatile(tol);
        /*-- RHT -- */ RHT_Consume_Volatile(k);
        /*-- RHT -- */ RHT_Consume_Volatile(iters);
        /*-- RHT -- */ RHT_Consume_Volatile(restarts);
#else
        /*-- RHT -- */ RHT_Consume_Check(tol);
        /*-- RHT -- */ RHT_Consume_Check(k);
        /*-- RHT -- */ RHT_Consume_Check(iters);
        /*-- RHT -- */ RHT_Consume_Check(restarts);
#endif
        /*-- RHT -- */ RHT_Consume_Volatile(res);

        //-- RHT - Not replicated -- doc.add("DCOP Calculation", "");
        //-- RHT - Not replicated -- doc.get("DCOP Calculation")->add("Init_cond_specified", false);
        //-- RHT - Not replicated -- doc.get("DCOP Calculation")->add("GMRES_tolerance", tol);
        //-- RHT - Not replicated -- doc.get("DCOP Calculation")->add("GMRES_subspace_dim", k);
        //-- RHT - Not replicated -- doc.get("DCOP Calculation")->add("GMRES_iterations", iters);
        //-- RHT - Not replicated -- doc.get("DCOP Calculation")->add("GMRES_restarts", restarts);
        //-- RHT - Not replicated -- doc.get("DCOP Calculation")->add("GMRES_native_residual", res);
    } else {
        //-- RHT - Not replicated -- doc.add("DCOP Calculation", "");
        //-- RHT - Not replicated -- doc.get("DCOP Calculation")->add("Init_cond_specified", true);
    }


#ifdef HAVE_MPI
    // Prepare rcounts and displs for a contiguous gather of the full solution vector.
    std::vector<int> rcounts(p, 0), displs(p, 0);
    /*-- RHT -- */ RHT_Consume_Volatile(num_my_rows);
//  NOT REPLICATED MPI_Gather(&num_my_rows, 1, MPI_INT, &rcounts[0], 1, MPI_INT, 0, MPI_COMM_WORLD);

    int i, length = rcounts.size();
    for (i = 0; i < length; i++) {
        /*-- RHT -- */ rcounts[i] = (int) RHT_Consume();
    }

    for (i = 1; i < p; i++) {
        displs[i] = displs[i - 1] + rcounts[i - 1];
        /*-- RHT -- */ RHT_Consume_Check(displs[i]);
    }

    std::vector<double> fullX(sum_rows, 0.0);
//  NOT REPLICATED  MPI_Gatherv(&init_cond[0], num_my_rows, MPI_DOUBLE, &fullX[0], &rcounts[0], &displs[0], MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

    if (pid == 0) {
        for (int i = 0; i < sum_rows; i++) {
            if (i < num_internal_nodes) {
                tempVar = i + 1;
            } else {
                tempVar = i + 1 - num_internal_nodes;
            }
            /*-- RHT -- */ RHT_Consume_Volatile(tempVar);
        }

        for (int i = 0; i < sum_rows; i++) {
#ifndef HAVE_MPI
            //            RHT_Consume_Volatile(init_cond[i]);
#endif
        }
    }

    // from now you won't be needing any more Ax = b solves
    // but you will be needing many (A + B/t_step)x = b solves
    // so change A to (A + B/t_step) right now
    // so you won't have to compute it at each time step

    distributed_sparse_matrix *A = dae->A;
    distributed_sparse_matrix *B = dae->B;

    std::vector<distributed_sparse_matrix_entry *>::iterator it1;
    int row_idx = start_row - 1;

    for (it1 = B->row_headers.begin(); it1 != B->row_headers.end(); it1++) {
        row_idx++;
        distributed_sparse_matrix_entry *curr = *it1;

        while (curr) {
            int col_idx = curr->column;
            double value = (curr->value) / t_step;
            /*-- RHT -- */ RHT_Consume_Check(value);
            /*-- RHT -- */ distributed_sparse_matrix_add_to_consumer(A, row_idx, col_idx, value, n, p);

            curr = curr->next_in_row;
        }
    }

    // this is where the actual transient simulation starts
    double t = t_start + t_step;
    double total_gmres_res = 0.0;
    int total_gmres_iters = 0;
    int trans_steps = 0;
    /*-- RHT -- */ RHT_Consume_Check(t);
    /*-- RHT -- */ RHT_Consume_Check(total_gmres_res);
    /*-- RHT -- */ RHT_Consume_Check(total_gmres_iters);
    /*-- RHT -- */ RHT_Consume_Check(trans_steps);

    while (t < t_stop) {
        trans_steps++;
        /*-- RHT -- */ RHT_Consume_Check(trans_steps);

        // new time point t => new value for b(t)

        /*-- RHT -- */ std::vector<double> b = evaluate_b_consumer(t, dae);

        // build the linear system Ax = b that needs to be solved at this time point
        // Backward Euler is used at every iteration

        std::vector<double> RHS;
        /*-- RHT -- */ sparse_matrix_vector_product_consumer(B, init_cond, RHS);

        for (int i = 0; i < num_my_rows; i++) {
            RHS[i] /= t_step;
            RHS[i] += b[i];
            /*-- RHT -- */ RHT_Consume_Check(RHS[i]);
        }

        // now solve the linear system just built using GMRES(k)
        /*-- RHT -- */ gmres_consumer(A, RHS, init_cond, tol, res, k, init_cond, iters, restarts);

        total_gmres_iters += iters;
        total_gmres_res += res;
        /*-- RHT -- */ RHT_Consume_Check(total_gmres_iters);
        /*-- RHT -- */ RHT_Consume_Check(total_gmres_res);

        // write the results to file
//        double io_tstart = mX_timer();
#ifdef HAVE_MPI
//        NOT REPLICATED -- MPI_Gatherv(&init_cond[0], num_my_rows, MPI_DOUBLE, &fullX[0], &rcounts[0], &displs[0], MPI_DOUBLE, 0,
//                    MPI_COMM_WORLD);
#endif

        // increment t
        t += t_step;
        /*-- RHT -- */ RHT_Consume_Check(t);
    }


    // Hurray, the transient simulation is done!
    // Document transient simulation performance

#if JUST_VOLATILES == 1
    /*-- RHT -- */ RHT_Consume_Volatile(trans_steps);
    /*-- RHT -- */ RHT_Consume_Volatile(tol);
    /*-- RHT -- */ RHT_Consume_Volatile(k);
    tempVar = total_gmres_iters / trans_steps;
    /*-- RHT -- */ RHT_Consume_Volatile(tempVar);
#else
    /*-- RHT -- */ RHT_Consume_Check(trans_steps);
    /*-- RHT -- */ RHT_Consume_Check(tol);
    /*-- RHT -- */ RHT_Consume_Check(k);
    tempVar = total_gmres_iters / trans_steps;
    /*-- RHT -- */ RHT_Consume_Check(tempVar);
#endif
    tempVar = total_gmres_res / trans_steps;
    /*-- RHT -- */ RHT_Consume_Volatile(tempVar);

    //doc.add("Transient Calculation", "");
    //doc.get("Transient Calculation")->add("Number_of_time_steps", trans_steps);
    //doc.get("Transient Calculation")->add("Time_start", t_start);
    //doc.get("Transient Calculation")->add("Time_end", t_stop);
    //doc.get("Transient Calculation")->add("Time_step", t_step);
    //doc.get("Transient Calculation")->add("GMRES_tolerance", tol);
    //doc.get("Transient Calculation")->add("GMRES_subspace_dim", k);
    //doc.get("Transient Calculation")->add("GMRES_average_iters", tempVar);
    //doc.get("Transient Calculation")->add("GMRES_average_res", tempVar2);
    //doc.get("Transient Calculation")->add("Matrix_setup_time", matrix_setup_tend);
    //doc.get("Transient Calculation")->add("Transient_calculation_time", tend - tstart);
    //doc.add("I/O File Time", io_tend);
    //doc.add("Total Simulation Time", sim_end);

    // Clean up
    mX_linear_DAE_utils::destroy(dae);
}
