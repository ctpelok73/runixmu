const fs = require('fs');
const path = require('path');
const express = require('express');
const { generateManifest, saveJson } = require('./updaterService');

const configPath = path.join(__dirname, 'config.json');
let config = JSON.parse(fs.readFileSync(configPath, 'utf8'));

const app = express();
app.use(express.urlencoded({ extended: true }));
app.use(express.json());

function requireAdmin(req, res, next) {
  const token = req.headers['x-admin-token'] || req.query.token;
  if (!token || token !== config.adminToken) {
    res.status(403).send('Forbidden');
    return;
  }
  next();
}

app.get('/admin/update', requireAdmin, (req, res) => {
  const html =
    '<html><head><meta charset="utf-8"><title>MU Updater Service</title></head>' +
    '<body style="font-family: sans-serif; padding: 20px;">' +
    '<h1>MU Updater Service</h1>' +
    '<form method="post" action="/admin/update/config?token=' + encodeURIComponent(config.adminToken) + '">' +
    '<div><label>Путь к клиенту на сервере:</label><br>' +
    '<input type="text" name="clientRoot" value="' + String(config.clientRoot || '') + '" style="width: 400px;"></div>' +
    '<div style="margin-top:10px;"><label>Текущая версия:</label><br>' +
    '<input type="text" name="version" value="' + String(config.version || '') + '" style="width: 200px;"></div>' +
    '<div style="margin-top:10px;"><button type="submit">Сохранить настройки</button></div>' +
    '</form>' +
    '<form method="post" action="/admin/update/generate?token=' + encodeURIComponent(config.adminToken) + '" style="margin-top:20px;">' +
    '<button type="submit">Сгенерировать manifest.json и version.json</button>' +
    '</form>' +
    '</body></html>';
  res.type('text/html').send(html);
});

app.post('/admin/update/config', requireAdmin, (req, res) => {
  config.clientRoot = req.body.clientRoot || config.clientRoot;
  config.version = req.body.version || config.version;
  fs.writeFileSync(configPath, JSON.stringify(config, null, 2), 'utf8');
  res.redirect('/admin/update?token=' + encodeURIComponent(config.adminToken));
});

app.post('/admin/update/generate', requireAdmin, (req, res) => {
  const root = config.clientRoot;
  const version = config.version;
  if (!root || !version) {
    res.status(400).send('clientRoot или version не заданы');
    return;
  }
  const manifest = generateManifest(root, version);
  const manifestPath = path.join(root, 'manifest.json');
  const versionPath = path.join(root, 'version.json');
  saveJson(manifestPath, manifest);
  saveJson(versionPath, { version });
  res.redirect('/admin/update?token=' + encodeURIComponent(config.adminToken));
});

app.get('/update/version', (req, res) => {
  const root = config.clientRoot;
  const versionFile = path.join(root, 'version.json');
  if (!fs.existsSync(versionFile)) {
    res.status(404).json({ error: 'version.json not found' });
    return;
  }
  const data = JSON.parse(fs.readFileSync(versionFile, 'utf8'));
  res.json(data);
});

app.get('/update/manifest', (req, res) => {
  const root = config.clientRoot;
  const manifestFile = path.join(root, 'manifest.json');
  if (!fs.existsSync(manifestFile)) {
    res.status(404).json({ error: 'manifest.json not found' });
    return;
  }
  const data = JSON.parse(fs.readFileSync(manifestFile, 'utf8'));
  res.json(data);
});

app.get('/update/files/*', (req, res) => {
  const root = config.clientRoot;
  const rel = req.params[0];
  const full = path.join(root, rel);
  if (!fs.existsSync(full)) {
    res.status(404).send('File not found');
    return;
  }
  res.sendFile(full);
});

app.listen(config.port, () => {
  console.log('MU Updater Service listening on port ' + config.port);
});

