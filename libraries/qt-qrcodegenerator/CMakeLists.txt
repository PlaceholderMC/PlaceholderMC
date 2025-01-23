cmake_minimum_required(VERSION 3.6)

project(qrcode)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_C_STANDARD_REQUIRED true)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)


if(QT_VERSION_MAJOR EQUAL 5)
    find_package(Qt5 COMPONENTS Core Gui REQUIRED)
elseif(Launcher_QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 COMPONENTS Core Gui Core5Compat REQUIRED)
    list(APPEND systeminfo_LIBS Qt${QT_VERSION_MAJOR}::Core5Compat)
endif()

add_library(qrcode STATIC qr.h qr.cpp QR-Code-generator/cpp/qrcodegen.cpp QR-Code-generator/cpp/qrcodegen.hpp )

target_link_libraries(qrcode Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui ${systeminfo_LIBS})


# needed for statically linked qrcode in shared libs on x86_64
set_target_properties(qrcode
    PROPERTIES POSITION_INDEPENDENT_CODE TRUE
)

target_include_directories(qrcode PUBLIC ./ PRIVATE QR-Code-generator/cpp/)

