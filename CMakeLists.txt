cmake_minimum_required(VERSION 3.8)
project(2018080601epoll_framework)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES main.cpp Epoll.cpp Epoll.h)
add_executable(2018080601epoll_framework ${SOURCE_FILES})