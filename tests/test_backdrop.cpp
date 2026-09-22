#include "platforms/CrystalBackdrop.h"
#include "ui/OrbitalGlassView.h"

#include <QtTest>

class BackdropTest : public QObject {
  Q_OBJECT
private slots:
  // Before the platform reports a material, the view assumes the painted
  // fallback — legible even if the native backdrop never arrives.
  void defaultsToTint() {
    locus::OrbitalGlassView view;
    QCOMPARE(view.backdrop(), locus::Backdrop::Tint);
  }

  void roundTrips() {
    locus::OrbitalGlassView view;
    view.setBackdrop(locus::Backdrop::Glass);
    QCOMPARE(view.backdrop(), locus::Backdrop::Glass);
    view.setBackdrop(locus::Backdrop::Frosted);
    QCOMPARE(view.backdrop(), locus::Backdrop::Frosted);
    view.setBackdrop(locus::Backdrop::Tint);
    QCOMPARE(view.backdrop(), locus::Backdrop::Tint);
  }

  // No overlay → nothing installed, on every platform.
  void nullOverlayIsTint() {
    QCOMPARE(locus::installCrystalBackdrop(nullptr, QRectF(), false),
             locus::Backdrop::Tint);
  }
};

QTEST_MAIN(BackdropTest)
#include "test_backdrop.moc"
