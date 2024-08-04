cmake_minimum_required(VERSION 3.20)

project(bjit)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
if ( NOT CMAKE_BUILD_TYPE )
    message( "Build Type not set, defaulting to Debug..." )
    set( CMAKE_BUILD_TYPE Debug )
endif()

if (CMAKE_BUILD_TYPE EQUAL "Debug")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
endif()

if (CMAKE_BUILD_TYPE EQUAL "Release")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
endif()

file(GLOB SRC_FILES src/*.c)
include_directories(include/)
add_executable(bjit ${SRC_FILES})

target_compile_definitions(${PROJECT_NAME} PRIVATE "DEBUG=$<IF:$<CONFIG:Debug>,1,0>")
add_compile_definitions("DEBUG=$<CONFIG:Debug>")

enable_testing()

add_test(NAME hello_world COMMAND bjit ../bf_tests/hello.bf)
set_tests_properties(hello_world PROPERTIES PASS_REGULAR_EXPRESSION "Hello World!")
