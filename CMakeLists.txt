cmake_minimum_required(VERSION 3.16)

project(Chat_Room LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

set(COMMON_SOURCES
    src/common/epoll.cc
    src/common/logger.cc
    src/common/protocol.cc
    src/common/socket.cc
    src/common/tcp_connection.cc
)

set(SERVER_SOURCES
    src/server/server_main.cc
    src/server/server.cc
    src/server/user_manager.cc
)

set(CLIENT_SOURCES
    src/client/client_main.cc
    src/client/client.cc
)

add_executable(server
    ${SERVER_SOURCES}
    ${COMMON_SOURCES}
)

add_executable(client
    ${CLIENT_SOURCES}
    ${COMMON_SOURCES}
)

target_include_directories(server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(server
    PRIVATE
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)

target_link_libraries(client
    PRIVATE
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)

target_compile_options(server
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_compile_options(client
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)