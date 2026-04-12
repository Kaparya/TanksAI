cmake_minimum_required(VERSION 3.15)
project(tanks_server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tanks_server
    server/main.cpp
    server/core/game.cpp
    server/net/websocket_server.cpp
    server/net/http_server.cpp
    server/features/combat/collision.cpp
    server/features/combat/bullet_manager.cpp
    server/features/particles/particle_system.cpp
    server/features/map/map_generator.cpp
    server/features/players/player_manager.cpp
    server/features/enemies/enemy_ai.cpp
    server/features/enemies/wave_manager.cpp
    server/features/economy/shop.cpp
    server/net/serializer.cpp
    server/net/message_handler.cpp
    server/net/connection_manager.cpp
)

target_compile_options(tanks_server PRIVATE -Wall -Wextra -O2)
