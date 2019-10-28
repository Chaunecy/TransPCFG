cmake_minimum_required(VERSION 3.15)
project(TransPCFG)

set(CMAKE_CXX_STANDARD 11)

add_executable(train transfer_learning_train.cpp)
add_executable(guess transfer_learning_guess.cpp)