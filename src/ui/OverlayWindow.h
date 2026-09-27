#pragma once

#include <QWidget>

class QLineEdit;

namespace locus {

class OverlayWindow : public QWidget {
  Q_OBJECT
public:
  explicit OverlayWindow(QWidget *content, QWidget *parent = nullptr);

  void showAt(QPoint globalCenter);
  void setContent(QWidget *content);
  void resizeToContent();

  // Type-to-search strip above the content. The field always has focus while
  // the overlay is open; Esc clears the query (then dismisses), Return
  // activates the first match.
  QString searchQuery() const;
  void resetSearch();

signals:
  void searchChanged(const QString &query);
  void searchActivated(); // Return pressed in the search field
  void searchDismissed(); // Esc pressed with an empty query

protected:
  void hideEvent(QHideEvent *event) override;

private:
  QLineEdit *search_ = nullptr;
  QWidget *content_ = nullptr;
};

} // namespace locus
