#include "ui/PrefsWindow.h"

#include "core/Pin.h"
#include "core/PinStore.h"
#include "core/Prefs.h"
#include "platform/IconProvider.h"
#include "platform/MacActivation.h"
#include "platform/MacLoginItem.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QDir>
#include <QDrag>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEasingCurve>
#include <QFileDialog>
#include <QFileInfo>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSlider>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVariantAnimation>
#include <QVBoxLayout>

namespace locus {
namespace {

struct Palette {
  QColor sidebarBg, separator, cardBg, cardBorder;
  QColor text, secondary, faint;
  QColor amber, amberBright, amberDeep, onAmber;
  QColor accentText; // amber that stays readable on the window background
  QColor amberTint;  // active nav / tile background
  QColor controlBg;  // segmented container, keycaps, slider groove
  QColor trackOff;
};

Palette paletteFor(Appearance appearance) {
  Palette p;
  if (appearance == Appearance::Dark) {
    p.sidebarBg = QColor(255, 255, 255, 8);
    p.separator = QColor(255, 255, 255, 18);
    p.cardBg = QColor(255, 255, 255, 12);
    p.cardBorder = QColor(255, 255, 255, 23);
    p.text = QColor(255, 255, 255, 235);
    p.secondary = QColor(255, 255, 255, 140);
    p.faint = QColor(255, 255, 255, 102);
    p.amber = QColor("#F5D68A");
    p.amberBright = QColor("#F8DD9C");
    p.amberDeep = QColor("#EEC86D");
    p.onAmber = QColor("#241A08");
    p.accentText = p.amber;
    p.amberTint = QColor(245, 214, 138, 40);
    p.controlBg = QColor(255, 255, 255, 15);
    p.trackOff = QColor(255, 255, 255, 45);
  } else {
    p.sidebarBg = QColor(0, 0, 0, 6);
    p.separator = QColor(0, 0, 0, 20);
    p.cardBg = QColor("#FFFFFF");
    p.cardBorder = QColor(0, 0, 0, 16);
    p.text = QColor("#1D1D1F");
    p.secondary = QColor("#6E6E73");
    p.faint = QColor("#8E8E93");
    // Light mode is monochrome: graphite accents instead of amber.
    p.amber = QColor("#1D1D1F");
    p.amberBright = QColor("#3A3A3C");
    p.amberDeep = QColor("#000000");
    p.onAmber = QColor("#FFFFFF");
    p.accentText = QColor("#1D1D1F");
    p.amberTint = QColor(0, 0, 0, 12);
    p.controlBg = QColor(0, 0, 0, 10);
    p.trackOff = QColor(0, 0, 0, 45);
  }
  return p;
}

QString css(const QColor &c) {
  if (c.alpha() == 255)
    return c.name();
  return QStringLiteral("rgba(%1,%2,%3,%4)")
      .arg(c.red())
      .arg(c.green())
      .arg(c.blue())
      .arg(c.alpha());
}

QPainterPath hexPath(qreal x, qreal y, qreal w, qreal h) {
  QPainterPath path;
  path.moveTo(x + w / 2.0, y);
  path.lineTo(x + w, y + h * 0.25);
  path.lineTo(x + w, y + h * 0.75);
  path.lineTo(x + w / 2.0, y + h);
  path.lineTo(x, y + h * 0.75);
  path.lineTo(x, y + h * 0.25);
  path.closeSubpath();
  return path;
}

// Sidebar glyphs: gear / hexagon / sliders, painted per palette state.
QPixmap navGlyph(int kind, const QColor &color) {
  QPixmap pm(32, 32);
  pm.setDevicePixelRatio(2);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  if (kind == 0) {
    p.drawEllipse(QPointF(8, 8), 2.6, 2.6);
    for (int i = 0; i < 8; ++i) {
      const qreal a = M_PI * i / 4.0;
      p.drawLine(QPointF(8 + std::cos(a) * 4.6, 8 + std::sin(a) * 4.6),
                 QPointF(8 + std::cos(a) * 6.4, 8 + std::sin(a) * 6.4));
    }
  } else if (kind == 1) {
    const QPolygonF hex = {QPointF(8, 2.5), QPointF(13, 5.4), QPointF(13, 10.6),
                           QPointF(8, 13.5), QPointF(3, 10.6), QPointF(3, 5.4)};
    p.drawPolygon(hex);
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(8, 8), 1.4, 1.4);
  } else {
    for (const qreal y : {4.0, 8.0, 12.0})
      p.drawLine(QPointF(2, y), QPointF(14, y));
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(5.5, 4), 1.8, 1.8);
    p.drawEllipse(QPointF(10.5, 8), 1.8, 1.8);
    p.drawEllipse(QPointF(7, 12), 1.8, 1.8);
  }
  return pm;
}

QIcon navIcon(int kind, const Palette &pal) {
  QIcon icon;
  icon.addPixmap(navGlyph(kind, pal.secondary), QIcon::Normal, QIcon::Off);
  icon.addPixmap(navGlyph(kind, pal.accentText), QIcon::Normal, QIcon::On);
  return icon;
}

class Toggle : public QWidget {
public:
  explicit Toggle(QWidget *parent = nullptr) : QWidget(parent) {
    setFixedSize(36, 22);
    setCursor(Qt::PointingHandCursor);
  }

  void setColors(const Palette &pal) {
    onBright_ = pal.amberBright;
    onDeep_ = pal.amberDeep;
    off_ = pal.trackOff;
    update();
  }

  bool isChecked() const { return checked_; }

  void setChecked(bool checked, bool animate) {
    if (checked_ == checked)
      return;
    checked_ = checked;
    if (animate) {
      auto *anim = new QVariantAnimation(this);
      anim->setStartValue(pos_);
      anim->setEndValue(checked_ ? 1.0 : 0.0);
      anim->setDuration(140);
      anim->setEasingCurve(QEasingCurve::InOutQuad);
      QObject::connect(anim, &QVariantAnimation::valueChanged, this,
                       [this](const QVariant &v) {
                         pos_ = v.toReal();
                         update();
                       });
      anim->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
      pos_ = checked_ ? 1.0 : 0.0;
      update();
    }
  }

  std::function<void(bool)> onToggled;

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      setChecked(!checked_, true);
      if (onToggled)
        onToggled(checked_);
    }
  }

  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF track(0.5, 0.5, width() - 1.0, height() - 1.0);
    p.setPen(Qt::NoPen);
    p.setBrush(off_);
    p.drawRoundedRect(track, height() / 2.0 - 0.5, height() / 2.0 - 0.5);
    if (pos_ > 0.001) {
      QLinearGradient onGrad(track.topLeft(), track.bottomLeft());
      onGrad.setColorAt(0, onBright_);
      onGrad.setColorAt(1, onDeep_);
      p.setOpacity(pos_);
      p.setBrush(onGrad);
      p.drawRoundedRect(track, height() / 2.0 - 0.5, height() / 2.0 - 0.5);
      p.setOpacity(1.0);
    }
    const qreal knobD = height() - 4.0;
    const qreal x = 2.0 + pos_ * (width() - knobD - 4.0);
    p.setPen(QPen(QColor(0, 0, 0, 45), 0.5));
    p.setBrush(Qt::white);
    p.drawEllipse(QRectF(x, 2.0, knobD, knobD));
  }

private:
  bool checked_ = false;
  qreal pos_ = 0.0;
  QColor onBright_ = QColor("#F8DD9C");
  QColor onDeep_ = QColor("#EEC86D");
  QColor off_ = QColor(255, 255, 255, 45);
};

class StyleTile : public QFrame {
public:
  enum Kind { Honeycomb, Orbit };

  StyleTile(Kind kind, const QString &title, QWidget *parent = nullptr)
      : QFrame(parent), kind_(kind), title_(title) {
    setFixedHeight(104);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
  }

  void setPaletteColors(const Palette &pal) {
    pal_ = pal;
    update();
  }
  void setCheckedState(bool checked) {
    checked_ = checked;
    update();
  }
  void setCaption(const QString &caption) {
    caption_ = caption;
    update();
  }
  void setTitle(const QString &title) {
    title_ = title;
    update();
  }

  std::function<void()> onClicked;

protected:
  void mouseReleaseEvent(QMouseEvent *event) override {
    if (rect().contains(event->pos()) && onClicked)
      onClicked();
  }

  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen(checked_ ? pal_.amber : pal_.cardBorder, 1));
    p.setBrush(checked_ ? pal_.amberTint : pal_.cardBg);
    p.drawRoundedRect(r, 10, 10);

    const qreal cx = width() / 2.0;
    const qreal top = 14.0;
    QColor cellColor = pal_.text;
    cellColor.setAlpha(38);
    if (kind_ == Honeycomb) {
      const qreal w = 12, h = 14, ox = cx - 19;
      const QPointF spots[8] = {{ox + 1, top},          {ox + 14, top},
                                {ox + 27, top},         {ox + 7.5, top + 11},
                                {ox + 20.5, top + 11},  {ox + 1, top + 22},
                                {ox + 14, top + 22},    {ox + 27, top + 22}};
      for (int i = 0; i < 8; ++i) {
        p.setPen(Qt::NoPen);
        p.setBrush(i == 3 ? pal_.amber : cellColor);
        p.drawPath(hexPath(spots[i].x(), spots[i].y(), w, h));
      }
    } else {
      const QPointF center(cx, top + 18);
      QColor track = pal_.text;
      track.setAlpha(40);
      QPen trackPen(track, 3);
      p.setPen(trackPen);
      p.setBrush(Qt::NoBrush);
      p.drawEllipse(center, 14, 14);
      QPen arcPen(pal_.amber, 3);
      arcPen.setCapStyle(Qt::RoundCap);
      p.setPen(arcPen);
      p.drawArc(QRectF(center.x() - 14, center.y() - 14, 28, 28), -31 * 16,
                63 * 16);
      p.setPen(Qt::NoPen);
      p.setBrush(pal_.amber);
      p.drawEllipse(center, 3, 3);
      QColor bead = pal_.text;
      bead.setAlpha(140);
      p.setBrush(bead);
      for (int i = 1; i < 6; ++i) {
        const qreal a = M_PI * i / 3.0;
        p.drawEllipse(
            QPointF(center.x() + std::cos(a) * 14, center.y() + std::sin(a) * 14),
            2, 2);
      }
    }

    QFont f = font();
    f.setPixelSize(12);
    f.setBold(checked_);
    p.setFont(f);
    p.setPen(checked_ ? pal_.text : pal_.secondary);
    p.drawText(QRectF(0, 58, width(), 16), Qt::AlignHCenter, title_);
    f.setBold(false);
    f.setPixelSize(10);
    p.setFont(f);
    p.setPen(checked_ ? pal_.accentText : pal_.faint);
    p.drawText(QRectF(0, 74, width(), 13), Qt::AlignHCenter, caption_);
  }

private:
  Kind kind_;
  QString title_;
  QString caption_;
  bool checked_ = false;
  Palette pal_ = paletteFor(Appearance::Dark);
};

class MiniHoneycomb : public QWidget {
public:
  explicit MiniHoneycomb(QWidget *parent = nullptr) : QWidget(parent) {
    setFixedHeight(118);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  }

  void setMetrics(qreal cellSize, qreal gap) {
    cellSize_ = cellSize;
    gap_ = gap;
    update();
  }
  void setPaletteColors(const Palette &pal) {
    pal_ = pal;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal cell = cellSize_;
    const qreal pitchX = cell + gap_;
    const qreal cellH = cell * 1.15;
    const qreal pitchY = cell * 0.9625;
    const qreal gridW = 3 * pitchX + cell;
    const qreal gridH = 2 * pitchY + cellH;
    const qreal s =
        qMin((width() - 40.0) / gridW, (height() - 24.0) / gridH);
    const qreal ox = (width() - gridW * s) / 2.0;
    const qreal oy = (height() - gridH * s) / 2.0;

    QColor cellFill = pal_.text;
    cellFill.setAlpha(16);
    QColor selectedFill = pal_.amber;
    selectedFill.setAlpha(56);

    int index = 0;
    for (int r = 0; r < 3; ++r) {
      const int n = r % 2 == 0 ? 3 : 4;
      const qreal rowW = (n - 1) * pitchX + cell;
      const qreal x0 = ox + (gridW - rowW) * s / 2.0;
      for (int c = 0; c < n; ++c) {
        const QPainterPath hex =
            hexPath(x0 + c * pitchX * s, oy + r * pitchY * s, cell * s,
                    cellH * s);
        const bool selected = index == 4;
        p.setPen(QPen(selected ? pal_.amber : pal_.cardBorder, 1));
        p.setBrush(selected ? selectedFill : cellFill);
        p.drawPath(hex);
        ++index;
      }
    }
  }

private:
  qreal cellSize_ = 80;
  qreal gap_ = 8;
  Palette pal_ = paletteFor(Appearance::Dark);
};

// Hotkey recorder rendering the shortcut as macOS keycap chips (⌃ Space);
// click it and press a combo to record. QKeySequenceEdit's native look does
// not fit the design language.
class HotkeyField : public QFrame {
public:
  explicit HotkeyField(QWidget *parent = nullptr) : QFrame(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(28);
    setMinimumWidth(160);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  }

  void setPaletteColors(const Palette &pal) {
    pal_ = pal;
    update();
  }
  void setPrompts(const QString &recording, const QString &empty) {
    recordingText_ = recording;
    emptyText_ = empty;
    update();
  }
  void setSequence(const QKeySequence &seq) {
    seq_ = seq;
    update();
  }

  std::function<void(const QKeySequence &)> onChanged;

protected:
  void mousePressEvent(QMouseEvent *) override {
    setFocus();
    setRecording(true);
  }

  void focusOutEvent(QFocusEvent *) override { setRecording(false); }

  void keyPressEvent(QKeyEvent *event) override {
    if (!recording_) {
      const int key = event->key();
      if (key == Qt::Key_Return || key == Qt::Key_Enter ||
          key == Qt::Key_Space)
        setRecording(true);
      else
        QFrame::keyPressEvent(event);
      return;
    }
    const int key = event->key();
    switch (key) {
    case Qt::Key_Escape:
      setRecording(false);
      return;
    case Qt::Key_Backspace:
    case Qt::Key_Delete:
      finish(QKeySequence());
      return;
    case Qt::Key_Control:
    case Qt::Key_Shift:
    case Qt::Key_Alt:
    case Qt::Key_Meta:
      return; // modifier-only press — wait for the real key
    default:
      break;
    }
    int combined = key;
    const Qt::KeyboardModifiers mods = event->modifiers();
    if (mods & Qt::ControlModifier)
      combined |= Qt::CTRL;
    if (mods & Qt::AltModifier)
      combined |= Qt::ALT;
    if (mods & Qt::ShiftModifier)
      combined |= Qt::SHIFT;
    if (mods & Qt::MetaModifier)
      combined |= Qt::META;
    finish(QKeySequence(combined));
  }

  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen((recording_ || hasFocus()) ? pal_.amber : pal_.cardBorder,
                  recording_ ? 1.5 : 1.0));
    p.setBrush(pal_.controlBg);
    p.drawRoundedRect(r, 6, 6);

    QFont f = font();
    f.setPixelSize(11);
    p.setFont(f);
    if (recording_) {
      p.setPen(pal_.accentText);
      p.drawText(rect(), Qt::AlignCenter, recordingText_);
      return;
    }
    if (seq_.isEmpty()) {
      p.setPen(pal_.faint);
      p.drawText(rect(), Qt::AlignCenter, emptyText_);
      return;
    }

    const QStringList chips = chipLabels();
    const QFontMetricsF fm(f);
    const qreal chipH = 18, pad = 12, gap = 4;
    qreal total = gap * (chips.size() - 1);
    QVector<qreal> widths;
    widths.reserve(chips.size());
    for (const QString &chip : chips) {
      const qreal w = qMax(fm.horizontalAdvance(chip) + pad, 22.0);
      widths.append(w);
      total += w;
    }
    qreal x = (width() - total) / 2.0;
    const qreal y = (height() - chipH) / 2.0;
    for (int i = 0; i < chips.size(); ++i) {
      const QRectF cr(x, y, widths[i], chipH);
      p.setPen(QPen(pal_.cardBorder, 1));
      p.setBrush(pal_.cardBg);
      p.drawRoundedRect(cr, 4, 4);
      p.setPen(pal_.text);
      p.drawText(cr, Qt::AlignCenter, chips[i]);
      x += widths[i] + gap;
    }
  }

private:
  void setRecording(bool on) {
    if (recording_ == on)
      return;
    recording_ = on;
    update();
  }
  void finish(const QKeySequence &seq) {
    seq_ = seq;
    setRecording(false);
    if (onChanged)
      onChanged(seq_);
  }
  QStringList chipLabels() const {
    QStringList chips;
    if (seq_.isEmpty())
      return chips;
    const int combined = seq_[0].toCombined();
    if (combined & Qt::CTRL)
      chips << QStringLiteral("⌃");
    if (combined & Qt::ALT)
      chips << QStringLiteral("⌥");
    if (combined & Qt::SHIFT)
      chips << QStringLiteral("⇧");
    if (combined & Qt::META)
      chips << QStringLiteral("⌘");
    const int base = combined & ~int(Qt::KeyboardModifierMask);
    chips << QKeySequence(base).toString(QKeySequence::NativeText);
    return chips;
  }

  QKeySequence seq_;
  QString recordingText_;
  QString emptyText_;
  bool recording_ = false;
  Palette pal_ = paletteFor(Appearance::Dark);
};

class DragHandle : public QWidget {
public:
  explicit DragHandle(QWidget *parent = nullptr) : QWidget(parent) {
    setFixedSize(6, 12);
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(128, 128, 128, 90));
    for (int row = 0; row < 3; ++row)
      for (int col = 0; col < 2; ++col)
        p.drawEllipse(QPointF(1.5 + col * 3, 1.5 + row * 4.5), 1.1, 1.1);
  }
};

class PinRowWidget : public QFrame {
public:
  PinRowWidget(const Pin &pin, const QIcon &icon, const QString &removeTooltip,
               QWidget *parent = nullptr)
      : QFrame(parent) {
    setObjectName(QStringLiteral("pinRow"));
    setAttribute(Qt::WA_StyledBackground, true);
    // The row covers the whole item, so it must let mouse events fall through
    // to the viewport or InternalMove drag-reordering never starts. The remove
    // button stays interactive.
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(8, 0, 8, 0);
    lay->setSpacing(10);
    auto *handle = new DragHandle(this);
    handle->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    lay->addWidget(handle);

    auto *iconLabel = new QLabel(this);
    iconLabel->setFixedSize(28, 28);
    iconLabel->setPixmap(icon.pixmap(QSize(28, 28), devicePixelRatioF()));
    iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    lay->addWidget(iconLabel);

    auto *name = new QLabel(pin.label, this);
    name->setObjectName(QStringLiteral("pinName"));
    name->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    lay->addWidget(name);
    lay->addStretch();

    auto *remove = new QToolButton(this);
    remove->setObjectName(QStringLiteral("removeBtn"));
    remove->setText(QStringLiteral("×"));
    remove->setFixedSize(24, 24);
    remove->setCursor(Qt::PointingHandCursor);
    remove->setToolTip(removeTooltip);
    lay->addWidget(remove);
    QObject::connect(remove, &QToolButton::clicked, this,
                     [this] { onRemove(); });

    setFixedHeight(46);
  }

  std::function<void()> onRemove = [] {};
};

// QListWidget with an actually visible drop indicator (accent pill + dot)
// and a floating row thumbnail while dragging — the default 1px line is
// easy to miss, and item widgets never render into Qt's drag pixmap.
class PinListWidget : public QListWidget {
public:
  explicit PinListWidget(QWidget *parent = nullptr) : QListWidget(parent) {}

  void setIndicatorColor(const QColor &color) {
    indicatorColor_ = color;
    viewport()->update();
  }

protected:
  void mousePressEvent(QMouseEvent *event) override {
    pressPos_ = event->pos();
    QListWidget::mousePressEvent(event);
  }

  void startDrag(Qt::DropActions supportedActions) override {
    QListWidgetItem *item = currentItem();
    QWidget *row = item ? itemWidget(item) : nullptr;
    if (!row) {
      QListWidget::startDrag(supportedActions);
      return;
    }
    auto *drag = new QDrag(this);
    drag->setMimeData(model()->mimeData(selectedIndexes()));
    drag->setPixmap(row->grab());
    drag->setHotSpot(pressPos_ - visualRect(indexFromItem(item)).topLeft());
    // InternalMove: the drop event performs the row move itself, so there is
    // no base-class bookkeeping to preserve here.
    drag->exec(Qt::MoveAction, Qt::MoveAction);
  }

  void dragMoveEvent(QDragMoveEvent *event) override {
    QListWidget::dragMoveEvent(event);
    dropPos_ = event->position().toPoint();
    viewport()->update();
  }

  void dragLeaveEvent(QDragLeaveEvent *event) override {
    QListWidget::dragLeaveEvent(event);
    dropPos_ = QPoint();
    viewport()->update();
  }

  void dropEvent(QDropEvent *event) override {
    QListWidget::dropEvent(event);
    dropPos_ = QPoint();
    viewport()->update();
  }

  void paintEvent(QPaintEvent *event) override {
    QListWidget::paintEvent(event);
    if (state() != QAbstractItemView::DraggingState || dropPos_.isNull())
      return;
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing);
    const QModelIndex idx = indexAt(dropPos_);
    if (idx.isValid() &&
        dropIndicatorPosition() == QAbstractItemView::OnItem) {
      QColor tint = indicatorColor_;
      tint.setAlpha(28);
      p.setPen(QPen(indicatorColor_, 2));
      p.setBrush(tint);
      p.drawRoundedRect(QRectF(visualRect(idx)).adjusted(2, 2, -2, -2), 8, 8);
      return;
    }
    qreal y = dropPos_.y();
    if (idx.isValid()) {
      const QRect r = visualRect(idx);
      y = dropIndicatorPosition() == QAbstractItemView::BelowItem
              ? r.bottom() + 1.0
              : r.top() - 1.0;
    } else if (count() > 0) {
      y = visualRect(model()->index(count() - 1, 0)).bottom() + 1.0;
    }
    p.setPen(Qt::NoPen);
    p.setBrush(indicatorColor_);
    p.drawEllipse(QPointF(10, y), 4, 4);
    p.drawRoundedRect(QRectF(18, y - 2, viewport()->width() - 26, 4), 2, 2);
  }

private:
  QPoint pressPos_;
  QPoint dropPos_;
  QColor indicatorColor_ = QColor("#EEC86D");
};

// Plain pane widgets overflow the window when a translation runs long —
// let them scroll instead of clipping at the pane edge.
QScrollArea *wrapInScrollArea(QWidget *content) {
  auto *area = new QScrollArea;
  area->setWidget(content);
  area->setWidgetResizable(true);
  area->setFrameShape(QFrame::NoFrame);
  area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  area->setAutoFillBackground(false);
  area->viewport()->setAutoFillBackground(false);
  // setWidget() force-enables autoFillBackground on the content (it would
  // paint the default pale Window color over our themed background).
  content->setAutoFillBackground(false);
  return area;
}

} // namespace
PrefsWindow::PrefsWindow(Prefs *prefs, PinStore *pins,
                         const IconProvider *icons, QWidget *parent)
    : QWidget(parent, Qt::Window), prefs_(prefs), pins_(pins), icons_(icons) {
  setWindowTitle(tr("Locus Settings"));
  setObjectName(QStringLiteral("prefsRoot"));
  setAttribute(Qt::WA_StyledBackground, true);
  resize(680, 620);
  setMinimumSize(620, 480);

  auto *root = new QHBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  auto *sidebar = new QFrame(this);
  sidebar->setObjectName(QStringLiteral("sidebar"));
  sidebar->setFixedWidth(208);
  auto *sbLay = new QVBoxLayout(sidebar);
  sbLay->setContentsMargins(12, 14, 12, 12);
  sbLay->setSpacing(4);
  sbLay->addSpacing(20); // keep clear of the native traffic lights

  navGroup_ = new QButtonGroup(this);
  navGroup_->setExclusive(true);
  const QStringList navNames = {tr("General"), tr("Pins"), tr("Density")};
  for (int i = 0; i < navNames.size(); ++i) {
    auto *btn = new QToolButton(sidebar);
    btn->setObjectName(QStringLiteral("navBtn"));
    btn->setText(navNames[i]);
    btn->setCheckable(true);
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setIconSize(QSize(16, 16));
    btn->setFixedHeight(34);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    navGroup_->addButton(btn, i);
    sbLay->addWidget(btn);
  }
  sbLay->addStretch();
  root->addWidget(sidebar);

  stack_ = new QStackedWidget(this);
  stack_->addWidget(wrapInScrollArea(buildGeneralPane()));
  stack_->addWidget(wrapInScrollArea(buildPinsPane()));
  stack_->addWidget(wrapInScrollArea(buildDensityPane()));
  root->addWidget(stack_, 1);

  connect(navGroup_, &QButtonGroup::idClicked, this,
          [this](int id) { stack_->setCurrentIndex(id); });
  navGroup_->button(0)->setChecked(true);

  setResolvedAppearance(Appearance::Dark);
  refreshFromModel();
}

void PrefsWindow::setResolvedAppearance(Appearance appearance) {
  appearance_ = appearance;
  applyPalette();
}

void PrefsWindow::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  macStyleSettingsWindow(this);
  refreshFromModel();
}

void PrefsWindow::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    close();
    return;
  }
  QWidget::keyPressEvent(event);
}

void PrefsWindow::applyPalette() {
  const Palette p = paletteFor(appearance_);
  const QString grad = QStringLiteral("qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                                      " stop:0 %1, stop:1 %2)")
                           .arg(css(p.amberBright), css(p.amberDeep));
  setStyleSheet(QStringLiteral(R"(
    #prefsRoot { background: %14; }
    QMainWindow, QDialog { background: transparent; }
    #sidebar { background: %1; border-right: 1px solid %2; }
    #navBtn { color: %3; border: none; border-radius: 8px; padding: 0 10px;
              font-size: 13px; text-align: left; }
    #navBtn:hover { background: %4; }
    #navBtn:checked { background: %5; color: %6; font-weight: 600; }
    QLabel { color: %3; font-size: 13px; background: transparent; }
    QLabel#paneTitle { font-size: 20px; font-weight: 600; }
    QLabel#caption { color: %7; font-size: 11px; }
    QLabel#valueLabel { color: %7; font-size: 12px; }
    QLabel#countChip { background: %4; color: %7; border-radius: 9px;
                       padding: 2px 8px; font-size: 11px; font-weight: 600; }
    QFrame#card { background: %8; border: 1px solid %9; border-radius: 12px; }
    QFrame#seg { background: %10; border: 1px solid %9; border-radius: 9px; }
    QToolButton#segBtn { color: %3; border: none; border-radius: 7px;
                         padding: 4px 12px; font-size: 12px; }
    QToolButton#segBtn:checked { background: %11; color: %12; font-weight: 600; }
    QPushButton#amberBtn { background: %11; color: %12; border: none;
                           border-radius: 8px; padding: 6px 12px;
                           font-size: 12px; font-weight: 600; }
    QPushButton#amberBtn:pressed { background: %13; }
    QSlider::groove:horizontal { height: 4px; background: %10;
                                 border-radius: 2px; }
    QSlider::sub-page:horizontal { background: %11; border-radius: 2px; }
    QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0;
                                 border-radius: 7px; background: white; }
    QListWidget { background: transparent; border: none; outline: none; }
    QListWidget::item { border: none; padding: 0; }
    QListWidget::item:selected { background: transparent; }
    QFrame#pinRow { border-radius: 8px; }
    QListWidget::item:hover { background: %4; border-radius: 8px; }
    QToolButton#removeBtn { color: %7; background: %4; border: none;
                            border-radius: 6px; font-size: 13px;
                            padding-bottom: 2px; }
    QToolButton#removeBtn:hover { color: %3; background: %9; }
    QToolTip { background: %8; color: %3; border: 1px solid %9; }
  )")
                    .arg(css(p.sidebarBg))          // 1
                    .arg(css(p.separator))          // 2
                    .arg(css(p.text))               // 3
                    .arg(css(p.cardBg))             // 4
                    .arg(css(p.amberTint))          // 5
                    .arg(css(p.accentText))         // 6
                    .arg(css(p.faint))              // 7
                    .arg(css(p.cardBg))             // 8
                    .arg(css(p.cardBorder))         // 9
                    .arg(css(p.controlBg))          // 10
                    .arg(grad)                      // 11
                    .arg(css(p.onAmber))            // 12
                    .arg(css(p.amberDeep))          // 13
                    .arg(css(appearance_ == Appearance::Dark
                                 ? QColor("#131017")
                                 : QColor("#F5F5F7")))); // 14

  // Window background itself (QWidget base color).
  QPalette wp = palette();
  wp.setColor(QPalette::Window, appearance_ == Appearance::Dark
                                    ? QColor("#131017")
                                    : QColor("#F5F5F7"));
  setPalette(wp);
  setAutoFillBackground(true);

  const auto navButtons = navGroup_->buttons();
  for (int i = 0; i < navButtons.size(); ++i)
    static_cast<QToolButton *>(navButtons[i])->setIcon(navIcon(i, p));

  static_cast<Toggle *>(loginToggle_)->setColors(p);
  static_cast<StyleTile *>(styleTileHex_)->setPaletteColors(p);
  static_cast<StyleTile *>(styleTileOrbit_)->setPaletteColors(p);
  static_cast<MiniHoneycomb *>(densityPreview_)->setPaletteColors(p);
  static_cast<PinListWidget *>(pinList_)->setIndicatorColor(p.amber);
  static_cast<HotkeyField *>(hotkeyField_)->setPaletteColors(p);
}

void PrefsWindow::retranslateUi() {
  setWindowTitle(tr("Locus Settings"));

  const QStringList navNames = {tr("General"), tr("Pins"), tr("Density")};
  const auto navButtons = navGroup_->buttons();
  for (int i = 0; i < navButtons.size() && i < navNames.size(); ++i)
    navButtons[i]->setText(navNames[i]);

  // General
  generalTitle_->setText(tr("General"));
  themeLabel_->setText(tr("Theme"));
  const QStringList themes = {tr("Dark"), tr("Light"), tr("System")};
  const auto themeButtons = themeGroup_->buttons();
  for (int i = 0; i < themeButtons.size() && i < themes.size(); ++i)
    themeButtons[i]->setText(themes[i]);
  languageLabel_->setText(tr("Language"));
  const QStringList languages = {tr("System"), tr("English"), tr("简体中文")};
  const auto langButtons = languageGroup_->buttons();
  for (int i = 0; i < langButtons.size() && i < languages.size(); ++i)
    langButtons[i]->setText(languages[i]);
  styleLabel_->setText(tr("Menu style"));
  static_cast<StyleTile *>(styleTileHex_)->setTitle(tr("Honeycomb"));
  static_cast<StyleTile *>(styleTileOrbit_)->setTitle(tr("Orbit"));
  hotkeyLabel_->setText(tr("Global hotkey"));
  static_cast<HotkeyField *>(hotkeyField_)
      ->setPrompts(tr("Press shortcut…"), tr("Not set"));
  hotkeyCaption_->setText(tr("Summons the launcher from any app, even "
                             "while Locus is in the background."));
  loginLabel_->setText(tr("Launch at login"));
  loginToggle_->setAccessibleName(tr("Launch at login"));

  // Pins
  pinsTitle_->setText(tr("Pinned Apps"));
  addAppBtn_->setText(tr("Add App"));
  pinsNote_->setText(
      tr("Drag to reorder — the widget reflows the grid instantly."));

  // Density
  densityTitle_->setText(tr("Density"));
  cellSizeLabel_->setText(tr("Cell size"));
  cellGapLabel_->setText(tr("Cell spacing"));
  iconSizeLabel_->setText(tr("Icon size"));
  previewCaption_->setText(tr("Live preview · selected cell highlighted"));
  densityNote_->setText(
      tr("When apps overflow the grid, a new row opens automatically."));

  // Tile captions, value labels and pin-row tooltips come from the model.
  refreshFromModel();
}

QWidget *PrefsWindow::buildGeneralPane() {
  auto *pane = new QWidget(this);
  auto *lay = new QVBoxLayout(pane);
  lay->setContentsMargins(28, 24, 28, 24);
  lay->setSpacing(16);

  generalTitle_ = new QLabel(tr("General"), pane);
  generalTitle_->setObjectName(QStringLiteral("paneTitle"));
  lay->addWidget(generalTitle_);

  // Theme
  auto *themeCard = new QFrame(pane);
  themeCard->setObjectName(QStringLiteral("card"));
  auto *themeRow = new QHBoxLayout(themeCard);
  themeRow->setContentsMargins(16, 14, 16, 14);
  themeLabel_ = new QLabel(tr("Theme"), themeCard);
  themeRow->addWidget(themeLabel_);
  auto *seg = new QFrame(themeCard);
  seg->setObjectName(QStringLiteral("seg"));
  auto *segLay = new QHBoxLayout(seg);
  segLay->setContentsMargins(2, 2, 2, 2);
  segLay->setSpacing(2);
  themeGroup_ = new QButtonGroup(this);
  themeGroup_->setExclusive(true);
  const QStringList themes = {tr("Dark"), tr("Light"), tr("System")};
  for (int i = 0; i < themes.size(); ++i) {
    auto *btn = new QToolButton(seg);
    btn->setObjectName(QStringLiteral("segBtn"));
    btn->setText(themes[i]);
    btn->setCheckable(true);
    themeGroup_->addButton(btn, i);
    segLay->addWidget(btn);
  }
  themeRow->addWidget(seg);
  lay->addWidget(themeCard);
  connect(themeGroup_, &QButtonGroup::idClicked, this, [this](int id) {
    prefs_->setAppearance(static_cast<Appearance>(id));
    emit appearanceChanged();
  });

  // Language
  auto *langCard = new QFrame(pane);
  langCard->setObjectName(QStringLiteral("card"));
  auto *langRow = new QHBoxLayout(langCard);
  langRow->setContentsMargins(16, 14, 16, 14);
  languageLabel_ = new QLabel(tr("Language"), langCard);
  langRow->addWidget(languageLabel_);
  auto *langSeg = new QFrame(langCard);
  langSeg->setObjectName(QStringLiteral("seg"));
  auto *langSegLay = new QHBoxLayout(langSeg);
  langSegLay->setContentsMargins(2, 2, 2, 2);
  langSegLay->setSpacing(2);
  languageGroup_ = new QButtonGroup(this);
  languageGroup_->setExclusive(true);
  const QStringList languages = {tr("System"), tr("English"),
                                 tr("简体中文")};
  for (int i = 0; i < languages.size(); ++i) {
    auto *btn = new QToolButton(langSeg);
    btn->setObjectName(QStringLiteral("segBtn"));
    btn->setText(languages[i]);
    btn->setCheckable(true);
    languageGroup_->addButton(btn, i);
    langSegLay->addWidget(btn);
  }
  langRow->addWidget(langSeg);
  lay->addWidget(langCard);
  connect(languageGroup_, &QButtonGroup::idClicked, this, [this](int id) {
    if (id == prefs_->language())
      return;
    prefs_->setLanguage(id);
    emit languageChanged();
  });

  // Menu style
  auto *styleCard = new QFrame(pane);
  styleCard->setObjectName(QStringLiteral("card"));
  auto *styleLay = new QVBoxLayout(styleCard);
  styleLay->setContentsMargins(16, 14, 16, 14);
  styleLay->setSpacing(12);
  styleLabel_ = new QLabel(tr("Menu style"), styleCard);
  styleLay->addWidget(styleLabel_);
  auto *tiles = new QHBoxLayout;
  tiles->setSpacing(10);
  auto *hexTile = new StyleTile(StyleTile::Honeycomb, tr("Honeycomb"),
                                styleCard);
  auto *orbitTile = new StyleTile(StyleTile::Orbit, tr("Orbit"), styleCard);
  styleTileHex_ = hexTile;
  styleTileOrbit_ = orbitTile;
  hexTile->onClicked = [this] {
    if (prefs_->styleId() == StyleId::Cellular)
      return;
    prefs_->setStyleId(StyleId::Cellular);
    refreshFromModel();
    emit styleChanged();
  };
  orbitTile->onClicked = [this] {
    if (prefs_->styleId() == StyleId::Orbital)
      return;
    prefs_->setStyleId(StyleId::Orbital);
    refreshFromModel();
    emit styleChanged();
  };
  tiles->addWidget(hexTile);
  tiles->addWidget(orbitTile);
  styleLay->addLayout(tiles);
  lay->addWidget(styleCard);

  // Hotkey
  auto *hotkeyCard = new QFrame(pane);
  hotkeyCard->setObjectName(QStringLiteral("card"));
  auto *hotkeyLay = new QVBoxLayout(hotkeyCard);
  hotkeyLay->setContentsMargins(16, 14, 16, 14);
  hotkeyLay->setSpacing(10);
  auto *hotkeyRow = new QHBoxLayout;
  hotkeyLabel_ = new QLabel(tr("Global hotkey"), hotkeyCard);
  hotkeyRow->addWidget(hotkeyLabel_);
  auto *hotkeyField = new HotkeyField(hotkeyCard);
  hotkeyField->setPrompts(tr("Press shortcut…"), tr("Not set"));
  hotkeyField_ = hotkeyField;
  hotkeyRow->addWidget(hotkeyField, 0, Qt::AlignRight);
  hotkeyLay->addLayout(hotkeyRow);
  hotkeyCaption_ = new QLabel(tr("Summons the launcher from any app, even "
                                 "while Locus is in the background."),
                              hotkeyCard);
  hotkeyCaption_->setObjectName(QStringLiteral("caption"));
  hotkeyCaption_->setWordWrap(true);
  hotkeyLay->addWidget(hotkeyCaption_);
  lay->addWidget(hotkeyCard);
  hotkeyField->onChanged = [this](const QKeySequence &seq) {
    prefs_->setHotkey(seq);
    emit hotkeyChanged();
  };

  // Launch at login
  auto *loginCard = new QFrame(pane);
  loginCard->setObjectName(QStringLiteral("card"));
  auto *loginRow = new QHBoxLayout(loginCard);
  loginRow->setContentsMargins(16, 14, 16, 14);
  loginLabel_ = new QLabel(tr("Launch at login"), loginCard);
  loginRow->addWidget(loginLabel_);
  auto *toggle = new Toggle(loginCard);
  toggle->setAccessibleName(tr("Launch at login"));
  loginToggle_ = toggle;
  toggle->onToggled = [this, toggle](bool on) {
    if (!macSetLaunchAtLogin(on)) {
      // Registration failed (e.g. unsigned dev run) — revert the switch.
      toggle->setChecked(!on, true);
    }
  };
  loginRow->addWidget(toggle, 0, Qt::AlignRight);
  lay->addWidget(loginCard);

  lay->addStretch();
  return pane;
}

QWidget *PrefsWindow::buildPinsPane() {
  auto *pane = new QWidget(this);
  auto *lay = new QVBoxLayout(pane);
  lay->setContentsMargins(28, 24, 28, 24);
  lay->setSpacing(16);

  auto *header = new QHBoxLayout;
  pinsTitle_ = new QLabel(tr("Pinned Apps"), pane);
  pinsTitle_->setObjectName(QStringLiteral("paneTitle"));
  header->addWidget(pinsTitle_);
  pinCount_ = new QLabel(pane);
  pinCount_->setObjectName(QStringLiteral("countChip"));
  header->addWidget(pinCount_);
  header->addStretch();
  addAppBtn_ = new QPushButton(tr("Add App"), pane);
  addAppBtn_->setObjectName(QStringLiteral("amberBtn"));
  addAppBtn_->setCursor(Qt::PointingHandCursor);
  header->addWidget(addAppBtn_);
  lay->addLayout(header);
  connect(addAppBtn_, &QPushButton::clicked, this, &PrefsWindow::addApp);

  auto *card = new QFrame(pane);
  card->setObjectName(QStringLiteral("card"));
  auto *cardLay = new QVBoxLayout(card);
  cardLay->setContentsMargins(6, 6, 6, 6);
  cardLay->setSpacing(0);
  pinList_ = new PinListWidget(card);
  pinList_->setFrameShape(QFrame::NoFrame);
  pinList_->setSelectionMode(QAbstractItemView::SingleSelection);
  pinList_->setDragDropMode(QAbstractItemView::InternalMove);
  pinList_->setDefaultDropAction(Qt::MoveAction);
  pinList_->setSpacing(2);
  pinList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  // The scroll-area viewport auto-fills QPalette::Base (white) — kill it so
  // the glass card shows through.
  pinList_->setAutoFillBackground(false);
  pinList_->viewport()->setAutoFillBackground(false);
  pinList_->setAttribute(Qt::WA_MacShowFocusRect, false);
  cardLay->addWidget(pinList_);
  lay->addWidget(card, 1);
  connect(pinList_->model(), &QAbstractItemModel::rowsMoved, this,
          [this] { commitPinOrder(); });

  pinsNote_ = new QLabel(
      tr("Drag to reorder — the widget reflows the grid instantly."), pane);
  pinsNote_->setObjectName(QStringLiteral("caption"));
  lay->addWidget(pinsNote_);
  return pane;
}

QWidget *PrefsWindow::buildDensityPane() {
  auto *pane = new QWidget(this);
  auto *lay = new QVBoxLayout(pane);
  lay->setContentsMargins(28, 24, 28, 24);
  lay->setSpacing(16);

  densityTitle_ = new QLabel(tr("Density"), pane);
  densityTitle_->setObjectName(QStringLiteral("paneTitle"));
  lay->addWidget(densityTitle_);

  auto *card = new QFrame(pane);
  card->setObjectName(QStringLiteral("card"));
  auto *cardLay = new QVBoxLayout(card);
  cardLay->setContentsMargins(16, 16, 16, 18);
  cardLay->setSpacing(18);

  auto makeSetting = [&](const QString &label, int min, int max,
                         QLabel *&labelOut, QSlider *&sliderOut,
                         QLabel *&valueOut) {
    auto *box = new QVBoxLayout;
    box->setSpacing(8);
    auto *row = new QHBoxLayout;
    labelOut = new QLabel(label, card);
    row->addWidget(labelOut);
    valueOut = new QLabel(card);
    valueOut->setObjectName(QStringLiteral("valueLabel"));
    row->addWidget(valueOut, 0, Qt::AlignRight);
    box->addLayout(row);
    sliderOut = new QSlider(Qt::Horizontal, card);
    sliderOut->setRange(min, max);
    box->addWidget(sliderOut);
    cardLay->addLayout(box);
  };
  makeSetting(tr("Cell size"), 64, 96, cellSizeLabel_, cellSizeSlider_,
              cellSizeValue_);
  makeSetting(tr("Cell spacing"), 4, 16, cellGapLabel_, cellGapSlider_,
              cellGapValue_);
  makeSetting(tr("Icon size"), 28, 48, iconSizeLabel_, iconSizeSlider_,
              iconSizeValue_);
  lay->addWidget(card);

  auto onSlider = [this] {
    DensityPrefs d = prefs_->density();
    d.cellSize = cellSizeSlider_->value();
    d.cellGap = cellGapSlider_->value();
    d.iconSize = iconSizeSlider_->value();
    prefs_->setDensity(d);
    cellSizeValue_->setText(tr("%1 px").arg(cellSizeSlider_->value()));
    cellGapValue_->setText(tr("%1 px").arg(cellGapSlider_->value()));
    iconSizeValue_->setText(tr("%1 px").arg(iconSizeSlider_->value()));
    static_cast<MiniHoneycomb *>(densityPreview_)
        ->setMetrics(d.cellSize, d.cellGap);
    emit densityChanged();
  };
  connect(cellSizeSlider_, &QSlider::valueChanged, this, onSlider);
  connect(cellGapSlider_, &QSlider::valueChanged, this, onSlider);
  connect(iconSizeSlider_, &QSlider::valueChanged, this, onSlider);

  auto *previewCard = new QFrame(pane);
  previewCard->setObjectName(QStringLiteral("card"));
  auto *previewLay = new QVBoxLayout(previewCard);
  previewLay->setContentsMargins(16, 18, 16, 16);
  previewLay->setSpacing(12);
  auto *preview = new MiniHoneycomb(previewCard);
  densityPreview_ = preview;
  previewLay->addWidget(preview);
  previewCaption_ =
      new QLabel(tr("Live preview · selected cell highlighted"), previewCard);
  previewCaption_->setObjectName(QStringLiteral("caption"));
  previewCaption_->setAlignment(Qt::AlignCenter);
  previewLay->addWidget(previewCaption_);
  lay->addWidget(previewCard);

  densityNote_ = new QLabel(
      tr("When apps overflow the grid, a new row opens automatically."), pane);
  densityNote_->setObjectName(QStringLiteral("caption"));
  lay->addWidget(densityNote_);
  lay->addStretch();
  return pane;
}

void PrefsWindow::refreshFromModel() {
  // General
  const int themeId = static_cast<int>(prefs_->appearance());
  if (auto *btn = themeGroup_->button(themeId))
    btn->setChecked(true);
  if (auto *btn = languageGroup_->button(prefs_->language()))
    btn->setChecked(true);
  const bool cellular = prefs_->styleId() == StyleId::Cellular;
  auto *hexTile = static_cast<StyleTile *>(styleTileHex_);
  auto *orbitTile = static_cast<StyleTile *>(styleTileOrbit_);
  hexTile->setCheckedState(cellular);
  orbitTile->setCheckedState(!cellular);
  hexTile->setCaption(cellular ? tr("Current") : tr("Grid"));
  orbitTile->setCaption(cellular ? tr("Ring") : tr("Current"));
  static_cast<HotkeyField *>(hotkeyField_)->setSequence(prefs_->hotkey());
  static_cast<Toggle *>(loginToggle_)
      ->setChecked(macLaunchAtLoginEnabled(), false);

  // Density
  const DensityPrefs d = prefs_->density();
  for (auto *slider : {cellSizeSlider_, cellGapSlider_, iconSizeSlider_})
    slider->blockSignals(true);
  cellSizeSlider_->setValue(int(d.cellSize));
  cellGapSlider_->setValue(int(d.cellGap));
  iconSizeSlider_->setValue(int(d.iconSize));
  for (auto *slider : {cellSizeSlider_, cellGapSlider_, iconSizeSlider_})
    slider->blockSignals(false);
  cellSizeValue_->setText(tr("%1 px").arg(int(d.cellSize)));
  cellGapValue_->setText(tr("%1 px").arg(int(d.cellGap)));
  iconSizeValue_->setText(tr("%1 px").arg(int(d.iconSize)));
  static_cast<MiniHoneycomb *>(densityPreview_)
      ->setMetrics(d.cellSize, d.cellGap);

  // Pins
  reloadPinRows();
}

void PrefsWindow::selectPane(int index) {
  if (auto *btn = navGroup_->button(index)) {
    btn->setChecked(true);
    stack_->setCurrentIndex(index);
  }
}

void PrefsWindow::reloadPinRows() {
  reloadingPins_ = true;
  pinList_->clear();
  for (const auto &pin : pins_->pins()) {
    auto *item = new QListWidgetItem;
    item->setData(Qt::UserRole, pin.id);
    item->setSizeHint(QSize(100, 46));
    pinList_->addItem(item);
    auto *row = new PinRowWidget(pin, icons_->iconForPath(pin.appPath),
                                 tr("Remove from Locus"), pinList_);
    const QString id = pin.id;
    row->onRemove = [this, id] {
      pins_->removePin(id);
      reloadPinRows();
      emit pinsChanged();
    };
    pinList_->setItemWidget(item, row);
  }
  pinCount_->setText(QString::number(pins_->pins().size()));
  reloadingPins_ = false;
}

void PrefsWindow::commitPinOrder() {
  if (reloadingPins_)
    return;
  QVector<Pin> reordered;
  reordered.reserve(pinList_->count());
  for (int i = 0; i < pinList_->count(); ++i) {
    const QString id = pinList_->item(i)->data(Qt::UserRole).toString();
    for (const auto &pin : pins_->pins()) {
      if (pin.id == id) {
        reordered.push_back(pin);
        break;
      }
    }
  }
  if (reordered.size() == pins_->pins().size()) {
    pins_->setPins(reordered);
    emit pinsChanged();
  }
  // Rebuilding synchronously would clear the list inside the model's
  // rowsMoved notification — defer it.
  QTimer::singleShot(0, this, [this] { reloadPinRows(); });
}

void PrefsWindow::addApp() {
#if defined(Q_OS_MAC)
  const QString startDir = QStringLiteral("/Applications");
  const QString filter = tr("Applications (*.app)");
#elif defined(Q_OS_WIN)
  const QString startDir = QStringLiteral("C:/Program Files");
  const QString filter = tr("Programs (*.exe)");
#else
  const QString startDir = QDir::homePath();
  const QString filter = tr("All files (*)");
#endif
  const QString path =
      QFileDialog::getOpenFileName(this, tr("Add App"), startDir, filter);
  if (path.isEmpty())
    return;
  // The native dialog returns bundle paths with a trailing slash, and
  // QFileInfo::completeBaseName() on those is empty — clean first.
  const QString clean = QDir::cleanPath(path);
  Pin pin;
  pin.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  pin.appPath = clean;
  pin.label = QFileInfo(clean).completeBaseName();
  pin.iconKey = clean;
  pins_->addPin(pin);
  reloadPinRows();
  emit pinsChanged();
}

} // namespace locus
