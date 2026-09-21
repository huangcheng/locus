cmake_minimum_required(VERSION 3.21)
project(navi VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Test Network)

set(NAVI_CORE_SOURCES
  src/core/Pin.cpp
  src/core/SessionController.cpp
  src/core/PinStore.cpp
  src/core/Prefs.cpp
  src/layout/OrbitalLayoutStrategy.cpp
  src/layout/CellularLayoutStrategy.cpp
  src/ui/OrbitalGlassView.cpp
  src/ui/CellularGlassView.cpp
  src/ui/OverlayWindow.cpp
  src/platform/IconProvider.cpp
  src/platform/AppLauncher.cpp
  src/platform/TrayController.cpp
  resources.qrc
)

if(APPLE)
  enable_language(OBJCXX)
  list(APPEND NAVI_CORE_SOURCES src/platform/MacActivation.mm src/platform/MacIcon.mm src/platform/MacOverlay.mm)
endif()

add_library(navi_core STATIC ${NAVI_CORE_SOURCES})
target_include_directories(navi_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(navi_core PUBLIC Qt6::Widgets)
if(APPLE)
  target_link_libraries(navi_core PUBLIC "-framework AppKit")
endif()

if(APPLE)
  add_executable(navi MACOSX_BUNDLE src/main.cpp)
  set_target_properties(navi PROPERTIES
    OUTPUT_NAME "Navi"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/macos/Info.plist"
    MACOSX_BUNDLE_BUNDLE_NAME "Navi"
    MACOSX_BUNDLE_GUI_IDENTIFIER "app.navi.launcher"
    MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
  )
else()
  add_executable(navi src/main.cpp)
endif()
target_link_libraries(navi PRIVATE navi_core Qt6::Widgets Qt6::Network)

enable_testing()

function(navi_add_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE navi_core Qt6::Test)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

navi_add_test(test_smoke tests/test_smoke.cpp)
navi_add_test(test_session tests/test_session.cpp)
navi_add_test(test_orbital_layout tests/test_orbital_layout.cpp)
navi_add_test(test_scene_hittest tests/test_scene_hittest.cpp)
navi_add_test(test_prefs tests/test_prefs.cpp)
navi_add_test(test_hexhittest tests/test_hexhittest.cpp)
navi_add_test(test_cellular_layout tests/test_cellular_layout.cpp)
navi_add_test(test_dockanim tests/test_dockanim.cpp)

