const express = require('express');
const { WebSocketServer } = require('ws');
const mqtt = require('mqtt');
const fs = require('fs');
const os = require('os');
const path = require('path');
const http = require('http');
const https = require('https');

// ── Cấu hình ─────────────────────────────────────────────────
const MQTT_URL          = 'mqtt://192.168.88.83:1883';
const MQTT_USER         = 'mqttuser';
const MQTT_PASS         = 'mqttpass';
const TOPIC_CMD         = 'home/door/command';
const TOPIC_STATE       = 'home/door/state';
const CMD_UNLOCK        = 'UNLOCK';
const CMD_LOCK          = 'LOCK';
const TOPIC_LIGHT_CMD   = 'home/light/command';
const TOPIC_LIGHT_STATE = 'home/light/state';
const TOPIC_FP_ENROLL   = 'home/finger/enroll';
const TOPIC_FP_DELETE   = 'home/finger/delete';
const TOPIC_FP_STATUS   = 'home/finger/status';
const TOPIC_RFID_SCAN   = 'home/rfid/scan';
const TOPIC_RFID_CMD    = 'home/rfid/cmd';
const TOPIC_KP_CMD      = 'home/keypad/cmd';
const TOPIC_KP_STATUS   = 'home/keypad/status';
const TOPIC_GAS         = 'home/gas';
const TOPIC_FACE_RESULT = 'home/door/face_result';
const TOPIC_CAM_STATUS  = 'home/status/cam';
const TOPIC_CAM_CMD     = 'home/cam/cmd';

const WEB_PORT          = Number(process.env.WEB_PORT || 3000);
const HTTPS_PORT        = Number(process.env.HTTPS_PORT || 3443);
const CERT_DIR          = path.join(__dirname, 'certs');
const HTTPS_KEY_PATH    = process.env.HTTPS_KEY_PATH || path.join(CERT_DIR, 'lan-key.pem');
const HTTPS_CERT_PATH   = process.env.HTTPS_CERT_PATH || path.join(CERT_DIR, 'lan-cert.pem');

function getLanIpv4Addresses() {
  const nets = os.networkInterfaces();
  const ips = [];

  Object.values(nets).forEach(entries => {
    (entries || []).forEach(entry => {
      if (!entry || entry.internal || entry.family !== 'IPv4') return;
      if (entry.address.startsWith('169.254.')) return;
      ips.push(entry.address);
    });
  });

  return [...new Set(ips)];
}

function isLoopbackHost(hostname = '') {
  const host = hostname.replace(/^\[|\]$/g, '');
  return host === 'localhost' || host === '127.0.0.1' || host === '::1';
}

function formatHostForUrl(hostname = '') {
  if (hostname.includes(':') && !hostname.startsWith('[')) {
    return `[${hostname}]`;
  }
  return hostname;
}

function logUrls(label, protocol, port, ips) {
  console.log(`[WEB] ${label}: ${protocol}://localhost:${port}`);
  ips.forEach(ip => console.log(`[WEB] ${label}: ${protocol}://${ip}:${port}`));
}

const LAN_IPS = getLanIpv4Addresses();

// ── Express ───────────────────────────────────────────────────
const app = express();
let httpsEnabled = false;

app.use((req, res, next) => {
  if (!httpsEnabled || req.secure || isLoopbackHost(req.hostname)) {
    next();
    return;
  }

  const redirectHost = formatHostForUrl(req.hostname);
  res.redirect(307, `https://${redirectHost}:${HTTPS_PORT}${req.originalUrl}`);
});

app.use(express.static('public'));

// ── WebSocket (browser ↔ server) ──────────────────────────────
const socketServers = [];

function broadcast(data) {
  const msg = JSON.stringify(data);
  socketServers.forEach(wss => {
    wss.clients.forEach(client => {
      if (client.readyState === 1) client.send(msg);
    });
  });
}

// ── MQTT (server ↔ broker) ────────────────────────────────────
let doorState     = 'unknown';
let lightState    = 'unknown';
let fpStatus      = '';
let rfidLog       = [];
let kpStatus      = '';
let gasValue      = null;
let faceLog       = [];
let camStatus     = null;
let mqttConnected = false;

const mqttClient = mqtt.connect(MQTT_URL, {
  username: MQTT_USER,
  password: MQTT_PASS,
  clientId: `webapp_${Math.random().toString(16).slice(2, 8)}`,
  reconnectPeriod: 3000,
  connectTimeout: 5000,
});

mqttClient.on('connect', () => {
  mqttConnected = true;
  console.log('[MQTT] Kết nối broker OK');
  mqttClient.subscribe(TOPIC_STATE,       { qos: 1 });
  mqttClient.subscribe(TOPIC_LIGHT_STATE, { qos: 1 });
  mqttClient.subscribe(TOPIC_FP_STATUS,   { qos: 1 });
  mqttClient.subscribe(TOPIC_RFID_SCAN,   { qos: 1 });
  mqttClient.subscribe(TOPIC_KP_STATUS,   { qos: 1 });
  mqttClient.subscribe(TOPIC_GAS,         { qos: 0 });
  mqttClient.subscribe(TOPIC_FACE_RESULT, { qos: 1 });
  mqttClient.subscribe(TOPIC_CAM_STATUS,  { qos: 0 });
  broadcast({ type: 'mqtt', connected: true });
});

mqttClient.on('disconnect', () => {
  mqttConnected = false;
  broadcast({ type: 'mqtt', connected: false });
});

mqttClient.on('error', err => {
  console.error('[MQTT] Lỗi:', err.message);
  mqttConnected = false;
  broadcast({ type: 'mqtt', connected: false });
});

mqttClient.on('reconnect', () => {
  console.log('[MQTT] Đang kết nối lại...');
  broadcast({ type: 'mqtt', connected: false });
});

mqttClient.on('message', (topic, payload) => {
  const val = payload.toString();

  if (topic === TOPIC_STATE) {
    if (val === doorState) return;
    doorState = val;
    console.log(`[DOOR] Trạng thái: ${doorState}`);
    broadcast({ type: 'state', state: doorState });
  } else if (topic === TOPIC_LIGHT_STATE) {
    if (val === lightState) return;
    lightState = val;
    console.log(`[LIGHT] Trạng thái: ${lightState}`);
    broadcast({ type: 'light', state: lightState });
  } else if (topic === TOPIC_FP_STATUS) {
    fpStatus = val;
    console.log(`[FP] Status: ${fpStatus}`);
    broadcast({ type: 'fp', status: fpStatus });
  } else if (topic === TOPIC_GAS) {
    gasValue = parseInt(val, 10);
    broadcast({ type: 'gas', value: gasValue });
  } else if (topic === TOPIC_KP_STATUS) {
    kpStatus = val;
    console.log(`[KP] Status: ${kpStatus}`);
    broadcast({ type: 'kp', status: kpStatus });
  } else if (topic === TOPIC_RFID_SCAN) {
    const [status, uid] = val.split(':').length >= 2
      ? [val.substring(0, val.indexOf(':')), val.substring(val.indexOf(':') + 1)]
      : [val, ''];
    const entry = { status, uid, time: Date.now() };
    rfidLog.unshift(entry);
    if (rfidLog.length > 20) rfidLog.pop();
    console.log(`[RFID] ${status}: ${uid}`);
    broadcast({ type: 'rfid', entry });
  } else if (topic === TOPIC_FACE_RESULT) {
    try {
      const data = JSON.parse(val);
      const entry = { ...data, time: Date.now() };
      faceLog.unshift(entry);
      if (faceLog.length > 20) faceLog.pop();
      console.log(`[FACE] ${data.status}: ${data.name || ''} (score=${data.score})`);
      broadcast({ type: 'face', entry });
      if (data.status === 'MATCH') {
        mqttClient.publish(TOPIC_CMD, CMD_UNLOCK, { qos: 1 });
        console.log(`[FACE] MATCH "${data.name}" → MỞ CỬA`);
      }
    } catch {}
  } else if (topic === TOPIC_CAM_STATUS) {
    try {
      camStatus = JSON.parse(val);
      broadcast({ type: 'cam_status', status: camStatus });
    } catch {}
  }
});

function handleSocketConnection(ws) {
  ws.send(JSON.stringify({ type: 'state', state: doorState }));
  ws.send(JSON.stringify({ type: 'light', state: lightState }));
  ws.send(JSON.stringify({ type: 'mqtt', connected: mqttConnected }));
  if (fpStatus)             ws.send(JSON.stringify({ type: 'fp', status: fpStatus }));
  if (kpStatus)             ws.send(JSON.stringify({ type: 'kp', status: kpStatus }));
  if (gasValue !== null)    ws.send(JSON.stringify({ type: 'gas', value: gasValue }));
  if (rfidLog.length)       ws.send(JSON.stringify({ type: 'rfid_history', log: rfidLog }));
  if (faceLog.length)       ws.send(JSON.stringify({ type: 'face_history', log: faceLog }));
  if (camStatus)            ws.send(JSON.stringify({ type: 'cam_status', status: camStatus }));

  ws.on('message', raw => {
    let msg;
    try { msg = JSON.parse(raw); } catch { return; }

    if (msg.cmd === 'unlock') {
      mqttClient.publish(TOPIC_CMD, CMD_UNLOCK, { qos: 1 });
      console.log('[WEB] Lệnh: MỞ CỬA');
    } else if (msg.cmd === 'lock') {
      mqttClient.publish(TOPIC_CMD, CMD_LOCK, { qos: 1 });
      console.log('[WEB] Lệnh: KHÓA CỬA');
    } else if (msg.cmd === 'light_on') {
      mqttClient.publish(TOPIC_LIGHT_CMD, 'ON', { qos: 1 });
      console.log('[WEB] Lệnh: BẬT ĐÈN');
    } else if (msg.cmd === 'light_off') {
      mqttClient.publish(TOPIC_LIGHT_CMD, 'OFF', { qos: 1 });
      console.log('[WEB] Lệnh: TẮT ĐÈN');
    } else if (msg.cmd === 'kp_setpass' && msg.pass) {
      if (msg.pass.length >= 4 && msg.pass.length <= 8) {
        mqttClient.publish(TOPIC_KP_CMD, `setpass:${msg.pass}`, { qos: 1 });
        console.log('[WEB] Lệnh: ĐỔI MẬT KHẨU BÀN PHÍM');
      }
    } else if (msg.cmd === 'rfid_add_next') {
      mqttClient.publish(TOPIC_RFID_CMD, 'add_next', { qos: 1 });
      console.log('[WEB] Lệnh: THÊM THẺ RFID MỚI');
    } else if (msg.cmd === 'rfid_remove' && msg.uid) {
      mqttClient.publish(TOPIC_RFID_CMD, `remove:${msg.uid}`, { qos: 1 });
      console.log(`[WEB] Lệnh: XÓA THẺ ${msg.uid}`);
    } else if (msg.cmd === 'fp_enroll' && msg.id >= 1 && msg.id <= 127) {
      mqttClient.publish(TOPIC_FP_ENROLL, String(msg.id), { qos: 1 });
      console.log(`[WEB] Lệnh: ĐĂNG KÝ VÂN TAY ID #${msg.id}`);
    } else if (msg.cmd === 'fp_delete' && msg.id >= 1 && msg.id <= 127) {
      mqttClient.publish(TOPIC_FP_DELETE, String(msg.id), { qos: 1 });
      console.log(`[WEB] Lệnh: XÓA VÂN TAY ID #${msg.id}`);
    } else if (msg.cmd === 'cam_cmd' && msg.payload) {
      mqttClient.publish(TOPIC_CAM_CMD, JSON.stringify(msg.payload), { qos: 1 });
      console.log(`[WEB] Lệnh CAM: ${JSON.stringify(msg.payload)}`);
    }
  });
}

function attachSocketServer(server) {
  const wss = new WebSocketServer({ server });
  wss.on('connection', handleSocketConnection);
  socketServers.push(wss);
  return wss;
}

// ── HTTP / HTTPS Servers ─────────────────────────────────────
const httpServer = http.createServer(app);
attachSocketServer(httpServer);

let httpsServer = null;

if (fs.existsSync(HTTPS_KEY_PATH) && fs.existsSync(HTTPS_CERT_PATH)) {
  try {
    httpsServer = https.createServer({
      key: fs.readFileSync(HTTPS_KEY_PATH),
      cert: fs.readFileSync(HTTPS_CERT_PATH),
    }, app);
    attachSocketServer(httpsServer);
    httpsEnabled = true;
  } catch (err) {
    console.error(`[HTTPS] Không đọc được cert: ${err.message}`);
  }
}

// ── Khởi động ─────────────────────────────────────────────────
httpServer.listen(WEB_PORT, () => {
  logUrls('HTTP', 'http', WEB_PORT, LAN_IPS);

  if (httpsEnabled) {
    console.log('[VOICE] Truy cập từ thiết bị khác trong LAN sẽ tự chuyển sang HTTPS.');
  } else {
    console.log(`[HTTPS] Chưa tìm thấy cert tại ${HTTPS_KEY_PATH} và ${HTTPS_CERT_PATH}`);
    console.log('[HTTPS] Để mic hoạt động trên thiết bị khác trong LAN, chạy: npm run gen-cert');
  }
});

if (httpsServer) {
  httpsServer.listen(HTTPS_PORT, () => {
    logUrls('HTTPS', 'https', HTTPS_PORT, LAN_IPS);
    console.log('[VOICE] Thiết bị khác trong LAN nên mở bằng URL HTTPS ở trên.');
  });
}
