// Trio Remote — minimal PebbleKit JS (must match watchface message key indices)
var K = {
    GLUCOSE: 0, TREND: 1, DELTA: 2, IOB: 3, COB: 4,
    LAST_LOOP: 5, GLUCOSE_STALE: 6, CMD_TYPE: 7, CMD_AMOUNT: 8,
    CMD_STATUS: 9, GRAPH_DATA: 10, GRAPH_COUNT: 11, LOOP_STATUS: 12,
    UNITS: 13, PUMP_STATUS: 14, RESERVOIR: 15,
    CONFIG_FACE_TYPE: 16, CONFIG_DATA_SOURCE: 17,
    CONFIG_HIGH_THRESHOLD: 18, CONFIG_LOW_THRESHOLD: 19,
    CONFIG_ALERT_HIGH_ENABLED: 20, CONFIG_ALERT_LOW_ENABLED: 21,
    CONFIG_ALERT_URGENT_LOW: 22, CONFIG_ALERT_SNOOZE_MIN: 23,
    CONFIG_COLOR_SCHEME: 24,
    BATTERY_PHONE: 25, WEATHER_TEMP: 26, WEATHER_ICON: 27,
    STEPS: 28, HEART_RATE: 29,
    PREDICTIONS_DATA: 30, PREDICTIONS_COUNT: 31,
    PUMP_BATTERY: 32, SENSOR_AGE: 33,
    CONFIG_CHANGED: 34, TAP_ACTION: 35,
    CONFIG_WEATHER_ENABLED: 36,
    CONFIG_COMP_SLOT_0: 37, CONFIG_COMP_SLOT_1: 38,
    CONFIG_COMP_SLOT_2: 39, CONFIG_COMP_SLOT_3: 40,
    CONFIG_CLOCK_24H: 41,
    CONFIG_GRAPH_SCALE_MODE: 42,
    CONFIG_GRAPH_TIME_RANGE: 43,
    TRIO_LINK: 44
};

var settings = {
    dataSource: 0,
    trioHost: 'http://127.0.0.1:8080',
    faceType: 0,
    colorScheme: 0,
    highThreshold: 180,
    lowThreshold: 70,
    urgentLow: 55,
    alertHighEnabled: true,
    alertLowEnabled: true,
    alertSnoozeMin: 15,
    weatherEnabled: true,
    glucoseUnits: 'mgdl',
    compSlot0: 1,
    compSlot1: 5,
    compSlot2: 6,
    compSlot3: 0,
    clock24h: true,
    graphScaleMode: 0,
    graphTimeRange: 0
};

function loadSettings() {
    try {
        var saved = localStorage.getItem('trio_settings');
        if (saved) {
            var parsed = JSON.parse(saved);
            for (var key in parsed) {
                if (parsed.hasOwnProperty(key)) settings[key] = parsed[key];
            }
        }
    } catch (e) {
        console.log('Trio Remote: settings load error ' + e);
    }
}

function saveSettings() {
    try {
        localStorage.setItem('trio_settings', JSON.stringify(settings));
    } catch (e) { /* ok */ }
}

function displayUnitsForWatch() {
    return settings.glucoseUnits === 'mmol' ? 'mmol' : 'mgdl';
}

function payloadGet(p, keyNum) {
    if (!p) return undefined;
    var k = keyNum | 0;
    if (p[k] !== undefined && p[k] !== null) return p[k];
    if (p[String(k)] !== undefined && p[String(k)] !== null) return p[String(k)];
    return undefined;
}

function httpPost(url, body, callback) {
    var xhr = new XMLHttpRequest();
    xhr.open('POST', url, true);
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.timeout = 15000;
    xhr.onload = function () {
        callback(xhr.status === 200 ? xhr.responseText : null);
    };
    xhr.onerror = function () { callback(null); };
    xhr.ontimeout = function () { callback(null); };
    xhr.send(body);
}

function sendCommand(type, amount) {
    if (settings.dataSource !== 0 && settings.dataSource !== 3) {
        var msg = {};
        msg[K.CMD_STATUS] = 'Commands need Trio API';
        Pebble.sendAppMessage(msg);
        return;
    }

    var endpoint = type === 1 ? '/api/bolus' : '/api/carbs';
    var body = type === 1
        ? JSON.stringify({ units: amount / 10.0 })
        : JSON.stringify({ grams: amount, absorptionHours: 3 });

    httpPost(settings.trioHost + endpoint, body, function (resp) {
        var statusMsg;
        if (resp == null) {
            statusMsg = 'Trio unreachable';
        } else {
            statusMsg = 'Sent';
            try {
                var r = JSON.parse(resp || '{}');
                statusMsg = r.message || r.status || statusMsg;
            } catch (e) { /* ok */ }
        }
        var msg = {};
        msg[K.CMD_STATUS] = statusMsg.substring(0, 63);
        Pebble.sendAppMessage(msg);
    });
}

var TRIO_CONFIG_PAGE_URL =
    'https://minimusclawdius.github.io/trio-pebble/config/index.html';

Pebble.addEventListener('showConfiguration', function () {
    var params = encodeURIComponent(JSON.stringify(settings));
    Pebble.openURL(TRIO_CONFIG_PAGE_URL + '#' + params);
});

Pebble.addEventListener('webviewclosed', function (e) {
    if (e && e.response) {
        try {
            var newSettings = JSON.parse(decodeURIComponent(e.response));
            for (var key in newSettings) {
                if (newSettings.hasOwnProperty(key)) settings[key] = newSettings[key];
            }
            saveSettings();
        } catch (ex) {
            console.log('Trio Remote: config parse error: ' + ex);
        }
    }
});

Pebble.addEventListener('appmessage', function (e) {
    var p = e.payload;
    var cmdType = payloadGet(p, K.CMD_TYPE);
    var cmdAmt = payloadGet(p, K.CMD_AMOUNT);
    if (cmdType !== undefined && cmdAmt !== undefined) {
        console.log('Trio Remote: cmd type=' + cmdType + ' amt=' + cmdAmt);
        sendCommand(cmdType | 0, cmdAmt | 0);
    }
});

Pebble.addEventListener('ready', function () {
    console.log('Trio Remote pkjs ready');
    loadSettings();
});
