cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(unlisted_scraper LANGUAGES C CXX)

# Locate the Geode SDK: env var first, then a local sibling checkout, then in-repo.
if(DEFINED ENV{GEODE_SDK} AND EXISTS "$ENV{GEODE_SDK}/CMakeLists.txt")
    set(GEODE_SDK "$ENV{GEODE_SDK}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Geode/CMakeLists.txt")
    get_filename_component(GEODE_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../Geode" ABSOLUTE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Geode/CMakeLists.txt")
    set(GEODE_SDK "${CMAKE_CURRENT_SOURCE_DIR}/Geode")
else()
    message(FATAL_ERROR "Geode SDK not found. Set the GEODE_SDK env var to a Geode checkout.")
endif()

message(STATUS "Using Geode SDK at ${GEODE_SDK}")

add_subdirectory(${GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/Geode)

add_library(${PROJECT_NAME} SHARED src/main.cpp)
target_link_libraries(${PROJECT_NAME} Geode::Geode)

setup_geode_mod(${PROJECT_NAME})
