#include "platforms/macos/MacIcon.h"

#import <AppKit/AppKit.h>

namespace locus {

QPixmap macIconForPath(const QString &path, int size) {
  @autoreleasepool {
    // Resolve symlinks/aliases first, otherwise NSWorkspace badges the icon
    // with the alias arrow (e.g. /Applications/Safari.app is a firmlink).
    NSString *resolved = path.toNSString();
    if (NSURL *real =
            [[NSURL fileURLWithPath:resolved] URLByResolvingSymlinksInPath])
      resolved = real.path;

    NSImage *img = [[NSWorkspace sharedWorkspace] iconForFile:resolved];
    if (!img)
      return {};
    [img setSize:NSMakeSize(size, size)];

    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
          initWithBitmapDataPlanes:nullptr
                        pixelsWide:size
                        pixelsHigh:size
                     bitsPerSample:8
                   samplesPerPixel:4
                          hasAlpha:YES
                          isPlanar:NO
                    colorSpaceName:NSDeviceRGBColorSpace
                      bitmapFormat:0
                       bytesPerRow:0
                      bitsPerPixel:0];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext
        setCurrentContext:[NSGraphicsContext
                              graphicsContextWithBitmapImageRep:rep]];
    [img drawInRect:NSMakeRect(0, 0, size, size)
           fromRect:NSZeroRect
          operation:NSCompositingOperationSourceOver
           fraction:1.0];
    [NSGraphicsContext restoreGraphicsState];

    NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG
                                    properties:@{}];
    [rep release];
    QPixmap pm;
    pm.loadFromData(static_cast<const uchar *>(png.bytes),
                    uint(png.length), "PNG");
    return pm;
  }
}

} // namespace locus
