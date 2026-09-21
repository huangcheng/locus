#pragma once

#include "core/Types.h"

#include <QWidget>

class QButtonGroup;
class QKeySequenceEdit;
class QLabel;
class QListWidget;
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

  /// Switch the visible pane (0 General, 1 Pins, 2 Density).
  void selectPane(int index);

signals:
  void appearanceChanged();
  void styleChanged();
  void densityChanged();
  void pinsChanged();

protected:
  void showEvent(QShowEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  QWidget *buildGeneralPane();
  QWidget *buildPinsPane();
  QWidget *buildDensityPane();
  void applyPalette();
  void reloadPinRows();
  void commitPinOrder();
  void addApp();

  Prefs *prefs_;
  PinStore *pins_;
  const IconProvider *icons_;
  Appearance appearance_ = Appearance::Dark;
  bool reloadingPins_ = false;

  QStackedWidget *stack_ = nullptr;
  QButtonGroup *navGroup_ = nullptr;

  // General pane
  QButtonGroup *themeGroup_ = nullptr;
  QWidget *styleTileHex_ = nullptr;
  QWidget *styleTileOrbit_ = nullptr;
  QKeySequenceEdit *hotkeyEdit_ = nullptr;
  QWidget *loginToggle_ = nullptr;

  // Pins pane
  QListWidget *pinList_ = nullptr;
  QLabel *pinCount_ = nullptr;

  // Density pane
  QSlider *cellSizeSlider_ = nullptr;
  QSlider *cellGapSlider_ = nullptr;
  QSlider *iconSizeSlider_ = nullptr;
  QLabel *cellSizeValue_ = nullptr;
  QLabel *cellGapValue_ = nullptr;
  QLabel *iconSizeValue_ = nullptr;
  QWidget *densityPreview_ = nullptr;
};

} // namespace locus
