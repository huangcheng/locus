#include "platforms/HotkeyManager.h"

#import <Carbon/Carbon.h>

// RegisterEventHotKey is the only global-hotkey API on macOS that works
// without Input Monitoring / Accessibility permission. InstallApplication-
// EventHandler is formally deprecated but remains the supported delivery
// mechanism for hot-key events.
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace {

locus::HotkeyManager *gOwner = nullptr;
EventHotKeyRef gHotKeyRef = nullptr;
EventHandlerRef gHandlerRef = nullptr;

OSStatus locusHotkeyHandler(EventHandlerCallRef, EventRef, void *) {
  if (gOwner)
    emit gOwner->triggered();
  return noErr;
}

bool carbonKeyCode(int key, UInt32 &keyCode) {
  switch (key) {
  // clang-format off
  case Qt::Key_A: keyCode = kVK_ANSI_A; return true;
  case Qt::Key_B: keyCode = kVK_ANSI_B; return true;
  case Qt::Key_C: keyCode = kVK_ANSI_C; return true;
  case Qt::Key_D: keyCode = kVK_ANSI_D; return true;
  case Qt::Key_E: keyCode = kVK_ANSI_E; return true;
  case Qt::Key_F: keyCode = kVK_ANSI_F; return true;
  case Qt::Key_G: keyCode = kVK_ANSI_G; return true;
  case Qt::Key_H: keyCode = kVK_ANSI_H; return true;
  case Qt::Key_I: keyCode = kVK_ANSI_I; return true;
  case Qt::Key_J: keyCode = kVK_ANSI_J; return true;
  case Qt::Key_K: keyCode = kVK_ANSI_K; return true;
  case Qt::Key_L: keyCode = kVK_ANSI_L; return true;
  case Qt::Key_M: keyCode = kVK_ANSI_M; return true;
  case Qt::Key_N: keyCode = kVK_ANSI_N; return true;
  case Qt::Key_O: keyCode = kVK_ANSI_O; return true;
  case Qt::Key_P: keyCode = kVK_ANSI_P; return true;
  case Qt::Key_Q: keyCode = kVK_ANSI_Q; return true;
  case Qt::Key_R: keyCode = kVK_ANSI_R; return true;
  case Qt::Key_S: keyCode = kVK_ANSI_S; return true;
  case Qt::Key_T: keyCode = kVK_ANSI_T; return true;
  case Qt::Key_U: keyCode = kVK_ANSI_U; return true;
  case Qt::Key_V: keyCode = kVK_ANSI_V; return true;
  case Qt::Key_W: keyCode = kVK_ANSI_W; return true;
  case Qt::Key_X: keyCode = kVK_ANSI_X; return true;
  case Qt::Key_Y: keyCode = kVK_ANSI_Y; return true;
  case Qt::Key_Z: keyCode = kVK_ANSI_Z; return true;
  case Qt::Key_0: keyCode = kVK_ANSI_0; return true;
  case Qt::Key_1: keyCode = kVK_ANSI_1; return true;
  case Qt::Key_2: keyCode = kVK_ANSI_2; return true;
  case Qt::Key_3: keyCode = kVK_ANSI_3; return true;
  case Qt::Key_4: keyCode = kVK_ANSI_4; return true;
  case Qt::Key_5: keyCode = kVK_ANSI_5; return true;
  case Qt::Key_6: keyCode = kVK_ANSI_6; return true;
  case Qt::Key_7: keyCode = kVK_ANSI_7; return true;
  case Qt::Key_8: keyCode = kVK_ANSI_8; return true;
  case Qt::Key_9: keyCode = kVK_ANSI_9; return true;
  case Qt::Key_Space: keyCode = kVK_Space; return true;
  case Qt::Key_Return:
  case Qt::Key_Enter: keyCode = kVK_Return; return true;
  case Qt::Key_Tab: keyCode = kVK_Tab; return true;
  case Qt::Key_Escape: keyCode = kVK_Escape; return true;
  case Qt::Key_Backspace: keyCode = kVK_Delete; return true;
  case Qt::Key_Delete: keyCode = kVK_ForwardDelete; return true;
  case Qt::Key_Left: keyCode = kVK_LeftArrow; return true;
  case Qt::Key_Right: keyCode = kVK_RightArrow; return true;
  case Qt::Key_Down: keyCode = kVK_DownArrow; return true;
  case Qt::Key_Up: keyCode = kVK_UpArrow; return true;
  case Qt::Key_Home: keyCode = kVK_Home; return true;
  case Qt::Key_End: keyCode = kVK_End; return true;
  case Qt::Key_PageUp: keyCode = kVK_PageUp; return true;
  case Qt::Key_PageDown: keyCode = kVK_PageDown; return true;
  case Qt::Key_F1: keyCode = kVK_F1; return true;
  case Qt::Key_F2: keyCode = kVK_F2; return true;
  case Qt::Key_F3: keyCode = kVK_F3; return true;
  case Qt::Key_F4: keyCode = kVK_F4; return true;
  case Qt::Key_F5: keyCode = kVK_F5; return true;
  case Qt::Key_F6: keyCode = kVK_F6; return true;
  case Qt::Key_F7: keyCode = kVK_F7; return true;
  case Qt::Key_F8: keyCode = kVK_F8; return true;
  case Qt::Key_F9: keyCode = kVK_F9; return true;
  case Qt::Key_F10: keyCode = kVK_F10; return true;
  case Qt::Key_F11: keyCode = kVK_F11; return true;
  case Qt::Key_F12: keyCode = kVK_F12; return true;
  case Qt::Key_Minus: keyCode = kVK_ANSI_Minus; return true;
  case Qt::Key_Equal: keyCode = kVK_ANSI_Equal; return true;
  case Qt::Key_BracketLeft: keyCode = kVK_ANSI_LeftBracket; return true;
  case Qt::Key_BracketRight: keyCode = kVK_ANSI_RightBracket; return true;
  case Qt::Key_Semicolon: keyCode = kVK_ANSI_Semicolon; return true;
  case Qt::Key_Apostrophe: keyCode = kVK_ANSI_Quote; return true;
  case Qt::Key_Comma: keyCode = kVK_ANSI_Comma; return true;
  case Qt::Key_Period: keyCode = kVK_ANSI_Period; return true;
  case Qt::Key_Slash: keyCode = kVK_ANSI_Slash; return true;
  case Qt::Key_Backslash: keyCode = kVK_ANSI_Backslash; return true;
  case Qt::Key_QuoteLeft: keyCode = kVK_ANSI_Grave; return true;
  // clang-format on
  default:
    return false;
  }
}

} // namespace

namespace locus {

HotkeyManager::HotkeyManager(QObject *parent) : QObject(parent) {}

HotkeyManager::~HotkeyManager() {
  setHotkey(QKeySequence());
  if (gOwner == this)
    gOwner = nullptr;
}

bool HotkeyManager::setHotkey(const QKeySequence &sequence) {
  if (gHotKeyRef) {
    UnregisterEventHotKey(gHotKeyRef);
    gHotKeyRef = nullptr;
  }
  if (sequence.isEmpty())
    return true;

  const int combined = sequence[0].toCombined();
  UInt32 keyCode = 0;
  if (!carbonKeyCode(combined & ~int(Qt::KeyboardModifierMask), keyCode)) {
    qWarning("Locus: hotkey key %#x is not supported", combined);
    return false;
  }
  UInt32 modifiers = 0;
  if (combined & Qt::META)
    modifiers |= cmdKey;
  if (combined & Qt::SHIFT)
    modifiers |= shiftKey;
  if (combined & Qt::ALT)
    modifiers |= optionKey;
  if (combined & Qt::CTRL)
    modifiers |= controlKey;
  if (!modifiers) {
    qWarning("Locus: hotkey needs at least one modifier");
    return false;
  }

  if (!gHandlerRef) {
    const EventTypeSpec spec = {kEventClassKeyboard, kEventHotKeyPressed};
    if (InstallApplicationEventHandler(&locusHotkeyHandler, 1, &spec, nullptr,
                                       &gHandlerRef) != noErr)
      return false;
  }
  const EventHotKeyID hotKeyID = {'locu', 1};
  const OSStatus status =
      RegisterEventHotKey(keyCode, modifiers, hotKeyID,
                          GetApplicationEventTarget(), 0, &gHotKeyRef);
  if (status != noErr) {
    qWarning("Locus: RegisterEventHotKey failed (%d) — combo likely taken",
             int(status));
    return false;
  }
  gOwner = this;
  return true;
}

} // namespace locus
