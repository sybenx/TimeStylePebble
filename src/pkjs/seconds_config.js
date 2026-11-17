// A small, self-contained config page for the "Seconds" settings. The main
// TimeStyle config page is hosted externally and doesn't know about these
// settings, so this page is served as a data: URI and links out to it.
// The watch side is src/c/seconds.c; app.js only has one-line hooks into
// this file, to keep the daily upstream merge free of conflicts.

var STORAGE_KEY = 'seconds_settings_v2';

// defaults (the "Classic" preset) must match the ones in src/c/seconds.c
var DEFAULTS = {
  seconds_mode: 2,          // 0 always, 1 hourly, 2 half-hourly, 3 quarter-hourly, 4 never
  active_hours: 0,          // 0 all day, 1 daytime (07:00-22:00), 2 custom
  active_start: 7 * 60,     // custom window, minutes since midnight
  active_end: 22 * 60,
  shake_on: 0,
  shake_duration: 30,
  seconds_battery_min: 0,
  seconds_quiet_hide: 1
};

function load() {
  var saved = {};
  try {
    saved = JSON.parse(window.localStorage.getItem(STORAGE_KEY)) || {};
  } catch (err) {}

  var result = {};
  for (var k in DEFAULTS) {
    result[k] = (saved[k] !== undefined) ? saved[k] : DEFAULTS[k];
  }
  return result;
}

function save(data) {
  var current = load();
  for (var k in DEFAULTS) {
    if (data[k] !== undefined) {
      current[k] = parseInt(data[k], 10);
    }
  }
  window.localStorage.setItem(STORAGE_KEY, JSON.stringify(current));
  return current;
}

var PAGE =
  '<!DOCTYPE html><html><head><meta charset="utf-8">' +
  '<meta name="viewport" content="width=device-width, initial-scale=1">' +
  '<title>Seconds</title><style>' +
  'body{font-family:sans-serif;margin:0;background:#222;color:#eee}' +
  'h1{font-size:18px;margin:0;padding:14px 16px;background:#111;text-transform:uppercase;letter-spacing:1px}' +
  'h2{font-size:13px;margin:0;padding:10px 16px;background:#2d2d2d;color:#ff9955;text-transform:uppercase;letter-spacing:1px}' +
  '.item{padding:12px 16px;border-bottom:1px solid #333}' +
  '.item>label{display:block;font-size:16px;margin-bottom:6px}' +
  '.item p{font-size:12px;color:#aaa;margin:6px 0 0}' +
  'select,input[type=time]{width:100%;font-size:16px;padding:8px;background:#333;color:#eee;border:1px solid #555;border-radius:4px;box-sizing:border-box}' +
  '.row{display:flex;gap:12px}.row>div{flex:1}.row label{font-size:13px;color:#aaa}' +
  'input[type=checkbox]{width:22px;height:22px;vertical-align:middle;margin-right:8px}' +
  '.hidden{display:none}' +
  '.btn{display:block;margin:16px;padding:12px;text-align:center;font-size:16px;border-radius:4px;border:0;width:calc(100% - 32px);color:#fff;background:#ff5500;text-decoration:none;box-sizing:border-box}' +
  '.btn.alt{background:#444}' +
  '</style></head><body><h1>Seconds</h1>' +

  '<div class="item"><label for="preset">Preset</label><select id="preset">' +
  '<option value="classic">Classic</option><option value="balanced">Balanced</option>' +
  '<option value="always">Always on</option><option value="saver">Battery saver</option>' +
  '<option value="custom">Custom</option></select>' +
  '<p>Classic: half-hourly. Balanced: hourly in the daytime, shake for more. ' +
  'Always on: every second, all the time. Battery saver: shake only. ' +
  'Changing any setting below switches this to Custom.</p></div>' +

  '<h2>When to show</h2>' +
  '<div class="item"><label for="mode">Show seconds</label><select id="mode">' +
  '<option value="0">Always</option>' +
  '<option value="1">Hourly (last minute of each hour)</option>' +
  '<option value="2">Half-hourly (:29 and :59)</option>' +
  '<option value="3">Quarter-hourly (:14, :29, :44, :59)</option>' +
  '<option value="4">Never</option></select>' +
  '<p>Seconds use battery because the screen updates every second while they show. ' +
  'The watch only wakes once a minute the rest of the time.</p></div>' +

  '<div class="item"><label for="hours">Active hours</label><select id="hours">' +
  '<option value="0">All day</option>' +
  '<option value="1">Daytime (7:00 - 22:00)</option>' +
  '<option value="2">Custom</option></select>' +
  '<p>Outside these hours, seconds are hidden.</p>' +
  '<div id="custom" class="row hidden" style="margin-top:10px">' +
  '<div><label for="start">From</label><input type="time" id="start"></div>' +
  '<div><label for="end">Until</label><input type="time" id="end"></div></div></div>' +

  '<h2>Save battery</h2>' +
  '<div class="item"><label for="batt">Battery saver</label><select id="batt">' +
  '<option value="0">Off</option><option value="10">Hide at 10% or below</option>' +
  '<option value="20">Hide at 20% or below</option><option value="30">Hide at 30% or below</option></select>' +
  '<p>Hides seconds when the battery is low. Ignored while charging.</p></div>' +

  '<div class="item"><label><input type="checkbox" id="quiet">Hide during Quiet Time</label>' +
  '<p>Hides seconds while your watch is in Quiet Time (e.g. at night).</p></div>' +

  '<h2>Shake to show</h2>' +
  '<div class="item"><label><input type="checkbox" id="shake">Shake to show seconds</label>' +
  '<p>Flick your wrist to show seconds for a little while. A shake always works, ' +
  'even outside active hours, on low battery, or in Quiet Time.</p></div>' +
  '<div class="item" id="shakeDurItem"><label for="dur">Shake duration</label><select id="dur">' +
  '<option value="15">15 seconds</option><option value="30">30 seconds</option>' +
  '<option value="60">60 seconds</option><option value="120">2 minutes</option></select></div>' +

  '<button class="btn" id="save">Save</button>' +
  '<a class="btn alt" href="__MAIN_URL__">Open general TimeStyle settings</a>' +

  '<script>' +
  'var S=__STATE__;' +
  'var PRESETS={' +
  'classic:{mode:2,hours:0,shake:0,batt:0,quiet:1},' +
  'balanced:{mode:1,hours:1,shake:1,batt:20,quiet:1},' +
  'always:{mode:0,hours:0,shake:0,batt:0,quiet:0},' +
  'saver:{mode:4,hours:0,shake:1,batt:30,quiet:1}};' +
  'function $(i){return document.getElementById(i);}' +
  'function toTime(m){return ("0"+Math.floor(m/60)).slice(-2)+":"+("0"+m%60).slice(-2);}' +
  'function fromTime(t){var p=(t||"0:0").split(":");return (+p[0])*60+(+p[1]);}' +
  'function read(){return {mode:+$("mode").value,hours:+$("hours").value,shake:$("shake").checked?1:0,' +
  'batt:+$("batt").value,quiet:$("quiet").checked?1:0,dur:+$("dur").value,' +
  'start:fromTime($("start").value),end:fromTime($("end").value)};}' +
  'function matches(v,p){return v.mode==p.mode&&v.hours==p.hours&&v.shake==p.shake&&' +
  'v.batt==p.batt&&v.quiet==p.quiet&&(!p.shake||v.dur==30);}' +
  'function refresh(){var v=read(),name="custom";' +
  'for(var k in PRESETS){if(matches(v,PRESETS[k])){name=k;break;}}' +
  '$("preset").value=name;' +
  '$("custom").className="row"+(v.hours==2?"":" hidden");' +
  '$("shakeDurItem").className="item"+(v.shake?"":" hidden");}' +
  'function apply(p){$("mode").value=p.mode;$("hours").value=p.hours;$("shake").checked=!!p.shake;' +
  '$("batt").value=p.batt;$("quiet").checked=!!p.quiet;if(p.shake)$("dur").value=30;refresh();}' +
  '$("mode").value=S.seconds_mode;$("hours").value=S.active_hours;' +
  '$("start").value=toTime(S.active_start);$("end").value=toTime(S.active_end);' +
  '$("shake").checked=!!S.shake_on;$("dur").value=S.shake_duration;' +
  '$("batt").value=S.seconds_battery_min;$("quiet").checked=!!S.seconds_quiet_hide;' +
  'var ids=["mode","hours","start","end","shake","dur","batt","quiet"];' +
  'for(var i=0;i<ids.length;i++){$(ids[i]).onchange=refresh;}' +
  '$("preset").onchange=function(){var p=PRESETS[this.value];if(p)apply(p);};' +
  'refresh();' +
  '$("save").onclick=function(){var v=read();' +
  'var d={seconds_mode:v.mode,active_hours:v.hours,active_start:v.start,active_end:v.end,' +
  'shake_on:v.shake,shake_duration:v.dur,seconds_battery_min:v.batt,seconds_quiet_hide:v.quiet};' +
  'document.location="pebblejs://close#"+encodeURIComponent(JSON.stringify(d));};' +
  '</script></body></html>';

// mainConfigUrl: link to the regular TimeStyle settings page
function buildUrl(mainConfigUrl) {
  var html = PAGE
    .replace('__STATE__', function () { return JSON.stringify(load()); })
    .replace('__MAIN_URL__', function () { return mainConfigUrl; });

  return 'data:text/html;charset=utf-8,' + encodeURIComponent(html);
}

// takes the page's response and returns an app message dictionary
function toDict(data) {
  var s = save(data);
  return {
    SettingSecondsMode: s.seconds_mode,
    SettingActiveHoursMode: s.active_hours,
    SettingActiveStartMin: s.active_start,
    SettingActiveEndMin: s.active_end,
    SettingShakeToShow: s.shake_on,
    SettingShakeDuration: s.shake_duration,
    SettingSecondsBatteryMin: s.seconds_battery_min,
    SettingSecondsQuietHide: s.seconds_quiet_hide
  };
}

// true if the response came from this page (as opposed to the main one)
function isSecondsResponse(data) {
  return !!data && data.seconds_mode !== undefined;
}

// if the response came from this page, sends its settings to the watch and
// returns true; the main page's responses are left to the caller
function handleResponse(data) {
  if (!isSecondsResponse(data)) {
    return false;
  }

  var dict = toDict(data);
  console.log('Preparing seconds message: ', JSON.stringify(dict));
  Pebble.sendAppMessage(dict, function () {
    console.log('Sent seconds settings to Pebble');
  }, function () {
    console.log('Failed to send seconds settings!');
  });
  return true;
}

module.exports = {
  buildUrl: buildUrl,
  toDict: toDict,
  isSecondsResponse: isSecondsResponse,
  handleResponse: handleResponse
};
