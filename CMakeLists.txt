cmake_minimum_required(VERSION 3.8)
project(miniXyceRHT)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES main.cpp
        generate_matrix.cpp generate_matrix.h
        HPC_Sparse_Matrix.cpp HPC_Sparse_Matrix.h
        compute_residual.cpp compute_residual.h
        ddot.cpp ddot.h
        dump_matlab_matrix.cpp dump_matlab_matrix.h
        exchange_externals.cpp exchange_externals.h
        HPCCG.cpp HPCCG.h
        HPC_sparsemv.cpp HPC_sparsemv.h
        make_local_matrix.cpp make_local_matrix.h
        mytimer.cpp mytimer.h
        read_HPC_row.cpp read_HPC_row.h
        waxpby.cpp waxpby.h
        YAML_Doc.cpp YAML_Doc.h
        YAML_Element.cpp YAML_Element.h
        RHT.h RHT.cpp atomicops.h readerwriterqueue.h QueueStressTest.cpp QueueStressTest.h miniXyce_ref/distributed_sparse_matrix_add_to.cpp miniXyce_ref/distributed_sparse_matrix_add_to.h)

add_executable(miniXyceRHT ${SOURCE_FILES})