cmake_minimum_required(VERSION 3.5)

project(swupdate-handlers)

add_library(rootfs-handler OBJECT rootfs-handler.cpp)

target_include_directories(rootfs-handler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(rootfs-handler PUBLIC -Wall -Wextra -std=c++17)

install(TARGETS rootfs-handler DESTINATION lib)
