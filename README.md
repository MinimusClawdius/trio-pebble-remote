# Trio Remote (standalone repo for CloudPebble)

This repository exists **only** so you can use **CloudPebble → Import from GitHub** on a normal project root: there is **no** “point CloudPebble at a subfolder” option for the main `trio-pebble` watchface repo.

| GitHub repo | CloudPebble project | Product |
|-------------|---------------------|---------|
| **`trio-pebble`** (watchface) | Project **A** — import this repo | Trio **watchface** |
| **`trio-pebble-remote`** (this repo) | Project **B** — import **this** repo URL | **Trio Remote** watch **app** |

Same machine / local dev: you can still build from **`trio-pebble/remote-app/`** if you use the Pebble CLI; that folder is the **monorepo copy** kept in sync with this repo when we release.

## First-time setup

1. Copy **`resources/images/menu_icon.png`** from **`trio-pebble`** (25×25 PNG) into this repo at the same path. Without it, `pebble build` will fail on the menu icon resource.
2. Create an **empty** GitHub repository named e.g. **`trio-pebble-remote`** (no README/license). Then:
   ```bash
   cd /path/to/trio-pebble-remote
   git remote add origin https://github.com/<you>/trio-pebble-remote.git
   git push -u origin main
   ```
3. In CloudPebble: **New project → Import from GitHub** → select **`trio-pebble-remote`**.
4. Enable **Configurable** in project settings (matches `package.json`).

## Keeping in sync with `trio-pebble`

When the watchface changes **`pebble.messageKeys`**, UUID, or command JS, update **this** repo the same way (or copy `remote-app/src` from `trio-pebble` into `src/` here). See **`SYNC_WITH_TRIO_PEBBLE.md`**.

## UUID

Do **not** change the UUID to match the watchface — that would replace the watchface install. This app’s UUID is in **`package.json`**.
