#!/usr/bin/env node
/**
 * Hot reload dev server with WebSocket pub/sub
 * - Serves static files from build/
 * - WebSocket endpoint for live reload
 * - HTTP endpoint to trigger reload from build script
 */

const http = require('http');
const fs = require('fs');
const path = require('path');
const { WebSocketServer } = require('ws');

const PORT = 8080;
const BUILD_DIR = path.join(__dirname, 'build');

// Track connected clients
const clients = new Set();

// Broadcast message to all connected clients
function broadcast(type, message) {
    const data = JSON.stringify({ type, message, time: Date.now() });
    for (const client of clients) {
        if (client.readyState === 1) { // OPEN
            client.send(data);
        }
    }
    console.log(`[broadcast] ${type}: ${message || ''}`);
}

// MIME types
const mimeTypes = {
    '.html': 'text/html',
    '.js': 'application/javascript',
    '.wasm': 'application/wasm',
    '.css': 'text/css',
    '.png': 'image/png',
    '.jpg': 'image/jpeg',
};

// HTTP server for static files and reload trigger
const server = http.createServer((req, res) => {
    // POST /reload - trigger reload from build script
    if (req.method === 'POST' && req.url === '/reload') {
        broadcast('reload', 'Build complete');
        res.writeHead(200);
        res.end('OK');
        return;
    }

    // POST /building - notify build started
    if (req.method === 'POST' && req.url === '/building') {
        broadcast('building', 'Build started');
        res.writeHead(200);
        res.end('OK');
        return;
    }

    // POST /error - notify build error
    if (req.method === 'POST' && req.url === '/error') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', () => {
            broadcast('error', body || 'Build failed');
            res.writeHead(200);
            res.end('OK');
        });
        return;
    }

    // Serve static files
    let filePath = req.url === '/' ? '/preview.html' : req.url;
    filePath = path.join(BUILD_DIR, filePath);

    const ext = path.extname(filePath);
    const contentType = mimeTypes[ext] || 'application/octet-stream';

    fs.readFile(filePath, (err, data) => {
        if (err) {
            res.writeHead(404);
            res.end('Not found');
            return;
        }

        // Inject WebSocket client into HTML
        if (ext === '.html') {
            const wsClient = `
<script>
(function() {
    const ws = new WebSocket('ws://' + location.host);
    const overlay = document.createElement('div');
    overlay.id = 'dev-overlay';
    overlay.style.cssText = 'display:none;position:fixed;top:0;left:0;right:0;padding:8px;text-align:center;font:14px monospace;z-index:9999';
    document.body.appendChild(overlay);

    ws.onmessage = function(e) {
        const msg = JSON.parse(e.data);
        if (msg.type === 'reload') {
            overlay.style.display = 'block';
            overlay.style.background = '#a6e3a1';
            overlay.style.color = '#1e1e2e';
            overlay.textContent = 'Reloading...';
            setTimeout(() => location.reload(), 100);
        } else if (msg.type === 'building') {
            overlay.style.display = 'block';
            overlay.style.background = '#f9e2af';
            overlay.style.color = '#1e1e2e';
            overlay.textContent = 'Building...';
        } else if (msg.type === 'error') {
            overlay.style.display = 'block';
            overlay.style.background = '#f38ba8';
            overlay.style.color = '#1e1e2e';
            overlay.textContent = 'Build failed: ' + msg.message;
        }
    };
    ws.onclose = function() {
        overlay.style.display = 'block';
        overlay.style.background = '#6c7086';
        overlay.style.color = '#cdd6f4';
        overlay.textContent = 'Dev server disconnected';
    };
})();
</script>
`;
            data = data.toString().replace('</body>', wsClient + '</body>');
        }

        res.writeHead(200, { 'Content-Type': contentType });
        res.end(data);
    });
});

// WebSocket server
const wss = new WebSocketServer({ server });

wss.on('connection', (ws) => {
    clients.add(ws);
    console.log(`[ws] Client connected (${clients.size} total)`);

    ws.on('close', () => {
        clients.delete(ws);
        console.log(`[ws] Client disconnected (${clients.size} total)`);
    });
});

server.listen(PORT, () => {
    console.log(`Dev server running at http://localhost:${PORT}/preview.html`);
    console.log('WebSocket ready for live reload');
    console.log('');
    console.log('Trigger reload: curl -X POST http://localhost:8080/reload');
    console.log('');
});
