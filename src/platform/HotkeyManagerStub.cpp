#include "HotkeyManager.h"

namespace locus {

HotkeyManager::HotkeyManager(QObject *parent) : QObject(parent) {}
HotkeyManager::~HotkeyManager() = default;

// TODO: RegisterHotKey on Windows, XCB/Wayland grab on Linux.
bool HotkeyManager::setHotkey(const QKeySequence &) { return false; }

} // namespace locus
