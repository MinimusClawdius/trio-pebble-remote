# Trio Pebble Remote App

Companion watch app for the Trio Pebble CGM watchface. Use hardware buttons to send bolus/carb commands to Trio.

## Quick Start

### Prerequisites
- Pebble SDK 3 installed ([CloudPebble](https://cloudpebble.net) or local SDK)
- Pebble watch connected
- Phone with Rebble app

### Local Build (No Docker)

```bash
cd /home/jt/.openclaw/workspace/trio-pebble/remote-app
pebble build
pebble install --phone <your-phone-ip>
```

**Build output:** Creates `trio-pebble-remote.zip` for installation.

### CloudPebble Build

1. Go to [CloudPebble](https://cloudpebble.net)
2. Import: `https://github.com/MinimusClawdius/trio-pebble-remote`
3. Enable capabilities: **Configurable**, **Uses health**, **Uses location**
4. Build and install

## Installation

After building:
1. Open Rebble app on your phone
2. Select your Pebble watch
3. Find "Trio Remote" in the project list
4. Install the `.zip` file

## Usage

- **UP/DOWN buttons:** Navigate menu
- **SELECT (double-tap):** Confirm action
- **BACK:** Exit

Assign Trio Remote to a Quick Launch shortcut on your watch for quick access.

## Architecture

- `main.c` - App lifecycle and menu logic
- `remote_menu.c/.h` - Bolus/carb menu implementation
- `pkjs/` - PebbleKit JS for data fetching

## Configuration

- Menu icon: `resources/images/menu_icon.png` (25×25px PNG)
- UUID: `fdd1d4b6-f2f2-4819-a26b-8ccad4264feb`
- SDK version: 3

## Notes

- Only works with **Trio** as the data source
- iPhone confirmation required for bolus commands
- Not available with Dexcom Share or Nightscout
