cmake_minimum_required(VERSION 3.9.4)
project(rainbow)

if(Launcher_QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 COMPONENTS Core Gui REQUIRED)
endif()

set(RAINBOW_SOURCES
src/rainbow.cpp
)

add_library(Launcher_rainbow STATIC ${RAINBOW_SOURCES})
target_include_directories(Launcher_rainbow PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(Launcher_rainbow Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui)
