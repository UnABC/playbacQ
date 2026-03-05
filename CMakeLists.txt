cmake_minimum_required(VERSION 3.20)
project(playbacQ 
	VERSION 0.0.1
	LANGUAGES CXX)

add_executable(main src/main.cpp)
target_compile_features(main PRIVATE cxx_std_23)
target_compile_options(main PRIVATE 
	-Wall -Wextra -Wpedantic -O3
)
