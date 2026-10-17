# UCRT runtime intake: SDK NuGet package check

Microsoft says app-local UCRT files, including `ucrtbase.dll` and `api-ms-win-*.dll` forwarders, are supplied under the Windows SDK's redist directory. See [Universal CRT deployment](https://learn.microsoft.com/en-us/cpp/windows/universal-crt-deployment?view=msvc-170).

I downloaded the Microsoft-owned [Windows SDK C++ x64 NuGet package, version 10.0.26100.8249](https://www.nuget.org/packages/Microsoft.Windows.SDK.CPP.x64/10.0.26100.8249) from the official NuGet package endpoint for local inspection. The 52,951,912-byte package has SHA-256 `dba411916afdd47c99fecdf7d5558df7679d291dc338746590ae9adf5fd0511b`; ZIP CRC verification passed. A case-insensitive scan of all **496** archive members found **zero** `ucrtbase.dll` or `api-ms-win-crt-*` files. Nothing was installed. The downloaded package remains in the workspace's `work/ucrt-sdk-intake` folder and is not redistributed here.

This package does not supply the missing UCRT runtime binary for the existing genuine MSVC DLL intake. The [227 direct import gaps](MSVC-CRT-IMPORT-GAP-OFFLINE-20261013.md) remain. Obtaining an official runtime payload or implementing reviewed UCRT semantics is separate offline work; downloading SDK headers and import libraries alone does not make the unchanged CRT DLLs load or run.
