cmake_minimum_required(VERSION 3.8)
project(miniXyceRHT)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES ../miniXyce_RHT/miniXyce_ref/mX_linear_ckt_simulator.cpp)

add_executable(miniXyceRHT ${SOURCE_FILES})