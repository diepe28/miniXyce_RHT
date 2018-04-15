cmake_minimum_required(VERSION 3.8)
project(miniXyceRHT)

#set(CMAKE_BUILD_TYPE Release) # optimizations, but debuging is not allowed
set(CMAKE_BUILD_TYPE Debug) # NO optimizations, debuging allowed

#set(CMAKE_VERBOSE_MAKEFILE ON)


find_package(MPI REQUIRED)

include_directories(${MPI_INCLUDE_PATH})

set(CMAKE_CXX_STANDARD 11)

#-ftree-vectorize

#-DUSING_MPI=1
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funroll-all-loops") #just for gcc

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_MPI -DMPICH_IGNORE_CXX_SEEK")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMINIXYCE_INFO=1 -DPRINT_OUTPUT=0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAPPROACH_USING_POINTERS=0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAPPROACH_ALREADY_CONSUMED=0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAPPROACH_NEW_LIMIT=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAPPROACH_WRITE_INVERTED_NEW_LIMIT=0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAPPROACH_NEW_ENQ_INDEX=0") # use with APPROACH_NEW_LIMIT
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAPPROACH_MOODY_CAMEL=0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCOUNT_QUEUE_DESYNC=0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBRANCH_HINT=1") # should always be true
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVAR_GROUPING=1") #only works for new limit and write inverted new limit


set(SOURCE_FILES ../miniXyce_RHT/miniXyce_ref/mX_linear_ckt_simulator.cpp
        ../miniXyce_RHT/miniXyce_ref/mX_source.cpp
        ../miniXyce_RHT/miniXyce_ref/mX_source.h
        ../miniXyce_RHT/miniXyce_ref/mX_linear_DAE.cpp
        ../miniXyce_RHT/miniXyce_ref/mX_linear_DAE.h
        ../miniXyce_RHT/miniXyce_ref/mX_parms.cpp
        ../miniXyce_RHT/miniXyce_ref/mX_parms.h
        ../miniXyce_RHT/miniXyce_ref/mX_sparse_matrix.cpp
        ../miniXyce_RHT/miniXyce_ref/mX_sparse_matrix.h
        ../miniXyce_RHT/miniXyce_ref/mX_parser.cpp
        ../miniXyce_RHT/miniXyce_ref/mX_parser.h
        ../miniXyce_RHT/miniXyce_ref/YAML_Doc.cpp
        ../miniXyce_RHT/miniXyce_ref/YAML_Doc.hpp
        ../miniXyce_RHT/miniXyce_ref/YAML_Element.cpp
        ../miniXyce_RHT/miniXyce_ref/YAML_Element.hpp
        ../miniXyce_RHT/miniXyce_ref/mX_timer.cpp
        ../miniXyce_RHT/miniXyce_ref/mX_timer.h
        ../miniXyce_RHT/miniXyce_ref/RHT.cpp
        ../miniXyce_RHT/miniXyce_ref/RHT.h
        )

#../miniXyce_RHT/miniXyce_ref/miniXyce_info.hpp

add_executable(miniXyceRHT ${SOURCE_FILES})

target_link_libraries(miniXyceRHT ${MPI_LIBRARIES})

target_link_libraries(miniXyceRHT pthread)

if(MPI_COMPILE_FLAGS)
    set_target_properties(miniXyceRHT PROPERTIES
            COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
endif()

if(MPI_LINK_FLAGS)
    set_target_properties(miniXyceRHT PROPERTIES
            LINK_FLAGS "${MPI_LINK_FLAGS}")
endif()