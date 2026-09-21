#include "core/SessionController.h"

#include <QSignalSpy>
#include <QtTest>

class SessionTest : public QObject {
  Q_OBJECT
private slots:
  void openClose() {
    navi::SessionController s;
    QCOMPARE(s.state(), navi::SessionController::State::Closed);
    s.open();
    QCOMPARE(s.state(), navi::SessionController::State::Open);
    QVERIFY(s.isOpen());
    s.close();
    QCOMPARE(s.state(), navi::SessionController::State::Closed);
  }

  void focusOnlyWhenOpen() {
    navi::SessionController s;
    s.setFocus(QStringLiteral("a"));
    QVERIFY(s.focusedId().isEmpty());
    s.open();
    s.setFocus(QStringLiteral("a"));
    QCOMPARE(s.focusedId(), QStringLiteral("a"));
  }

  void activateEmitsAndCloses() {
    navi::SessionController s;
    QSignalSpy spy(&s, &navi::SessionController::activateRequested);
    s.open();
    s.setFocus(QStringLiteral("code"));
    QCOMPARE(s.activate(), QStringLiteral("code"));
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("code"));
    QCOMPARE(s.state(), navi::SessionController::State::Closed);
    QVERIFY(s.focusedId().isEmpty());
  }

  void activateWithoutFocusDoesNothing() {
    navi::SessionController s;
    QSignalSpy spy(&s, &navi::SessionController::activateRequested);
    s.open();
    QVERIFY(s.activate().isEmpty());
    QCOMPARE(spy.size(), 0);
    QCOMPARE(s.state(), navi::SessionController::State::Open);
  }
};

QTEST_MAIN(SessionTest)
#include "test_session.moc"
