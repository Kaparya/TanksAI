cmake_minimum_required(VERSION 3.15)
project(tanks_server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tanks_server
    server/main.cpp
    server/game_engine.cpp
    server/websocket_server.cpp
    server/http_server.cpp
)

target_compile_options(tanks_server PRIVATE -Wall -Wextra -O2)
