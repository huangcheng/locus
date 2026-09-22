#include "platforms/macos/MacLoginItem.h"

#import <ServiceManagement/ServiceManagement.h>

namespace locus {

bool macLaunchAtLoginEnabled() {
  if (@available(macOS 13.0, *))
    return SMAppService.mainAppService.status == SMAppServiceStatusEnabled;
  return false;
}

bool macSetLaunchAtLogin(bool enabled) {
  if (@available(macOS 13.0, *)) {
    SMAppService *service = SMAppService.mainAppService;
    NSError *error = nil;
    const BOOL ok = enabled ? [service registerAndReturnError:&error]
                            : [service unregisterAndReturnError:&error];
    return ok == YES;
  }
  return false;
}

} // namespace locus
