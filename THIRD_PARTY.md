# Third-party parts

TrayVitals is plain C and uses only Windows' own libraries, plus the parts below. Each one is pinned to an exact file and SHA-256, so nothing can change without it showing up in this repo.

## TraySense by DMTWEAKS

TrayVitals started as a fork of [DMTWEAKS/traysense](https://github.com/DMTWEAKS/traysense), MIT License. Its copyright notice is kept in [LICENSE](LICENSE).

## PawnIO driver (CPU temperature and power)

Windows doesn't let programs read the CPU's temperature sensors directly. TrayVitals uses [PawnIO](https://github.com/namazso/PawnIO.Setup) by namazso for this. It's a small, signed, open-source driver, the same one LibreHardwareMonitor and Fan Control use.

| | |
|---|---|
| File | `PawnIO_setup.exe` 2.2.0, from the [official release](https://github.com/namazso/PawnIO.Setup/releases/tag/2.2.0) |
| SHA-256 | `1f519a22e47187f70a1379a48ca604981c4fcf694f4e65b734aaa74a9fba3032` |
| Where | Bundled inside TrayVitals-Setup.exe. It isn't stored in this repo; `build.sh` downloads it from the official release and stops if the checksum doesn't match. The installer checks it again before running it. |
| License | PawnIO's own license, see [PawnIO.Setup](https://github.com/namazso/PawnIO.Setup) |

## PawnIO modules

The small programs that run inside the PawnIO driver and read the sensors, signed by PawnIO's author.

| File | SHA-256 |
|---|---|
| [`src/TrayVitals/pawnio/IntelMSR.bin`](src/TrayVitals/pawnio/IntelMSR.bin) (Intel) | `5bfda87500160076158befc77300f81a388d72b11df2c794a3939ebe66777098` |
| [`src/TrayVitals/pawnio/AMDFamily17.bin`](src/TrayVitals/pawnio/AMDFamily17.bin) (AMD Ryzen) | `099dc01d6db97ea997fec4a461e191cc64b9d7ce47c9d2153c451c56c2adcf50` |

They come from [PawnIO.Modules](https://github.com/namazso/PawnIO.Modules) release 0.2.9 (`release_0_2_9.zip`, SHA-256 `5eb25b128b0940c53c36418dc4f74fba66d5a80e6bf3d95eb0ae62f4d946222d`) and are embedded in TrayVitals.exe unchanged. License: LGPL-2.1, see [`src/TrayVitals/pawnio/COPYING`](src/TrayVitals/pawnio/COPYING).

## NVIDIA NVML (GPU power)

For NVIDIA graphics cards, TrayVitals reads GPU power through `nvml.dll`, which comes with the NVIDIA driver. It isn't shipped with TrayVitals; TrayVitals uses the copy the driver installed, and only from Windows' system folder or NVIDIA's folder in Program Files.

## AMD Display Library (GPU power on Radeon cards)

For AMD Radeon graphics cards, TrayVitals reads GPU power through `atiadlxx.dll`, which comes with the AMD driver. It isn't shipped with TrayVitals; TrayVitals loads it only from Windows' system folder. The structures and sensor numbers TrayVitals uses come from AMD's [ADL SDK](https://github.com/GPUOpen-LibrariesAndSDKs/display-library) (MIT License); no ADL code is copied into this repo.

## Windows Imaging Component

The installer shows its two menu pictures (`src/installer/images/`, made from TrayVitals' own menus) with Windows' built-in image decoder. Nothing extra is shipped for that.

## Build tools

| Tool | Used for |
|---|---|
| [MinGW-w64](https://www.mingw-w64.org/) GCC | Compiling. GitHub's Windows build machine already has it. |
| GitHub Actions: [`checkout`](https://github.com/actions/checkout), [`upload-artifact`](https://github.com/actions/upload-artifact), [`attest-build-provenance`](https://github.com/actions/attest-build-provenance) | The release build. Each is pinned to an exact commit in [build.yml](.github/workflows/build.yml). |
