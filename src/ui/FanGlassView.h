#pragma once

#include "core/MenuView.h"
#include "core/Types.h"

#include <QElapsedTimer>
#include <QHash>
#include <QIcon>
#include <QTimer>
#include <QWidget>

namespace locus {

/// Fan v2 widget: glass cards on an arc, gold focus glow, tilt-aware hit-test.
class FanGlassView : public QWidget, public MenuView {
  Q_OBJECT
public:
  explicit FanGlassView(QWidget *parent = nullptr);

  void setScene(const SceneModel &scene) override;
  QWidget *widget() override { return this; }
  QSize sizeHint() const override { return QSize(extent_, extent_); }

  void setAppearance(Appearance appearance);
  void setIcon(const QString &pinId, const QIcon &icon);

  /// Density "Icon size" slider — the face-up card glyph, clamped to the
  /// card width so it can never overflow the card.
  void setIconSize(qreal size) {
    iconSize_ = size;
    update();
  }

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
  void setHoverTarget(const QString &id);
  void advanceAnimation();
  QString labelFor(const QString &id) const;

  SceneModel scene_;
  Appearance appearance_ = Appearance::Dark;
  qreal iconSize_ = 40.0;
  QHash<QString, QIcon> icons_;
  QString lastHover_;
  int extent_ = 560;

  QHash<QString, qreal> emphasis_;
  QTimer animTimer_;
  QElapsedTimer animClock_;
};

} // namespace locus
