# Deployment API

> **DISCLAIMER:** Files are proxied directly from Roblox's official Content Delivery Network at user request. **TuxBlox does not host, store or cache any client binaries on its own servers.**

> **NOTICE:** As of June 24, 2026, **authentication is required to download the MacPlayer binaries.** To protect our infrastructure from automated scraper abuse and ensure fair bandwidth for all users, you must now be authenticated to our servers to initialize a download request.

## What is the Deployment API?
The Deployment API is a utility that queries and downloads Roblox Game Client version files from Roblox's Content Delivery Network.

The Deployment API is used by the [Deployment Downloader](./Deployment%20Downloader) to Download Roblox MacPlayer version files.
### If you want to make your own Deployment API, check out the [Roblox ClientSettings API reference](https://create.roblox.com/docs/cloud/reference/domains/clientsettings)

## Can I use this API?
As long as you authenticate to our servers, feel free to hook your own tools, scripts, or external apps directly into our Deployment Downloader API. We're completely open to third-party integrations.

The API accepts simple `GET` requests structured around target deployment channels and specific version hashes:
```http
GET https://rdd.tuxblox.net/download?channel=[channel]&version=[version]
```

## How do I use the API?
There are multiple ways to use the API depending on your framework.

### Using cURL (command-line)
```bash
curl -L -o tuxblox-macplayer.zip "https://rdd.tuxblox.net/download?channel=[channel]&version=[version]"
```

### Using Python (`requests`)
```py
import requests

url = "https://rdd.tuxblox.net/download"
params = {
    "channel": "zlive",
    "version": "version-abcdef1234567890"
}

with requests.get(url, params=params, stream=True) as response:
    response.raise_for_status()
    with open("tuxblox-macplayer.zip", "wb") as f:
        for chunk in response.iter_content(chunk_size=8192):
            f.write(chunk)
```

### Using Node.js (`Axios`)
```js
const axios = require('axios');
const fs = require('fs');

async function downloadBinary() {
    const writer = fs.createWriteStream('tuxblox-macplayer.zip');
    
    const response = await axios({
        url: 'https://rdd.tuxblox.net/download',
        method: 'GET',
        params: {
            channel: 'zlive',
            version: 'version-abcdef1234567890'
        },
        responseType: 'stream'
    });

    response.data.pipe(writer);

    return new Promise((resolve, reject) => {
        writer.on('finish', resolve);
        writer.on('error', reject);
    });
}
```
