#include "platforms/HotkeyManager.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>

#include <qt_windows.h>

// RegisterHotKey is the Windows global-hotkey API: no elevation, no
// permission prompt. Registering with a null hwnd makes WM_HOTKEY arrive as
// a thread message on the registering thread; Qt's Windows event dispatcher
// hands every dispatched message to installed native filters, so the filter
// below sees it without any Win32 message-pump plumbing of our own.

namespace {

// Hotkey ids are thread-scoped; one well-known id covers the single
// shortcut Locus owns.
constexpr int kHotkeyId = 0x10C5;

bool vkForQtKey(int key, UINT &vk) {
  switch (key) {
  // clang-format off
  case Qt::Key_Space:        vk = VK_SPACE;     return true;
  case Qt::Key_Return:
  case Qt::Key_Enter:        vk = VK_RETURN;    return true;
  case Qt::Key_Tab:          vk = VK_TAB;       return true;
  case Qt::Key_Escape:       vk = VK_ESCAPE;    return true;
  case Qt::Key_Backspace:    vk = VK_BACK;      return true;
  case Qt::Key_Delete:       vk = VK_DELETE;    return true;
  case Qt::Key_Insert:       vk = VK_INSERT;    return true;
  case Qt::Key_Left:         vk = VK_LEFT;      return true;
  case Qt::Key_Right:        vk = VK_RIGHT;     return true;
  case Qt::Key_Down:         vk = VK_DOWN;      return true;
  case Qt::Key_Up:           vk = VK_UP;        return true;
  case Qt::Key_Home:         vk = VK_HOME;      return true;
  case Qt::Key_End:          vk = VK_END;       return true;
  case Qt::Key_PageUp:       vk = VK_PRIOR;     return true;
  case Qt::Key_PageDown:     vk = VK_NEXT;      return true;
  case Qt::Key_Minus:        vk = VK_OEM_MINUS; return true;
  case Qt::Key_Equal:        vk = VK_OEM_PLUS;  return true;
  case Qt::Key_BracketLeft:  vk = VK_OEM_4;     return true;
  case Qt::Key_BracketRight: vk = VK_OEM_6;     return true;
  case Qt::Key_Semicolon:    vk = VK_OEM_1;     return true;
  case Qt::Key_Apostrophe:   vk = VK_OEM_7;     return true;
  case Qt::Key_Comma:        vk = VK_OEM_COMMA; return true;
  case Qt::Key_Period:       vk = VK_OEM_PERIOD;return true;
  case Qt::Key_Slash:        vk = VK_OEM_2;     return true;
  case Qt::Key_Backslash:    vk = VK_OEM_5;     return true;
  case Qt::Key_QuoteLeft:    vk = VK_OEM_3;     return true;
  // clang-format on
  default:
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
      vk = UINT('A' + (key - Qt::Key_A));
      return true;
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
      vk = UINT('0' + (key - Qt::Key_0));
      return true;
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
      vk = UINT(VK_F1 + (key - Qt::Key_F1));
      return true;
    }
    return false;
  }
}

class HotkeyNativeFilter : public QAbstractNativeEventFilter {
public:
  locus::HotkeyManager *owner = nullptr;

  bool nativeEventFilter(const QByteArray &eventType, void *message,
                         qintptr *) override {
    if (eventType != QByteArrayLiteral("windows_generic_MSG"))
      return false;
    const MSG *msg = static_cast<const MSG *>(message);
    if (msg->message == WM_HOTKEY &&
        msg->wParam == static_cast<WPARAM>(kHotkeyId) && owner)
      emit owner->triggered();
    return false;
  }
};

HotkeyNativeFilter *gFilter = nullptr;
bool gRegistered = false;

} // namespace

namespace locus {

HotkeyManager::HotkeyManager(QObject *parent) : QObject(parent) {
  if (!gFilter) {
    gFilter = new HotkeyNativeFilter;
    if (auto *app = QCoreApplication::instance())
      app->installNativeEventFilter(gFilter);
  }
}

HotkeyManager::~HotkeyManager() {
  setHotkey(QKeySequence());
  if (gFilter && gFilter->owner == this) {
    if (QCoreApplication::instance())
      QCoreApplication::instance()->removeNativeEventFilter(gFilter);
    delete gFilter;
    gFilter = nullptr;
  }
}

bool HotkeyManager::setHotkey(const QKeySequence &sequence) {
  if (gRegistered) {
    UnregisterHotKey(nullptr, kHotkeyId);
    gRegistered = false;
  }
  if (sequence.isEmpty())
    return true;

  const int combined = sequence[0].toCombined();
  UINT vk = 0;
  if (!vkForQtKey(combined & ~int(Qt::KeyboardModifierMask), vk)) {
    qWarning("Locus: hotkey key %#x is not supported", combined);
    return false;
  }
  UINT mods = MOD_NOREPEAT; // auto-repeat must not re-trigger summon
  if (combined & Qt::SHIFT)
    mods |= MOD_SHIFT;
  if (combined & Qt::CTRL)
    mods |= MOD_CONTROL;
  if (combined & Qt::ALT)
    mods |= MOD_ALT;
  if (combined & Qt::META)
    mods |= MOD_WIN;
  if (!(mods & ~MOD_NOREPEAT)) {
    qWarning("Locus: hotkey needs at least one modifier");
    return false;
  }

  if (!RegisterHotKey(nullptr, kHotkeyId, mods, vk)) {
    qWarning("Locus: RegisterHotKey failed (%lu) — combo likely taken",
             GetLastError());
    return false;
  }
  gRegistered = true;
  gFilter->owner = this;
  return true;
}

} // namespace locus
