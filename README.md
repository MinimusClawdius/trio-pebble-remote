# Trio Pebble Remote

Companion **watch app** (not a watchface) for bolus / carb entry into **Trio** via Rebble → HTTP `127.0.0.1`.

## Requirements

- Trio iOS with **Pebble integration enabled** (local HTTP listening, default port **8080**)
- Rebble app on the phone (PebbleKit JS must run)
- Watchface **Trio Pebble** can stay installed; open **Trio Remote** to send commands

## Install (CloudPebble)

1. Import `https://github.com/MinimusClawdius/trio-pebble-remote`
2. Build → install on watch
3. In Rebble, open **Trio Remote** settings once and confirm host is `http://127.0.0.1:8080` (or your Trio port)

## Home screen

Two large buttons: **BOLUS** (top) and **CARBS** (bottom).

- **Touch** (Pebble Time 2 / platforms with `PBL_TOUCH`): tap a button
- **Buttons**: UP/DOWN focus, SELECT open, **BACK exits** (no Exit row)
- Status strip at the bottom shows send result

## Usage

1. Open **Trio Remote** on the watch (Quick Launch recommended)
2. **Bolus** or **Carbs** → UP/DOWN adjust amount → **SELECT**
3. **Confirm screen** — read the amount, **SELECT again** to send (BACK edits amount)
4. Status shows on the main menu footer (`Bolus delivered`, `Trio unreachable`, …)
5. Trio applies on the phone after a successful POST (watch double-confirm is the safety gate)

## Flow

```
Watch C  AppMessage(cmd)  →  phone pkjs  POST /api/bolus|/api/carbs  →  Trio loopback
         ← status text    ←  HTTP result
```

## Configuration

Rebble → **Trio Remote** → settings (not the watchface page):

| Setting | Purpose |
|---------|---------|
| Host / IP + Port | Trio loopback HTTP (default `127.0.0.1:8080`) |
| Bolus step / starting amount | UP/DOWN step and initial bolus on the watch |
| Carb step / starting amount | UP/DOWN step and initial carbs on the watch |

Offline config HTML is embedded in the app (`config/index.html` → `pkjs/settings/generated.js`).

## Troubleshooting

| Watch status | Meaning |
|--------------|---------|
| Trio unreachable | Trio suspended, integration off, or wrong port/host |
| Trio timeout | Phone JS got no HTTP response in 15s |
| Bolus delivered / Carbs delivered | Trio accepted and executed |
| Phone link busy / Phone not ready | AppMessage outbox failed — reopen Remote, keep Rebble alive |

Export **Settings → Services → Pebble → Export Pebble log** on Trio; look for `http_post` / `enact_bolus` / `store_carbs`.

## Version

1.2.0 — self-contained C (no missing headers), host normalize, clearer HTTP/status feedback.
