#pragma once

#include "core/MenuView.h"
#include "core/Types.h"

#include <QHash>
#include <QIcon>
#include <QWidget>

namespace locus {

class OrbitalGlassView : public QWidget, public MenuView {
  Q_OBJECT
public:
  explicit OrbitalGlassView(QWidget *parent = nullptr);

  void setScene(const SceneModel &scene) override;
  QWidget *widget() override { return this; }
  QSize sizeHint() const override { return QSize(560, 560); }

  void setAppearance(Appearance appearance);
  void setIcon(const QString &pinId, const QIcon &icon);

signals:
  void itemHovered(const QString &id);
  void itemActivated(const QString &id);
  void rotationDelta(qreal radians);
  void dismissRequested();

protected:
  void paintEvent(QPaintEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  SceneModel scene_;
  Appearance appearance_ = Appearance::Dark;
  QHash<QString, QIcon> icons_;
  QString lastHover_;
};

} // namespace locus
