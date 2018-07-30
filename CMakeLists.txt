cmake_minimum_required(VERSION 3.7)
project(miniXyceRHT)

set(CMAKE_BUILD_TYPE Release) # optimizations, but debuging is not allowed
#set(CMAKE_BUILD_TYPE Debug) # NO optimizations, debuging allowed

#set(CMAKE_VERBOSE_MAKEFILE ON)


find_package(MPI REQUIRED)

include_directories(${MPI_INCLUDE_PATH})

set(CMAKE_CXX_STANDARD 11)

# fast math or not, there is a problem when nan values are found when this options is enabled
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")

# mpi or not
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_MPI -DMPICH_IGNORE_CXX_SEEK -DUSING_MPI")

# General compilation flags
add_definitions(-ftree-vectorize -flto -funroll-all-loops -mavx2 -lm)
add_definitions(-DMINIXYCE_INFO=1 -DPRINT_OUTPUT=0 -DPERCENTAGE_OF_REPLICATION=0)

# These are other approaches of the literature
#add_definitions(-DAPPROACH_USING_POINTERS=1)
#add_definitions(-DAPPROACH_ALREADY_CONSUMED=1)
add_definitions(-DAPPROACH_WANG=1)

#add_definitions(-DJUST_VOLATILES=1)

# These are our approaches
#add_definitions(-DAPPROACH_MIX_WANG=1)
#add_definitions(-DAPPROACH_MIX_IMPROVED=1)
#add_definitions(-DVAR_GROUPING=1) # it can be used with any of the above approaches

#add_definitions(-DOUR_IMPROVEMENTS=1)

# no longer used
#add_definitions(-DAPPROACH_NEW_LIMIT=1)
#add_definitions(-DAPPROACH_WRITE_INVERTED_NEW_LIMIT=1)

#Init flags current
set(FLAGS_CURRENT "anything")

set(SOURCE_FILES
        miniXyce_ref/mX_source.cpp miniXyce_ref/mX_linear_ckt_simulator.cpp
        miniXyce_ref/mX_source.h
        miniXyce_ref/mX_linear_DAE.cpp miniXyce_ref/mX_linear_DAE.h
        miniXyce_ref/mX_parms.cpp miniXyce_ref/mX_parms.h
        miniXyce_ref/mX_sparse_matrix.cpp miniXyce_ref/mX_sparse_matrix.h
        miniXyce_ref/mX_parser.cpp miniXyce_ref/mX_parser.h
        miniXyce_ref/YAML_Doc.cpp miniXyce_ref/YAML_Doc.hpp
        miniXyce_ref/YAML_Element.cpp miniXyce_ref/YAML_Element.hpp
        miniXyce_ref/mX_timer.cpp miniXyce_ref/mX_timer.h
        miniXyce_ref/RHT.cpp miniXyce_ref/RHT.h
        )

function(createExecutable exeName compFlags varFlag ourImprovs)
    add_executable(${exeName} ${SOURCE_FILES})
    #this line causes problems when used with cmake 3.7 (version of g5k machines)
    #set_target_properties(${exeName} PROPERTIES COMPILE_FLAGS "    ${compFlags}")
    set_property(TARGET ${exeName} APPEND PROPERTY COMPILE_DEFINITIONS ${compFlags})
    if(varFlag EQUAL 1)
        set_property(TARGET ${exeName} APPEND PROPERTY COMPILE_DEFINITIONS "VAR_GROUPING=1")
    endif()
    if(varFlag EQUAL 2)
        set_property(TARGET ${exeName} APPEND PROPERTY COMPILE_DEFINITIONS "JUST_VOLATILES=1")
    endif()
    if(ourImprovs EQUAL 1)
        set_property(TARGET ${exeName} APPEND PROPERTY COMPILE_DEFINITIONS "OUR_IMPROVEMENTS=1")
    endif()
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
createExecutable("miniXyce-Current" ${FLAGS_CURRENT} 2 1)

# using pointer flags

set(FLAGS_USING_POINTERS "APPROACH_USING_POINTERS=1")

# using already consumed value flags
set(FLAGS_ALREADY_CONSUMED "APPROACH_ALREADY_CONSUMED=1")

# using WANG value flags
set(FLAGS_WANG "APPROACH_WANG=1")

# using no sync consumer approachh
set(FLAGS_NO_SYNC_CONSUMER "APPROACH_CONSUMER_NO_SYNC=1")

# using new limit approach (includes the previous)
set(FLAGS_NEW_LIMIT "APPROACH_NEW_LIMIT=1")

# using new limit write inverted approach
set(FLAGS_NEW_LIMIT_INVERTED "APPROACH_WRITE_INVERTED_NEW_LIMIT=1")

# using mix-wang approach
set(FLAGS_MIX_WANG "APPROACH_MIX_WANG=1")

# un comment the next lines to create multiple executables, with different configurations
#using pointers
#createExecutable("miniXyce-UP" ${FLAGS_USING_POINTERS} 0)
#createExecutable("miniXyce-UP-VG" ${FLAGS_USING_POINTERS} 1)

# already consumed/home/diego/Documents/workspace/miniXyce_RHT/tests
#createExecutable("miniXyce-AC" ${FLAGS_ALREADY_CONSUMED} 0)
#createExecutable("miniXyce-AC-VG" ${FLAGS_ALREADY_CONSUMED} 1)

# WANG
#createExecutable("miniXyce-Wang" ${FLAGS_WANG} 0 0)
#createExecutable("miniXyce-Wang-VG" ${FLAGS_WANG} 1 0)
#createExecutable("miniXyce-Wang-JV" ${FLAGS_WANG} 2 0)

#createExecutable("IMP-miniXyce-Wang" ${FLAGS_WANG} 0 1)
#createExecutable("IMP-miniXyce-Wang-VG" ${FLAGS_WANG} 1 1)
#createExecutable("IMP-miniXyce-Wang-JV" ${FLAGS_WANG} 2 1)

# no sync consumer
#createExecutable("miniXyce-NSC" ${FLAGS_NO_SYNC_CONSUMER} 0)
#createExecutable("miniXyce-NSC-VG" ${FLAGS_NO_SYNC_CONSUMER} 1)

# new limit
#createExecutable("miniXyce-NL" ${FLAGS_NEW_LIMIT} 0)
#createExecutable("miniXyce-NL-VG" ${FLAGS_NEW_LIMIT} 1)

# new limit inverted
#createExecutable("miniXyce-NLI" ${FLAGS_NEW_LIMIT_INVERTED} 0)
#createExecutable("miniXyce-NLI-VG" ${FLAGS_NEW_LIMIT_INVERTED} 1)

# # mix wang approach
#createExecutable("miniXyce-MIX" ${FLAGS_MIX_WANG} 0)
#createExecutable("miniXyce-MIX-VG" ${FLAGS_MIX_WANG} 1)

# Best approach
#createExecutable("miniXyce-RHT" ${FLAGS_NEW_LIMIT} 1)
