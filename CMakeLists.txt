cmake_minimum_required(VERSION 3.16)
project(cpp-reactor-server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g -O0 -fsanitize=address,undefined")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
include_directories(${CMAKE_SOURCE_DIR}/include)

add_library(reactor_net
    src/base/Buffer.cpp
    src/net/Channel.cpp
    src/net/EpollPoller.cpp
    src/net/EventLoop.cpp
    src/net/Acceptor.cpp
    src/net/TcpConnection.cpp
)

add_executable(reactor_skeleton examples/reactor-skeleton.cpp)
target_link_libraries(reactor_skeleton PRIVATE reactor_net)