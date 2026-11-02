cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(unlisted_scraper LANGUAGES C CXX)

# Locate the Geode SDK: env var first, then a local sibling checkout, then in-repo.
if(DEFINED ENV{GEODE_SDK} AND EXISTS "$ENV{GEODE_SDK}/CMakeLists.txt")
    set(GEODE_SDK "$ENV{GEODE_SDK}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Geode/CMakeLists.txt")
    get_filename_component(GEODE_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../Geode" ABSOLUTE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/geode-sdk-clone/CMakeLists.txt")
    set(GEODE_SDK "${CMAKE_CURRENT_SOURCE_DIR}/geode-sdk-clone")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Geode/CMakeLists.txt")
    set(GEODE_SDK "${CMAKE_CURRENT_SOURCE_DIR}/Geode")
else()
    message(FATAL_ERROR "Geode SDK not found. Set the GEODE_SDK env var to a Geode checkout.")
endif()

message(STATUS "Using Geode SDK at ${GEODE_SDK}")

add_subdirectory(${GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/Geode)

add_library(${PROJECT_NAME} SHARED src/main.cpp)

# The SDK's link target has been renamed across versions; take whichever exists.
set(GEODE_LINK_TARGET "")
foreach(candidate Geode::Loader Geode::Geode Geode::GeodeLoader Geode)
    if(TARGET ${candidate})
        set(GEODE_LINK_TARGET ${candidate})
        break()
    endif()
endforeach()

if(NOT GEODE_LINK_TARGET)
    message(FATAL_ERROR "No Geode link target found - check the SDK's loader/CMakeLists.txt")
endif()

message(STATUS "Linking ${PROJECT_NAME} against ${GEODE_LINK_TARGET}")
target_link_libraries(${PROJECT_NAME} ${GEODE_LINK_TARGET})

setup_geode_mod(${PROJECT_NAME})
