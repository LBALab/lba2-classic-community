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
| `G` | Save bug state | Saves current game state for reproducing issues |
| `L` | Load bug state | Loads a previously saved bug state (from main menu) |
| `F9` | Screenshot | Captures screenshot as PNG |
| `F10` | Terrain benchmark | Runs 20-loop terrain rendering benchmark (exterior only) |
| `F11` | Scene benchmark | Runs 20-loop full scene rendering benchmark (exterior only) |
| `F12` | Toggle ASCII mode | Toggles between text input and gameplay input modes |

### What is ASCII Mode?

ASCII mode controls how the game interprets keyboard input:
- **ON**: Captures typed characters (for text entry like player names, cheat codes)
- **OFF**: Captures raw key scancodes (for gameplay controls)

Pressing `F12` toggles between these modes. This was useful during development for testing text input vs gameplay.

## File Locations

### Screenshots (`F9`)

Screenshots are saved to your user data directory under `save/shoot/`:

| Platform | Location |
|----------|----------|
| Linux | `~/.local/share/Twinsen/LBA2/save/shoot/LBA00000.png` |
| Windows | `%APPDATA%\Twinsen\LBA2\save\shoot\LBA00000.png` |
| macOS | `~/Library/Application Support/Twinsen/LBA2/save/shoot/LBA00000.png` |

### Bug Saves (`G` key)

Bug saves are currently saved to `./bugs/` relative to where you run the game. This is a legacy path from the original Adeline debug system.

The name "bugs" comes from the original French development team - these were used to save game states when reproducing bug reports.

## Cheat Codes

Cheat codes are entered by **typing the letters during normal gameplay** - no special key combination needed. Just type the word while playing:

| Code | Debug Version | Effect |
|------|---------------|--------|
| `LIFE` | `IFE` | Restores full health |
| `MAGIC` | `MAGIC` | Restores magic points |
| `FULL` | `FULL` | Restores health + magic + clovers |
| `GOLD` | `GOLD` | Adds 50 kashes (or zlitos on Zeelich) |
| `SPEED` | `SPEED` | Toggles FPS display |
| `CLOVER` | `CLOVER` | Fills clover leaves |
| `BOX` | `BOX` | Adds a clover box |
| `PINGOUIN` | `PINGOUIN` | Adds 5 meca-penguins |

When a cheat activates, you'll see a confirmation message like "Life Found" on screen.

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

## Known Limitations

### Command-Line Cube Selection (Not Implemented)

The original debug tools included a command-line option to start at a specific cube/scene:
```bash
./lba2 <cube_number>
```

This feature is **not currently functional**. The `SlideDemo` function that handled this is commented out in `PERSO.CPP`. The command-line argument is currently interpreted as a save game filename instead.

## Related Files

- `SOURCES/DEFINES.H` - Contains the `DEBUG_TOOLS` define and `PATH_SAVE_BUGS`
- `SOURCES/PERSO.CPP` - Main debug key handlers, screenshot functions
- `SOURCES/GAMEMENU.CPP` - Bug save/load menu integration
- `SOURCES/CHEATCOD.CPP` - Cheat code handling
- `SOURCES/DIRECTORIES.CPP` - File path resolution (screenshots, saves)
