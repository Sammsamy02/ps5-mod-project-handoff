# ModBridge app launcher

This small PS5 payload asks the console's browser to open
`http://127.0.0.1:8085/app`. The ModBridge native host must already be running
and listening on that address. The launcher does not start the server, install
an app, or package a home-screen icon.

The source follows the browser sample in the pinned community SDK v0.43:
[`samples/browser/main.c`](https://github.com/ps5-payload-dev/sdk/blob/d9c9519116944a7f1c22d262012c53b80b3520b7/samples/browser/main.c).
It uses `sceUserServiceInitialize`, `sceSystemServiceLaunchWebBrowser` and
`sceUserServiceTerminate`, linking the SDK's existing `libSceSystemService`
and `libSceUserService` stubs. Service errors are printed as hexadecimal return
codes; startup failure prevents the launch call, and successful initialization
is followed by termination even when the launch fails.

## Local build

From this directory, using the already cached SDK and LLVM tools:

```sh
PS5_PAYLOAD_SDK=/absolute/path/to/ps5-payload-sdk \
LLVM_CONFIG=/absolute/path/to/LLVM-21.1.8-macOS-ARM64/bin/llvm-config \
make BUILD_DIR=/absolute/path/to/owned-build-directory
```

Output: `modbridge-app-launcher.elf`. There is deliberately no deploy or run
target in this Makefile. Compilation is the current evidence; browser launch,
controller support, browser lifetime, and Nexus callback behavior on the PS5
still require a console test. A successful browser-launch API result would not
by itself establish that the page finished loading.

## Remaining app integration

The app owner must compose native-host startup/readiness with browser opening,
then provide the eventual home-screen entry and lifetime handling. This file
keeps the source-backed browser-opening step independently buildable while
that composition is developed. It is not an embedded WebView implementation.

Derived sample code is GPL-3.0-or-later. The existing complete license is at
`../native-probe/third_party/SDK-GPL-3.0.txt`; preserve that license and the SDK
source attribution with any copied launcher artifact.
