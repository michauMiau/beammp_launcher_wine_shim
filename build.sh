set -xe

x86_64-w64-mingw32-gcc -Wformat -fPIC -static -shared -Iminhook_prebuilt/include main.c -Lminhook_prebuilt/bin/ -Wl,-Bdynamic -lMinHook.x64 -lws2_32 -o wintrust.dll
cp minhook_prebuilt/bin/MinHook.x64.dll ./
