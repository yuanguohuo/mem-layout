cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(mem_layout VERSION 1.0)

add_executable(basic src/test_basic.cpp)
target_include_directories(basic
	PRIVATE /home/yuanguo.hyg/local/boost-1.82.0/include
	PRIVATE /opt/homebrew/include)

#target_link_libraries(basic PRIVATE fmt)

add_executable(serialize src/test_serialize.cpp)
target_include_directories(serialize
	PRIVATE /home/yuanguo.hyg/local/boost-1.82.0/include
	PRIVATE /opt/homebrew/include)
#target_link_libraries(serialize PRIVATE fmt)

add_executable(alignment src/test_alignment.cpp)
target_include_directories(alignment
	PRIVATE /home/yuanguo.hyg/local/boost-1.82.0/include
	PRIVATE /opt/homebrew/include)
#target_link_libraries(alignment PRIVATE fmt)
