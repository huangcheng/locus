# Locus phase 2: search, pin-anything, context menu, release notes

Scope decision (2026-09-26): type-to-search, pin-anything, pin context menu,
release notes in the update prompt. All fit existing seams; no new
dependencies.

## 1. Type-to-search in the overlay

Key insight: **filter pins before layout, not inside views**. The views stay
untouched.

```
key typed ──► OverlayWindow search bar (QLineEdit, hidden until typing)
              ──► queryChanged(QString)
main.cpp: rebuilds SceneModel from pins filtered by label.contains(query)
          ──► view->setScene(scene)   (existing path, all 3 styles work)
```

Amendment (implemented): the search strip is **always visible** (Raycast
style), not hidden-until-typing — simpler focus story, no key-stealing
filter. Views are `Qt::NoFocus`; the strip owns Esc/Return/typing.

- `OverlayWindow` gains a search strip: slim `QLineEdit` (frameless,
  glass-styled, placeholder "Type to filter…"), always visible.
- Focus flow: the strip has focus while the overlay is open. Esc clears the
  query (second Esc dismisses), Return activates the first visible pin
  (first item of the filtered scene — view-agnostic, not view-internal).
- Empty result: scene with zero items → views render an empty grid (no
  special copy in v1).
- Reopening the overlay resets the query (search is per-summon, like
  Spotlight).
- Tests: filter function is pure (`filterPins(pins, query)` in core) —
  case-insensitive, matches label substring, empty query = identity. Widget
  test: type "saf" → scene contains only Safari.

## 2. Pin anything (files, folders)

- `AppLauncher::launch` already uses `QDesktopServices::openUrl` — folders
  open in Explorer/Finder, files in their handler. Zero launcher changes.
- Drop filter `isDroppableAppPath` → `isDroppablePath`: accept any existing
  local file or directory (all platforms). Reject: URLs (web pins are a
  later feature), nonexistent paths.
- Add App dialog filter: `Programs (*.exe *.lnk);;All files (*)`.
- Icon: `QFileIconProvider` already covers files/folders.
- Label: `completeBaseName()`; folders use `fileName()`.
- Tests: `test_drop_add` — junk-drop case flips (a .txt IS now pinnable);
  new cases: folder accepted, `https://` URL rejected, nonexistent path
  rejected.

## 3. Pin context menu

- Views emit a new signal from `contextMenuEvent`:
  `itemContextMenuRequested(QString pinId, QPoint globalPos)` (per-view
  hit-test already exists: `hitTest(scene_, pos)`).
- main.cpp builds a `QMenu` on demand:
  - **Launch** — existing activation path
  - **Open file location** — Win: `explorer.exe /select,<path>`; mac:
    `open -R`; Linux: open parent dir
  - **Run as administrator** — Windows only, `.exe` only: ShellExecute
    "runas" (small `AppLauncher::launchElevated`)
  - **Unpin** — `pins.removePin(id)` + existing `rebuild()`; no Settings
    round-trip
- Rename deferred (needs an inline editor to not look native; separate
  design).
- Menu styled via the existing palette; dismissed by overlay hide (menu is
  a child of the view widget).
- Test: contextMenuEvent at a cell → signal fires with correct id (extend
  `test_scene_hittest` pattern); Unpin removes from PinStore.

## 4. Release notes in the update prompt

- Feed: `<description>` element in `appcast.xml.in` carrying plain-text
  notes.
- `UpdateInfo` gains `QString notes`; parser reads `<description>` text.
- Updates card: when state=Available and notes non-empty, status caption
  shows first ~3 lines (word-wrap, elided) — no new widget.
- CI ordering fix: `gh release create` (with `--generate-notes`) must run
  BEFORE feed generation; then `gh release view v$v --json body -q .body`
  → XML-escape → into `@NOTES@` → upload appcast.xml as a second step.
- Test: fixture gains `<description>`; parse asserts notes round-trip.

## Explicitly deferred

Pin rename (needs inline editor design), web-URL pins (needs favicon
strategy), usage-aware ordering (needs launch-count store), Ed25519 signing
(macOS prerequisite), overlay drop target.
