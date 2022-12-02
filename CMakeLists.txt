cmake_minimum_required (VERSION 3.8)
project(NETWORKING LANGUAGES CXX)

if(WIN32)
endif(WIN32)

if(UNIX)
endif(UNIX)


add_library(CLIENT STATIC "src/client/client.cpp" "src/client/client.h")
add_library(SERVER STATIC "src/server/server.cpp" "src/server/server.h")
