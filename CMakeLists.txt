project(fqterm)

set(FQTERM_VERSION "0.9.4")

cmake_minimum_required( VERSION 2.4.2 FATAL_ERROR )

set(CMAKE_MODULE_PATH
  ${CMAKE_SOURCE_DIR}/cmake
)

IF(NOT CMAKE_BUILD_TYPE) 
    SET(CMAKE_BUILD_TYPE "MinsizeRel") 
ENDIF(NOT CMAKE_BUILD_TYPE)

set(CMAKE_COLOR_MAKEFILE ON)

find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})

qt4_add_translation(fqterm_qm_files
  ${CMAKE_SOURCE_DIR}/res/dict/fqterm_zh_CN.ts
  ${CMAKE_SOURCE_DIR}/res/dict/ui_zh_CN.ts
  ${CMAKE_SOURCE_DIR}/res/dict/protocol_zh_CN.ts
)

add_custom_target(fqterm_qm_target
  DEPENDS ${fqterm_qm_files}
)

IF(WIN32)
  FILE(READ ${QT_LIBRARY_DIR}/QtCore.prl FQ_CORE_QT_CONFIG)
ELSE(WIN32)
  FILE(READ ${QT_LIBRARY_DIR}/libQtCore.prl FQ_CORE_QT_CONFIG)
ENDIF(WIN32)

IF(NOT FQ_CORE_QT_CONFIG MATCHES "static")
  SET(FQTERM_USE_STATIC_QT false)
ELSE(NOT FQ_CORE_QT_CONFIG MATCHES "static")
  SET(FQTERM_USE_STATIC_QT true)
ENDIF(NOT FQ_CORE_QT_CONFIG MATCHES "static")

SET(SUB_CMAKELIST "UNKNOWN")

IF(WIN32)
  SET(SUB_CMAKELIST "CMakeLists.txt.win32")
ELSE(WIN32)
  IF(APPLE)
    SET(SUB_CMAKELIST "CMakeLists.txt.macos")
  ELSE(APPLE)
    IF(UNIX)
      IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
        SET(SUB_CMAKELIST "CMakeLists.txt.linux")
      ELSE(CMAKE_SYSTEM_NAME MATCHES "Linux")
		IF(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
		  SET(SUB_CMAKELIST "CMakeLists.txt.freebsd")
		ENDIF(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
      ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")
    ENDIF(UNIX)
  ENDIF(APPLE)
ENDIF(WIN32)

IF(SUB_CMAKELIST MATCHES "UNKNOWN")
  MESSAGE(FATAL_ERROR "Unsupported system type detected!")
ELSE(SUB_CMAKELIST MATCHES "UNKNOWN")
  INCLUDE("${SUB_CMAKELIST}")
ENDIF(SUB_CMAKELIST MATCHES "UNKNOWN")
