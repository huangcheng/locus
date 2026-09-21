#pragma once

namespace locus {

/// Launch at login via SMAppService (macOS 13+). Both return false on older
/// systems or when the app is not running from a bundle.
bool macLaunchAtLoginEnabled();
bool macSetLaunchAtLogin(bool enabled);

} // namespace locus
