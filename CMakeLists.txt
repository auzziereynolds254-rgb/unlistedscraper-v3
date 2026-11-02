cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(unlisted_scraper LANGUAGES C CXX)

if(DEFINED ENV{GEODE_SDK} AND EXISTS "$ENV{GEODE_SDK}/CMakeLists.txt")
    set(GEODE_SDK "$ENV{GEODE_SDK}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Geode/CMakeLists.txt")
    get_filename_component(GEODE_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../Geode" ABSOLUTE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/geode-sdk-clone/CMakeLists.txt")
    set(GEODE_SDK "${CMAKE_CURRENT_SOURCE_DIR}/geode-sdk-clone")
else()
    message(FATAL_ERROR "Geode SDK not found.")
endif()

add_subdirectory(${GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/Geode)

add_library(${PROJECT_NAME} SHARED src/main.cpp)
setup_geode_mod(${PROJECT_NAME})

# Link the SDK if it exports a library target; the name varies across versions.
foreach(candidate Geode::Loader Geode::Geode Geode::SDK Geode GeodeLoader)
    if(TARGET ${candidate})
        target_link_libraries(${PROJECT_NAME} ${candidate})
        message(STATUS "Linked against ${candidate}")
        break()
    endif()
endforeach()
