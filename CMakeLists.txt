project(fqterm)

set(FQTERM_VERSION "0.9.1-beta")

cmake_minimum_required( VERSION 2.4.2 FATAL_ERROR )

set(CMAKE_MODULE_PATH
  ${CMAKE_SOURCE_DIR}/cmake
)

IF(NOT CMAKE_BUILD_TYPE) 
    SET(CMAKE_BUILD_TYPE "RelWithDebInfo") 
ENDIF(NOT CMAKE_BUILD_TYPE)

set(CMAKE_COLOR_MAKEFILE ON)

find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})

qt4_add_translation(fqterm_qm_files
  ${CMAKE_SOURCE_DIR}/res/dict/fqterm_zh_CN.ts
  ${CMAKE_SOURCE_DIR}/res/dict/ui_zh_CN.ts
)

add_custom_target(fqterm_qm_target
  DEPENDS ${fqterm_qm_files}
)

IF(WIN32)
    INCLUDE(CMakeLists.txt.win32)
ELSE(WIN32)
    IF(APPLE)
        INCLUDE(CMakeLists.txt.macos)
    ELSE(APPLE)
        INCLUDE(CMakeLists.txt.linux)
    ENDIF(APPLE)
ENDIF(WIN32)
