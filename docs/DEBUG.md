# Debug Tools

This document describes the developer debug tools available when building with `-DDEBUG_TOOLS=ON`.

These tools were originally used by the Adeline Software team during LBA2 development and have been restored for use in the community fork.

## Building with Debug Tools

```bash
cmake -B build -DSOUND_BACKEND=sdl -DMVIDEO_BACKEND=smacker -DDEBUG_TOOLS=ON
cmake --build build
```

## Command-Line Cube Selection

With DEBUG_TOOLS enabled, you can start the game directly at any scene (cube):

```bash
./lba2 42      # Starts at cube 42
./lba2 193     # Starts at the intro scene
```

This bypasses the main menu and loads directly into the specified scene. Useful for quickly testing specific areas of the game.

## Keyboard Shortcuts

### In-Game Debug Keys

| Key | Function | Description |
|-----|----------|-------------|
| `D` | Debug overlay | Shows debug info: island, cube, chapter, objects, zones, FPS, memory |
| `F` | Toggle FPS | Toggles FPS counter display (same as `SPEED` cheat) |
| `T` | Advance timer | Advances game timer by 200 ticks |
| `Z` | Toggle ZV | Toggles collision volume (Zone de Vie) rendering |
| `E` | Toggle horizon | Toggles horizon drawing on/off |
| `V` | Teleport to magic ball | Moves Twinsen to the magic ball's current position |
| `M` | Show palette | Displays the current color palette |
| `K` | Spawn bonus | Spawns a bonus item above the player |
| `A` | Play test video | Plays "bu.acf" video and reinitializes the scene |
| `B` | Toggle sounds | Toggles sample/sound playback on/off |
| `G` | Save bug state | Saves current game state for reproducing issues |
| `L` | Load bug state | Loads a previously saved bug state (from main menu) |
| `F9` / `PrintScreen` | Screenshot | Captures screenshot as PNG |
| `F10` | Terrain benchmark | Runs 20-loop terrain rendering benchmark (exterior only) |
| `F11` | Scene benchmark | Runs 20-loop full scene rendering benchmark (exterior only) |
| `F12` | Toggle ASCII mode | Toggles between text input and gameplay input modes |

Note: `D` and `F` are also available in TEST_TOOLS builds. All other keys require DEBUG_TOOLS.

### What is ASCII Mode?

ASCII mode controls how the game interprets keyboard input:
- **ON**: Captures typed characters (for text entry like player names, cheat codes)
- **OFF**: Captures raw key scancodes (for gameplay controls)

Pressing `F12` toggles between these modes. This was useful during development for testing text input vs gameplay.

## File Locations

All debug-related files are saved to your user data directory:

| Platform | Base Path |
|----------|-----------|
| Linux | `~/.local/share/Twinsen/LBA2/` |
| Windows | `%APPDATA%\Twinsen\LBA2\` |
| macOS | `~/Library/Application Support/Twinsen/LBA2/` |

### Screenshots (`F9`)

Saved to `save/shoot/LBA00000.png`, `LBA00001.png`, etc.

### Bug Saves (`G` key)

Saved to `save/bugs/*.lba`

The name "bugs" comes from the original French development team - these were used to save game states when reproducing bug reports.

### Regular Saves

Saved to `save/*.lba`

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

### Cheat Codes in DEBUG_TOOLS Builds

In DEBUG_TOOLS builds, many letter keys are bound to debug functions (`D`, `F`, `T`, `Z`, `E`, `V`, `M`, `K`, `A`, `B`). This means most cheat codes will not work correctly because pressing these keys triggers the debug function instead.

**To use cheat codes in a debug build:**
1. Press `F12` to toggle ASCII mode ON
2. Type the cheat code
3. Press `F12` again to toggle ASCII mode OFF

In non-debug builds, cheat codes work without any special steps.

## Bug Save/Load System

The bug save/load system allows developers to save the exact game state at any point and reload it later for debugging.

### Saving a Bug State
1. Press `G` during gameplay
2. Enter a name for the save
3. State is saved to the bugs directory

### Loading a Bug State
1. From the main menu, press `L`
2. Select the bug save to load

## On-Screen Debug Information

When DEBUG_TOOLS is enabled, the game may display:
- Timer desync warnings ("SaveTimer & RestoreTimer Decale")
- Polygon limit warnings ("MaxPolySea Atteint!")
- Memory and performance statistics

## Troubleshooting

### Screenshots not saving

If screenshots are not being saved, check:
1. The `save/shoot/` directory exists and is writable
2. File permissions on your user data directory
3. Check the game log for "Error: Failed to save screenshot" messages

If the directory was created by running the game as root, you may need to fix permissions:
```bash
sudo chown -R $USER:$USER ~/.local/share/Twinsen/
```

### F9/F12 not working on Windows

On some Windows configurations, F9 and F12 may not be detected by the game. This can be caused by:
- Windows Terminal or other terminal emulators intercepting function keys
- System-wide keyboard hooks (GeForce Experience, Xbox Game Bar, etc.)
- WSL2 not passing certain keys through to Linux applications

**Workarounds:**
- Use `PrintScreen` instead of `F9` for screenshots
- Try running the native Windows build instead of WSL2
- Check your terminal emulator's key binding settings

## Related Files

- `SOURCES/DEFINES.H` - Contains the `DEBUG_TOOLS` define
- `SOURCES/PERSO.CPP` - Main debug key handlers, screenshot functions, debug overlay, cube selection
- `SOURCES/PERSO.H` - Debug function declarations
- `SOURCES/GAMEMENU.CPP` - Bug save/load menu integration
- `SOURCES/CHEATCOD.CPP` - Cheat code handling
- `SOURCES/DIRECTORIES.CPP` - File path resolution (screenshots, saves, bugs)
- `SOURCES/DISKFUNC.CPP` - Directory creation (save, shoot, bugs)
