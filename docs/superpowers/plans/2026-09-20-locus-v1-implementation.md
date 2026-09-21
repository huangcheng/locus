# Locus v1 (Orbital) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship a Qt Widgets hotkey/tray orbital launcher with Session + pluggable layout/view boundaries, Orbital strategy + liquid-glass view, pins/prefs, native icons and launch.

**Architecture:** Platform shell owns overlay/tray/hotkey/icons/launch. `SessionController` owns open/focus/activate with no geometry. `LayoutStrategy` builds `SceneModel`; `MenuView` paints and hit-tests. v1 implements `OrbitalLayoutStrategy` + `OrbitalGlassView` only; Cellular/Pie reserved as interfaces.

**Tech Stack:** C++20, Qt 6.11 (Widgets + Test), CMake 3.21+, Catch2 or Qt Test for unit tests, QSettings for persistence.

**Spec:** `docs/superpowers/specs/2026-09-20-locus-architecture-design.md`

## Global Constraints

- Product working title remains **Locus** until renamed; keep namespaces/`TARGET` as `locus`.
- Qt Widgets only — no Electron, no Qt Quick.
- Engine must not depend on a specific UI shape (orbital/cellular/pie).
- Icons stay upright in widget space (`PlacedItem.bounds` axis-aligned).
- Overlay is floating widget-sized (default 560×560), not fullscreen.
- Pins are manual only via prefs.
- v1 ships Orbital only; Cellular/Pie are interface stubs, not UIs.
- Persistence: **QSettings** (INI/native).
- Style switch: **factory** returns strategy+view pair by `styleId`.

---

## File structure

```
CMakeLists.txt
README.md
src/
  main.cpp
  core/
    Types.h                 # PinId, StyleId, Appearance
    Pin.h / Pin.cpp         # Pin record
    SceneModel.h            # PlacedItem, Decoration, SceneModel, HubInfo
    LayoutStrategy.h        # pure interface
    MenuView.h              # pure interface (QWidget-facing later)
    SessionController.h/.cpp
    PinStore.h/.cpp
    Prefs.h/.cpp
    StyleFactory.h/.cpp     # creates strategy+view for styleId
  layout/
    OrbitalLayoutStrategy.h/.cpp
    OrbitalState.h          # rotationRadians
  ui/
    OverlayWindow.h/.cpp    # frameless transparent host
    OrbitalGlassView.h/.cpp # paints SceneModel
    PrefsDialog.h/.cpp
  platform/
    IconProvider.h/.cpp     # QIcon from app path (macOS first)
    AppLauncher.h/.cpp
    HotkeyManager.h/.cpp    # QHotkey or native Carbon/Cocoa stub
    TrayController.h/.cpp
tests/
  test_session.cpp
  test_orbital_layout.cpp
  test_scene_hittest.cpp
```

---

### Task 1: CMake scaffold + core types

**Files:**
- Create: `CMakeLists.txt`, `README.md`, `src/core/Types.h`, `src/core/Pin.h`, `src/core/SceneModel.h`, `src/main.cpp` (minimal QApplication stub)
- Test: none yet (headers-only)

**Interfaces:**
- Produces: `locus::PinId` (`QString`), `locus::StyleId` enum `{ Orbital, Cellular, Pie }`, `locus::Appearance` `{ Dark, Light, System }`, `locus::Pin`, `locus::ItemRole`, `locus::PlacedItem`, `locus::Decoration`, `locus::HubInfo`, `locus::SceneModel`

- [ ] **Step 1: Create CMake project**

```cmake
cmake_minimum_required(VERSION 3.21)
project(locus VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Test)
add_library(locus_core STATIC
  src/core/Pin.cpp
  # more sources added in later tasks
)
target_include_directories(locus_core PUBLIC src)
target_link_libraries(locus_core PUBLIC Qt6::Widgets)
add_executable(locus src/main.cpp)
target_link_libraries(locus PRIVATE locus_core Qt6::Widgets)
enable_testing()
add_executable(locus_tests tests/test_session.cpp)
target_link_libraries(locus_tests PRIVATE locus_core Qt6::Test)
add_test(NAME locus_tests COMMAND locus_tests)
```

Start with `Pin.cpp` empty stub so the static lib links; expand sources per task.

- [ ] **Step 2: Define types**

`SceneModel.h` (excerpt):

```cpp
namespace locus {
enum class ItemRole { Item, Hub, Decoration };
enum class DecorationKind { Ellipse, Path };
struct PlacedItem {
  QString id;
  QRectF bounds;
  int z = 0;
  ItemRole role = ItemRole::Item;
  std::optional<qreal> angle;
};
struct Decoration {
  QString name;
  DecorationKind kind = DecorationKind::Ellipse;
  QRectF bounds;       // ellipse bounding rect
  QString styleKey;    // e.g. "orbital.inner", "hover.well"
};
struct HubInfo {
  QString selectedTitle;
  QString brandSubtitle = QStringLiteral("LOCUS");
};
struct SceneModel {
  QVector<PlacedItem> items;
  HubInfo hub;
  QVector<Decoration> decorations;
  QVector<QString> hitOrder; // front-to-back ids
};
}
```

- [ ] **Step 3: Minimal `main.cpp` that starts and quits**

```cpp
#include <QApplication>
int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("Locus"));
  QCoreApplication::setApplicationName(QStringLiteral("Locus"));
  return 0; // Task 7+ will exec()
}
```

- [ ] **Step 4: Configure and build**

Run:
```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
```
Expected: success

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt README.md src .gitignore docs
git commit -m "chore: scaffold Qt6 locus_core and SceneModel types"
```

---

### Task 2: SessionController (TDD)

**Files:**
- Create: `src/core/SessionController.h`, `src/core/SessionController.cpp`, `tests/test_session.cpp`
- Modify: `CMakeLists.txt` (add SessionController.cpp to `locus_core`)

**Interfaces:**
- Consumes: `Pin`, `SceneModel` types
- Produces:
  ```cpp
  class SessionController : public QObject {
    Q_OBJECT
  public:
    enum class State { Closed, Open, Activating };
    explicit SessionController(QObject *parent = nullptr);
    State state() const;
    bool isOpen() const;
    QString focusedId() const;
    StyleId styleId() const;
    void setStyleId(StyleId);
    void open();
    void close();
    void setFocus(const QString &id); // no-op if closed or empty
    // returns focused id if transitioning to Activating; empty if none
    QString activate();
  signals:
    void opened();
    void closed();
    void focusChanged(const QString &id);
    void activateRequested(const QString &id);
  };
  ```

- [ ] **Step 1: Write failing tests**

```cpp
void SessionTest::openClose() {
  locus::SessionController s;
  QCOMPARE(s.state(), locus::SessionController::State::Closed);
  s.open();
  QCOMPARE(s.state(), locus::SessionController::State::Open);
  s.close();
  QCOMPARE(s.state(), locus::SessionController::State::Closed);
}
void SessionTest::focusOnlyWhenOpen() {
  locus::SessionController s;
  s.setFocus(QStringLiteral("a"));
  QVERIFY(s.focusedId().isEmpty());
  s.open();
  s.setFocus(QStringLiteral("a"));
  QCOMPARE(s.focusedId(), QStringLiteral("a"));
}
void SessionTest::activateEmitsAndCloses() {
  locus::SessionController s;
  QSignalSpy spy(&s, &locus::SessionController::activateRequested);
  s.open();
  s.setFocus(QStringLiteral("code"));
  QCOMPARE(s.activate(), QStringLiteral("code"));
  QCOMPARE(spy.size(), 1);
  QCOMPARE(s.state(), locus::SessionController::State::Closed);
}
```

- [ ] **Step 2: Run tests — expect FAIL** (class missing)

Run: `ctest --test-dir build --output-on-failure` after rebuild

- [ ] **Step 3: Implement SessionController**

Minimal state machine per spec table; `activate()` no-ops (returns `{}`) if closed or no focus.

- [ ] **Step 4: Run tests — expect PASS**

- [ ] **Step 5: Commit**

```bash
git commit -am "feat: add SessionController open/focus/activate"
```

---

### Task 3: OrbitalLayoutStrategy (TDD)

**Files:**
- Create: `src/core/LayoutStrategy.h`, `src/layout/OrbitalState.h`, `src/layout/OrbitalLayoutStrategy.h/.cpp`, `tests/test_orbital_layout.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `QVector<Pin>`, density prefs, `focusedId`, `OrbitalState`
- Produces:
  ```cpp
  struct DensityPrefs {
    qreal minIconSize = 34;
    int maxPerRing = 12;
    qreal gap = 12;
    qreal widgetSize = 560;
  };
  class LayoutStrategy {
  public:
    virtual ~LayoutStrategy() = default;
    virtual SceneModel build(const QVector<Pin> &pins,
                             const QString &focusedId,
                             const DensityPrefs &density) = 0;
  };
  class OrbitalLayoutStrategy : public LayoutStrategy {
  public:
    void setRotationRadians(qreal r);
    qreal rotationRadians() const;
    SceneModel build(...) override;
  };
  ```

Packing rules (v1):
- Center at `(widgetSize/2, widgetSize/2)`.
- Inner ring radius `136`, icon size `max(minIconSize, 48)` while count ≤ 8 and ≤ `maxPerRing`.
- Overflow → outer ring radius `239`, icon size `minIconSize`.
- Angles: evenly spaced from `-π/2` (top) + `rotationRadians`.
- Decorations: `orbital.inner`, `orbital.outer` ellipses; if `focusedId` matches an item, add `hover.well` ellipse around that item’s bounds (inflated 12px).
- Hub title = focused pin label, else empty; brand `LOCUS`.
- `hitOrder` = items sorted by descending `z` (hover well does not steal hits; only `Item` roles).

- [ ] **Step 1: Failing test — 8 pins → one ring**

```cpp
void OrbitalTest::eightPinsOneRing() {
  locus::OrbitalLayoutStrategy s;
  locus::DensityPrefs d;
  auto pins = makePins(8);
  auto scene = s.build(pins, pins[0].id, d);
  int items = 0;
  for (const auto &it : scene.items)
    if (it.role == locus::ItemRole::Item) ++items;
  QCOMPARE(items, 8);
  QVERIFY(std::any_of(scene.decorations.begin(), scene.decorations.end(),
    [](const auto &d){ return d.styleKey == QLatin1String("orbital.inner"); }));
}
```

- [ ] **Step 2: Failing test — 20 pins → two rings; rotation moves bounds**

```cpp
void OrbitalTest::rotationChangesPositions() {
  locus::OrbitalLayoutStrategy s;
  auto pins = makePins(8);
  locus::DensityPrefs d;
  auto a = s.build(pins, {}, d);
  s.setRotationRadians(M_PI / 4);
  auto b = s.build(pins, {}, d);
  QVERIFY(a.items[0].bounds.center() != b.items[0].bounds.center());
}
```

- [ ] **Step 3: Implement strategy**

- [ ] **Step 4: Tests PASS**

- [ ] **Step 5: Commit**

```bash
git commit -am "feat: add OrbitalLayoutStrategy packing and decorations"
```

---

### Task 4: PinStore + Prefs (QSettings)

**Files:**
- Create: `src/core/PinStore.h/.cpp`, `src/core/Prefs.h/.cpp`
- Test: extend `tests/test_session.cpp` or add `tests/test_prefs.cpp` with temp `QSettings` path via `QSettings::setPath` / organization under `QDir::temp()`

**Interfaces:**
```cpp
class PinStore {
public:
  QVector<Pin> pins() const;
  void setPins(QVector<Pin>);
  void addPin(Pin);
  void removePin(const QString &id);
  void movePin(int from, int to);
  void load();
  void save() const;
};
class Prefs {
public:
  Appearance appearance() const; void setAppearance(Appearance);
  StyleId styleId() const; void setStyleId(StyleId);
  DensityPrefs density() const; void setDensity(DensityPrefs);
  QKeySequence hotkey() const; void setHotkey(QKeySequence); // default Ctrl+Space
  void load(); void save() const;
};
```

Default seed pins (dev): empty list — prefs UI adds later; for manual smoke, `main` may seed 3 fake pins if store empty.

- [ ] **Step 1: Tests round-trip pins and density**
- [ ] **Step 2: Implement with QSettings keys `pins`, `appearance`, `style`, `density/*`, `hotkey`**
- [ ] **Step 3: PASS + commit**

```bash
git commit -am "feat: persist pins and prefs via QSettings"
```

---

### Task 5: Hit-test helper + MenuView interface

**Files:**
- Create: `src/core/HitTest.h` (inline), `src/core/MenuView.h`, `tests/test_scene_hittest.cpp`

**Interfaces:**
```cpp
// First Item in hitOrder whose bounds contains point; else {}
inline QString hitTest(const SceneModel &scene, QPointF p) {
  for (const QString &id : scene.hitOrder) {
    for (const auto &it : scene.items) {
      if (it.id == id && it.role == ItemRole::Item && it.bounds.contains(p))
        return id;
    }
  }
  return {};
}
class MenuView { // abstract; QWidget subclass in ui/
public:
  virtual ~MenuView() = default;
  virtual void setScene(const SceneModel &) = 0;
  virtual QWidget *widget() = 0;
};
```

- [ ] **Step 1–4: TDD hitTest; commit**

```bash
git commit -am "feat: add SceneModel hitTest helper"
```

---

### Task 6: OrbitalGlassView + OverlayWindow

**Files:**
- Create: `src/ui/OrbitalGlassView.h/.cpp`, `src/ui/OverlayWindow.h/.cpp`
- Modify: `src/main.cpp` (smoke show)

**Behavior:**
- `OrbitalGlassView` : `QWidget`, transparent, paints smoke disc, decorations by `styleKey`, icons as rounded rects + `QIcon` (placeholder colored tiles if icon missing), hub text.
- Appearance from `Prefs` (dark/light fills matching Ardot amber language at a simplified level).
- Mouse move → `hitTest` → emit `itemHovered(id)`; click → `itemActivated(id)`; wheel → emit `rotationDelta(radians)`.
- `OverlayWindow`: `Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool`, translucent background, hosts view at 560×560, `showAt(QPoint globalCenter)`.

- [ ] **Step 1: Implement view paint + signals**
- [ ] **Step 2: Implement overlay host**
- [ ] **Step 3: Manual smoke — run `locus` briefly with fake pins (temporary in main)**
- [ ] **Step 4: Commit**

```bash
git commit -am "feat: add OrbitalGlassView and frameless OverlayWindow"
```

---

### Task 7: Platform — tray, hotkey, icons, launch (macOS first)

**Files:**
- Create: `src/platform/IconProvider.h/.cpp`, `AppLauncher.h/.cpp`, `HotkeyManager.h/.cpp`, `TrayController.h/.cpp`
- Modify: `src/main.cpp` wire Session ↔ Strategy ↔ View ↔ Overlay ↔ Tray/Hotkey

**macOS v1:**
- `IconProvider::iconForPath(path)` — `QFileIconProvider` or NSWorkspace via thin ObjC++ if needed; start with `QFileIconProvider`.
- `AppLauncher::launch(path)` — `QDesktopServices::openUrl(QUrl::fromLocalFile(path))` for `.app` bundles.
- `HotkeyManager` — use `QAction` with `Qt::ApplicationShortcut` **only if** window focused is insufficient; for global hotkey on macOS, implement with Carbon/RegisterEventHotKey **or** document dependency on `QHotkey` if added via FetchContent. Prefer **FetchContent QHotkey** (Skycoder42) for speed.
- Tray: Show / Preferences / Quit.

Wiring in `main`:
```
hotkey/tray Show → session.open() → strategy.build → view.setScene → overlay.showAt(cursor)
view.itemHovered → session.setFocus → rebuild → setScene
view.itemActivated / session.activate → launcher.launch → session.close → overlay.hide
Esc → close
```

- [ ] **Step 1: Implement platform adapters**
- [ ] **Step 2: Wire application object / main**
- [ ] **Step 3: Manual: tray appears, hotkey toggles overlay**
- [ ] **Step 4: Commit**

```bash
git commit -am "feat: wire tray, hotkey, launch, and session loop"
```

---

### Task 8: PrefsDialog + StyleFactory

**Files:**
- Create: `src/core/StyleFactory.h/.cpp`, `src/ui/PrefsDialog.h/.cpp`
- Modify: tray opens prefs

**StyleFactory:**
```cpp
struct StyleBundle {
  std::unique_ptr<LayoutStrategy> strategy;
  std::unique_ptr<MenuView> view; // or QWidget* owned by overlay
};
StyleBundle makeStyle(StyleId id, Appearance appearance);
```
v1: only `Orbital` returns real impl; `Cellular`/`Pie` fall back to Orbital with `qWarning`.

PrefsDialog tabs: Pins (list + add file dialog for `.app`) / Density / Hotkey recorder / Appearance.

- [ ] **Step 1–3: Implement, smoke prefs save, commit**

```bash
git commit -am "feat: add PrefsDialog and StyleFactory"
```

---

### Task 9: README + polish + verify design open items

**Files:**
- Modify: `README.md`, spec open items resolved in-doc

- [ ] Document build/run, hotkey default, working title note
- [ ] Resolve spec “Open for planning”: QSettings, factory, single-monitor cursor center for v1
- [ ] Commit

```bash
git commit -am "docs: README and close spec open items"
```

---

## Spec coverage check

| Spec item | Task |
| --- | --- |
| Session + no geometry | 2 |
| SceneModel / PlacedItem | 1 |
| Orbital strategy + decorations | 3 |
| hit-test | 5 |
| Overlay floating widget | 6 |
| Tray + hotkey + launch + icons | 7 |
| Pins + prefs | 4, 8 |
| Engine/UI decouple + factory | 8 |
| Cellular/Pie stubs only | 8 |
| Unit tests strategy/session | 2, 3, 5 |

## Placeholder scan

None intentional — QHotkey via FetchContent is specified in Task 7.
