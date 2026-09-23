#include "platforms/macos/MacOverlay.h"

#import <AppKit/AppKit.h>

#include <QCoreApplication>
#include <QMouseEvent>
#include <QWidget>

namespace locus {

void macMakeOverlayLiveWhenInactive(QWidget *overlay, QWidget *content) {
  if (!overlay || !content)
    return;

  NSView *view = (__bridge NSView *)(void *)overlay->winId();
  if (NSWindow *win = [view window]) {
    win.acceptsMouseMovedEvents = YES;
    win.hidesOnDeactivate = NO;
  }

  // While Locus is not the active app, AppKit delivers no mouse-moved events
  // to our window — hover selection and the animations would go dead.
  // A global monitor still sees them; forward the ones inside the overlay
  // as synthesized Qt mouse moves (and a Leave when the cursor exits).
  //
  // Exactly one monitor exists at a time and is owned by the current content
  // widget. A style switch calls this again with a NEW content while the old
  // one is only queued for deletion (OverlayWindow::setContent deleteLaters
  // it), so the previous monitor must be dropped here, up front — its block
  // captured the old content pointer.
  static id monitor = nil;
  static QWidget *monitoredContent = nullptr;
  static BOOL wasInside = NO;
  if (monitor) {
    [NSEvent removeMonitor:monitor];
    monitor = nil;
    monitoredContent = nullptr;
    wasInside = NO;
  }

  monitor = [NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskMouseMoved
                                             handler:^(NSEvent *event) {
    if (!overlay->isVisible()) {
      wasInside = NO;
      return;
    }
    const CGPoint p = CGEventGetLocation([event CGEvent]); // global, top-left origin
    const QPoint globalPt(int(p.x), int(p.y));
    const QPoint local = content->mapFromGlobal(globalPt);
    if (content->rect().contains(local)) {
      wasInside = YES;
      QMouseEvent move(QEvent::MouseMove, QPointF(local), QPointF(globalPt),
                       Qt::NoButton, Qt::NoButton, Qt::NoModifier);
      QCoreApplication::sendEvent(content, &move);
    } else if (wasInside) {
      wasInside = NO;
      QEvent leave(QEvent::Leave);
      QCoreApplication::sendEvent(content, &leave);
    }
  }];
  monitoredContent = content;

  // AppKit retains the monitor; drop it when THIS content goes away so the
  // block never dereferences a dead widget. The remove-up-front above keeps
  // `monitor` pointing at this content's monitor until then.
  QObject::connect(content, &QObject::destroyed, [content] {
    if (monitoredContent == content && monitor) {
      [NSEvent removeMonitor:monitor];
      monitor = nil;
      monitoredContent = nullptr;
      wasInside = NO;
    }
  });

}

} // namespace locus
