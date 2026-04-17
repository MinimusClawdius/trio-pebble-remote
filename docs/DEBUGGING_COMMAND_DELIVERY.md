# Debugging Pebble Remote Command Delivery

This guide helps you identify where communication is failing between the Pebble Remote watch app and the Trio iOS app.

## Architecture Overview

The command delivery flow has 4 stages:

1. **Pebble Remote (watch)** → AppMessage → **Trio-Pebble-Remote (JS on phone)**
2. **Trio-Pebble-Remote (JS)** → HTTP POST → **Trio iOS app** (HTTP server :8080)
3. **Trio iOS app** → HTTP response → **Trio-Pebble-Remote (JS)**
4. **Trio-Pebble-Remote (JS)** → AppMessage → **Pebble Remote (watch)**

## Logging Points to Check

### A. C-side (Pebble Remote - Watch)

**Files:** `src/main.c`, `src/modules/remote_send_ui.c`, `src/remote_menu.c`

**What to check in watch logs:**
1. `Outbox sent to phone` - Confirms watch successfully sent to phone via AppMessage
2. `remote: queue cmd type=X amount=Y` - Confirms command was queued for sending
3. `remote: outbox_send failed=D` - Shows AppMessage send failure (D = error code)
4. `remote: begin send cmd_type=X amount=Y timeout_ms=Z` - Start of send sequence
5. `remote: status timeout cmd_type=X amount=Y` - Timeout waiting for phone response
6. `remote: status received: S` - Phone responded with status `S`
7. `remote: late status ignored: S` - Response came too late (window already closed)

### B. JS-side (Trio-Pebble-Remote - Phone)

**File:** `src/pkjs/index.js`

**What to check in phone logs:**
1. `Trio Remote: cmd type=X amt=Y` - Received command from watch via AppMessage
2. `Trio Remote: HTTP POST URL body=B` - Making HTTP request to Trio
3. `Trio Remote: HTTP STATUS from URL body=R` - Trio's HTTP response
4. `Trio Remote: status sent to watch: S` - Successfully sent status back to watch
5. `Trio Remote: status send failed to watch: E` - Failed to send back to watch (E = error details)

### C. Trio iOS App

**Files:** `PebbleLocalAPIServer.swift`, `PebbleCommandManager.swift`

**What to check in Trio logs:**
1. `http_post` log entry for `POST /api/bolus → delivered id=U units=X` - Trio received and processed command
2. `http_post` log entry for `POST /api/carbs → delivered id=C grams=X` - Trio received and processed carbs
3. Check Trio's file logs for Pebble API requests
4. Verify Trio is running and HTTP server is active on port 8080
5. Check iOS logs for Trio app (especially backgrounding issues)

## Common Failure Points & Troubleshooting

### 1. Watch → Phone (AppMessage)

**Symptoms:**
- No logs from Trio Remote JS
- Watch logs show `outbox_send failed=D`

**Fix:**
- Ensure Rebble app is in foreground when sending commands
- Check Bluetooth connection between Pebble and phone
- Restart Trio-Pebble-remote watch app if stuck
- Try sending command multiple times to see if intermittent

### 2. Phone → Trio (HTTP)

**Symptoms:**
- Trio Remote JS logs show HTTP request but Trio doesn't respond
- HTTP timeout or connection refused

**Fix:**
- Verify Trio is running and HTTP server is active
- Check Trio's Pebble integration is enabled
- Confirm Trio hasn't been backgrounded/suspended by iOS
- Check Trio's file logs for Pebble API requests
- Keep Trio in foreground during testing

### 3. Trio → Phone (HTTP Response)

**Symptoms:**
- HTTP request made but timeout or error response

**Fix:**
- Check Trio app logs for HTTP server activity
- Verify Trio app isn't suspended by iOS
- Ensure Trio's HTTP server is bound to 127.0.0.1:8080
- Try `curl http://127.0.0.1:8080/api/pebble/v1/ping` to test server (from iPad with Trio in Split View)

### 4. Phone → Watch (AppMessage Response)

**Symptoms:**
- Trio responds but watch doesn't receive status
- Watch logs show timeout but Trio Remote JS shows status sent

**Fix:**
- Check Trio Remote JS logs for `status send failed to watch`
- Verify AppMessage is properly configured
- Check watch is still connected to phone
- Restart watch app to reset AppMessage connection

## Testing Strategy

1. **Start with watch logs** - Check watch logs first to see if command was sent
2. **Check phone logs** - Look for Trio Remote JS activity
3. **Check Trio logs** - Verify Trio received and processed command
4. **Verify Trio is running** - HTTP server must be active on port 8080
5. **Test with simple command** - Use small bolus/carb to isolate issue

## Expected Response Flow

1. Watch sends command via AppMessage
2. Trio Remote JS receives and logs: `cmd type=X amt=Y`
3. Trio Remote JS makes HTTP POST to Trio
4. Trio processes and logs: `POST /api/bolus → delivered id=U units=X`
5. Trio returns HTTP 200 with `{"status":"delivered",...}`
6. Trio Remote JS receives HTTP response and logs: `HTTP 200 from URL`
7. Trio Remote JS sends AppMessage back to watch with status
8. Watch receives status and shows toast/updates UI

## Key Changes from Trio Commit e6f0cdee1

- Commands now execute immediately (no iPhone confirmation step)
- HTTP response changed from 202 to 200
- Response from `pending_confirmation` to `delivered`
- Removed enqueue notification system

## Debug Commands

- **Check Trio's Pebble logs:** Look for `PebbleIntegrationFileLogger` entries in Trio's file export
- **Check Trio is running HTTP server:** Verify in Trio settings Pebble integration shows "Running"
- **Check Rebble logs:** Look for Trio Remote app logs in Rebble app's log viewer
- **Verify iOS hasn't suspended Trio:** Keep Trio in foreground during testing
- **Test Trio HTTP server directly:** Use Safari in Split View with Trio on iPad to hit `http://127.0.0.1:8080/api/pebble/v1/ping`

## Recent Commits

- **trio-pebble-remote commit 47f5317:** Added comprehensive logging for phone/Trio handoff
- **trio commit e6f0cdee1:** Changed from queued commands to immediate delivery
- **trio-pebble commit db26919:** Fixed CONFIG_GRAPH_TIME_RANGE webpack parse error
- **trio-pebble commit ea8ccac:** Remote send UI improvements
- **trio-pebble commit 8fbf8e7:** HTTP 2xx response handling for remote-app
- **trio-pebble-remote commit 73e7882:** Treat Trio bolus/carbs POST as success on 2xx
- **trio-pebble-remote commit ed02d11:** PDCS assets for color watches
- **trio-pebble-remote commit cf944b8:** Remote 1.3.1 picker chrome fix
- **trio-pebble-remote commit 994be93:** Trio Remote v1.2 confirm dialog, progress, toasts
- **trio-pebble-remote commit 73e7882:** HTTP 2xx success handling

## File Locations

- **Watch C code:** `trio-pebble-remote/src/`
- **Phone JS code:** `trio-pebble-remote/src/pkjs/`
- **Watchface repo (monorepo copy):** `trio-pebble/remote-app/`
- **Trio repo:** `trio/` (check `Sources/Services/PebbleManager/`)

## Getting Help

When reporting issues, include:
1. Watch logs (all APP_LOG entries around command time)
2. Phone logs (all Trio Remote console.log entries)
3. Trio logs (PebbleIntegrationFileLogger entries)
4. Trio app version
5. Pebble watch model and firmware
6. Rebble app version
7. Exact steps to reproduce
- **Date/time of issue** - for correlating with Trio logs
- **Command type** - bolus or carbs
- **Amount** - what was being sent
- **Trio status** - was Trio running and showing Pebble "Running"?
- **Rebble status** - was Rebble app in foreground?
- **iOS version** - for backgrounding issues
- **Pebble firmware** - for compatibility issues