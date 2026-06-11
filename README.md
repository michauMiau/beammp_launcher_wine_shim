### beammp_launcher_wine_shim
I took the very hard work of compiling this so you don't have to compile it,
the files are in the release **https://github.com/michauMiau/beammp_launcher_wine_shim/releases/tag/1**

thx to kethen for making this fix to beammp so you can run it in proton
This fix is required until pull request #00 is merged

### HOW TO SETUP FOR BOTTLES
1. go into your bottle, go to dll overrides
2. type wintrust and select native first then builtin
3. put the files wintrust and hook(something) into the folder where beammp is installed, for me it's /home/**Your_username**/.var/app/com.usebottles.bottles/data/bottles/bottles/BeamMP/drive_c/users/**Your_username**/AppData/Roaming/BeamMP-Launcher/
5. Play with friends!! 

### SETUP FOR STEAM
Copy the files from the release to the BeamMP install directory
Add to the BeamMP launcher launch options the following,

export WINEDLLOVERRIDES="wintrust=n,b" %command%

#### Original README
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
