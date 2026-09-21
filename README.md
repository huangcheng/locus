# Navi (working title)

Cross-platform desktop launcher summoned at the cursor — Qt Widgets, hotkey + tray, pluggable UI skins (Cellular honeycomb by default; set `style=0` in QSettings for Orbital, Pie later).

Product name TBD.

## Build

```bash
cmake -S . -B build2 -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build2
ctest --test-dir build2 --output-on-failure
open build2/Navi.app
```

On macOS, run the **`.app` bundle** (not the raw binary) so the menu-bar tray icon can appear. Look for a small ring icon near the clock; right-click → **Show Navi**. Navi is a menu-bar agent (`LSUIElement`) — no Dock tile, no app menu bar.

Requires Qt 6.5+ (Widgets). First launch seeds pins from apps under `/Applications`. Esc dismisses the widget. Cells magnify toward the cursor, Dock-style. Navi is single-instance: launching it again just summons the running one. Global hotkey and Preferences UI are next.

If the menu-bar icon is still missing: check **Control Center → Menu Bar** (or System Settings → Control Center) and ensure icons aren’t overloaded; since Navi is an agent it never appears in the Dock — confirm it's alive with `pgrep -x Navi`.

Working title: **Navi** (final name TBD).

## Docs

- Spec: `docs/superpowers/specs/2026-09-20-navi-architecture-design.md`
- Plan: `docs/superpowers/plans/2026-09-20-navi-v1-implementation.md`
- Plan: `docs/superpowers/plans/2026-09-21-honeycomb-widget.md`
