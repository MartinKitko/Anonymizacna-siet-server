cmake_minimum_required(VERSION 3.18)
project(AnonSietServer C)

set(CMAKE_C_STANDARD 99)

add_executable(AnonSietServer server_main.c server_definitions.c server_definitions.h downloader.c downloader.h)

target_link_libraries(AnonSietServer pthread curl)