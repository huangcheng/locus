#pragma once

#include <QKeySequence>
#include <QObject>

namespace locus {

/// System-wide hotkey registration. Emits triggered() when the user presses
/// the shortcut while any app is focused. macOS uses Carbon
/// RegisterEventHotKey, which needs no accessibility permission; Windows
/// uses RegisterHotKey; other platforms are stubbed until implemented.
class HotkeyManager : public QObject {
  Q_OBJECT
public:
  explicit HotkeyManager(QObject *parent = nullptr);
  ~HotkeyManager() override;

  /// Registers the sequence globally; an empty sequence just unregisters.
  /// Returns false when the combo cannot be registered (already taken,
  /// unsupported key, or no modifier).
  bool setHotkey(const QKeySequence &sequence);

signals:
  void triggered();
};

} // namespace locus
