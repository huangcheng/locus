#pragma once

#include "core/Types.h"

#include <QObject>
#include <QString>

namespace locus {

class SessionController : public QObject {
  Q_OBJECT
public:
  enum class State { Closed, Open, Activating };

  explicit SessionController(QObject *parent = nullptr);

  State state() const { return state_; }
  bool isOpen() const { return state_ == State::Open; }
  QString focusedId() const { return focusedId_; }
  StyleId styleId() const { return styleId_; }
  void setStyleId(StyleId id);

  void open();
  void close();
  void setFocus(const QString &id);
  /// Returns focused id if activation proceeds; empty otherwise.
  QString activate();

signals:
  void opened();
  void closed();
  void focusChanged(const QString &id);
  void activateRequested(const QString &id);

private:
  State state_ = State::Closed;
  QString focusedId_;
  StyleId styleId_ = StyleId::Orbital;
};

} // namespace locus
