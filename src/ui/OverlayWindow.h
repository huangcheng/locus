#pragma once

#include <QWidget>

namespace locus {

class OverlayWindow : public QWidget {
  Q_OBJECT
public:
  explicit OverlayWindow(QWidget *content, QWidget *parent = nullptr);

  void showAt(QPoint globalCenter);
  void setContent(QWidget *content);
  void resizeToContent();

private:
  QWidget *content_ = nullptr;
};

} // namespace locus
