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

## Usage

1. Open **Trio Remote** on the watch (Quick Launch recommended)
2. **Remote bolus** or **Remote carbs** → adjust → **SELECT** to send
3. Status appears on the **Cancel** row (`Bolus delivered`, `Trio unreachable`, etc.)
4. Trio applies immediately (no phone confirm queue UI required)

## Flow

```
Watch C  AppMessage(cmd)  →  phone pkjs  POST /api/bolus|/api/carbs  →  Trio loopback
         ← status text    ←  HTTP result
```

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
