cmake_minimum_required(VERSION 3.10)
project(capyJs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE EQUAL "Release")
    add_compile_options(-O2)
endif()

include_directories(include)

add_executable(capy
 src/capy_main.cc
)
 
add_library(server src/server.cc)
target_link_libraries(capy server uv)


# -- Tests ---------------------------------------------
add_executable(mem_pool_test
    tests/mem_pool_test.cc
)

add_executable(mem_pool_perf
    tests/mem_pool_perf.cc
)

add_executable(client_state_test
    tests/client_state_test.cc
)

target_link_libraries(client_state_test uv)