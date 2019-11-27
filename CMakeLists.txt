# Минимальная версия CMake.
cmake_minimum_required(VERSION 3.7.2)

# Создание проекта.
project(microsha)

# Директории заголовков.
include_directories(include)

# Добавление файлов исходного кода.
file(GLOB SOURCES "source/*.cpp")
add_executable(microsha ${SOURCES})

# Флаги компиляции.
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wpedantic -Wextra -fexceptions -O0 -g3 -ggdb --std=c++17")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -Wextra -O3 --std=c++17")

target_link_libraries(microsha stdc++fs)
