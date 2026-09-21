#include "core/Prefs.h"

namespace locus {

Prefs::Prefs(QSettings *settings) : settings_(settings) {}

void Prefs::setAppearance(Appearance a) {
  appearance_ = a;
  save();
}

void Prefs::setStyleId(StyleId id) {
  styleId_ = id;
  save();
}

void Prefs::setDensity(DensityPrefs d) {
  density_ = d;
  save();
}

void Prefs::setHotkey(QKeySequence seq) {
  hotkey_ = std::move(seq);
  save();
}

void Prefs::load() {
  if (!settings_)
    return;
  const int appearanceInt =
      settings_->value(QStringLiteral("appearance"), static_cast<int>(Appearance::System))
          .toInt();
  appearance_ = (appearanceInt >= 0 &&
                 appearanceInt <= static_cast<int>(Appearance::System))
                    ? static_cast<Appearance>(appearanceInt)
                    : Appearance::System;
  const int styleInt =
      settings_->value(QStringLiteral("style"), static_cast<int>(StyleId::Cellular))
          .toInt();
  styleId_ = (styleInt >= 0 && styleInt <= static_cast<int>(StyleId::Pie))
                 ? static_cast<StyleId>(styleInt)
                 : StyleId::Cellular;
  density_.minIconSize =
      settings_->value(QStringLiteral("density/minIconSize"), 34.0).toDouble();
  density_.maxPerRing =
      settings_->value(QStringLiteral("density/maxPerRing"), 12).toInt();
  density_.gap = settings_->value(QStringLiteral("density/gap"), 12.0).toDouble();
  density_.widgetSize =
      settings_->value(QStringLiteral("density/widgetSize"), 560.0).toDouble();
  density_.cellSize =
      settings_->value(QStringLiteral("density/cellSize"), 80.0).toDouble();
  density_.cellGap =
      settings_->value(QStringLiteral("density/cellGap"), 8.0).toDouble();
  density_.iconSize =
      settings_->value(QStringLiteral("density/iconSize"), 40.0).toDouble();
  const QString hotkeyStr =
      settings_->value(QStringLiteral("hotkey"), QStringLiteral("Meta+Space"))
          .toString();
  // "Ctrl+Space" was the pre-macOS-aware default (Qt::CTRL = Command there);
  // migrate it to the real Control key. User-recorded shortcuts are untouched.
  hotkey_ = QKeySequence(hotkeyStr == QStringLiteral("Ctrl+Space")
                             ? QStringLiteral("Meta+Space")
                             : hotkeyStr);
}

void Prefs::save() const {
  if (!settings_)
    return;
  settings_->setValue(QStringLiteral("appearance"), static_cast<int>(appearance_));
  settings_->setValue(QStringLiteral("style"), static_cast<int>(styleId_));
  settings_->setValue(QStringLiteral("density/minIconSize"), density_.minIconSize);
  settings_->setValue(QStringLiteral("density/maxPerRing"), density_.maxPerRing);
  settings_->setValue(QStringLiteral("density/gap"), density_.gap);
  settings_->setValue(QStringLiteral("density/widgetSize"), density_.widgetSize);
  settings_->setValue(QStringLiteral("density/cellSize"), density_.cellSize);
  settings_->setValue(QStringLiteral("density/cellGap"), density_.cellGap);
  settings_->setValue(QStringLiteral("density/iconSize"), density_.iconSize);
  settings_->setValue(QStringLiteral("hotkey"), hotkey_.toString());
}

} // namespace locus
