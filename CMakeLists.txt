cmake_minimum_required(VERSION 3.20)
project(visa_vista_api LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CROW_ENABLE_SSL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  Crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG v1.2.1
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Crow)

find_package(SQLite3 REQUIRED)
add_executable(visa_vista_api server.cpp)
target_link_libraries(visa_vista_api PRIVATE Crow::Crow SQLite::SQLite3)
target_compile_options(visa_vista_api PRIVATE -Wall -Wextra -Wpedantic)
