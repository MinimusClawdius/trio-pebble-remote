// Trio Remote — PebbleKit JS (message key indices must match package.json)
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
    TRIO_LINK: 44,
    SUGGESTED_BOLUS_TENTHS: 45,
    REMOTE_DEFAULT_BOLUS_TENTHS: 46,
    REMOTE_DEFAULT_CARB_G: 47,
    REMOTE_BOLUS_STEP_TENTHS: 48,
    REMOTE_CARB_STEP_G: 49
};

/** Only settings used by Trio Remote (not the full watchface config). */
var settings = {
    trioHost: 'http://127.0.0.1:8080',
    dataSource: 0,
    remoteDefaultBolusTenths: 20,
    remoteDefaultCarbG: 15,
    remoteBolusStepTenths: 1,
    remoteCarbStepG: 5
};

/**
 * Embedded settings HTML — Pebble WebView treats raw.githubusercontent.com as text/plain,
 * which shows source instead of a form. data: URLs render as HTML reliably.
 * (See also src/pkjs/remote_settings.html — keep in sync when editing fields.)
 */
var REMOTE_SETTINGS_PAGE = [
    '<!DOCTYPE html><html><head><meta charset="utf-8"/>',
    '<meta name="viewport" content="width=device-width, initial-scale=1"/>',
    '<title>Trio Remote</title><style>',
    'body{font-family:system-ui,sans-serif;margin:16px;max-width:420px}',
    'label{display:block;margin-top:12px;font-weight:600}',
    'input{width:100%;box-sizing:border-box;padding:8px;margin-top:4px;font-size:16px}',
    'p{color:#444;font-size:14px}',
    'button{margin-top:20px;padding:12px 20px;font-size:16px;width:100%}',
    '</style></head><body><h2>Trio Remote</h2>',
    '<p>Defaults when you open the bolus or carb picker, and how much each UP/DOWN press changes.</p>',
    '<label for="trioHost">Trio HTTP base</label>',
    '<input id="trioHost" type="text" placeholder="http://127.0.0.1:8080"/>',
    '<label for="defBolus">Default bolus (tenths of a unit)</label>',
    '<input id="defBolus" type="number" min="1" max="300" step="1"/>',
    '<p class="hint">Example: 25 = 2.5 U</p>',
    '<label for="defCarb">Default carbs (grams)</label>',
    '<input id="defCarb" type="number" min="1" max="250" step="1"/>',
    '<label for="stepBolus">Bolus step (tenths per press)</label>',
    '<input id="stepBolus" type="number" min="1" max="50" step="1"/>',
    '<p class="hint">Example: 1 = 0.1 U per press</p>',
    '<label for="stepCarb">Carb step (grams per press)</label>',
    '<input id="stepCarb" type="number" min="1" max="25" step="1"/>',
    '<button type="button" id="save">Save &amp; return</button><script>',
    'function parseHash(){try{var h=(location.hash||"").replace(/^#/,"");',
    'if(!h)return{};return JSON.parse(decodeURIComponent(h))||{};}catch(e){return{};}}',
    'var cfg=parseHash();',
    'document.getElementById("trioHost").value=cfg.trioHost||"http://127.0.0.1:8080";',
    'document.getElementById("defBolus").value=cfg.remoteDefaultBolusTenths!=null?cfg.remoteDefaultBolusTenths:20;',
    'document.getElementById("defCarb").value=cfg.remoteDefaultCarbG!=null?cfg.remoteDefaultCarbG:15;',
    'document.getElementById("stepBolus").value=cfg.remoteBolusStepTenths!=null?cfg.remoteBolusStepTenths:1;',
    'document.getElementById("stepCarb").value=cfg.remoteCarbStepG!=null?cfg.remoteCarbStepG:5;',
    'document.getElementById("save").onclick=function(){',
    'var out={trioHost:document.getElementById("trioHost").value.trim()||"http://127.0.0.1:8080",',
    'remoteDefaultBolusTenths:parseInt(document.getElementById("defBolus").value,10)||20,',
    'remoteDefaultCarbG:parseInt(document.getElementById("defCarb").value,10)||15,',
    'remoteBolusStepTenths:parseInt(document.getElementById("stepBolus").value,10)||1,',
    'remoteCarbStepG:parseInt(document.getElementById("stepCarb").value,10)||5};',
    'document.location="pebblejs://close#"+encodeURIComponent(JSON.stringify(out));};',
    '</script></body></html>'
].join('');

function loadSettings() {
    try {
        var saved = localStorage.getItem('trio_remote_settings');
        if (saved) {
            var parsed = JSON.parse(saved);
            for (var key in parsed) {
                if (parsed.hasOwnProperty(key) && settings.hasOwnProperty(key)) {
                    settings[key] = parsed[key];
                }
            }
        }
    } catch (e) {
        console.log('Trio Remote: settings load error ' + e);
    }
}

function saveSettings() {
    try {
        localStorage.setItem('trio_remote_settings', JSON.stringify(settings));
    } catch (e) { /* ok */ }
}

function pushDefaultsToWatch() {
    var m = {};
    m[K.REMOTE_DEFAULT_BOLUS_TENTHS] = settings.remoteDefaultBolusTenths | 0;
    m[K.REMOTE_DEFAULT_CARB_G] = settings.remoteDefaultCarbG | 0;
    m[K.REMOTE_BOLUS_STEP_TENTHS] = settings.remoteBolusStepTenths | 0;
    m[K.REMOTE_CARB_STEP_G] = settings.remoteCarbStepG | 0;
    Pebble.sendAppMessage(m, function () { }, function (e) {
        console.log('Trio Remote: send defaults failed ' + e);
    });
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
        callback(xhr.status >= 200 && xhr.status < 300 ? xhr.responseText : null);
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

Pebble.addEventListener('showConfiguration', function () {
    var params = encodeURIComponent(JSON.stringify(settings));
    Pebble.openURL('data:text/html;charset=utf-8,' + encodeURIComponent(REMOTE_SETTINGS_PAGE) + '#' + params);
});

Pebble.addEventListener('webviewclosed', function (e) {
    if (e && e.response) {
        try {
            var newSettings = JSON.parse(decodeURIComponent(e.response));
            for (var key in newSettings) {
                if (newSettings.hasOwnProperty(key) && settings.hasOwnProperty(key)) {
                    settings[key] = newSettings[key];
                }
            }
            saveSettings();
            pushDefaultsToWatch();
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
    pushDefaultsToWatch();
});
