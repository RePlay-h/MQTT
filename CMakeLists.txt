cmake_minimum_required(VERSION 3.2.4)

project(mqtt_server)

set(CMAKE_CXX_STANDARD 20)

find_package(Boost 1.81.0 COMPONENTS REQUIRED)

add_executable(mqtt_server main.cpp network/server.hpp network/server.cpp network/log/log.hpp utility/core.hpp utility/mqtt.hpp utility/mqtt.cpp utility/trie.hpp)

target_include_directories(mqtt_server PRIVATE ${Boost_INCLUDE_DIRS})

target_link_libraries(mqtt_server  ${Boost_LIBRARIES})


