# Uplay_R1

## Credits

This is a fork [Rat431/Mini_Uplay_API_Emu](https://github.com/Rat431/Mini_Uplay_API_Emu) of a fork [georgboe/Mini_Uplay_API_Emu](https://github.com/georgboe/Mini_Uplay_API_Emu), with code copied from [ServerEmus/Uplay.upc_r1](https://github.com/ServerEmus/Uplay.upc_r1), steam files from [rlabrecque/SteamworksSDK](https://github.com/rlabrecque/SteamworksSDK), uses [TsudaKageyu/minhook](https://github.com/TsudaKageyu/minhook) and [ThirteenAG/Ultimate-ASI-Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases)

## Instructions

**64-bit games:** Use `uplay_asi64.asi + emu.upc_r1_loader64.dll + dinput8.dll + steam_api64.dll` (for games with DLL check) or download `uplay_r1_loader64.dll` and rename to your dll name.

**32-bit games:** Use `uplay_asi.asi + emu.upc_r1_loader.dll + dinput8.dll + steam_api.dll` (for games with DLL check) or download `uplay_r1_loader.dll` and rename to your dll name.

> Note that steam_api.dll and steam_api64.dll are not required for the emu to work.
> Its just for steam hooking with the game, to get stuff like the overlay working.

## Compile
```bash
make or mingw32-make.exe
```

# Information

Saves and Achievements using the emulator will be saved on 
`%APPDATA%\UplayEmu\{userId}\{appId}\<respective folder>` with fallback to game folder

When a game triggers an achievement it will create <id>.ini in achievement folder without name and description.
Ubisoft achievements at least the ones that use R1_loader are locked server side.
You need to unlock that achievement on ubisoft connect if you want it on steam

Uplay.ini will be on the game folder and created upon opening the game.

> Licenses for ASI Loader and MinHook is on releases zip