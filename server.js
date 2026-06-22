const http = require('http');
const fs = require('fs');
const path = require('path');
const PORT = 7000;

const ALLOWED_HOSTS = ['tuxblox.net', 'www.tuxblox.net'];

const MIME_TYPES = {
    '.html': 'text/html; charset=utf-8',
    '.css': 'text/css; charset=utf-8',
    '.js': 'application/javascript; charset=utf-8',
    '.png': 'image/png',
    '.jpg': 'image/jpeg',
    '.jpeg': 'image/jpeg',
    '.gif': 'image/gif',
    '.svg': 'image/svg+xml',
    '.ico': 'image/x-icon',
    '.sh': 'text/plain; charset=utf-8',
    '.txt': 'text/plain; charset=utf-8',
    '.sha256': 'text/plain; charset=utf-8',
    '.json': 'application/json; charset=utf-8',
};

let redirectRules = [];
try {
    const redirectsPath = path.join(__dirname, 'config', 'redirect.json');
    if (fs.existsSync(redirectsPath)) {
        const rawData = fs.readFileSync(redirectsPath, 'utf8');
        const jsonRules = JSON.parse(rawData);
        
        redirectRules = Object.entries(jsonRules).map(([key, value]) => {
            const hasWildcardKey = key.endsWith('*');
            const hasWildcardValue = value.endsWith('*');
            
            const matchPrefix = hasWildcardKey ? key.slice(0, -1) : key;
            const targetPrefix = hasWildcardValue ? value.slice(0, -1) : value;

            return {
                key,
                value,
                hasWildcardKey,
                hasWildcardValue,
                matchPrefix,
                targetPrefix
            };
        });
        console.log(`Loaded ${redirectRules.length} redirect rules from redirects.json`);
    }
} catch (err) {
    console.error('Error reading or parsing redirects.json:', err.message);
}

function logTraffic(req, statusCode) {
    const ip = req.headers['x-forwarded-for'] || req.socket.remoteAddress || 'Unknown IP';
    const userAgent = req.headers['user-agent'] || 'Unknown Agent';
    const requestTarget = `${req.method} ${req.url}`;
    console.log(`[${ip}] [${userAgent}] ${requestTarget}: ${statusCode}`);
}

function serveFileIfExists(filePath, defaultMime, res, req, callback) {
    fs.stat(filePath, (err, stats) => {
        if (!err && stats.isFile()) {
            const ext = path.extname(filePath).toLowerCase();
            const contentType = MIME_TYPES[ext] || defaultMime;
            res.writeHead(200, {
                'Content-Type': contentType,
                'X-Content-Type-Options': 'nosniff'
            });
            fs.createReadStream(filePath).pipe(res);
            logTraffic(req, 200);
            if (callback) callback(true);
        } else {
            if (callback) callback(false);
        }
    });
}

const server = http.createServer((req, res) => {
    const hostHeader = (req.headers.host || '').split(':')[0].toLowerCase();

    if (!ALLOWED_HOSTS.includes(hostHeader)) {
        res.writeHead(403, { 'Content-Type': 'text/plain' });
        res.end('403 Forbidden');
        logTraffic(req, 403);
        return;
    }

    const rawUrl = req.url;
    const [urlPath, queryString] = rawUrl.split('?');
    const suffix = queryString ? `?${queryString}` : '';

    for (const rule of redirectRules) {
        if (rule.hasWildcardKey) {
            const strictPrefix = rule.matchPrefix; 
            const cleanPrefix = strictPrefix.endsWith('/') ? strictPrefix.slice(0, -1) : strictPrefix; 

            if (urlPath.startsWith(strictPrefix) || urlPath === cleanPrefix) {
                let restOfUrl = '';
                if (urlPath.startsWith(strictPrefix)) {
                    restOfUrl = urlPath.substring(strictPrefix.length);
                }

                let redirectTarget;
                if (rule.hasWildcardValue) {
                    const baseTarget = rule.targetPrefix.endsWith('/') ? rule.targetPrefix : `${rule.targetPrefix}/`;
                    
                    if (restOfUrl === '') {
                        redirectTarget = `${baseTarget}${suffix}`;
                    } else {
                        redirectTarget = `${baseTarget}${restOfUrl}${suffix}`;
                    }
                } else {
                    redirectTarget = `${rule.value}${suffix}`;
                }

                res.writeHead(302, { 'Location': redirectTarget });
                res.end();
                logTraffic(req, 302);
                return;
            }
        } else {
            if (urlPath === rule.key) {
                res.writeHead(302, { 'Location': `${rule.value}${suffix}` });
                res.end();
                logTraffic(req, 302);
                return;
            }
        }
    }

    if (urlPath === '/index.html' || urlPath === '/index') {
        res.writeHead(301, { 'Location': `/${suffix}` });
        res.end();
        logTraffic(req, 301);
        return;
    }

    if (urlPath.endsWith('.html')) {
        const cleanPath = urlPath.slice(0, -5);
        res.writeHead(301, { 'Location': `${cleanPath}${suffix}` });
        res.end();
        logTraffic(req, 301);
        return;
    }

    const BASE_WEB_DIR = path.join(__dirname, 'assets');

    let safePath = path.normalize(urlPath).replace(/^(\.\.[\/\\])+/, '');
    safePath = safePath.replace(/\/\.(?=\/|$)/g, '');

    if (safePath.startsWith('/') || safePath.startsWith('\\')) {
        safePath = safePath.substring(1);
    }
    if (safePath === '') {
        safePath = 'index.html';
    }

    const hasExt = path.extname(safePath) !== '';
    const pageFileName = hasExt ? safePath : `${safePath}.html`;
    const pageTarget = path.join(BASE_WEB_DIR, 'pages', pageFileName);
    
    if (!pageTarget.startsWith(BASE_WEB_DIR)) {
        res.writeHead(403, { 'Content-Type': 'text/plain' });
        res.end('403 Forbidden');
        logTraffic(req, 403);
        return;
    }

    serveFileIfExists(pageTarget, 'text/html; charset=utf-8', res, req, (served) => {
        if (served) return;

        const assetFolders = ['public', 'images', 'styles'];
        let folderIdx = 0;

        function checkNextAssetFolder() {
            if (folderIdx >= assetFolders.length) {
                const errorPage = path.join(BASE_WEB_DIR, 'misc', 'error.html');
                serveFileIfExists(errorPage, 'text/html; charset=utf-8', res, req, (errorServed) => {
                    if (!errorServed) {
                        res.writeHead(404, { 'Content-Type': 'text/plain' });
                        res.end('404 Not Found');
                        logTraffic(req, 404);
                    }
                });
                return;
            }

            const currentFolder = assetFolders[folderIdx];
            const assetTarget = path.join(BASE_WEB_DIR, currentFolder, safePath);

            if (!assetTarget.startsWith(BASE_WEB_DIR)) {
                folderIdx++;
                checkNextAssetFolder();
                return;
            }

            serveFileIfExists(assetTarget, 'application/octet-stream', res, req, (assetServed) => {
                if (assetServed) return;
                folderIdx++;
                checkNextAssetFolder();
            });
        }

        checkNextAssetFolder();
    });
});

server.listen(PORT, () => {
    console.log(`TuxBlox server active on port ${PORT}`);
});