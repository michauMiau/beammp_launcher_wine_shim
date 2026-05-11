### beammp_launcher_wine_shim

#### Build

1. Install mingw64 windows toolchain
2. Run build.sh

#### Build using podman

1. Install podman
2. Run build_podman.sh

#### Install

- After building, place `wintrust.dll` and `MinHook.x64.dll` next to `BeamMP-Launcher.exe`

#### Run

Run the following commands to start the launcher with the shim loaded

```
export WINEDLLOVERRIDES="wintrust=n,b"
wine BeamMP-Launcher.exe
```
