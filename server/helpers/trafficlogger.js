const fs = require('fs');
const path = require('path');

const isotranslations = require('../config/isotranslations.json');
const LOG_DIR = path.join(__dirname, '..', 'logs', 'traffic');

let logBuffer = [];
let logFilePath = '';

function isotocountry(key) {
  return isotranslations[key] ?? key;
}

function getTimestamp() {
    const now = new Date();
    const pad = (n) => String(n).padStart(2, '0');
    return `${pad(now.getDate())}-${pad(now.getMonth() + 1)}-${now.getFullYear()}-${pad(now.getHours())}-${pad(now.getMinutes())}-${pad(now.getSeconds())}`;
}

function initLogging(service = 'General') {
    if (!fs.existsSync(LOG_DIR)) {
        fs.mkdirSync(LOG_DIR, { recursive: true });
    }
    const filename = `${service}-${getTimestamp()}.log`;
    logFilePath = path.join(LOG_DIR, filename);
    fs.writeFileSync(logFilePath, `<< Log Session Started at ${new Date().toISOString()} >>\n`);
    console.log(`Logging active. Target file: ${logFilePath}`);
    setInterval(flushLogs, 5 * 60 * 1000);
}

function flushLogs() {
    if (logBuffer.length === 0) return;

    const dataToAppend = logBuffer.join('\n') + '\n';
    logBuffer = []; 

    fs.appendFile(logFilePath, dataToAppend, (err) => {
        if (err) console.error(`<< AssetDelivery >> Failed to write logs to disk:`, err);
    });
}

function flushLogsSync() {
    if (logBuffer.length === 0) return;
    const dataToAppend = logBuffer.join('\n') + '\n';
    try {
        fs.appendFileSync(logFilePath, dataToAppend);
    } catch (err) {
        console.error(`<< AssetDelivery >> Failed to write logs on shutdown:`, err);
    }
}

function logTraffic(service, req, statusCode) {
    const ip = req.headers['cf-connecting-ip'] || req.headers['x-forwarded-for'] || req.socket.remoteAddress || 'Unknown IP';
    const originalcountryheader = req.headers['cf-ipcountry'];
    let country;
    if (originalcountryheader == null || originalcountryheader === '') {
        country = '??';
    } else if (/^[\x00-\x7F]+$/.test(originalcountryheader) === false) {
        country = '!!'; 
    } else if (originalcountryheader === 'T1' || originalcountryheader === 'T2') {
        country = 'tor node';
    } else {
        country = originalcountryheader;
    }
    const userAgent = req.headers['user-agent'] || 'Unknown Agent';
    const requestTarget = `${req.method} ${req.url}`;
    const timestamp = new Date().toISOString();
    
    const rawLog = `<< ${service} >> [${timestamp}] [${country}] [${ip}] [${userAgent}] ${requestTarget}: ${statusCode}`;
    logBuffer.push(rawLog);

    const reset = '\x1b[0m';
    
    let statusColor = '\x1b[37m';
    const statusGroup = Math.floor(statusCode / 100);
    if (statusGroup === 2) statusColor = '\x1b[32m';
    else if (statusGroup === 3) statusColor = '\x1b[33m';
    else if (statusGroup === 4 || statusGroup === 5) statusColor = '\x1b[31m';

    let countryColor;
    if (country === 'tor node') {
        countryColor = '\x1b[35m';
    } else if (country === '??') {
        countryColor = '\x1b[90m';
    } else if (country === '!!') {
        countryColor = '\x1b[31m';
    } else {
        countryColor = '\x1b[34m';
    }

    console.log(`<< ${service} >> ${countryColor}[${isotocountry(country)}]${reset} ${statusColor}[${ip}] [${userAgent}] ${requestTarget}: ${statusCode}${reset}`);
}

module.exports = {
    initLogging,
    logTraffic,
    flushLogsSync
};