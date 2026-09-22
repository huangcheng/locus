#include "platform/MacOverlay.h"

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

#include <QCoreApplication>
#include <QMouseEvent>
#include <QRectF>
#include <QWidget>

// The crystal backdrop is purely decorative — it must never appear in the
// event chain. A plain NSVisualEffectView sibling already swallowed mouse
// delivery to Qt's view (hover/comet went dead), so both backdrop kinds
// override hitTest: to nil. NSGlassEffectView may not exist at compile time,
// so its passthrough subclass is built at runtime.
@interface LocusPassthroughVibrancyView : NSVisualEffectView
@end
@implementation LocusPassthroughVibrancyView
- (NSView *)hitTest:(NSPoint)point {
  return nil;
}
@end

static NSView *locusPassthroughHitTest(id self, SEL _cmd, NSPoint point) {
  return nil;
}

static Class locusPassthroughGlassClass() {
  static Class cls = nil;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    Class glass = NSClassFromString(@"NSGlassEffectView");
    if (!glass)
      return;
    cls = objc_allocateClassPair(glass, "LocusPassthroughGlassView", 0);
    if (cls) {
      Method hitTest = class_getInstanceMethod([NSView class],
                                               @selector(hitTest:));
      class_addMethod(cls, @selector(hitTest:),
                      (IMP)locusPassthroughHitTest,
                      method_getTypeEncoding(hitTest));
      objc_registerClassPair(cls);
    }
  });
  return cls;
}

namespace locus {

void macInstallCrystalBackdrop(QWidget *overlay, const QRectF &discRect,
                               bool dark) {
  if (!overlay)
    return;
  NSView *view = (__bridge NSView *)(void *)overlay->winId();
  NSWindow *win = [view window];
  if (!win)
    return;

  // The glass lives in its OWN top-level borderless window, one level below
  // the overlay, with ignoresMouseEvents = YES. Earlier attempts: a subview
  // composites above Qt's painting (swallows the UI); a frame-view sibling
  // and a child window both perturbed the overlay window's layout/level so
  // the window server stopped routing mouse events to it. A detached
  // companion window touches nothing of the overlay — and can never receive
  // an event itself.
  static NSWindow *glassWin = nil;
  static NSView *backdrop = nil;
  if (!discRect.isValid()) { // style change or overlay hiding: drop the glass
    if (glassWin) {
      [glassWin orderOut:nil];
      glassWin = nil;
      backdrop = nil;
    }
    return;
  }

  // Compute the glass frame from Qt's own global geometry, never from the
  // NSWindow frame: right after showAt the native window still reports its
  // stale position (move/show apply asynchronously), which detached the
  // glass from the widget on the first summon. AppKit global coordinates
  // flip y against the primary (menu-bar) screen's height.
  const QPoint overlayTopLeft = overlay->mapToGlobal(QPoint(0, 0));
  NSScreen *primary = [NSScreen screens].firstObject;
  const CGFloat primaryH = primary ? primary.frame.size.height : 0.0;
  const qreal gx = overlayTopLeft.x() + discRect.x();
  const qreal gy = overlayTopLeft.y() + discRect.y();
  // The window is one margin larger than the disc on every side: Liquid
  // Glass draws its edge refraction slightly OUTSIDE the view's bounds, and
  // a flush window clipped it ("edges have been cut"). The window is clear
  // and click-through, so the extra frame is invisible.
  const CGFloat kMargin = 28.0;
  const NSRect onScreen =
      NSMakeRect(gx - kMargin, primaryH - (gy + discRect.height()) - kMargin,
                 discRect.width() + 2 * kMargin,
                 discRect.height() + 2 * kMargin);
  const NSRect glassRect = NSMakeRect(kMargin, kMargin, discRect.width(),
                                      discRect.height());

  // Real Liquid Glass where available (macOS 26+); vibrancy + mask below it.
  Class glassClass = locusPassthroughGlassClass();
  if (!glassWin) {
    glassWin = [[NSWindow alloc] initWithContentRect:onScreen
                                           styleMask:NSWindowStyleMaskBorderless
                                             backing:NSBackingStoreBuffered
                                               defer:NO];
    glassWin.opaque = NO;
    glassWin.backgroundColor = [NSColor clearColor];
    glassWin.hasShadow = NO;
    glassWin.ignoresMouseEvents = YES;
    glassWin.level = win.level - 1;
    if (glassClass) {
      backdrop = [[glassClass alloc] initWithFrame:glassRect];
    } else {
      NSVisualEffectView *blur =
          [[LocusPassthroughVibrancyView alloc] initWithFrame:glassRect];
      blur.blendingMode = NSVisualEffectBlendingModeBehindWindow;
      blur.state = NSVisualEffectStateActive;
      blur.material = NSVisualEffectMaterialPopover;
      backdrop = blur;
    }
    [glassWin.contentView addSubview:backdrop];
    [glassWin orderFront:nil];
  }
  glassWin.level = win.level - 1;
  [glassWin setFrame:onScreen display:YES];
  // Keep the glass pinned directly beneath the overlay in z-order.
  [glassWin orderWindow:NSWindowBelow relativeTo:[win windowNumber]];
  [backdrop setFrame:glassRect];
  if ([backdrop respondsToSelector:@selector(setCornerRadius:)]) {
    [(id)backdrop setCornerRadius:discRect.height() / 2.0];
  } else if ([backdrop isKindOfClass:[NSVisualEffectView class]]) {
    NSVisualEffectView *blur = (NSVisualEffectView *)backdrop;
    const CGFloat W = onScreen.size.width, H = onScreen.size.height;
    blur.maskImage = [NSImage imageWithSize:NSMakeSize(W, H)
                                    flipped:NO
                             drawingHandler:^BOOL(NSRect) {
                               [[NSColor colorWithWhite:0 alpha:1] set];
                               [[NSBezierPath
                                   bezierPathWithOvalInRect:glassRect] fill];
                               return YES;
                             }];
  }
  // The widget may force an appearance different from the system's.
  backdrop.appearance = [NSAppearance
      appearanceNamed:dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
}

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
