cmake_minimum_required(VERSION 3.10)
project(SQLiteApp)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")

find_package(SQLite3 REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem)

if(SQLite3_FOUND AND Boost_FOUND)    
    message(STATUS "SQLite3 library path: ${SQLite3_LIBRARIES}")
    message(STATUS "Boost found: ${Boost_VERSION}")

    include_directories(${SQLite3_INCLUDE_DIRS})
    include_directories(${Boost_INCLUDE_DIRS})

    add_executable(app main.cpp)

    target_link_libraries(app Boost::filesystem)
    target_link_libraries(app ${SQLite3_LIBRARIES})
else()
    message(FATAL_ERROR "Required dependencies (SQLite3 or Boost) not found. Please install missing dependencies.")
endif()
