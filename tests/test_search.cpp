// Type-to-search: the pure filter, plus the overlay's key contract —
// typing filters, Return activates, Esc clears-then-dismisses.
#include "core/Pin.h"
#include "core/SearchFilter.h"
#include "ui/OverlayWindow.h"

#include <QApplication>
#include <QLineEdit>
#include <QtTest>

using namespace locus;

namespace {
QVector<Pin> threePins() {
  QVector<Pin> pins;
  for (const char *id : {"safari", "safari-tools", "mail"}) {
    Pin p;
    p.id = QString::fromLatin1(id);
    p.label = p.id;
    pins.push_back(p);
  }
  return pins;
}
} // namespace

class SearchTest : public QObject {
  Q_OBJECT
private slots:
  void filterMatchesCaseInsensitively() {
    const QVector<Pin> pins = threePins();
    QCOMPARE(filterPins(pins, QString()).size(), 3); // identity
    QCOMPARE(filterPins(pins, QStringLiteral("SAF")).size(), 2);
    QCOMPARE(filterPins(pins, QStringLiteral("mail")).first().id,
             QStringLiteral("mail"));
    QVERIFY(filterPins(pins, QStringLiteral("zzz")).isEmpty());
  }

  void overlaySearchKeyContract() {
    OverlayWindow overlay(new QWidget);
    overlay.resize(400, 300);
    QLineEdit *field = overlay.findChild<QLineEdit *>();
    QVERIFY(field);

    QSignalSpy changed(&overlay, &OverlayWindow::searchChanged);
    QSignalSpy activated(&overlay, &OverlayWindow::searchActivated);
    QSignalSpy dismissed(&overlay, &OverlayWindow::searchDismissed);

    QTest::keyClicks(field, QStringLiteral("sa"));
    QCOMPARE(overlay.searchQuery(), QStringLiteral("sa"));
    QCOMPARE(changed.count(), 2); // "s", then "sa"

    // Return activates (the first match, handled by main.cpp).
    QTest::keyClick(field, Qt::Key_Return);
    QCOMPARE(activated.count(), 1);

    // Esc with a query clears it; Esc on empty dismisses.
    QTest::keyClick(field, Qt::Key_Escape);
    QCOMPARE(overlay.searchQuery(), QString());
    QCOMPARE(dismissed.count(), 0);
    QTest::keyClick(field, Qt::Key_Escape);
    QCOMPARE(dismissed.count(), 1);

    // Per-summon reset.
    QTest::keyClicks(field, QStringLiteral("mail"));
    overlay.resetSearch();
    QCOMPARE(overlay.searchQuery(), QString());
  }
};

QTEST_MAIN(SearchTest)
#include "test_search.moc"
