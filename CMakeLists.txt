cmake_minimum_required (VERSION 3.8)
project(NETWORKING LANGUAGES CXX)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(CLIENT STATIC "src/client/client.cpp" "src/client/client.h")
add_library(SERVER STATIC "src/server/server.cpp" "src/server/server.h" "src/server/networkexception.h" "src/server/errorcodes.cpp" "src/server/errorcodes.h" "src/server/logger.h" "src/server/logger.cpp")
target_include_directories(SERVER PRIVATE src/server)
