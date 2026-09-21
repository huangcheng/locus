#include "ui/DockAnim.h"

#include <QtTest>

class DockAnimTest : public QObject {
  Q_OBJECT
private slots:
  void falloffCurve() {
    QCOMPARE(locus::dockanim::falloff(0.0, 150.0), 1.0);
    QCOMPARE(locus::dockanim::falloff(150.0, 150.0), 0.0);
    QCOMPARE(locus::dockanim::falloff(400.0, 150.0), 0.0);
    // midpoint of the cosine falloff is ~0.5
    QVERIFY(locus::dockanim::falloff(75.0, 150.0) > 0.4);
    QVERIFY(locus::dockanim::falloff(75.0, 150.0) < 0.6);
    // monotonically decreasing inside the radius
    QVERIFY(locus::dockanim::falloff(20.0, 150.0) >
            locus::dockanim::falloff(60.0, 150.0));
  }

  void easeOutBackShape() {
    QCOMPARE(locus::dockanim::easeOutBack(0.0), 0.0);
    QCOMPARE(locus::dockanim::easeOutBack(1.0), 1.0);
    QVERIFY(locus::dockanim::easeOutBack(0.7) > 1.0);  // overshoot pop
    QVERIFY(locus::dockanim::easeOutBack(0.3) < 1.0);
  }

  void pressDipShape() {
    QCOMPARE(locus::dockanim::pressDip(0.0, 0.12), 1.0);
    QCOMPARE(locus::dockanim::pressDip(1.0, 0.12), 1.0);
    QVERIFY(qAbs(locus::dockanim::pressDip(0.5, 0.12) - 0.88) < 1e-9);
  }
};

QTEST_MAIN(DockAnimTest)
#include "test_dockanim.moc"
