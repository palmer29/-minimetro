cmake_minimum_required(VERSION 3.14)
project(Minimetro VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 11)

# Включаем FetchContent для автоматической загрузки raylib
include(FetchContent)

# Проверяем, установлен ли raylib локально
find_package(raylib QUIET)

if(NOT raylib_FOUND)
    message(STATUS "raylib not found, downloading from GitHub...")
    
    # Загружаем raylib из репозитория
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.0  # или master для последней версии
    )
    
    # Настройки для сборки raylib
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(raylib)
endif()

# Создаём исполняемый файл
add_executable(minimetro src/main.cpp)

target_link_libraries(minimetro PRIVATE raylib)