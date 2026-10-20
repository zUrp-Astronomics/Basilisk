# SOURCE: the INDI focuser driver pinefeat_cef of indilib/indi, built on its own (6_Driver/indi/README.md)
# AUTHOR: engineer
# DATE: 2026-10-02
# STATUS: active — built and linked by 6_Driver/indi/build.sh, job `indi` of .gitea/workflows/ci.yml (revised
#         2026-10-19, ticket #645: upstream's name, INDI 2, nothing installed beside the binary)
cmake_minimum_required(VERSION 3.13)
project(indi_pinefeat_cef_focus CXX)

#   cmake -S . -B build && make -C build && sudo make -C build install
# Requires libindi-dev 2.x. The binary has upstream's name: installed under CMAKE_INSTALL_PREFIX, /usr/local by
# default, it stands beside the one of the indi-bin package (/usr/bin) without overwriting it (README.md).

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# libindi-dev ships pkg-config only; libindi.pc names the client library, the driver library is indidriver
find_package(PkgConfig REQUIRED)
pkg_check_modules(INDI REQUIRED libindi>=2)
include_directories(${INDI_INCLUDE_DIRS})
link_directories(${INDI_LIBRARY_DIRS})

add_executable(indi_pinefeat_cef_focus pinefeat_cef.cpp)
target_link_libraries(indi_pinefeat_cef_focus indidriver)

install(TARGETS indi_pinefeat_cef_focus RUNTIME DESTINATION bin)
