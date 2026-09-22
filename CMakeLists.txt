cmake_minimum_required(VERSION 3.21)
project(locus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Test Network LinguistTools)

set(LOCUS_CORE_SOURCES
  src/core/Localization.cpp
  src/core/Pin.cpp
  src/core/SessionController.cpp
  src/core/PinStore.cpp
  src/core/Prefs.cpp
  src/layout/OrbitalLayoutStrategy.cpp
  src/layout/CellularLayoutStrategy.cpp
  src/layout/FanLayoutStrategy.cpp
  src/ui/OrbitalGlassView.cpp
  src/ui/CellularGlassView.cpp
  src/ui/FanGlassView.cpp
  src/ui/HoneycombMark.cpp
  src/ui/OverlayWindow.cpp
  src/ui/PrefsWindow.cpp
  src/platforms/IconProvider.cpp
  src/platforms/AppLauncher.cpp
  src/platforms/TrayController.cpp
  src/platforms/HotkeyManager.h
  resources.qrc
)

if(APPLE)
  enable_language(OBJCXX)
  list(APPEND LOCUS_CORE_SOURCES src/platforms/macos/MacActivation.mm src/platforms/macos/MacIcon.mm src/platforms/macos/MacLoginItem.mm src/platforms/macos/MacOverlay.mm src/platforms/macos/MacHotkey.mm src/platforms/macos/CrystalBackdrop_mac.mm)
else()
  list(APPEND LOCUS_CORE_SOURCES src/platforms/stub/MacStubs.cpp src/platforms/stub/CrystalBackdrop_stub.cpp)
  if(WIN32)
    list(APPEND LOCUS_CORE_SOURCES src/platforms/windows/WinHotkey.cpp)
  else()
    list(APPEND LOCUS_CORE_SOURCES src/platforms/stub/HotkeyManagerStub.cpp)
  endif()
endif()

add_library(locus_core STATIC ${LOCUS_CORE_SOURCES})
target_include_directories(locus_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(locus_core PUBLIC LOCUS_VERSION="${PROJECT_VERSION}")
target_link_libraries(locus_core PUBLIC Qt6::Widgets)
if(APPLE)
  target_link_libraries(locus_core PUBLIC "-framework AppKit" "-framework ServiceManagement" "-framework Carbon")
endif()

if(APPLE)
  add_executable(locus MACOSX_BUNDLE src/main.cpp macos/AppIcon.icns)
  set_source_files_properties(macos/AppIcon.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(locus PROPERTIES
    OUTPUT_NAME "Locus"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/macos/Info.plist"
    MACOSX_BUNDLE_BUNDLE_NAME "Locus"
    MACOSX_BUNDLE_GUI_IDENTIFIER "im.cheng.locus"
    MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
    MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
  )
else()
  add_executable(locus src/main.cpp)
endif()
target_link_libraries(locus PRIVATE locus_core Qt6::Widgets Qt6::Network)

# Translations: .ts → .qm at build time, embedded at :/i18n (attached to the
# executable target so the static-lib linker-drop issue can't apply).
# Regenerate sources after adding tr() strings with:
#   cmake --build build2 --target update_translations
qt_add_translations(locus
  TS_FILES resources/i18n/locus_zh_CN.ts
  RESOURCE_PREFIX "/i18n"
)

enable_testing()

function(locus_add_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE locus_core Qt6::Test)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

locus_add_test(test_smoke tests/test_smoke.cpp)
locus_add_test(test_session tests/test_session.cpp)
locus_add_test(test_backdrop tests/test_backdrop.cpp)
locus_add_test(test_orbital_layout tests/test_orbital_layout.cpp)
locus_add_test(test_fan_layout tests/test_fan_layout.cpp)
locus_add_test(test_scene_hittest tests/test_scene_hittest.cpp)
locus_add_test(test_prefs tests/test_prefs.cpp)
locus_add_test(test_hexhittest tests/test_hexhittest.cpp)
locus_add_test(test_cellular_layout tests/test_cellular_layout.cpp)
locus_add_test(test_dockanim tests/test_dockanim.cpp)

