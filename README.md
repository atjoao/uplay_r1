# Mini_Uplay_API_Emu

This is a fork [Rat/Mini_Uplay_API_Emu](https://github.com/Rat431/Mini_Uplay_API_Emu) of a fork [georgboe/Mini_Uplay_API_Emu](https://github.com/georgboe/Mini_Uplay_API_Emu), with code copied from [ServerEmus/Uplay.upc_r1](https://github.com/ServerEmus/Uplay.upc_r1)

## Instructions

For games with that might dll check, use ASI Loader: `uplay_asi.asi + emu.upc_r1_loader.dll + dinput8.dll`

For games without dll check, just rename `emu.upc_r1_loader.dll` to `uplay_r1_loader.dll` (or whatever your game uses).

## Compile

**ASI Loader (64-bit):**
```bash
g++ -shared -static -I include -I src/minhook/include -o uplay_asi.asi src/uplay_hook.cpp src/minhook/src/hook.c src/minhook/src/buffer.c src/minhook/src/trampoline.c src/minhook/src/hde/hde64.c -lkernel32 -luser32
```

**ASI Loader (32-bit):**
```bash
g++ -m32 -shared -static -I include -I src/minhook/include -o uplay_asi.asi src/uplay_hook.cpp src/minhook/src/hook.c src/minhook/src/buffer.c src/minhook/src/trampoline.c src/minhook/src/hde/hde32.c -lkernel32 -luser32
```

**Emulator DLL (64-bit):**
```bash
g++ -shared -static -o emu.upc_r1_loader64.dll src/dllmain.cpp src/pch.cpp src/uplay_data.cpp -lkernel32 -luser32 -ladvapi32
```

**Emulator DLL (32-bit):**
```bash
g++ -m32 -shared -static -o emu.upc_r1_loader.dll src/dllmain.cpp src/pch.cpp src/uplay_data.cpp -lkernel32 -luser32 -ladvapi32
```
