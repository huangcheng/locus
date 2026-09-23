// Regression: clicking the VISIBLE (raised) focused card must activate that
// card. The raised card paints upright, morphed to a square, lifted 26px and
// scaled 1.07 (FanGlassView paintEvent); before the fix, mousePressEvent
// hit-tested the REST geometry, so the top of the visible card resolved to
// the back hand's card — launching the wrong app.
#include "core/Pin.h"
#include "core/SceneModel.h"
#include "layout/FanLayoutStrategy.h"
#include "ui/FanGlassView.h"

#include <QMouseEvent>
#include <QSignalSpy>
#include <QtTest>

namespace {

QVector<locus::Pin> makePins(int n) {
  QVector<locus::Pin> pins;
  pins.reserve(n);
  for (int i = 0; i < n; ++i) {
    locus::Pin p;
    p.id = QStringLiteral("p%1").arg(i);
    p.label = QStringLiteral("App %1").arg(i);
    p.appPath = QStringLiteral("/tmp/app%1.app").arg(i);
    pins.push_back(p);
  }
  return pins;
}

const locus::PlacedItem *itemFor(const locus::SceneModel &scene,
                                 const QString &id) {
  for (const auto &it : scene.items)
    if (it.id == id && it.role == locus::ItemRole::Item)
      return &it;
  return nullptr;
}

// Mirror of FanGlassView's raised geometry: square (h morphs to w), lifted
// kPullOut, scaled kRaiseScale about the card center.
constexpr qreal kPullOut = 26.0;
constexpr qreal kRaiseScale = 1.07;

QRectF raisedRect(const locus::PlacedItem &card) {
  const QPointF c = card.bounds.center();
  const qreal w = card.bounds.width() * kRaiseScale;
  return QRectF(c.x() - w / 2.0, c.y() - kPullOut - w / 2.0, w, w);
}

void pressAt(locus::FanGlassView &view, const QPointF &local) {
  QMouseEvent press(QEvent::MouseButtonPress, local, QPointF(), Qt::LeftButton,
                    Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&view, &press);
}

} // namespace

class FanPressTest : public QObject {
  Q_OBJECT
private slots:
  void pressOnRaisedCardTopActivatesIt() {
    locus::FanLayoutStrategy strategy;
    const auto pins = makePins(18); // two hands: back hand sits above hand 0
    const QString focused = pins[4].id;
    const locus::SceneModel scene =
        strategy.build(pins, focused, locus::DensityPrefs());

    const locus::PlacedItem *card = itemFor(scene, focused);
    QVERIFY(card);

    // A point on the top of the card as the user SEES it raised — above the
    // card's rest shape, which only the raised geometry covers.
    const QRectF raised = raisedRect(*card);
    const QPointF top(raised.center().x(),
                      raised.top() + raised.height() * 0.15);
    QVERIFY(raised.contains(top));
    // Precondition for the regression: outside the card's rest shape, i.e.
    // the old rest-geometry hit-test never resolved it to the focused card.
    QVERIFY(!card->bounds.contains(top));

    locus::FanGlassView view;
    view.setScene(scene);
    view.resize(view.sizeHint());

    QSignalSpy activated(&view, &locus::FanGlassView::itemActivated);
    pressAt(view, top);
    QCOMPARE(activated.size(), 1);
    QCOMPARE(activated.at(0).at(0).toString(), focused);
  }

  void pressOnCoveredRestAreaStillActivatesRaised() {
    // Bottom of the raised card overlaps its own rest shape; the raised card
    // must win even where a plain hit-test could also match it.
    locus::FanLayoutStrategy strategy;
    const auto pins = makePins(18);
    const QString focused = pins[4].id;
    const locus::SceneModel scene =
        strategy.build(pins, focused, locus::DensityPrefs());
    const locus::PlacedItem *card = itemFor(scene, focused);
    QVERIFY(card);

    locus::FanGlassView view;
    view.setScene(scene);
    view.resize(view.sizeHint());

    QSignalSpy activated(&view, &locus::FanGlassView::itemActivated);
    pressAt(view, raisedRect(*card).center());
    QCOMPARE(activated.size(), 1);
    QCOMPARE(activated.at(0).at(0).toString(), focused);
  }

  void pressOnEmptyCanvasDismisses() {
    locus::FanLayoutStrategy strategy;
    const locus::SceneModel scene =
        strategy.build(makePins(4), {}, locus::DensityPrefs());

    locus::FanGlassView view;
    view.setScene(scene);
    view.resize(view.sizeHint());

    QSignalSpy dismissed(&view, &locus::FanGlassView::dismissRequested);
    QSignalSpy activated(&view, &locus::FanGlassView::itemActivated);
    pressAt(view, QPointF(2, 2));
    QCOMPARE(dismissed.size(), 1);
    QCOMPARE(activated.size(), 0);
  }
};

QTEST_MAIN(FanPressTest)
#include "test_fan_press.moc"
