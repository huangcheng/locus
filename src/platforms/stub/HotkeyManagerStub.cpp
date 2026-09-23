#include "platforms/HotkeyManager.h"

namespace locus {

HotkeyManager::HotkeyManager(QObject *parent) : QObject(parent) {}
HotkeyManager::~HotkeyManager() = default;

// XCB/Wayland global-shortcut grab — not implemented yet.
bool HotkeyManager::setHotkey(const QKeySequence &) { return false; }

} // namespace locus
