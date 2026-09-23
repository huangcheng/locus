#pragma once

#include "core/Types.h"

#include <QWidget>

class QButtonGroup;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
class QStackedWidget;
class QToolButton;

namespace locus {

class IconProvider;
class PinStore;
class Prefs;

/// Settings window: sidebar + stacked panes (General / Pins / Density),
/// styled after the honeycomb design language (glass cards, amber accent).
class PrefsWindow : public QWidget {
  Q_OBJECT
public:
  PrefsWindow(Prefs *prefs, PinStore *pins, const IconProvider *icons,
              QWidget *parent = nullptr);

  /// The widget's effective theme (System already resolved), so the window
  /// matches what the launcher currently looks like.
  void setResolvedAppearance(Appearance appearance);

  /// Re-sync every control from prefs/pins (call before showing).
  void refreshFromModel();

  /// Re-apply every user-visible string after the app language changes.
  void retranslateUi();

  /// Switch the visible pane (0 General, 1 Pins, 2 Density).
  void selectPane(int index);

signals:
  void appearanceChanged();
  void styleChanged();
  void densityChanged();
  void pinsChanged();
  void languageChanged();
  void hotkeyChanged();

protected:
  void showEvent(QShowEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  QWidget *buildGeneralPane();
  QWidget *buildPinsPane();
  QWidget *buildDensityPane();
  QWidget *buildAboutPane();
  void updateAboutLinks();
  void applyPalette();
  void reloadPinRows();
  void commitPinOrder();
  void addApp();
  void updateDensityStrings();

  Prefs *prefs_;
  PinStore *pins_;
  const IconProvider *icons_;
  Appearance appearance_ = Appearance::Dark;
  bool reloadingPins_ = false;

  QStackedWidget *stack_ = nullptr;
  QButtonGroup *navGroup_ = nullptr;

  // General pane
  QLabel *generalTitle_ = nullptr;
  QLabel *themeLabel_ = nullptr;
  QButtonGroup *themeGroup_ = nullptr;
  QLabel *languageLabel_ = nullptr;
  QButtonGroup *languageGroup_ = nullptr;
  QLabel *styleLabel_ = nullptr;
  QWidget *styleTileHex_ = nullptr;
  QWidget *styleTileOrbit_ = nullptr;
  QWidget *styleTileFan_ = nullptr;
  QLabel *hotkeyLabel_ = nullptr;
  QLabel *hotkeyCaption_ = nullptr;
  QWidget *hotkeyField_ = nullptr;
  QLabel *loginLabel_ = nullptr;
  QWidget *loginToggle_ = nullptr;

  // Pins pane
  QLabel *pinsTitle_ = nullptr;
  QListWidget *pinList_ = nullptr;
  QLabel *pinCount_ = nullptr;
  QPushButton *addAppBtn_ = nullptr;
  QLabel *pinsNote_ = nullptr;

  // Density pane
  QLabel *densityTitle_ = nullptr;
  QLabel *cellSizeLabel_ = nullptr;
  QLabel *cellGapLabel_ = nullptr;
  QLabel *iconSizeLabel_ = nullptr;
  QSlider *cellSizeSlider_ = nullptr;
  QSlider *cellGapSlider_ = nullptr;
  QSlider *iconSizeSlider_ = nullptr;
  QLabel *cellSizeValue_ = nullptr;
  QLabel *cellGapValue_ = nullptr;
  QLabel *iconSizeValue_ = nullptr;
  QLabel *previewCaption_ = nullptr;
  QLabel *densityNote_ = nullptr;
  QWidget *densityPreview_ = nullptr;

  // About pane
  QLabel *aboutTitle_ = nullptr;
  QWidget *aboutLogo_ = nullptr;
  QLabel *aboutName_ = nullptr;
  QLabel *aboutVersion_ = nullptr;
  QLabel *aboutTagline_ = nullptr;
  QLabel *aboutCopyright_ = nullptr;
};

} // namespace locus
