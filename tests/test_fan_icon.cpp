// Regression: a full-bleed icon must not be cropped on the face-up card.
// paintAppIcon overscales icons by kIconBleed to eat the ~20% transparent
// margin macOS icons carry; Windows/Linux shell icons are already full-bleed,
// so the overscale cut their edges off (visible as clipped app icons in the
// Fan style). The probe icon paints a ring hard against its canvas edge —
// any overscale pushes the ring out of the rounded clip and it vanishes.
#include "core/Pin.h"
#include "core/SceneModel.h"
#include "layout/FanLayoutStrategy.h"
#include "ui/FanGlassView.h"

#include <QPainter>
#include <QtTest>

namespace {

// Full-bleed probe: opaque white disc with a blue ring at the canvas edge.
// The pixmap must be LARGER than the painted box: QIcon::paint only scales
// down, so the overscale bug never touches icons whose largest pixmap is
// smaller than the target — real .ico/.icns files all carry 256px art.
QIcon ringIcon() {
  QPixmap pm(256, 256);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  p.setBrush(Qt::white);
  p.setPen(QPen(QColor(0, 80, 255), 20));
  p.drawEllipse(QRectF(10.0, 10.0, 236.0, 236.0));
  return QIcon(pm);
}

} // namespace

class FanIconTest : public QObject {
  Q_OBJECT
private slots:
  void fullBleedIconEdgeRingOnFaceUpCard() {
    locus::Pin pin;
    pin.id = QStringLiteral("p0");
    pin.label = QStringLiteral("Probe");
    pin.appPath = QStringLiteral("/tmp/probe");

    locus::FanLayoutStrategy strategy;
    const locus::SceneModel scene =
        strategy.build({pin}, pin.id, locus::DensityPrefs());

    const locus::PlacedItem *card = nullptr;
    for (const auto &it : scene.items)
      if (it.id == pin.id && it.role == locus::ItemRole::Item)
        card = &it;
    QVERIFY(card);

    locus::FanGlassView view;
    view.setScene(scene);
    view.setIcon(pin.id, ringIcon());
    view.resize(view.sizeHint());

    const QImage img = view.grab().toImage();

    // Raised-card center (paintEvent: lifted kPullOut, scaled kRaiseScale);
    // the ring is the only blue content anywhere in the scene.
    const QPointF c = card->bounds.center() + QPointF(0, -26.0);
    bool foundRing = false;
    for (int dx = 24; dx <= 44; ++dx) {
      const QColor px = img.pixelColor(qRound(c.x() - dx), qRound(c.y()));
      if (px.blue() > 180 && px.red() < 100) {
        foundRing = true;
        break;
      }
    }
#ifdef Q_OS_MAC
    // macOS keeps the margin-eating overscale: a full-bleed probe is cropped
    // by design there (real macOS icons carry the ~20% margin it consumes).
    QVERIFY(!foundRing);
#else
    QVERIFY2(foundRing, "icon edge ring cropped — full-bleed icon overscaled");
#endif
  }
};

QTEST_MAIN(FanIconTest)
#include "test_fan_icon.moc"
