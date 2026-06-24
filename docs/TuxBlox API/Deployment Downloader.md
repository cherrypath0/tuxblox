# Deployment Downloader

> **COMPATIBILITY WARNING:** This deployment utility only pulls the **official MacPlayer binaries**, which are required for the TuxBlox runtime translation layer. TuxBlox modifies the binaries on the server side purely for optimization.

> **DISCLAIMER:** Files are proxied directly from Roblox's official Content Delivery Network at user request. **TuxBlox does not host, store or cache any client binaries on its own servers.**

## What is a Deployment Downloader?
A Deployment Downloader (also known as a Roblox Deployment Downloader or RDD) is a utility that downloads Roblox Game Client version files from Roblox's Content Delivery Network.

Instead of forcing you to hunt down version hashes manually, the downloader automates the process of communicating with the delivery network pipelines. It tracks down the exact deployment channel you need and streams the necessary execution files directly to your environment.

## How It Works
When you request a build, the downloader kicks off a clean, stateless chain of events:
1. **Version Lookup:** The utility queries the deployment channels to find the precise version hash matching the latest (or specified) release.
2. **Real-Time Streaming:** Rather than distributing pre-packaged files from a third-party server, the downloader acts as a direct pipeline, streaming the original binaries straight from the source.
3. **On-the-Fly Optimization:** As the data passes through, it is optimized in real-time to make sure the incoming macOS binaries play nicely with the local TuxBlox translation environment.
4. **Environment Deployment:** The finalized files are structured immediately into your target directory, ready for the runtime layer to take over.

### For more information about version channels and hashes, check out the [list of Deployment Builds](./Deployment%20Builds)

## Why Do We Use It?
**Operating systems change, and game clients update constantly.** By utilizing a live deployment downloader instead of relying on static, bundled installers, the installation environment stays incredibly lightweight, secure, and resilient against breaking updates. You get clean, official files every single time, formatted exactly the way your system needs to run them.

### For more information about the API, check out the [Deployment API](./Deployment%20API) documentation