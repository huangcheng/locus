#include "core/SessionController.h"

namespace locus {

SessionController::SessionController(QObject *parent) : QObject(parent) {}

void SessionController::setStyleId(StyleId id) { styleId_ = id; }

void SessionController::open() {
  if (state_ == State::Open)
    return;
  state_ = State::Open;
  emit opened();
}

void SessionController::close() {
  if (state_ == State::Closed)
    return;
  state_ = State::Closed;
  focusedId_.clear();
  emit closed();
}

void SessionController::setFocus(const QString &id) {
  if (state_ != State::Open)
    return;
  if (focusedId_ == id)
    return;
  focusedId_ = id;
  emit focusChanged(focusedId_);
}

QString SessionController::activate() {
  if (state_ != State::Open || focusedId_.isEmpty())
    return {};
  const QString id = focusedId_;
  state_ = State::Activating;
  emit activateRequested(id);
  close();
  return id;
}

} // namespace locus
