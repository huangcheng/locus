#pragma once

#include "core/MenuView.h"
#include "core/Types.h"

#include <QBasicTimer>
#include <QElapsedTimer>
#include <QHash>
#include <QIcon>
#include <QPixmap>
#include <QPointF>
#include <QWidget>

class QPainter;

namespace locus {

class CellularGlassView : public QWidget, public MenuView {
  Q_OBJECT
public:
  explicit CellularGlassView(QWidget *parent = nullptr);

  void setScene(const SceneModel &scene) override;
  QWidget *widget() override { return this; }
  QSize sizeHint() const override;

  void setAppearance(Appearance appearance);
  void setIcon(const QString &pinId, const QIcon &icon);
  void setIconSize(qreal size);

  /// Replays the center-out open pop. Call each time the widget is shown.
  void playOpenAnimation();

signals:
  void itemHovered(const QString &id);
  void itemActivated(const QString &id);
  void dismissRequested();

protected:
  void paintEvent(QPaintEvent *event) override;
  void timerEvent(QTimerEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  QRectF gridBounds() const;
  QPointF origin() const; // scene → view translation (see .cpp)
  qreal cellScale(const QString &id, qint64 now) const;
  void updateScales();

  SceneModel scene_;
  Appearance appearance_ = Appearance::Dark;
  qreal iconSize_ = 40.0;
  QHash<QString, QPixmap> pixmaps_; // high-res render cache for smooth scaling
  QString lastHover_;

  // Dock-style motion state
  QVector<QString> lastIds_;   // pin order in the current scene
  QVector<QString> openOrder_; // ids sorted by distance from grid center
  QHash<QString, qreal> scales_; // current animated hover scale per pin
  qint64 shownAt_ = -1;        // ms timestamp of the open pop, -1 = done
  QString pressedId_;
  qint64 pressedAt_ = -1;
  QPointF cursorPos_;
  bool cursorInside_ = false;
  QBasicTimer animTimer_;
  QElapsedTimer clock_;
};

} // namespace locus
