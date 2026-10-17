cmake_minimum_required(VERSION 3.14)
project(BallisticCalculator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(ballistics
  src/main.cpp
  src/Atmosphere.cpp
  src/Ballistics.cpp
  src/Units.cpp
  src/InputFile.cpp
)

target_include_directories(ballistics PRIVATE include)

if(MSVC)
  target_compile_options(ballistics PRIVATE /W4)
else()
  target_compile_options(ballistics PRIVATE -Wall -Wextra -Wpedantic)
endif()