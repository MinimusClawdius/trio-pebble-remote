# Sync checklist (trio-pebble ↔ trio-pebble-remote)

When editing the **Trio** watchface repo, mirror these into **this** repo if they change:

1. **`package.json`**
   - `pebble.messageKeys` (entire block — indices must match watch C + JS).
   - `pebble.uuid` — keep **this** repo’s UUID unless you intentionally ship a new app id.
   - `pebble.sdkVersion`, `targetPlatforms`, `capabilities` (Remote only needs **configurable** today).

2. **`src/pkjs/index.js`**
   - `K` key map, `sendCommand`, `httpPost`, `settings` / `loadSettings` / config URL if Trio HTTP or config page changes.

3. **`src/remote_menu.c`**, **`src/main.c`**, **`src/modules/remote_send_ui.*`**
   - **Source of truth for Trio Remote UX is this repo** (confirm / progress / toast live here only; the watchface repo does not ship `remote_send_ui`).
   - If you maintain `trio-pebble/remote-app/`, copy these paths from **here** into `remote-app/src/` when syncing the monorepo.

4. **Menu icon**
   - `resources/images/menu_icon.png` — same 25×25 asset as `trio-pebble`.

From **`trio-pebble`** repo root (Windows):

`pwsh -File scripts/sync-trio-pebble-remote.ps1`

That copies `remote-app/package.json`, `wscript`, and `remote-app/src/` into this repo. You still need **`resources/images/menu_icon.png`** here (not copied by the script).
