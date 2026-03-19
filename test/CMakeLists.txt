cmake_minimum_required(VERSION 3.14)
project(playbacq_test CXX)

file(GLOB_RECURSE TEST_SRC *.cc *.cpp)

add_executable(${PROJECT_NAME} ${TEST_SRC})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Drogon::Drogon
    playbacq_core
)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}
)

ParseAndAddDrogonTests(${PROJECT_NAME})
