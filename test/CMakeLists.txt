cmake_minimum_required(VERSION 3.14)
project(playbacq_test CXX)

file(GLOB TEST_SRC *.cc *.cpp)

add_executable(${PROJECT_NAME} ${TEST_SRC})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Drogon::Drogon
    playbacq_core
)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}
)

# Register the test executable with CTest
enable_testing()
add_test(
    NAME playbacq_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/playbacq_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
