cmake_minimum_required(VERSION 3.5)
project(miniXyceRHT)

set(CMAKE_BUILD_TYPE Release) # optimizations, but debuging is not allowed
#set(CMAKE_BUILD_TYPE Debug) # NO optimizations, debuging allowed

#set(CMAKE_VERBOSE_MAKEFILE ON)


find_package(MPI REQUIRED)

include_directories(${MPI_INCLUDE_PATH})

set(CMAKE_CXX_STANDARD 11)

#-ftree-vectorize

# mpi or not
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_MPI -DMPICH_IGNORE_CXX_SEEK")

# print output to console and files
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPRINT_OUTPUT=0")

# fast math or not, there is a problem when nan values are found when this options is enabled
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")

# General compilation flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funroll-all-loops") #just for gcc
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMINIXYCE_INFO=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBRANCH_HINT=1") # should always be true
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCOUNT_QUEUE_DESYNC=0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAPPROACH_NEW_ENQ_INDEX=0") # use with APPROACH_NEW_LIMIT

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPERCENTAGE_OF_REPLICATION=0")

# Modify this flags to test different approaches
set(FLAGS_CURRENT "")
set(FLAGS_CURRENT "${FLAGS_CURRENT} -DAPPROACH_USING_POINTERS=0")
set(FLAGS_CURRENT "${FLAGS_CURRENT} -DAPPROACH_ALREADY_CONSUMED=0")
set(FLAGS_CURRENT "${FLAGS_CURRENT} -DAPPROACH_NEW_LIMIT=1")
set(FLAGS_CURRENT "${FLAGS_CURRENT} -DAPPROACH_WRITE_INVERTED_NEW_LIMIT=0")
set(FLAGS_CURRENT "${FLAGS_CURRENT} -DVAR_GROUPING=1") # it can be used with any of the below approaches


set(SOURCE_FILES
        miniXyce_ref/mX_source.cpp
        miniXyce_ref/mX_linear_ckt_simulator.cpp
        miniXyce_ref/mX_source.h
        miniXyce_ref/mX_linear_DAE.cpp
        miniXyce_ref/mX_linear_DAE.h
        miniXyce_ref/mX_parms.cpp
        miniXyce_ref/mX_parms.h
        miniXyce_ref/mX_sparse_matrix.cpp
        miniXyce_ref/mX_sparse_matrix.h
        miniXyce_ref/mX_parser.cpp
        miniXyce_ref/mX_parser.h
        miniXyce_ref/YAML_Doc.cpp
        miniXyce_ref/YAML_Doc.hpp
        miniXyce_ref/YAML_Element.cpp
        miniXyce_ref/YAML_Element.hpp
        miniXyce_ref/mX_timer.cpp
        miniXyce_ref/mX_timer.h
        miniXyce_ref/RHT.cpp
        miniXyce_ref/RHT.h
        )

function(createExecutable exeName compFlags)
    add_executable(${exeName} ${SOURCE_FILES})
    set_target_properties(${exeName} PROPERTIES COMPILE_FLAGS "${compFlags}")
    target_link_libraries(${exeName} ${MPI_LIBRARIES})
    target_link_libraries(${exeName} pthread)
    target_link_libraries(${exeName} m) #math
    if(MPI_COMPILE_FLAGS)
        set_target_properties(${exeName} PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
    endif()

    if(MPI_LINK_FLAGS)
        set_target_properties(${exeName} PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
    endif()
endfunction(createExecutable)



#current, for testing
createExecutable("miniXyceRHT" ${FLAGS_CURRENT})

# using pointer flags
set(FLAGS_USING_POINTERS "-DAPPROACH_USING_POINTERS=1")
set(FLAGS_USING_POINTERS_VG " ${FLAGS_USING_POINTERS} -DVAR_GROUPING=1")

# using already consumed value flags
set(FLAGS_ALREADY_CONSUMED "-DAPPROACH_ALREADY_CONSUMED=1")
set(FLAGS_ALREADY_CONSUMED_VG " ${FLAGS_ALREADY_CONSUMED} -DVAR_GROUPING=1")

# using new limit approach
set(FLAGS_NEW_LIMIT "-DAPPROACH_NEW_LIMIT=1")
set(FLAGS_NEW_LIMIT_VG " ${FLAGS_NEW_LIMIT} -DVAR_GROUPING=1")

# using new limit write inverted approach
set(FLAGS_NEW_LIMIT_INVERTED "-DAPPROACH_WRITE_INVERTED_NEW_LIMIT=1")
set(FLAGS_NEW_LIMIT_INVERTED_VG " ${FLAGS_NEW_LIMIT_INVERTED} -DVAR_GROUPING=1")

# un comment the next lines to create multiple executables, with different configurations
#using pointers
#createExecutable("miniXyce-UP" ${FLAGS_USING_POINTERS})
#createExecutable("miniXyce-UP-VG" ${FLAGS_USING_POINTERS_VG})

# already consumed
createExecutable("miniXyce-AC" ${FLAGS_ALREADY_CONSUMED})
#createExecutable("miniXyce-AC-VG" ${FLAGS_ALREADY_CONSUMED_VG})

# new limit
#createExecutable("miniXyce-NL" ${FLAGS_NEW_LIMIT})
#createExecutable("miniXyce-NL-VG" ${FLAGS_NEW_LIMIT_VG})

# new limit inverted
#createExecutable("miniXyce-NLI" ${FLAGS_NEW_LIMIT_INVERTED})
#createExecutable("miniXyce-NLI-VG" ${FLAGS_NEW_LIMIT_INVERTED_VG})

