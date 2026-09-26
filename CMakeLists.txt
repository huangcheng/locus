cmake_minimum_required(VERSION 3.21)
project(locus VERSION 0.1.0 LANGUAGES CXX)

# Matches LSMinimumSystemVersion in macos/Info.plist. Without this the binary
# inherits the build SDK's minimum (macOS 27) and refuses to launch on 13–26.
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Test Network LinguistTools)
find_package(Qt6 QUIET COMPONENTS GuiPrivate) # distro packages may lack it

set(LOCUS_CORE_SOURCES
  src/core/Localization.cpp
  src/core/Pin.cpp
  src/core/SessionController.cpp
  src/core/PinStore.cpp
  src/core/UpdateChecker.cpp
  src/core/UpdateDownloader.cpp
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
target_link_libraries(locus_core PUBLIC Qt6::Widgets Qt6::Network)
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
  if(WIN32)
    # Windows: embed app icon + VERSIONINFO (Explorer Properties, taskbar,
    # installer). configure_file keeps the rc version in sync with project().
    enable_language(RC)
    string(REPLACE "." "," LOCUS_VERSION_COMMA "${PROJECT_VERSION}.0")
    configure_file(windows/locus.rc.in ${CMAKE_CURRENT_BINARY_DIR}/locus.rc @ONLY)
  endif()
  add_executable(locus src/main.cpp $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_BINARY_DIR}/locus.rc>)
  if(WIN32)
    # GUI subsystem: no console window when launched from Explorer, shortcuts,
    # or the installer's post-install run. Qt6::EntryPoint supplies WinMain
    # which forwards to our plain main().
    set_target_properties(locus PROPERTIES WIN32_EXECUTABLE TRUE)
    if(TARGET Qt6::EntryPoint)
      target_link_libraries(locus PRIVATE Qt6::EntryPoint)
    endif()
  endif()
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
locus_add_test(test_fan_press tests/test_fan_press.cpp)
locus_add_test(test_fan_icon tests/test_fan_icon.cpp)
locus_add_test(test_pins_remove tests/test_pins_remove.cpp)
locus_add_test(test_prefs tests/test_prefs.cpp)
# Window-system-level event injection needs Qt's private QWSI header. Use the
# GuiPrivate target where the installer provides it; distro packages (Debian)
# ship the headers without the Private component config, so fall back to the
# versioned include dirs directly.
if(TARGET Qt6::GuiPrivate)
  target_link_libraries(test_pins_remove PRIVATE Qt6::GuiPrivate)
else()
  target_include_directories(test_pins_remove PRIVATE
    "${Qt6Gui_INCLUDE_DIRS}/${Qt6Gui_VERSION}/QtGui"
    "${Qt6Core_INCLUDE_DIRS}/${Qt6Core_VERSION}/QtCore")
endif()
locus_add_test(test_hexhittest tests/test_hexhittest.cpp)
locus_add_test(test_cellular_layout tests/test_cellular_layout.cpp)
locus_add_test(test_dockanim tests/test_dockanim.cpp)
locus_add_test(test_drop_add tests/test_drop_add.cpp)
if(TARGET Qt6::GuiPrivate)
  target_link_libraries(test_drop_add PRIVATE Qt6::GuiPrivate)
else()
  target_include_directories(test_drop_add PRIVATE
    "${Qt6Gui_INCLUDE_DIRS}/${Qt6Gui_VERSION}/QtGui"
    "${Qt6Core_INCLUDE_DIRS}/${Qt6Core_VERSION}/QtCore")
endif()
locus_add_test(test_update tests/test_update.cpp)

# --- Packaging -----------------------------------------------------------
# macOS: DragNDrop DMG · Windows: ZIP · Linux: TGZ. The Qt resources (icons,
# translations) are embedded in the binary, so each artifact is self-contained.
set(CPACK_PACKAGE_NAME "Locus")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "HUANG Cheng")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Your favorite apps, always at hand.")

# CPack's install step strips the build-tree rpath, so the packaged app must
# carry the Qt libs itself: macdeployqt / windeployqt bundle them next to the
# binary. Find the tool next to Qt's qmake.
get_target_property(QMAKE_EXE Qt6::qmake IMPORTED_LOCATION)
get_filename_component(QT_BIN_DIR "${QMAKE_EXE}" DIRECTORY)
# The install step rewrites the app's rpath to INSTALL_RPATH (empty by
# default); keep it pointing at the Qt libs so the packaged binary resolves
# them for macdeployqt, which then rewrites everything to @executable_path.
if(APPLE)
  set_target_properties(locus PROPERTIES INSTALL_RPATH "${QT_BIN_DIR}/../lib")
endif()

if(APPLE)
  set(CPACK_GENERATOR "DragNDrop")
  set(CPACK_DMG_VOLUME_NAME "Locus")
  install(TARGETS locus BUNDLE DESTINATION .)
  find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${QT_BIN_DIR}")
  if(NOT MACDEPLOYQT_EXECUTABLE)
    message(FATAL_ERROR "macdeployqt not found next to qmake (${QT_BIN_DIR})")
  endif()
  # A pure-Widgets app never loads Qml; macdeployqt still pulls in the
  # VirtualKeyboard plugin's Qml framework chain. Drop it — dead weight, and
  # its re-signing is what trips codesign verification.
  install(CODE "
    # macdeployqt shells out to `strip`/`codesign` — homebrew binutils shadows
    # Apple's strip on dev machines and corrupts Mach-O frameworks, so run it
    # with a system-only PATH.
    set(ENV{PATH} \"/usr/bin:/bin:/usr/sbin:/sbin\")
    execute_process(
      COMMAND \"${MACDEPLOYQT_EXECUTABLE}\"
              \"\${CMAKE_INSTALL_PREFIX}/Locus.app\"
      RESULT_VARIABLE _deploy)
    if(NOT _deploy EQUAL 0)
      message(FATAL_ERROR \"macdeployqt failed (\${_deploy})\")
    endif()
    file(GLOB _qmljunk
      \"\${CMAKE_INSTALL_PREFIX}/Locus.app/Contents/Frameworks/QtQml*\"
      \"\${CMAKE_INSTALL_PREFIX}/Locus.app/Contents/Frameworks/QtVirtualKeyboard*\"
      \"\${CMAKE_INSTALL_PREFIX}/Locus.app/Contents/Frameworks/QtQuick*\")
    foreach(_fw \${_qmljunk})
      message(STATUS \"stripped \${_fw}\")
    endforeach()
    file(REMOVE_RECURSE \${_qmljunk}
      \"\${CMAKE_INSTALL_PREFIX}/Locus.app/Contents/PlugIns/virtualkeyboard\")
    # Deleting signed content breaks the seal — re-sign ad-hoc (real signing
    # + notarization is postponed for now).
    execute_process(
      COMMAND /usr/bin/codesign --force --sign -
              \"\${CMAKE_INSTALL_PREFIX}/Locus.app\"
      RESULT_VARIABLE _sign)
    if(NOT _sign EQUAL 0)
      message(FATAL_ERROR \"codesign re-sign failed (\${_sign})\")
    endif()")
elseif(WIN32)
  set(CPACK_GENERATOR "ZIP")
  install(TARGETS locus RUNTIME DESTINATION .)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
  if(NOT WINDEPLOYQT_EXECUTABLE)
    message(FATAL_ERROR "windeployqt not found next to qmake (${QT_BIN_DIR})")
  endif()
  install(CODE "
    file(GLOB_RECURSE _locus_exe \"\${CMAKE_INSTALL_PREFIX}/locus*.exe\")
    execute_process(
      COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --no-compiler-runtime
              --no-translations \${_locus_exe}
      RESULT_VARIABLE _deploy)
    if(NOT _deploy EQUAL 0)
      message(FATAL_ERROR \"windeployqt failed (\${_deploy})\")
    endif()")
else()
  set(CPACK_GENERATOR "TGZ")
  install(TARGETS locus RUNTIME DESTINATION .)
endif()
include(CPack)
