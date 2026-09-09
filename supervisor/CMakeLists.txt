# ESP-IDF project file for the supervisor.
#
# Requires ESP-IDF v5.5 or newer.
#
#   . $HOME/esp/esp-idf/export.sh
#   idf.py set-target esp32s3
#   idf.py build flash monitor
#
# This firmware has NO authority over the motors. If a change here can affect
# how the truck drives, the change is wrong. See docs/ARCHITECTURE.md.

cmake_minimum_required(VERSION 3.16)

set(SUPPORTED_TARGETS esp32)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(speed-supervisor)
