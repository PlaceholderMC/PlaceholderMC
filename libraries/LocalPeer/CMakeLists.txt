cmake_minimum_required(VERSION 3.9.4)
project(LocalPeer)

if(Launcher_QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 COMPONENTS Core Network Core5Compat REQUIRED)
    list(APPEND LocalPeer_LIBS Qt${QT_VERSION_MAJOR}::Core5Compat)
endif()

set(SINGLE_SOURCES
src/LocalPeer.cpp
src/LockedFile.cpp
src/LockedFile.h
include/LocalPeer.h
)

if(UNIX)
    list(APPEND SINGLE_SOURCES
        src/LockedFile_unix.cpp
    )
endif()

if(WIN32)
    list(APPEND SINGLE_SOURCES
        src/LockedFile_win.cpp
    )
endif()

add_library(LocalPeer STATIC ${SINGLE_SOURCES})
target_include_directories(LocalPeer PUBLIC include)

target_link_libraries(LocalPeer Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network ${LocalPeer_LIBS})
