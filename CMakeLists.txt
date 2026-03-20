cmake_minimum_required(VERSION 3.14)
project(playbacq CXX)

cmake_policy(SET CMP0167 NEW)

include(CheckIncludeFileCXX)

check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)

# Enable coroutines support for the check
set(CMAKE_CXX_FLAGS_SAVE "${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
check_include_file_cxx(coroutine HAS_COROUTINE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_SAVE}")
if(NOT "${CMAKE_CXX_STANDARD}" STREQUAL "")
    # Do nothing
elseif(HAS_ANY AND HAS_STRING_VIEW AND HAS_COROUTINE)
    set(CMAKE_CXX_STANDARD 23)
elseif(HAS_ANY AND HAS_STRING_VIEW)
    set(CMAKE_CXX_STANDARD 17)
else()
    set(CMAKE_CXX_STANDARD 14)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Drogon CONFIG REQUIRED)
find_package(AWSSDK REQUIRED COMPONENTS s3)

file(GLOB SRC_DIR *.cpp)
list(REMOVE_ITEM SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

# for FFmpeg worker
add_executable(playbacq_worker worker/worker.cpp)
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(CURL REQUIRED)
target_link_libraries(playbacq_worker PRIVATE
    Boost::system
    Boost::filesystem
    hiredis
    redis++
    ${AWSSDK_LINK_LIBRARIES}
    zstd
    CURL::libcurl
)
target_include_directories(playbacq_worker PRIVATE ${Boost_INCLUDE_DIRS})

if(CMAKE_CXX_STANDARD LESS 17)
    message(FATAL_ERROR "c++17 or higher is required")
elseif(CMAKE_CXX_STANDARD LESS 23)
    message(STATUS "use c++17")
else()
    message(STATUS "use c++23")
endif()

aux_source_directory(controllers CTL_SRC)
aux_source_directory(filters FILTER_SRC)
aux_source_directory(plugins PLUGIN_SRC)
aux_source_directory(models MODEL_SRC)

# コアライブラリの作成
add_library(playbacq_core OBJECT
    ${SRC_DIR}
    ${CTL_SRC}
    ${FILTER_SRC}
    ${PLUGIN_SRC}
    ${MODEL_SRC}
)

target_link_libraries(playbacq_core PUBLIC
    Drogon::Drogon
    ${AWSSDK_LINK_LIBRARIES}
    zstd
)

drogon_create_views(playbacq_core ${CMAKE_CURRENT_SOURCE_DIR}/views
    ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(playbacq_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/models
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins
)

add_executable(${PROJECT_NAME} main.cpp)

# Enable testing
enable_testing()

# Add tests if BUILD_TESTING is enabled
if(BUILD_TESTING)
    add_subdirectory(test)
else()
    # Still need to add subdirectory for non-test builds
    # but mark it optional
endif()
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic -O3
)
target_link_libraries(${PROJECT_NAME} PRIVATE playbacq_core)
