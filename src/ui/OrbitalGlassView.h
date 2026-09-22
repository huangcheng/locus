#pragma once

#include "core/MenuView.h"
#include "core/Types.h"

#include <QElapsedTimer>
#include <QHash>
#include <QIcon>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

namespace locus {

/// Orbit v2 widget: glass disc with concentric chip tracks, a cursor-tracking
/// spotlight with a sparkle trail, and a center hub that shows the Locus logo
/// at idle and the hovered app's icon while hovering. Hover state is
/// animated: chips ease their emphasis and the hub crossfades — all snapped
/// instantly under Reduce Motion.
class OrbitalGlassView : public QWidget, public MenuView {
  Q_OBJECT
public:
  explicit OrbitalGlassView(QWidget *parent = nullptr);

  void setScene(const SceneModel &scene) override;
  QWidget *widget() override { return this; }
  QSize sizeHint() const override { return QSize(extent_, extent_); }

  void setAppearance(Appearance appearance);
  void setIcon(const QString &pinId, const QIcon &icon);

  /// Disc bounds in widget coordinates; the platform crystal backdrop masks
  /// its backdrop blur to this circle.
  QRectF discRect() const { return discBounds(); }

signals:
  void itemHovered(const QString &id);
  void itemActivated(const QString &id);
  void dismissRequested();

protected:
  void paintEvent(QPaintEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  QRectF discBounds() const;
  qreal sparkRadius() const; // outermost track radius — the spark's orbit
  const PlacedItem *hoveredItem() const;
  void setHoverTarget(const QString &id);
  void advanceAnimation();

  SceneModel scene_;
  Appearance appearance_ = Appearance::Dark;
  QHash<QString, QIcon> icons_;
  QString lastHover_;
  int extent_ = 560;

  // Animation state (0..1 unless noted). The timer runs only while at least
  // one value is away from its target.
  QHash<QString, qreal> emphasis_; // per-chip hover emphasis
  qreal hubFade_ = 0.0; // 0 = logo, 1 = hovered app's icon
  QString hubIconId_;   // last non-empty hover, used while fading out
  qreal spinDeg_ = 0.0; // spark head angle, degrees
  qreal spinTargetDeg_ = 0.0;
  qreal spinTailDeg_ = 0.0; // tail length, grows with chase speed
  int spinDir_ = 1;         // tail extends opposite to the chase direction
  qreal spinAlpha_ = 0.0;
  bool spinInside_ = false; // cursor is inside the disc
  QTimer animTimer_;
  QElapsedTimer animClock_;
};

} // namespace locus
