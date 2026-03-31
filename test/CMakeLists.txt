cmake_minimum_required(VERSION 3.14)
project(playbacq_test CXX)

file(GLOB TEST_SRC *.cc *.cpp)

add_executable(${PROJECT_NAME} ${TEST_SRC})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Drogon::Drogon
    playbacq_core
)

option(BUILD_LOCAL_DEV "Build for local development with MinIO" OFF)
if(BUILD_LOCAL_DEV)
    target_compile_definitions(playbacq_core PRIVATE USE_INTERNAL_S3)
    target_compile_definitions(playbacq_worker PRIVATE USE_INTERNAL_S3)
    target_compile_definitions(playbacq_test PRIVATE USE_INTERNAL_S3)
    message(STATUS "Build Mode: LOCAL_DEV (MinIO enabled)")
else()
    message(STATUS "Build Mode: PRODUCTION (MinIO disabled)")
endif()

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

option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

if(ENABLE_COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g --coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")
    message(STATUS "Coverage is ENABLED")
endif()
