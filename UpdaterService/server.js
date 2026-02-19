const fs = require('fs');
const path = require('path');
const express = require('express');
const multer = require('multer');
const { generateManifest, saveJson } = require('./updaterService');

const configPath = path.join(__dirname, 'config.json');
let config = JSON.parse(fs.readFileSync(configPath, 'utf8'));

function sanitizeRelativePath(rel) {
  if (!rel) {
    return '';
  }
  let s = String(rel).replace(/\\/g, '/');
  const parts = s.split('/').filter((p) => p && p !== '.' && p !== '..');
  return parts.join('/');
}

const storage = multer.diskStorage({
  destination: (req, file, cb) => {
    const relDir = sanitizeRelativePath(req.body.targetPath || '');
    const root = config.clientRoot || '';
    const dest = relDir ? path.join(root, relDir) : root;
    if (!dest) {
      return cb(new Error('clientRoot is not configured'), '');
    }
    fs.mkdirSync(dest, { recursive: true });
    cb(null, dest);
  },
  filename: (req, file, cb) => {
    cb(null, file.originalname);
  }
});

const upload = multer({ storage });

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
  let filesHtml = '';
  const root = config.clientRoot;
  if (root && fs.existsSync(root)) {
    const manifestPath = path.join(root, 'manifest.json');
    if (fs.existsSync(manifestPath)) {
      try {
        const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
        const files = Array.isArray(manifest.files) ? manifest.files : [];
        filesHtml += '<h2 style="margin-top:30px;">Файлы из manifest.json</h2>';
        filesHtml += '<div style="max-height:300px; overflow:auto; border:1px solid #ccc; padding:5px;">';
        filesHtml += '<table cellpadding="4" cellspacing="0" style="border-collapse:collapse; width:100%;">';
        filesHtml += '<tr style="background:#eee;"><th style="text-align:left;">Путь</th><th style="text-align:right;">Размер, байт</th></tr>';
        for (const item of files) {
          const p = String(item.path || '');
          const size = typeof item.size === 'number' ? item.size : 0;
          filesHtml += '<tr><td>' + p.replace(/</g, '&lt;').replace(/>/g, '&gt;') + '</td><td style="text-align:right;">' + size + '</td></tr>';
        }
        filesHtml += '</table></div>';
      } catch (e) {
        filesHtml += '<p style="color:red;margin-top:20px;">Ошибка чтения manifest.json: ' + String(e.message || e) + '</p>';
      }
    } else {
      filesHtml += '<p style="margin-top:20px;">manifest.json ещё не создан. Сначала сгенерируйте его.</p>';
    }
  } else {
    filesHtml += '<p style="margin-top:20px;">clientRoot не задан или путь недоступен.</p>';
  }

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
    '<h2 style="margin-top:30px;">Загрузить файл в клиент</h2>' +
    '<form method="post" enctype="multipart/form-data" action="/admin/files/upload?token=' + encodeURIComponent(config.adminToken) + '">' +
    '<div><label>Относительный путь внутри clientRoot (например, Data или Data/Config):</label><br>' +
    '<input type="text" name="targetPath" value="" style="width: 400px;"></div>' +
    '<div style="margin-top:10px;"><input type="file" name="file"></div>' +
    '<div style="margin-top:10px;"><button type="submit">Загрузить файл</button></div>' +
    '</form>' +
    '<div id="dropzone" style="margin-top:20px; padding:20px; border:2px dashed #888; text-align:center; color:#555;">' +
    'Перетащите сюда файлы для загрузки в clientRoot' +
    '</div>' +
    filesHtml +
    '<script>' +
    '(function(){' +
    'var dz=document.getElementById("dropzone");' +
    'if(!dz)return;' +
    'function stop(e){e.preventDefault();e.stopPropagation();}' +
    'dz.addEventListener("dragenter",stop);' +
    'dz.addEventListener("dragover",function(e){stop(e);dz.style.background="#eef";});' +
    'dz.addEventListener("dragleave",function(e){stop(e);dz.style.background="";});' +
    'dz.addEventListener("drop",function(e){' +
    'stop(e);dz.style.background="";' +
    'var files=e.dataTransfer.files;' +
    'if(!files||files.length===0)return;' +
    'var target=document.querySelector("input[name=targetPath]");' +
    'var targetPath=target?target.value:"";' +
    'var token="' + encodeURIComponent(config.adminToken) + '";' +
    'var remaining=files.length;' +
    'for(var i=0;i<files.length;i++){' +
    'var f=files[i];' +
    'var fd=new FormData();' +
    'fd.append("targetPath",targetPath);' +
    'fd.append("file",f,f.name);' +
    'var xhr=new XMLHttpRequest();' +
    'xhr.open("POST","/admin/files/upload?token="+token,true);' +
    'xhr.onreadystatechange=function(){' +
    'if(xhr.readyState===4){remaining--;if(remaining<=0){window.location.reload();}}' +
    '};' +
    'xhr.send(fd);' +
    '}' +
    '});' +
    '})();' +
    '</script>' +
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

app.post('/admin/files/upload', requireAdmin, upload.single('file'), (req, res) => {
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

app.get('/update/files/*splat', (req, res) => {
  const root = config.clientRoot;
  const splat = req.params.splat;
  const rel = Array.isArray(splat) ? splat.join('/') : String(splat || '');
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
