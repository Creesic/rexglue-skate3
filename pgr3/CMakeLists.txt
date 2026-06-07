
cmake_minimum_required(VERSION 3.25)

set(PGR3_PROJECT_LANGUAGES CXX)
if(APPLE)
    list(APPEND PGR3_PROJECT_LANGUAGES OBJC OBJCXX)
endif()
project(PGR3 LANGUAGES ${PGR3_PROJECT_LANGUAGES})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

set(PGR3_SOURCES
    src/main.cpp
)

if(WIN32)
    add_executable(pgr3 WIN32 ${PGR3_SOURCES})
else()
    add_executable(pgr3 ${PGR3_SOURCES})
endif()

if(APPLE)
    set_target_properties(pgr3 PROPERTIES
        XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
        XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS "YES"
    )
endif()

rexglue_setup_target(pgr3)
