cmake_minimum_required(VERSION 3.16)
project(FreeRTOSVehicleBodyController LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(body_controller
    src/body_controller.cpp
)
target_include_directories(body_controller PUBLIC include)

target_compile_options(body_controller PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Werror>
)

add_executable(body_controller_sim sim/main.cpp)
target_link_libraries(body_controller_sim PRIVATE body_controller)

add_executable(body_controller_tests tests/test_body_controller.cpp)
target_link_libraries(body_controller_tests PRIVATE body_controller)

enable_testing()
add_test(NAME body_controller_tests COMMAND body_controller_tests)
