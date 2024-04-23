cmake_minimum_required(VERSION 3.0)
project(SQLiteApp)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")

find_package(SQLite3 REQUIRED)
include_directories(${SQLite3_INCLUDE_DIRS})
message(STATUS "SQLite3 library path: ${SQLite3_LIBRARIES}")

add_executable(app main.cpp)

target_link_libraries(app ${SQLite3_LIBRARIES})
