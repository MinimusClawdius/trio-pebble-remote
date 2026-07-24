// Trio Remote — PebbleKit JS: watch AppMessage → HTTP POST Trio loopback
// Config is remote-only (host/port + U/g steps), not the watchface page.
var getSettingsHtml = require('./settings/generated.js');

var K = {
    CMD_TYPE: 7,
    CMD_AMOUNT: 8,
    CMD_STATUS: 9,
    BOLUS_STEP_TENTHS: 50,
    BOLUS_DEFAULT_TENTHS: 51,
    CARB_STEP_GRAMS: 52,
    CARB_DEFAULT_GRAMS: 53
};

var DEFAULT_TRIO_HOST = 'http://127.0.0.1:8080';

var settings = {
    trioHost: DEFAULT_TRIO_HOST,
    bolusStepTenths: 1,
    bolusDefaultTenths: 20,
    carbStepGrams: 5,
    carbDefaultGrams: 15
};

function clampInt(v, min, max, fallback) {
    v = parseInt(v, 10);
    if (isNaN(v)) return fallback;
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

function normalizeTrioHost() {
    var h = settings.trioHost == null ? '' : String(settings.trioHost).replace(/^\s+|\s+$/g, '');
    if (!h) {
        h = DEFAULT_TRIO_HOST;
    } else {
        if (h.indexOf('://') < 0) h = 'http://' + h;
        h = h.replace(/\/+$/, '');
        if (h.indexOf('http://') !== 0 && h.indexOf('https://') !== 0) {
            h = DEFAULT_TRIO_HOST;
        }
    }
    settings.trioHost = h;
    return h;
}

function normalizePrefs() {
    settings.bolusStepTenths = clampInt(settings.bolusStepTenths, 1, 50, 1);
    settings.bolusDefaultTenths = clampInt(settings.bolusDefaultTenths, 1, 300, 20);
    settings.carbStepGrams = clampInt(settings.carbStepGrams, 1, 50, 5);
    settings.carbDefaultGrams = clampInt(settings.carbDefaultGrams, 1, 250, 15);
}

function loadSettings() {
    try {
        var saved = localStorage.getItem('trio_remote_settings');
        if (!saved) {
            saved = localStorage.getItem('trio_settings');
        }
        if (saved) {
            var parsed = JSON.parse(saved);
            if (parsed.trioHost != null) settings.trioHost = parsed.trioHost;
            if (parsed.bolusStepTenths != null) settings.bolusStepTenths = parsed.bolusStepTenths;
            if (parsed.bolusDefaultTenths != null) settings.bolusDefaultTenths = parsed.bolusDefaultTenths;
            if (parsed.carbStepGrams != null) settings.carbStepGrams = parsed.carbStepGrams;
            if (parsed.carbDefaultGrams != null) settings.carbDefaultGrams = parsed.carbDefaultGrams;
        }
    } catch (e) {
        console.log('[TrioRemote] settings load error ' + e);
    }
    normalizeTrioHost();
    normalizePrefs();
}

function saveSettings() {
    normalizeTrioHost();
    normalizePrefs();
    try {
        localStorage.setItem(
            'trio_remote_settings',
            JSON.stringify({
                trioHost: settings.trioHost,
                bolusStepTenths: settings.bolusStepTenths,
                bolusDefaultTenths: settings.bolusDefaultTenths,
                carbStepGrams: settings.carbStepGrams,
                carbDefaultGrams: settings.carbDefaultGrams
            })
        );
    } catch (e) { /* ok */ }
}

function payloadGet(p, keyNum) {
    if (!p) return undefined;
    var k = keyNum | 0;
    if (p[k] !== undefined && p[k] !== null) return p[k];
    if (p[String(k)] !== undefined && p[String(k)] !== null) return p[String(k)];
    if (keyNum === K.CMD_TYPE && p.KEY_CMD_TYPE != null) return p.KEY_CMD_TYPE;
    if (keyNum === K.CMD_AMOUNT && p.KEY_CMD_AMOUNT != null) return p.KEY_CMD_AMOUNT;
    return undefined;
}

function sendStatus(text) {
    var msg = {};
    msg[K.CMD_STATUS] = String(text || '').substring(0, 63);
    Pebble.sendAppMessage(msg);
}

function pushPrefsToWatch() {
    normalizePrefs();
    var msg = {};
    msg[K.BOLUS_STEP_TENTHS] = settings.bolusStepTenths | 0;
    msg[K.BOLUS_DEFAULT_TENTHS] = settings.bolusDefaultTenths | 0;
    msg[K.CARB_STEP_GRAMS] = settings.carbStepGrams | 0;
    msg[K.CARB_DEFAULT_GRAMS] = settings.carbDefaultGrams | 0;
    console.log('[TrioRemote] push prefs ' + JSON.stringify(msg));
    Pebble.sendAppMessage(
        msg,
        function () {
            console.log('[TrioRemote] prefs sent to watch');
        },
        function (e) {
            console.log('[TrioRemote] prefs send failed ' + JSON.stringify(e));
        }
    );
}

function httpPost(url, body, callback) {
    var xhr = new XMLHttpRequest();
    xhr.open('POST', url, true);
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.timeout = 15000;
    xhr.onload = function () {
        callback(xhr.status >= 200 && xhr.status < 300, xhr.status, xhr.responseText || '');
    };
    xhr.onerror = function () {
        callback(false, 0, '');
    };
    xhr.ontimeout = function () {
        callback(false, -1, '');
    };
    try {
        xhr.send(body);
    } catch (e) {
        callback(false, -2, String(e));
    }
}

function statusFromResponse(ok, code, resp, fallbackOk) {
    if (code === 0) return 'Trio unreachable';
    if (code === -1) return 'Trio timeout';
    if (code === -2) return 'HTTP error';
    try {
        var r = JSON.parse(resp || '{}');
        if (r.message) return String(r.message);
        if (r.error) return String(r.error);
        if (r.status) return String(r.status);
    } catch (e) { /* ignore */ }
    if (ok) return fallbackOk || 'Sent';
    return 'HTTP ' + code;
}

function sendCommand(type, amount) {
    normalizeTrioHost();
    type = type | 0;
    amount = amount | 0;

    var endpoint = type === 1 ? '/api/bolus' : '/api/carbs';
    var body =
        type === 1
            ? JSON.stringify({ units: amount / 10.0 })
            : JSON.stringify({ grams: amount, absorptionHours: 3 });

    var url = settings.trioHost + endpoint;
    console.log('[TrioRemote] POST ' + url + ' body=' + body);
    sendStatus(type === 1 ? 'Sending bolus…' : 'Sending carbs…');

    httpPost(url, body, function (ok, code, resp) {
        var statusMsg = statusFromResponse(ok, code, resp, type === 1 ? 'Bolus OK' : 'Carbs OK');
        if (statusMsg === 'delivered') {
            statusMsg = type === 1 ? 'Bolus delivered' : 'Carbs delivered';
        }
        sendStatus(statusMsg);
    });
}

Pebble.addEventListener('showConfiguration', function () {
    loadSettings();
    var current = JSON.stringify({
        trioHost: settings.trioHost,
        bolusStepTenths: settings.bolusStepTenths,
        bolusDefaultTenths: settings.bolusDefaultTenths,
        carbStepGrams: settings.carbStepGrams,
        carbDefaultGrams: settings.carbDefaultGrams
    });
    var html = getSettingsHtml();
    var urlString = 'data:text/html;charset=utf-8,' + html + '#' + encodeURIComponent(current);
    console.log('[TrioRemote] open remote config host=' + settings.trioHost + ' len=' + urlString.length);
    Pebble.openURL(urlString);
});

Pebble.addEventListener('webviewclosed', function (e) {
    if (!e || !e.response) return;
    try {
        var raw = e.response;
        // Some firmwares already decode; try both
        var newSettings;
        try {
            newSettings = JSON.parse(decodeURIComponent(raw));
        } catch (e1) {
            newSettings = JSON.parse(raw);
        }
        if (!newSettings || typeof newSettings !== 'object') return;
        // Empty object from Cancel
        if (newSettings.trioHost == null && newSettings.bolusStepTenths == null) {
            console.log('[TrioRemote] config cancelled');
            return;
        }
        if (newSettings.trioHost != null) settings.trioHost = newSettings.trioHost;
        if (newSettings.bolusStepTenths != null) settings.bolusStepTenths = newSettings.bolusStepTenths;
        if (newSettings.bolusDefaultTenths != null) settings.bolusDefaultTenths = newSettings.bolusDefaultTenths;
        if (newSettings.carbStepGrams != null) settings.carbStepGrams = newSettings.carbStepGrams;
        if (newSettings.carbDefaultGrams != null) settings.carbDefaultGrams = newSettings.carbDefaultGrams;
        saveSettings();
        pushPrefsToWatch();
        var hostShort = settings.trioHost.replace(/^https?:\/\//, '');
        sendStatus('Saved ' + hostShort);
        console.log('[TrioRemote] config saved ' + JSON.stringify(settings));
    } catch (ex) {
        console.log('[TrioRemote] config parse error: ' + ex);
    }
});

Pebble.addEventListener('appmessage', function (e) {
    var p = e.payload || {};
    var cmdType = payloadGet(p, K.CMD_TYPE);
    var cmdAmt = payloadGet(p, K.CMD_AMOUNT);
    if (cmdType !== undefined && cmdAmt !== undefined) {
        console.log('[TrioRemote] cmd type=' + cmdType + ' amt=' + cmdAmt);
        sendCommand(cmdType | 0, cmdAmt | 0);
    }
});

Pebble.addEventListener('ready', function () {
    console.log('[TrioRemote] pkjs ready');
    loadSettings();
    saveSettings();
    // Delay slightly so watch inbox is open
    setTimeout(pushPrefsToWatch, 400);
});
