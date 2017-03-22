cmake_minimum_required(VERSION 3.6)
project(SIGMOD_2017_Contest)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        compile.sh
        constant.h
        find.h
        harness.cpp
        main.cpp
        Makefile
        README
        run.sh
        token.h
        trie.h)

add_executable(SIGMOD_2017_Contest ${SOURCE_FILES})