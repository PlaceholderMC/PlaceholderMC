project(systeminfo)

if(Launcher_QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 COMPONENTS Core Core5Compat REQUIRED)
    list(APPEND systeminfo_LIBS Qt${QT_VERSION_MAJOR}::Core5Compat)
endif()

set(systeminfo_SOURCES
include/sys.h
include/distroutils.h
src/distroutils.cpp
)

if (WIN32)
    list(APPEND systeminfo_SOURCES src/sys_win32.cpp)
elseif (UNIX)
    if(APPLE)
        list(APPEND systeminfo_SOURCES src/sys_apple.cpp)
    else()
        list(APPEND systeminfo_SOURCES src/sys_unix.cpp)
    endif()
endif()

add_library(systeminfo STATIC ${systeminfo_SOURCES})
target_link_libraries(systeminfo Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Network ${systeminfo_LIBS})
target_include_directories(systeminfo PUBLIC include)

ecm_add_test(src/sys_test.cpp LINK_LIBRARIES systeminfo Qt${QT_VERSION_MAJOR}::Test TEST_NAME sys)
