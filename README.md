# Locus

Cross-platform desktop launcher summoned at the cursor — Qt Widgets, global
hotkey + tray, pluggable UI skins (Cellular honeycomb by default; Orbital and
Fan in Settings).

By [HUANG Cheng](https://cheng.im) · MIT License (see `LICENSE`)

## Build

Requires Qt 6.5+ (Widgets, Network, Test, LinguistTools) and CMake 3.21+.

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"   # or your Qt path
cmake --build build
ctest --test-dir build --output-on-failure
open build/Locus.app        # macOS — run the .app bundle, not the raw binary
``+
On Linux, the executable is `build/locus`; run tests headless with
`QT_QPA_PLATFORM=offscreen ctest --test-dir build`.

## Use

On macOS, look for the small honeycomb icon near the clock: **left-click** it
to summon the widget, **right-click** for Preferences/Quit. Locus is a
menu-bar agent (`LSUIElement`) — no Dock tile, no app menu bar.

Launch starts hidden. Summon it with the tray click, the global hotkey
(default ⌃Space on macOS, Ctrl+Alt+Space on Windows — record your own in
Settings), or by launching Locus again (single-instance: the new process pings
the running one). Esc dismisses. First launch seeds a dozen common apps;
add, remove, and drag-reorder pins in Settings. Cells magnify toward the
cursor, Dock-style.

If the menu-bar icon is missing: check **System Settings → Control Center**
and ensure the menu bar isn’t overloaded; since Locus is an agent it never
appears in the Dock — confirm it's alive with `pgrep -x Locus`.

## Platform status

| Platform | Launcher | Tray | Global hotkey | Native glass backdrop |
| --- | --- | --- | --- | --- |
| macOS 13+ | ✅ | ✅ | ✅ Carbon | ✅ Vibrancy / Liquid Glass |
| Windows 10+ | ✅ | ✅ | ✅ RegisterHotKey | painted tint |
| Linux/X11 | ✅ | ✅ | stub (not yet) | painted tint |

## Docs

- Spec: `docs/superpowers/specs/2026-09-20-locus-architecture-design.md`
- Plan: `docs/superpowers/plans/2026-09-20-locus-v1-implementation.md`
- Plan: `docs/superpowers/plans/2026-09-21-honeycomb-widget.md`
