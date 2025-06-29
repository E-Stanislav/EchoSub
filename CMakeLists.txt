cmake_minimum_required(VERSION 3.16)
project(EchoSub VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt MOC and UIC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Qt 6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia MultimediaWidgets Concurrent)

# FFmpeg
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED 
    libavcodec 
    libavformat 
    libavutil 
    libswscale 
    libswresample
)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${FFMPEG_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/main.cpp
    src/core/mediaplayer.cpp
    src/core/audiodecoder.cpp
    src/core/videodecoder.cpp
    src/core/simplemediaplayer.cpp
    src/core/WhisperModelSettingsDialog.cpp
    src/ui/mainwindow.cpp
    src/ui/videowidget.cpp
    src/ui/controlspanel.cpp
    src/media/ffmpegwrapper.cpp
)

set(HEADERS
    include/core/mediaplayer.h
    include/core/audiodecoder.h
    include/core/videodecoder.h
    src/core/simplemediaplayer.h
    include/ui/mainwindow.h
    include/ui/videowidget.h
    include/ui/controlspanel.h
    include/media/ffmpegwrapper.h
)

# Create executable
add_executable(echosub ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(echosub 
    Qt6::Core 
    Qt6::Widgets 
    Qt6::Multimedia 
    Qt6::MultimediaWidgets
    Qt6::Concurrent
    ${FFMPEG_LIBRARIES}
    ${FFMPEG_LDFLAGS}
)

# Compiler flags
target_compile_options(echosub PRIVATE ${FFMPEG_CFLAGS_OTHER})

# Set output directory
set_target_properties(echosub PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install
install(TARGETS echosub DESTINATION bin)

# macOS specific settings
if(APPLE)
    set_target_properties(echosub PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist
    )
    set(MACOSX_BUNDLE_ICON_FILE icon.icns)
    set_source_files_properties(resources/icons/icon.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

# Print configuration info
message(STATUS "EchoSub Configuration:")
message(STATUS "  Qt6 version: ${Qt6_VERSION}")
message(STATUS "  FFmpeg libraries: ${FFMPEG_LIBRARIES}")
message(STATUS "  FFmpeg include dirs: ${FFMPEG_INCLUDE_DIRS}")

# =====================
# Simple Player (Qt-only)
# =====================
set(SIMPLE_PLAYER_SOURCES
    src/simple_player_test.cpp
    src/core/simplemediaplayer.cpp
    src/core/WhisperModelSettingsDialog.cpp
    src/ui/videowidget.cpp
)

set(SIMPLE_PLAYER_HEADERS
    src/core/simplemediaplayer.h
    src/core/WhisperModelSettingsDialog.h
    include/ui/videowidget.h
)

add_executable(simple_player ${SIMPLE_PLAYER_SOURCES} ${SIMPLE_PLAYER_HEADERS})

target_link_libraries(simple_player 
    Qt6::Core 
    Qt6::Widgets 
    Qt6::Multimedia 
    Qt6::MultimediaWidgets
    Qt6::Concurrent
)

set_target_properties(simple_player PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS simple_player DESTINATION bin)

if(APPLE)
    set_target_properties(simple_player PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info_simple_player.plist
        MACOSX_BUNDLE_ICON_FILE icon.icns
    )
    set_source_files_properties(resources/icons/icon.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

message(STATUS "Simple Media Player Configuration:")
message(STATUS "  Qt6 version: ${Qt6_VERSION}")
message(STATUS "  Using built-in Qt multimedia components")

qt_add_resources(APP_ICONS resources/icons/icons.qrc) 