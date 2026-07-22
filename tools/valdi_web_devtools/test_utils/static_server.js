const path = require('path');
const fs = require('fs');
const http = require('http');

const CONTENT_TYPES = {
  '.html': 'text/html',
  '.js': 'application/javascript',
  '.css': 'text/css',
  '.json': 'application/json',
  '.map': 'application/json',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.gif': 'image/gif',
  '.svg': 'image/svg+xml',
  '.woff': 'font/woff',
  '.woff2': 'font/woff2',
  '.ttf': 'font/ttf',
  '.eot': 'application/vnd.ms-fontobject',
};

function createStaticServer(config) {
  const { staticDirs, spaRoutes = [] } = config;

  function findFile(urlPath) {
    const filename = urlPath.substring(1);
    for (const dir of staticDirs) {
      const filePath = path.join(dir, filename);
      if (fs.existsSync(filePath)) return filePath;
    }
    return null;
  }

  const server = http.createServer((req, res) => {
    let urlPath = req.url.split('?')[0];

    if (urlPath === '/' || (!urlPath.includes('.') && spaRoutes.some(r => urlPath === r || urlPath.startsWith(r + '/')))) {
      urlPath = '/index.html';
    }

    const filePath = findFile(urlPath);

    if (!filePath) {
      res.writeHead(404);
      return res.end('Not Found');
    }

    fs.readFile(filePath, (err, data) => {
      if (err) {
        res.writeHead(404);
        return res.end('Not Found');
      }
      const contentType = CONTENT_TYPES[path.extname(filePath)] || 'application/octet-stream';
      res.writeHead(200, { 'Content-Type': contentType, 'Cache-Control': 'no-store' });
      res.end(data);
    });
  });

  return server;
}

function startServer(config, port = 0) {
  return new Promise((resolve, reject) => {
    const server = createStaticServer(config);
    server.listen(port, err => {
      if (err) reject(err);
      else resolve(server);
    });
  });
}

module.exports = { createStaticServer, startServer };
