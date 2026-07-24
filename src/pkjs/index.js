// Trio Remote — PebbleKit JS: watch AppMessage → HTTP POST Trio loopback
// Keys must match package.json messageKeys / watchface.
var K = {
    CMD_TYPE: 7,
    CMD_AMOUNT: 8,
    CMD_STATUS: 9
};

var DEFAULT_TRIO_HOST = 'http://127.0.0.1:8080';

var settings = {
    // Remote always talks to Trio local API (ignore legacy Dexcom/NS sources).
    dataSource: 0,
    trioHost: DEFAULT_TRIO_HOST
};

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

function loadSettings() {
    try {
        var saved = localStorage.getItem('trio_remote_settings');
        if (!saved) {
            // Migrate older key if present
            saved = localStorage.getItem('trio_settings');
        }
        if (saved) {
            var parsed = JSON.parse(saved);
            if (parsed.trioHost != null) settings.trioHost = parsed.trioHost;
            if (parsed.dataSource != null) settings.dataSource = parsed.dataSource;
        }
    } catch (e) {
        console.log('[TrioRemote] settings load error ' + e);
    }
    normalizeTrioHost();
    // Companion app is Trio-only for commands
    settings.dataSource = 0;
}

function saveSettings() {
    try {
        localStorage.setItem(
            'trio_remote_settings',
            JSON.stringify({ trioHost: settings.trioHost, dataSource: 0 })
        );
    } catch (e) { /* ok */ }
}

function payloadGet(p, keyNum) {
    if (!p) return undefined;
    var k = keyNum | 0;
    if (p[k] !== undefined && p[k] !== null) return p[k];
    if (p[String(k)] !== undefined && p[String(k)] !== null) return p[String(k)];
    // Named keys if platform expands messageKeys
    if (keyNum === K.CMD_TYPE && p.KEY_CMD_TYPE != null) return p.KEY_CMD_TYPE;
    if (keyNum === K.CMD_AMOUNT && p.KEY_CMD_AMOUNT != null) return p.KEY_CMD_AMOUNT;
    return undefined;
}

function sendStatus(text) {
    var msg = {};
    msg[K.CMD_STATUS] = String(text || '').substring(0, 63);
    Pebble.sendAppMessage(
        msg,
        function () {
            console.log('[TrioRemote] status → watch: ' + msg[K.CMD_STATUS]);
        },
        function (e) {
            console.log('[TrioRemote] status send failed: ' + JSON.stringify(e));
        }
    );
}

/**
 * POST JSON; callback(ok, statusCode, responseText).
 * Non-2xx still returns body so the watch can show Trio error messages.
 */
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
        console.log('[TrioRemote] xhr send exception ' + e);
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
        console.log(
            '[TrioRemote] POST result ok=' +
                ok +
                ' code=' +
                code +
                ' body=' +
                String(resp).substring(0, 120)
        );
        var statusMsg = statusFromResponse(ok, code, resp, type === 1 ? 'Bolus OK' : 'Carbs OK');
        // Map delivered → clearer watch text
        if (statusMsg === 'delivered') {
            statusMsg = type === 1 ? 'Bolus delivered' : 'Carbs delivered';
        }
        sendStatus(statusMsg);
    });
}

var TRIO_CONFIG_PAGE_URL =
    'https://minimusclawdius.github.io/trio-pebble/config/index.html';

Pebble.addEventListener('showConfiguration', function () {
    loadSettings();
    var params = encodeURIComponent(JSON.stringify({
        trioHost: settings.trioHost,
        dataSource: 0
    }));
    console.log('[TrioRemote] open config host=' + settings.trioHost);
    Pebble.openURL(TRIO_CONFIG_PAGE_URL + '#' + params);
});

Pebble.addEventListener('webviewclosed', function (e) {
    if (e && e.response) {
        try {
            var newSettings = JSON.parse(decodeURIComponent(e.response));
            if (newSettings.trioHost != null) settings.trioHost = newSettings.trioHost;
            normalizeTrioHost();
            settings.dataSource = 0;
            saveSettings();
            console.log('[TrioRemote] config saved host=' + settings.trioHost);
            sendStatus('Host ' + settings.trioHost.replace('http://', ''));
        } catch (ex) {
            console.log('[TrioRemote] config parse error: ' + ex);
        }
    }
});

Pebble.addEventListener('appmessage', function (e) {
    var p = e.payload || {};
    console.log('[TrioRemote] appmessage keys=' + JSON.stringify(p));
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
});
