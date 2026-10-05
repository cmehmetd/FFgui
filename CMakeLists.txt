# FFgui — Qt + C++ ile yazılmış FFmpeg arayüzü.
# Copyright (C) 2026 FFgui contributors
# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.21)
project(FFgui VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(FFgui
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/icons.qrc
)

target_link_libraries(FFgui PRIVATE Qt6::Widgets)

install(TARGETS FFgui RUNTIME DESTINATION bin)
