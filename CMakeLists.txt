cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(unlisted_scraper LANGUAGES C CXX)

# Locate the Geode SDK: env var first, then known clone locations.
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

# Diagnostic: show what the SDK actually exports.
get_property(_sdk_targets DIRECTORY "${GEODE_SDK}/loader" PROPERTY BUILDSYSTEM_TARGETS)
message(STATUS "SDK loader targets: ${_sdk_targets}")

setup_geode_mod(${PROJECT_NAME})

set(GEODE_LINK_TARGET "")
foreach(candidate Geode::Loader Geode::Geode Geode::SDK Geode::GeodeLoader Geode)
    if(TARGET ${candidate})
        set(GEODE_LINK_TARGET ${candidate})
        break()
    endif()
endforeach()

if(GEODE_LINK_TARGET)
    message(STATUS "Linking ${PROJECT_NAME} against ${GEODE_LINK_TARGET}")
    target_link_libraries(${PROJECT_NAME} ${GEODE_LINK_TARGET})
else()
    message(WARNING "No Geode link target found; relying on setup_geode_mod")
endif()
