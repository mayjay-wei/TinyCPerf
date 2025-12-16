cmake_minimum_required(VERSION 4.0)
project(TinyCPerf C)

set(CMAKE_C_STANDARD 11)

include_directories(inc)

add_executable(TinyCPerf
        inc/c_profiler.h
        src/main.c)

target_link_libraries(TinyCPerf PRIVATE m)

#add_compile_options(-finstrument-functions)
