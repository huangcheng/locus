# Locus — Architecture Design

**Date:** 2026-09-20  
**Status:** Draft for review  
**Stack:** Qt Widgets (not Electron, not Qt Quick)  
**Visual source:** Ardot file `727830845408975` — frames `01b` / `01c` (liquid glass orbital)

## Goal

Locus is a cross-platform, hotkey + tray summonable floating radial launcher. Pins are manual. The product must support multiple UI skins over time without rewriting core behavior:

- **Orbital** (v1) — multi-ring selection orbitals, liquid glass
- **Cellular** (later)
- **Pie / Kando-like** (later)

## Decision: Engine / UI decoupling (option C)

Shared **SessionController** + pluggable **LayoutStrategy** → common **SceneModel**; pluggable **MenuView** only paints and hit-tests. Platform shell owns OS glue (overlay, tray, hotkey, icons, launch).

```
Platform (Qt shell)
  hotkey · tray · overlay · native icons · launch
        │
SessionController
  summon → hover → select → activate / dismiss
  owns focused item id + open/closed; no geometry
        │
LayoutStrategy (pluggable)
  Orbital | Cellular | Pie
  pins + prefs + rotation → SceneModel
        │
SceneModel
  PlacedItem{id, bounds, z, role} · decorations · hub
        │
MenuView (pluggable paint)
  OrbitalGlassView | CellularView | PieView
  paint SceneModel · hit-test → item id → Session
```

| Layer | Owns | Must not |
| --- | --- | --- |
| Session | Interaction state (`isOpen`, `focusedId`, `styleId`) | Geometry, packing, pixels |
| Strategy | Layout math → `SceneModel` | Launch apps, own window |
| View | Paint + pointer → id | Pack pins, persist prefs |
| Platform | Overlay, tray, hotkey, icons, launch | Style-specific packing |

## SceneModel

### PlacedItem

- `id` — pin / app id
- `bounds` — axis-aligned rect in widget space (icons stay upright while the ring revolves)
- `z` — draw order
- `role` — `Item` | `Hub` | `Decoration`
- optional `angle` — slot / wedge metadata (views may ignore)

### SceneModel

- `items: PlacedItem[]`
- `hub` — center label (`selectedTitle`, brand subtitle)
- `decorations` — orbitals, blooms, hover wells as named shapes (ellipse/path + style key), not hard-coded in Session
- `hitOrder` — front-to-back ids for picking

### Orbital strategy (v1)

- **Input:** ordered pins + density prefs (`minIconSize`, `maxPerRing`, gap)
- **Behavior:** greedy multi-ring packing → radii + angles; emit outer/inner orbital decorations; per-item bounds on those radii; hover-well decoration for `focusedId`
- **Rotation:** drag/scroll offset lives in strategy state (e.g. `OrbitalState`), not in Session
- **Contract:** Session passes `focusedId`; strategy rebuilds decorations; View never recomputes packing

### Later strategies (same SceneModel)

- **Cellular** → grid cells; cell chrome as decorations
- **Pie** → wedge paths as decorations; icon bounds at wedge centroids

## Session flow

States: `Closed → Open → (Hovering) → Activating → Closed`

| Event | Effect |
| --- | --- |
| Hotkey / tray Show | `Open`; strategy builds scene; overlay shown at cursor (or last position) |
| Pointer move | hit-test → `focusedId`; strategy refresh decorations; hub title updates |
| Click / Enter | `Activating` → Platform launches pin → `Closed`; overlay hides |
| Esc / hotkey again / click outside widget | `Closed` |
| Drag / scroll (Orbital) | forwarded to strategy as rotation delta; Session stays `Open` |

```
Hotkey/Tray → Session.open()
Pointer     → View.hitTest() → Session.setFocus(id) → Strategy.rebuild() → View.paint()
Activate    → Session.activate() → Platform.launch(pin) → Session.close()
```

## Platform overlay

- Frameless, transparent, always-on-top widget sized to the active view bounds (e.g. 560×560), not fullscreen
- Click-through outside the painted disc/mask; input captured inside
- Tray icon + global hotkey at app start
- Icon provider + launch adapter behind small OS-specific interfaces (macOS / Windows / Linux)

## Pins store

- Ordered list: `{ id, appId/path, label, iconKey }`
- Persist locally (JSON or QSettings)
- Add / reorder / remove only via Prefs UI (manual pins)
- Session reads pins; strategies never write them

## Prefs

Shared dialog from tray (not inside the radial widget):

- Appearance: Dark / Light / System
- Style: `orbital` (v1); `cellular` / `pie` stubbed or hidden until built
- Density: min icon size, max-per-ring (Orbital; other styles ignore or map later)
- Hotkey binding

## Visual direction (Orbital v1)

Ardot frames **01b Liquid Glass Dark** and **01c Liquid Glass Light**:

- Floating frosted disc, not a fullscreen dimmer
- Per-track orbitals (inner + outer)
- Real/native app icons, upright
- Per-item hover well on the orbital for the focused pin
- Warm amber selection language; dual appearance schemes

## v1 scope

**Ships**

- Qt Widgets shell: overlay + tray + hotkey + launch + native icons
- SessionController + SceneModel
- Orbital strategy + OrbitalGlass view
- Prefs: Pins / Density / Hotkey / Appearance

**Cuts**

- Cellular and pie UIs (interfaces reserved only)
- Nested menus / folders
- Cloud sync, plugins, scripting
- Electron / Qt Quick

## Testing

- Strategy packing and Session transitions: unit-testable without a window
- View hit-test: fixed `SceneModel` fixtures

## Open for implementation planning

Resolved in v1 plan / code:

- Persistence: **QSettings** (pins as JSON blob under `pins/json`)
- Style switch: **StyleFactory** (Task 8; Orbital only for now, wired directly in `main`)
- Overlay placement: cursor-centered, single monitor for v1
- Animation: deferred (view concern)
