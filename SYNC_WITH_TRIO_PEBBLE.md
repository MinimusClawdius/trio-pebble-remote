# Sync checklist (trio-pebble ↔ trio-pebble-remote)

When editing the **Trio** watchface repo, mirror these into **this** repo if they change:

1. **`package.json`**
   - `pebble.messageKeys` (entire block — indices must match watch C + JS).
   - `pebble.uuid` — keep **this** repo’s UUID unless you intentionally ship a new app id.
   - `pebble.sdkVersion`, `targetPlatforms`, `capabilities` (Remote only needs **configurable** today).

2. **`src/pkjs/index.js`**
   - `K` key map, `sendCommand`, `httpPost`, `settings` / `loadSettings` / config URL if Trio HTTP or config page changes.

3. **`src/remote_menu.c`** / **`src/main.c`**
   - Copy from `trio-pebble/remote-app/src/` when bolus/carbs UI or AppMessage sizes change.

4. **Menu icon**
   - `resources/images/menu_icon.png` — same 25×25 asset as `trio-pebble`.

Optional: from `trio-pebble` repo root, after editing `remote-app/`, copy into this repo:

`remote-app/src` → `src` (overwrite), then adjust paths if any.
