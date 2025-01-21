cmake_minimum_required(VERSION 3.15)  # minimum version required by QuaZip

project(Launcher)

string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BUILD_DIR}" IS_IN_SOURCE_BUILD)
if(IS_IN_SOURCE_BUILD)
    message(FATAL_ERROR "You are building the Launcher in-source. Please separate the build tree from the source tree.")
endif()

##################################### Set CMake options #####################################
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

# Output all executables and shared libs in the main build folder, not in subfolders.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
if(UNIX)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
endif()
set(CMAKE_JAVA_TARGET_OUTPUT_DIR ${PROJECT_BINARY_DIR}/jars)

######## Set compiler flags ########
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_C_STANDARD_REQUIRED true)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
include(GenerateExportHeader)
if(MSVC)
    # /GS Adds buffer security checks, default on but incuded anyway to mirror gcc's fstack-protector flag
    # /permissive- specify standards-conforming compiler behavior, also enabled by Qt6, default on with std:c++20
    # Use /W4 as /Wall includes unnesserey warnings such as added padding to structs
    set(CMAKE_CXX_FLAGS "/GS /permissive- /W4 ${CMAKE_CXX_FLAGS}")

    # /EHs Enables stack unwind semantics for standard C++ exceptions to ensure stackframes are unwound
    # and object deconstructors are called when an exception is caught.
    # without it memory leaks and a warning is printed
    # /EHc tells the compiler to assume that functions declared as extern "C" never throw a C++ exception
    # This appears to not always be a defualt compiler option in CMAKE
    set(CMAKE_CXX_FLAGS "/EHsc ${CMAKE_CXX_FLAGS}")

    # LINK accepts /SUBSYSTEM whics sets if we are a WINDOWS (gui) or a CONSOLE programs
    # This implicitly selects an entrypoint specific to the subsystem selected
    # qtmain/QtEntryPointLib provides the correct entrypoint (wWinMain) for gui programs
    # Additinaly LINK autodetects we use a GUI so we can omit /SUBSYSTEM
    # This allows tests to still use have console without using seperate linker flags
    # /LTCG allows for linking wholy optimizated programs
    # /MANIFEST:NO disables generating a manifest file, we instead provide our own
    # /STACK sets the stack reserve size, ATL's pack list needs 3-4 MiB as of November 2022, provide 8 MiB
    set(CMAKE_EXE_LINKER_FLAGS "/LTCG /MANIFEST:NO /STACK:8388608 ${CMAKE_EXE_LINKER_FLAGS}")

    # /GL enables whole program optimizations
    # /Gw helps reduce binary size
    # /Gy allows the compiler to package individual functions
    # /guard:cf enables control flow guard
    foreach(lang C CXX)
        set("CMAKE_${lang}_FLAGS_RELEASE" "/GL /Gw /Gy /guard:cf")
    endforeach()

    # See https://github.com/ccache/ccache/issues/1040
    # Note, CMake 3.25 replaces this with CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
    # See https://cmake.org/cmake/help/v3.25/variable/CMAKE_MSVC_DEBUG_INFORMATION_FORMAT.html
    foreach(config DEBUG RELWITHDEBINFO)
        foreach(lang C CXX)
            set(flags_var "CMAKE_${lang}_FLAGS_${config}")
            string(REGEX REPLACE "/Z[Ii]" "/Z7" ${flags_var} "${${flags_var}}")
        endforeach()
    endforeach()

    if(CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreadedDLL")
        set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG Release "")
        set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release "")
    endif()
else()
    set(CMAKE_CXX_FLAGS "-Wall -pedantic -fstack-protector-strong --param=ssp-buffer-size=4 ${CMAKE_CXX_FLAGS}")

    # ATL's pack list needs more than the default 1 Mib stack on windows
    if(WIN32)
        set(CMAKE_EXE_LINKER_FLAGS "-Wl,--stack,8388608 ${CMAKE_EXE_LINKER_FLAGS}")
    endif()
endif()

# Fix build with Qt 5.13
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DQT_NO_DEPRECATED_WARNINGS=Y")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DQT_DISABLE_DEPRECATED_BEFORE=0x050C00")

# Fix aarch64 build for toml++
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTOML_ENABLE_FLOAT16=0")

# set CXXFLAGS for build targets
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -D_FORTIFY_SOURCE=2 ${CMAKE_CXX_FLAGS_RELEASE}")

option(DEBUG_ADDRESS_SANITIZER "Enable Address Sanitizer in Debug builds" OFF)

# If this is a Debug build turn on address sanitiser
if ((CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo") AND DEBUG_ADDRESS_SANITIZER)
    message(STATUS "Address Sanitizer enabled for Debug builds, Turn it off with -DDEBUG_ADDRESS_SANITIZER=off")
    if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
        if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            # using clang with clang-cl front end
            message(STATUS "Address Sanitizer available on Clang MSVC frontend")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address /Oy-")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=address /Oy-")
        else()
            # AppleClang and Clang
            message(STATUS "Address Sanitizer available on Clang")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
        endif()
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # GCC
        message(STATUS "Address Sanitizer available on GCC")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
        link_libraries("asan")
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        message(STATUS "Address Sanitizer available on MSVC")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address /Oy-")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=address /Oy-")
    else()
        message(STATUS "Address Sanitizer not available on compiler ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()


option(ENABLE_LTO "Enable Link Time Optimization" off)

if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
        if(CMAKE_BUILD_TYPE)
            if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
                message(STATUS "IPO / LTO enabled")
            else()
                message(STATUS "Not enabling IPO / LTO on debug builds")
            endif()
        else()
            message(STATUS "IPO / LTO will only be enabled for release builds")
        endif()
    else()
        message(STATUS "IPO / LTO not supported: <${ipo_error}>")
    endif()
endif()

option(BUILD_TESTING "Build the testing tree." ON)

find_package(ECM QUIET NO_MODULE)
if(NOT ECM_FOUND)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libraries/extra-cmake-modules/CMakeLists.txt")
        message(STATUS "Using bundled ECM")
        set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libraries/extra-cmake-modules/modules;${CMAKE_MODULE_PATH}")
    else()
        message(FATAL_ERROR
            " Could not find ECM\n \n"
            " Either install ECM using the system package manager or clone submodules\n"
            " Submodules can be cloned with 'git submodule update --init --recursive'")
    endif()
else()
    set(CMAKE_MODULE_PATH "${ECM_MODULE_PATH};${CMAKE_MODULE_PATH}")
endif()
include(CTest)
include(ECMAddTests)
if(BUILD_TESTING)
    enable_testing()
endif()

##################################### Set Application options #####################################

######## Set URLs ########
set(Launcher_NEWS_RSS_URL "https://prismlauncher.org/feed/feed.xml" CACHE STRING "URL to fetch Prism Launcher's news RSS feed from.")
set(Launcher_NEWS_OPEN_URL "https://prismlauncher.org/news" CACHE STRING "URL that gets opened when the user clicks 'More News'")
set(Launcher_HELP_URL "https://prismlauncher.org/wiki/help-pages/%1" CACHE STRING "URL (with arg %1 to be substituted with page-id) that gets opened when the user requests help")
set(Launcher_LOGIN_CALLBACK_URL "https://prismlauncher.org/successful-login" CACHE STRING "URL that gets opened when the user successfully logins.")
set(Launcher_FMLLIBS_BASE_URL "https://files.prismlauncher.org/fmllibs/" CACHE STRING "URL for FML Libraries.")

######## Set version numbers ########
set(Launcher_VERSION_MAJOR 10)
set(Launcher_VERSION_MINOR 0)

set(Launcher_VERSION_NAME "${Launcher_VERSION_MAJOR}.${Launcher_VERSION_MINOR}")
set(Launcher_VERSION_NAME4 "${Launcher_VERSION_MAJOR}.${Launcher_VERSION_MINOR}.0.0")
set(Launcher_VERSION_NAME4_COMMA "${Launcher_VERSION_MAJOR},${Launcher_VERSION_MINOR},0,0")

# Build platform.
set(Launcher_BUILD_PLATFORM "unknown" CACHE STRING "A short string identifying the platform that this build was built for. Only used to display in the about dialog.")

# Github repo URL with releases for updater
set(Launcher_UPDATER_GITHUB_REPO "https://github.com/PrismLauncher/PrismLauncher" CACHE STRING "Base github URL for the updater.")

# Name to help updater identify valid artifacts
set(Launcher_BUILD_ARTIFACT "" CACHE STRING "Artifact name to help the updater identify valid artifacts.")

# The metadata server
set(Launcher_META_URL "https://meta.prismlauncher.org/v1/" CACHE STRING "URL to fetch Launcher's meta files from.")

# Imgur API Client ID
set(Launcher_IMGUR_CLIENT_ID "5b97b0713fba4a3" CACHE STRING "Client ID you can get from Imgur when you register an application")

# Bug tracker URL
set(Launcher_BUG_TRACKER_URL "https://github.com/PrismLauncher/PrismLauncher/issues" CACHE STRING "URL for the bug tracker.")

# Translations Platform URL
set(Launcher_TRANSLATIONS_URL "https://hosted.weblate.org/projects/prismlauncher/launcher/" CACHE STRING "URL for the translations platform.")
set(Launcher_TRANSLATION_FILES_URL "https://i18n.prismlauncher.org/" CACHE STRING "URL for the translations files.")

# Matrix Space
set(Launcher_MATRIX_URL "https://prismlauncher.org/matrix" CACHE STRING "URL to the Matrix Space")

# Discord URL
set(Launcher_DISCORD_URL "https://prismlauncher.org/discord" CACHE STRING "URL for the Discord guild.")

# Subreddit URL
set(Launcher_SUBREDDIT_URL "https://prismlauncher.org/reddit" CACHE STRING "URL for the subreddit.")

# Builds
set(Launcher_FORCE_BUNDLED_LIBS OFF CACHE BOOL "Prevent using system libraries, if they are available as submodules")
set(Launcher_QT_VERSION_MAJOR "6" CACHE STRING "Major Qt version to build against")

# Java downloader
set(Launcher_ENABLE_JAVA_DOWNLOADER_DEFAULT ON)

# Although we recommend enabling this, we cannot guarantee binary compatibility on
# differing Linux/BSD/etc distributions. Downstream packagers should be explicitly opt-ing into this
# feature if they know it will work with their distribution.
if(UNIX AND NOT APPLE)
  set(Launcher_ENABLE_JAVA_DOWNLOADER_DEFAULT OFF)
endif()

# Java downloader
option(Launcher_ENABLE_JAVA_DOWNLOADER "Build the java downloader feature" ${Launcher_ENABLE_JAVA_DOWNLOADER_DEFAULT})

# Native libraries
if(UNIX AND APPLE)
    set(Launcher_GLFW_LIBRARY_NAME "libglfw.dylib" CACHE STRING "Name of native glfw library")
    set(Launcher_OPENAL_LIBRARY_NAME "libopenal.dylib" CACHE STRING "Name of native openal library")
elseif(UNIX)
    set(Launcher_GLFW_LIBRARY_NAME "libglfw.so" CACHE STRING "Name of native glfw library")
    set(Launcher_OPENAL_LIBRARY_NAME "libopenal.so" CACHE STRING "Name of native openal library")
elseif(WIN32)
    set(Launcher_GLFW_LIBRARY_NAME "glfw.dll" CACHE STRING "Name of native glfw library")
    set(Launcher_OPENAL_LIBRARY_NAME "OpenAL.dll" CACHE STRING "Name of native openal library")
endif()

# API Keys
# NOTE: These API keys are here for convenience. If you rebrand this software or intend to break the terms of service
# of these platforms, please change these API keys beforehand.
# Be aware that if you were to use these API keys for malicious purposes they might get revoked, which might cause
# breakage to thousands of users.
# If you don't plan to use these features of this software, you can just remove these values.

# By using this key in your builds you accept the terms of use laid down in
# https://docs.microsoft.com/en-us/legal/microsoft-identity-platform/terms-of-use
set(Launcher_MSA_CLIENT_ID "c36a9fb6-4f2a-41ff-90bd-ae7cc92031eb" CACHE STRING "Client ID you can get from Microsoft Identity Platform when you register an application")

# By using this key in your builds you accept the terms and conditions laid down in
# https://support.curseforge.com/en/support/solutions/articles/9000207405-curse-forge-3rd-party-api-terms-and-conditions
# NOTE: CurseForge requires you to change this if you make any kind of derivative work.
# This key was issued specifically for Prism Launcher
set(Launcher_CURSEFORGE_API_KEY "$2a$10$wuAJuNZuted3NORVmpgUC.m8sI.pv1tOPKZyBgLFGjxFp/br0lZCC" CACHE STRING "API key for the CurseForge platform")

set(Launcher_COMPILER_NAME ${CMAKE_CXX_COMPILER_ID})
set(Launcher_COMPILER_VERSION ${CMAKE_CXX_COMPILER_VERSION})
set(Launcher_COMPILER_TARGET_SYSTEM ${CMAKE_SYSTEM_NAME})
set(Launcher_COMPILER_TARGET_SYSTEM_VERSION ${CMAKE_SYSTEM_VERSION})
set(Launcher_COMPILER_TARGET_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})

#### Check the current Git commit and branch
include(GetGitRevisionDescription)
git_get_exact_tag(Launcher_GIT_TAG)
get_git_head_revision(Launcher_GIT_REFSPEC Launcher_GIT_COMMIT)

message(STATUS "Git commit: ${Launcher_GIT_COMMIT}")
message(STATUS "Git tag: ${Launcher_GIT_TAG}")
message(STATUS "Git refspec: ${Launcher_GIT_REFSPEC}")

string(TIMESTAMP TODAY "%Y-%m-%d")
set(Launcher_BUILD_TIMESTAMP "${TODAY}")

################################ 3rd Party Libs ################################

# Successive configurations of cmake without cleaning the build dir will cause zlib fallback to fail due to cached values
# Record when fallback triggered and skip this find_package
if(NOT Launcher_FORCE_BUNDLED_LIBS AND NOT FORCE_BUNDLED_ZLIB)
    find_package(ZLIB QUIET)
endif()
if(NOT ZLIB_FOUND)
    set(FORCE_BUNDLED_ZLIB TRUE CACHE BOOL "")
    mark_as_advanced(FORCE_BUNDLED_ZLIB)
endif()

# Find the required Qt parts
include(QtVersionlessBackport)
if(Launcher_QT_VERSION_MAJOR EQUAL 6)
    set(QT_VERSION_MAJOR 6)
    find_package(Qt6 REQUIRED COMPONENTS Core CoreTools Widgets Concurrent Network Test Xml Core5Compat NetworkAuth)
    find_package(Qt6 COMPONENTS DBus)
    list(APPEND Launcher_QT_DBUS Qt6::DBus)
    list(APPEND Launcher_QT_LIBS Qt6::Core5Compat)

    if(NOT Launcher_FORCE_BUNDLED_LIBS)
        find_package(QuaZip-Qt6 1.3 QUIET)
    endif()
    if (NOT QuaZip-Qt6_FOUND)
        set(QUAZIP_QT_MAJOR_VERSION ${QT_VERSION_MAJOR} CACHE STRING "Qt version to use (4, 5 or 6), defaults to ${QT_VERSION_MAJOR}" FORCE)
        set(FORCE_BUNDLED_QUAZIP 1)
    endif()
else()
    message(FATAL_ERROR "Qt version ${Launcher_QT_VERSION_MAJOR} is not supported")
endif()

if(Launcher_QT_VERSION_MAJOR EQUAL 6)
    set(QT_PLUGINS_DIR  ${QT${QT_VERSION_MAJOR}_INSTALL_PREFIX}/${QT${QT_VERSION_MAJOR}_INSTALL_PLUGINS})
    set(QT_LIBS_DIR     ${QT${QT_VERSION_MAJOR}_INSTALL_PREFIX}/${QT${QT_VERSION_MAJOR}_INSTALL_LIBS})
    set(QT_LIBEXECS_DIR ${QT${QT_VERSION_MAJOR}_INSTALL_PREFIX}/${QT${QT_VERSION_MAJOR}_INSTALL_LIBEXECS})
endif()

# NOTE: Qt 6 already sets this by default
if (Qt5_POSITION_INDEPENDENT_CODE)
    SET(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if(NOT Launcher_FORCE_BUNDLED_LIBS)
    # Find toml++
    find_package(tomlplusplus 3.2.0 QUIET)

    # Find ghc_filesystem
    find_package(ghc_filesystem QUIET)

    # Find cmark
    find_package(cmark QUIET)
endif()

include(ECMQtDeclareLoggingCategory)

####################################### Program Info #######################################

set(Launcher_APP_BINARY_NAME "prismlauncher" CACHE STRING "Name of the Launcher binary")
add_subdirectory(program_info)

####################################### Install layout #######################################

set(Launcher_ENABLE_UPDATER NO)
set(Launcher_BUILD_UPDATER NO)

if (NOT APPLE AND (NOT Launcher_UPDATER_GITHUB_REPO STREQUAL "" AND NOT Launcher_BUILD_ARTIFACT STREQUAL ""))
	set(Launcher_BUILD_UPDATER YES)
endif()

if(NOT (UNIX AND APPLE))
    # Install "portable.txt" if selected component is "portable"
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${Launcher_Portable_File}" DESTINATION "." COMPONENT portable EXCLUDE_FROM_ALL)
endif()

if(UNIX AND APPLE)
    set(BINARY_DEST_DIR "${Launcher_Name}.app/Contents/MacOS")
    set(LIBRARY_DEST_DIR "${Launcher_Name}.app/Contents/MacOS")
    set(PLUGIN_DEST_DIR "${Launcher_Name}.app/Contents/MacOS")
    set(FRAMEWORK_DEST_DIR "${Launcher_Name}.app/Contents/Frameworks")
    set(RESOURCES_DEST_DIR "${Launcher_Name}.app/Contents/Resources")
    set(JARS_DEST_DIR "${Launcher_Name}.app/Contents/MacOS/jars")

    # Apps to bundle
    set(APPS "\${CMAKE_INSTALL_PREFIX}/${Launcher_Name}.app")

    # Mac bundle settings
    set(MACOSX_BUNDLE_BUNDLE_NAME "${Launcher_DisplayName}")
    set(MACOSX_BUNDLE_INFO_STRING "${Launcher_DisplayName}: A custom launcher for Minecraft that allows you to easily manage multiple installations of Minecraft at once.")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "org.prismlauncher.${Launcher_Name}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${Launcher_VERSION_NAME}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${Launcher_VERSION_NAME}")
    set(MACOSX_BUNDLE_LONG_VERSION_STRING "${Launcher_VERSION_NAME}")
    set(MACOSX_BUNDLE_ICON_FILE ${Launcher_Name}.icns)
    set(MACOSX_BUNDLE_COPYRIGHT "${Launcher_Copyright_Mac}")
    set(MACOSX_SPARKLE_UPDATE_PUBLIC_KEY "v55ZWWD6QlPoXGV6VLzOTZxZUggWeE51X8cRQyQh6vA=" CACHE STRING "Public key for Sparkle update feed")
    set(MACOSX_SPARKLE_UPDATE_FEED_URL "https://prismlauncher.org/feed/appcast.xml" CACHE STRING "URL for Sparkle update feed")

    set(MACOSX_SPARKLE_DOWNLOAD_URL "https://github.com/sparkle-project/Sparkle/releases/download/2.6.4/Sparkle-2.6.4.tar.xz" CACHE STRING "URL to Sparkle release archive")
    set(MACOSX_SPARKLE_SHA256 "50612a06038abc931f16011d7903b8326a362c1074dabccb718404ce8e585f0b" CACHE STRING "SHA256 checksum for Sparkle release archive")
    set(MACOSX_SPARKLE_DIR "${CMAKE_BINARY_DIR}/frameworks/Sparkle")

    # directories to look for dependencies
    set(DIRS ${QT_LIBS_DIR} ${QT_LIBEXECS_DIR} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${MACOSX_SPARKLE_DIR})

    if(NOT MACOSX_SPARKLE_UPDATE_PUBLIC_KEY STREQUAL "" AND NOT MACOSX_SPARKLE_UPDATE_FEED_URL STREQUAL "")
        set(Launcher_ENABLE_UPDATER YES)
    endif()

    # install as bundle
    set(INSTALL_BUNDLE "full" CACHE STRING "Use fixup_bundle to bundle dependencies")

    # Add the icon
    install(FILES ${Launcher_Branding_ICNS} DESTINATION ${RESOURCES_DEST_DIR} RENAME ${Launcher_Name}.icns)

elseif(UNIX)
    include(KDEInstallDirs)

    set(BINARY_DEST_DIR "bin")
    set(LIBRARY_DEST_DIR "lib${LIB_SUFFIX}")
    set(JARS_DEST_DIR "share/${Launcher_Name}")

    # install as bundle with no dependencies included
    set(INSTALL_BUNDLE "nodeps" CACHE STRING "Use fixup_bundle to bundle dependencies")

    # Set RPATH
    SET(Launcher_BINARY_RPATH "$ORIGIN/")

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${Launcher_Desktop} DESTINATION ${KDE_INSTALL_APPDIR})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${Launcher_MetaInfo} DESTINATION ${KDE_INSTALL_METAINFODIR})
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${Launcher_SVG} DESTINATION "${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${Launcher_mrpack_MIMEInfo} DESTINATION ${KDE_INSTALL_MIMEDIR})

    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/launcher/qtlogging.ini" DESTINATION "share/${Launcher_Name}")

    if (INSTALL_BUNDLE STREQUAL full)
        set(PLUGIN_DEST_DIR "plugins")
        set(BUNDLE_DEST_DIR ".")
        set(RESOURCES_DEST_DIR ".")

        # Apps to bundle
        set(APPS "\${CMAKE_INSTALL_PREFIX}/bin/${Launcher_APP_BINARY_NAME}")

        # directories to look for dependencies
        set(DIRS ${QT_LIBS_DIR} ${QT_LIBEXECS_DIR} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    endif()

    if(Launcher_ManPage)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${Launcher_ManPage} DESTINATION "${KDE_INSTALL_MANDIR}/man6")
    endif()

    # Install basic runner script if component "portable" is selected
    configure_file(launcher/Launcher.in "${CMAKE_CURRENT_BINARY_DIR}/LauncherScript" @ONLY)
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/LauncherScript" DESTINATION "." RENAME ${Launcher_Name} COMPONENT portable EXCLUDE_FROM_ALL)

elseif(WIN32)
    set(BINARY_DEST_DIR ".")
    set(LIBRARY_DEST_DIR ".")
    set(PLUGIN_DEST_DIR ".")
    set(RESOURCES_DEST_DIR ".")
    set(JARS_DEST_DIR "jars")

    # Apps to bundle
    set(APPS "\${CMAKE_INSTALL_PREFIX}/${Launcher_Name}.exe")

    # directories to look for dependencies
    set(DIRS ${QT_LIBS_DIR} ${QT_LIBEXECS_DIR} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

    # install as bundle
    set(INSTALL_BUNDLE "full" CACHE STRING "Use fixup_bundle to bundle dependencies")
else()
    message(FATAL_ERROR "Platform not supported")
endif()



################################ Included Libs ################################

include(ExternalProject)
set_directory_properties(PROPERTIES EP_BASE External)

option(NBT_BUILD_SHARED "Build NBT shared library" OFF)
option(NBT_USE_ZLIB "Build NBT library with zlib support" OFF)
option(NBT_BUILD_TESTS "Build NBT library tests" OFF) #FIXME: fix unit tests.
add_subdirectory(libraries/libnbtplusplus)

add_subdirectory(libraries/systeminfo) # system information library
add_subdirectory(libraries/launcher) # java based launcher part for Minecraft
add_subdirectory(libraries/javacheck) # java compatibility checker
if(FORCE_BUNDLED_ZLIB)
    message(STATUS "Using bundled zlib")

    set(CMAKE_POLICY_DEFAULT_CMP0069 NEW) # Suppress cmake warnings and allow INTERPROCEDURAL_OPTIMIZATION for zlib
    set(SKIP_INSTALL_ALL ON)
    add_subdirectory(libraries/zlib EXCLUDE_FROM_ALL)

    # On OS where unistd.h exists, zlib's generated header defines `Z_HAVE_UNISTD_H`, while the included header does not.
    # We cannot safely undo the rename on those systems, and they generally have packages for zlib anyway.
    check_include_file(unistd.h NEED_GENERATED_ZCONF)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libraries/zlib/zconf.h.included" AND NOT NEED_GENERATED_ZCONF)
        # zlib's cmake script renames a file, dirtying the submodule, see https://github.com/madler/zlib/issues/162
        message(STATUS "Undoing Rename")
        message(STATUS "    ${CMAKE_CURRENT_SOURCE_DIR}/libraries/zlib/zconf.h")
        file(RENAME "${CMAKE_CURRENT_SOURCE_DIR}/libraries/zlib/zconf.h.included" "${CMAKE_CURRENT_SOURCE_DIR}/libraries/zlib/zconf.h")
    endif()

    set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/libraries/zlib" "${CMAKE_CURRENT_SOURCE_DIR}/libraries/zlib" CACHE STRING "" FORCE)
    set_target_properties(zlibstatic PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}")
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
    set(ZLIB_LIBRARY ZLIB::ZLIB CACHE STRING "zlib library name")

    find_package(ZLIB REQUIRED)
else()
    message(STATUS "Using system zlib")
endif()
if (FORCE_BUNDLED_QUAZIP)
    message(STATUS "Using bundled QuaZip")
    set(BUILD_SHARED_LIBS 0)  # link statically to avoid conflicts.
    set(QUAZIP_INSTALL 0)
    add_subdirectory(libraries/quazip) # zip manipulation library
else()
    message(STATUS "Using system QuaZip")
endif()
add_subdirectory(libraries/rainbow) # Qt extension for colors
add_subdirectory(libraries/LocalPeer) # fork of a library from Qt solutions
if(NOT tomlplusplus_FOUND)
    message(STATUS "Using bundled tomlplusplus")
    add_subdirectory(libraries/tomlplusplus) # toml parser
else()
    message(STATUS "Using system tomlplusplus")
endif()
if(NOT cmark_FOUND)
    message(STATUS "Using bundled cmark")
    set(ORIGINAL_BUILD_TESTING ${BUILD_TESTING})
    set(BUILD_TESTING 0)
    set(BUILD_SHARED_LIBS 0)
    add_subdirectory(libraries/cmark EXCLUDE_FROM_ALL) # Markdown parser
    add_library(cmark::cmark ALIAS cmark)
    set(BUILD_TESTING ${ORIGINAL_BUILD_TESTING})
else()
    message(STATUS "Using system cmark")
endif()
add_subdirectory(libraries/gamemode)
add_subdirectory(libraries/murmur2) # Hash for usage with the CurseForge API
if (NOT ghc_filesystem_FOUND)
    message(STATUS "Using bundled ghc_filesystem")
    add_subdirectory(libraries/filesystem) # Implementation of std::filesystem for old C++, for usage in old macOS
else()
    message(STATUS "Using system ghc_filesystem")
endif()
add_subdirectory(libraries/qdcss) # css parser

############################### Built Artifacts ###############################

add_subdirectory(buildconfig)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
# NOTE: this must always be last to appease the CMake deity of quirky install command evaluation order.
add_subdirectory(launcher)
