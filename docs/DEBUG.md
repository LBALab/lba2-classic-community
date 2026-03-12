# Debug Tools

This document describes the developer debug tools available when building with `-DDEBUG_TOOLS=ON`.

These tools were originally used by the Adeline Software team during LBA2 development and have been restored for use in the community fork.

## Building with Debug Tools

```bash
cmake -B build -DSOUND_BACKEND=sdl -DMVIDEO_BACKEND=smacker -DDEBUG_TOOLS=ON
cmake --build build
```

## Keyboard Shortcuts

### In-Game Debug Keys

| Key | Function | Description |
|-----|----------|-------------|
| `T` | Advance timer | Advances game timer by 200 ticks |
| `Z` | Toggle ZV | Toggles collision volume (Zone de Vie) rendering |
| `E` | Toggle horizon | Toggles horizon drawing on/off |
| `G` | Save bug state | Saves current game state to `./bugs/` for reproducing issues |
| `L` | Load bug state | Loads a previously saved bug state from main menu |
| `F9` | Screenshot | Captures screenshot as PNG to current directory |
| `F10` | Terrain benchmark | Runs 20-loop terrain rendering benchmark (exterior only) |
| `F11` | Scene benchmark | Runs 20-loop full scene rendering benchmark (exterior only) |
| `F12` | Toggle ASCII mode | Toggles ASCII input mode |

### Debug Cheat Codes

When DEBUG_TOOLS is enabled, some cheat codes use shorter versions:

| Code | Normal | Debug | Effect |
|------|--------|-------|--------|
| LIFE | `LIFE` | `IFE` | Restores full health |
| MAGIC | `MAGIC` | `MAGIC` | Restores magic points |
| FULL | `FULL` | `FULL` | Restores everything |
| GOLD | `GOLD` | `GOLD` | Adds 50 kashes/zlitos |
| SPEED | `SPEED` | `SPEED` | Toggles FPS display |
| CLOVER | `CLOVER` | `CLOVER` | Fills clover leaves |
| BOX | `BOX` | `BOX` | Adds clover box |
| PINGOUIN | `PINGOUIN` | `PINGOUIN` | Adds 5 meca-penguins |

## Bug Save/Load System

The bug save/load system allows developers to save the exact game state at any point and reload it later for debugging.

### Saving a Bug State
1. Press `G` during gameplay
2. Enter a name for the save
3. State is saved to `./bugs/` directory

### Loading a Bug State
1. From the main menu, press `L`
2. Select the bug save to load

## On-Screen Debug Information

When DEBUG_TOOLS is enabled, the game may display:
- Timer desync warnings ("SaveTimer & RestoreTimer Decale")
- Polygon limit warnings ("MaxPolySea Atteint!")
- Memory and performance statistics

## Command-Line Options

With DEBUG_TOOLS enabled, you can start at a specific cube:

```bash
./lba2 <cube_number>
```

## Related Files

- `SOURCES/DEFINES.H` - Contains the `DEBUG_TOOLS` define
- `SOURCES/PERSO.CPP` - Main debug key handlers
- `SOURCES/GAMEMENU.CPP` - Bug save/load menu integration
- `SOURCES/CHEATCOD.CPP` - Cheat code handling
