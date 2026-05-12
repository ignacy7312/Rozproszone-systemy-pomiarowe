import { createServer } from 'node:http';
import { readFile, stat } from 'node:fs/promises';
import { createReadStream } from 'node:fs';
import { extname, join, normalize, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = fileURLToPath(new URL('.', import.meta.url));
const publicDir = resolve(__dirname, 'public');
const port = Number(process.env.PORT || 5173);
const apiTarget = new URL(process.env.API_TARGET || 'http://localhost:5001');

const mimeTypes = new Map([
  ['.html', 'text/html; charset=utf-8'],
  ['.js', 'text/javascript; charset=utf-8'],
  ['.css', 'text/css; charset=utf-8'],
  ['.json', 'application/json; charset=utf-8'],
  ['.svg', 'image/svg+xml'],
]);

function send(res, statusCode, body, headers = {}) {
  res.writeHead(statusCode, { 'content-type': 'text/plain; charset=utf-8', ...headers });
  res.end(body);
}

function safePublicPath(urlPath) {
  const decoded = decodeURIComponent(urlPath.split('?')[0]);
  const candidate = normalize(join(publicDir, decoded === '/' ? 'index.html' : decoded));
  if (!candidate.startsWith(publicDir)) return null;
  return candidate;
}

async function proxyApi(req, res) {
  const path = req.url.replace(/^\/api/, '') || '/';
  const target = new URL(path, apiTarget);

  try {
    const upstream = await fetch(target, {
      method: req.method,
      headers: { accept: req.headers.accept || 'application/json' },
    });

    const headers = Object.fromEntries(upstream.headers.entries());
    headers['access-control-allow-origin'] = '*';
    res.writeHead(upstream.status, headers);
    res.end(Buffer.from(await upstream.arrayBuffer()));
  } catch (error) {
    send(res, 502, JSON.stringify({ message: 'Proxy nie może połączyć się z API', details: String(error) }), {
      'content-type': 'application/json; charset=utf-8',
    });
  }
}

async function serveStatic(req, res) {
  const filePath = safePublicPath(req.url || '/');
  if (!filePath) return send(res, 403, 'Forbidden');

  try {
    const info = await stat(filePath);
    const finalPath = info.isDirectory() ? join(filePath, 'index.html') : filePath;
    const ext = extname(finalPath);
    res.writeHead(200, { 'content-type': mimeTypes.get(ext) || 'application/octet-stream' });
    createReadStream(finalPath).pipe(res);
  } catch {
    try {
      const fallback = await readFile(join(publicDir, 'index.html'));
      res.writeHead(200, { 'content-type': 'text/html; charset=utf-8' });
      res.end(fallback);
    } catch {
      send(res, 404, 'Not found');
    }
  }
}

createServer((req, res) => {
  if (req.url?.startsWith('/api')) {
    void proxyApi(req, res);
    return;
  }
  void serveStatic(req, res);
}).listen(port, () => {
  console.log(`WSYST Dashboard: http://localhost:${port}`);
  console.log(`Proxy /api -> ${apiTarget.href}`);
});
