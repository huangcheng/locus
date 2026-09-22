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
  static BOOL wasInside = NO;
  static id monitor =
      [NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskMouseMoved
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

  // AppKit retains the monitor for the process lifetime; drop it if the
  // overlay is ever destroyed so the block never dereferences dead widgets.
  QObject::connect(content, &QObject::destroyed, [] {
    if (monitor) {
      [NSEvent removeMonitor:monitor];
      monitor = nil;
    }
  });
}

} // namespace locus
