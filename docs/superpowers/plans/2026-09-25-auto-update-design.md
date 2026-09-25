# Locus auto-update design

Decisions (2026-09-25): custom Qt checker (not WinSparkle), HTTPS + SHA256
verification for now, macOS included in the design (Sparkle 2, implemented
when the mac build ships).

## Overview

```
GitHub Release (tag v0.2.0)
├── Locus-0.2.0-windows-x64-setup.exe      ← existing CI build
├── Locus-0.2.0-macos.dmg                  ← existing CI build
└── appcast.xml                            ← CI generates + attaches
        ↑
app fetches https://github.com/<owner>/locus/releases/latest/download/appcast.xml
```

`/releases/latest/download/` always resolves to the newest release's asset —
no extra hosting, no mutable files in the repo.

## Feed format (appcast.xml)

Sparkle-flavoured RSS so both platforms read one file; items are filtered by
`sparkle:os`:

```xml
<rss version="2.0" xmlns:sparkle="http://www.andymatuschak.org/xml-namespaces/sparkle">
  <channel>
    <title>Locus</title>
    <item>
      <title>0.2.0</title>
      <pubDate>Fri, 25 Sep 2026 12:00:00 +0000</pubDate>
      <enclosure url="https://github.com/<owner>/locus/releases/download/v0.2.0/Locus-0.2.0-windows-x64-setup.exe"
                 sparkle:version="0.2.0" sparkle:os="windows-x64"
                 length="24129834" type="application/octet-stream"
                 sparkle:sha256="<hex>"/>
      <enclosure url="https://github.com/<owner>/locus/releases/download/v0.2.0/Locus-0.2.0-macos.dmg"
                 sparkle:version="0.2.0" sparkle:os="macos"
                 length="..." type="application/octet-stream"
                 sparkle:edSignature="<only once Sparkle ships>"/>
    </item>
  </channel>
</rss>
```

Note: `sparkle:sha256` is our extension — Sparkle 2 itself mandates
`edSignature`, so macOS needs an EdDSA keypair regardless of the Windows
"SHA256-only" decision. Deferred until the mac updater ships.

## Windows: custom checker (v1, implement now)

New code, QtNetwork only (already linked):

- `core/UpdateChecker.{h,cpp}`
  - `check()` → GET feed → parse with QXmlStreamReader → pick newest item
    where `sparkle:os="windows-x64"` → semver-compare `sparkle:version`
    against `LOCUS_VERSION` (already defined by CMake)
  - signals: `updateAvailable(UpdateInfo)` / `upToDate()` /
    `checkFailed(QString)` — failures are silent in auto-check, shown only
    for manual checks
- `core/UpdateDownloader.{h,cpp}`
  - streams reply to `%TEMP%/locus-update-<ver>.exe`, incremental
    QCryptographicHash::Sha256, verifies against feed hash, deletes on
    mismatch and NEVER runs an unverified file
- UI: glass-styled prompt consistent with Locus (not a native dialog):
  - Settings → General gets an "Updates" row: current version, "Check now"
    button, last-checked caption
  - auto-check 5s after launch, throttled to 24h via QSettings timestamp
  - update prompt: version, size, [Download & Install] [Later]
  - progress = simple determinate bar in the same prompt
- Apply: `QProcess::startDetached(installer, {"/VERYSILENT", "/NORESTART",
  "/SUPPRESSMSGBOXES"})` then `QApplication::quit()`. Inno upgrades in place
  (fixed AppId already set). The app quits itself first, so no
  CloseApplications forcing is needed.
- Failure modes: offline/404 → silent auto-check; hash mismatch → file
  deleted, error only on manual check; partial download → discarded.

## macOS (design only)

Sparkle 2 framework in the bundle:
- Info.plist: `SUFeedURL` (same appcast URL) + `SUPublicEDKey`
- one-time `generate_keys`, private key in CI secrets
- CI signs the dmg with `sign_update` and writes `sparkle:edSignature` into
  the feed item
- Sparkle handles check/download/install UI; no custom code beyond wiring.

## CI (`.github/workflows/release.yml`, new)

Trigger: `push: tags: ["v*"]`. Per-OS matrix reusing ci.yml steps, then:
1. Build + test + package (Windows: ISCC; macOS: dmg).
2. `sha256sum` the installer.
3. Generate `appcast.xml` from a template (`installer/appcast.xml.in`) with
   tag-derived version, asset URLs, size, hash, date.
4. `gh release create` / upload installer + appcast.xml.

Version single source of truth: the git tag; CMake `project(locus VERSION)`
and `installer.iss` `#define AppVersion` are bumped to match in the release
commit (or the workflow validates they agree and fails the tag otherwise —
cheap and catches drift).

## Phasing

1. **v1 (Windows)**: UpdateChecker + UpdateDownloader + Settings row +
   update prompt + apply-via-Inno + release.yml with appcast generation.
   Tests: feed parsing (fixtures), semver compare, hash-mismatch refusal.
2. **macOS**: Sparkle wiring when the dmg is actually distributed.

Explicitly out of scope: delta updates, background auto-install without
consent, Linux self-update (package managers own that; show a link only).
