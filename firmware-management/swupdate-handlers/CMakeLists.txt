cmake_minimum_required(VERSION 3.5)

project(swupdate-handlers)

add_library(rootfs-handler OBJECT rootfs-handler.c common.c)

target_include_directories(rootfs-handler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(rootfs-handler PUBLIC -Wall -Wextra)

#install(TARGETS rootfs-handler LIBRARY DESTINATION lib COMPONENT library)
