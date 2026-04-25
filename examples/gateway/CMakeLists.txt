# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(gateway)

target_include_directories(app PRIVATE include ${CMAKE_CURRENT_BINARY_DIR})

target_sources(app PRIVATE
  src/fatal_error.c
  src/main.c
  git_describe.h
)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/mk-git-describe.cmake
  "execute_process(\n"
  "  COMMAND git describe --always --dirty\n"
  "  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}\n"
  "  OUTPUT_VARIABLE GIT_DESCRIBE\n"
  "  OUTPUT_STRIP_TRAILING_WHITESPACE\n"
  "  ERROR_QUIET\n"
  ")\n"
  "if(NOT GIT_DESCRIBE)\n"
  "  set(GIT_DESCRIBE unknown)\n"
  "endif()\n"
  "file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/git_describe.h \"#define GIT_DESCRIBE \${GIT_DESCRIBE}\\n\")\n")

add_custom_command(OUTPUT git_describe.h always_run
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/mk-git-describe.cmake
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
