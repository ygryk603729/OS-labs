cmake_minimum_required(VERSION 3.14)
project(wolf_game LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")

add_executable(host_mq host.cpp conn_mq.cpp)
target_compile_definitions(host_mq PRIVATE USE_MQ=1)
target_link_libraries(host_mq rt pthread)

add_executable(host_fifo host.cpp conn_fifo.cpp)
target_compile_definitions(host_fifo PRIVATE USE_FIFO=1)
target_link_libraries(host_fifo pthread)

add_executable(host_sock host.cpp conn_sock.cpp)
target_compile_definitions(host_sock PRIVATE USE_SOCK=1)
target_link_libraries(host_sock pthread)