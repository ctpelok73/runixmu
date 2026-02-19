const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

function walkDir(rootDir) {
  const result = [];
  const stack = [rootDir];
  while (stack.length > 0) {
    const current = stack.pop();
    const entries = fs.readdirSync(current, { withFileTypes: true });
    for (const entry of entries) {
      const fullPath = path.join(current, entry.name);
      if (entry.isDirectory()) {
        stack.push(fullPath);
      } else if (entry.isFile()) {
        result.push(fullPath);
      }
    }
  }
  return result;
}

function relPath(fullPath, rootDir) {
  let rel = path.relative(rootDir, fullPath);
  rel = rel.split(path.sep).join('/');
  return rel;
}

function fileHash(fullPath) {
  const hash = crypto.createHash('md5');
  const data = fs.readFileSync(fullPath);
  hash.update(data);
  return hash.digest('hex');
}

function generateManifest(clientRoot, version) {
  const root = path.resolve(clientRoot);
  const files = walkDir(root);
  const items = [];
  for (const f of files) {
    const r = relPath(f, root);
    if (r === 'version.json' || r === 'manifest.json') {
      continue;
    }
    const stat = fs.statSync(f);
    items.push({
      path: r,
      size: stat.size,
      md5: fileHash(f)
    });
  }
  return {
    version,
    generatedAt: new Date().toISOString(),
    files: items
  };
}

function saveJson(filePath, obj) {
  const dir = path.dirname(filePath);
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
  fs.writeFileSync(filePath, JSON.stringify(obj, null, 2), 'utf8');
}

module.exports = {
  generateManifest,
  saveJson
};

