set(fqterm_SRCS
  fqterm_autoupdate.h
  fqterm_frame.h
  fqterm_screen.h
  fqterm_window.h
  fqterm_wndmgr.h
  fqterm_autoupdate.cpp
  fqterm_frame.cpp
  fqterm_screen.cpp
  fqterm_window.cpp
  fqterm_wndmgr.cpp
)

qt4_automoc(${fqterm_SRCS})

qt4_create_translation(fqterm_qm_file
  ${fqterm_SRCS}
  ${CMAKE_SOURCE_DIR}/res/dict/fqterm_zh_CN.ts
)

include_directories(
  ${QT_INCLUDE_DIR}
  ${QT_QTCORE_INCLUDE_DIR}
  ${QT_QTGUI_INCLUDE_DIR}
  ${QT_QTNETWORK_INCLUDE_DIR}
  ${QT_QTSCRIPT_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../common
  ${CMAKE_CURRENT_SOURCE_DIR}/../protocol
  ${CMAKE_CURRENT_SOURCE_DIR}/../terminal
  ${CMAKE_CURRENT_SOURCE_DIR}/../utilities
  ${CMAKE_CURRENT_SOURCE_DIR}/../ui
  ${CMAKE_CURRENT_BINARY_DIR}/../ui
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_library(fqterm_main
  ${fqterm_SRCS}
)

add_dependencies(fqterm_main
  fqterm_common
  fqterm_terminal
  fqterm_protocol
  fqterm_ui
  fqterm_utilities
)
