cmake_minimum_required(VERSION 3.20)
project(XtreemRgbToggleNative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/out)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/out)

add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

set(LOGIC_SRC
  src/diag.cpp
  src/persist.cpp
  src/autostart.cpp
  src/pawnio.cpp
  src/smbus.cpp
  src/xtreem.cpp
  src/hw_worker.cpp
)

add_executable(XtreemRgbToggle WIN32
  ${LOGIC_SRC}
  src/ui.cpp
  src/main.cpp
  app.manifest
)
target_include_directories(XtreemRgbToggle PRIVATE src)
target_link_libraries(XtreemRgbToggle PRIVATE comctl32 comdlg32 shell32 advapi32 user32 gdi32)
target_compile_options(XtreemRgbToggle PRIVATE /W3 /EHsc /utf-8)

add_executable(xtreem_logic_tests
  ${LOGIC_SRC}
  tests/test_logic.cpp
)
target_include_directories(xtreem_logic_tests PRIVATE src)
target_compile_definitions(xtreem_logic_tests PRIVATE XTREEM_TEST)
target_link_libraries(xtreem_logic_tests PRIVATE advapi32 shell32 ole32)
target_compile_options(xtreem_logic_tests PRIVATE /W3 /EHsc /utf-8)

add_executable(xtreem_gui_smoke WIN32
  tests/test_gui_smoke.cpp
  app.manifest
)
target_link_libraries(xtreem_gui_smoke PRIVATE user32)
target_compile_definitions(xtreem_gui_smoke PRIVATE XTREEM_GUI_SMOKE)
target_compile_options(xtreem_gui_smoke PRIVATE /W3 /utf-8)
