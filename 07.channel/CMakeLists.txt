cmake_minimum_required(VERSION 3.21)
project(CppCoroutines-Tasks)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -latomic -pthread")
set(THREADS_PREFER_PTHREAD_FLAG ON)
add_executable("coroutine-task"
        main.cpp
        io_utils.cpp)