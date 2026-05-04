# Debug Tools

This document describes the developer debug tools available when building with `-DDEBUG_TOOLS=ON`.

These tools were originally used by the Adeline Software team during LBA2 development and have been restored for use in the community fork.

## Building with Debug Tools

```bash
cmake -B build -DDEBUG_TOOLS=ON
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
| `A` | Play test video | Plays **bu** (test ACF video) and reinitializes the scene |
| `B` | Toggle sounds | Toggles sample/sound playback on/off |
| `G` | Save bug state | Saves current game state for reproducing issues |
| `L` | Load bug state | Loads a previously saved bug state (from main menu) |
| `F9` | Screenshot | Captures screenshot as PCX (original format) |
| `F10` | Terrain benchmark | Runs 20-loop terrain rendering benchmark (exterior only) |
| `F11` | Scene benchmark | Runs 20-loop full scene rendering benchmark (exterior only) |
| `F12` | Toggle ASCII mode | Toggles between text input and gameplay input modes (unless console toggle is set to F12) |

Note: `D` and `F` are also available in TEST_TOOLS builds. All other keys require DEBUG_TOOLS. The always-on console toggle is configurable via `ConsoleToggleKey` in `lba2.cfg` and defaults to `F12`, so ASCII mode toggle on `F12` can be shadowed when that default is used.

### What is ASCII Mode?

ASCII mode controls how the game interprets keyboard input:
- **ON**: Captures typed characters (for text entry like player names, cheat codes)
- **OFF**: Captures raw key scancodes (for gameplay controls)

Pressing `F12` toggles between these modes. If console toggle also uses `F12` (default), set `ConsoleToggleKey` to a different scancode in `lba2.cfg`.

## File Locations

All debug-related files are saved to your user data directory:

| Platform | Base Path |
|----------|-----------|
| Linux | `~/.local/share/Twinsen/LBA2/` |
| Windows | `%APPDATA%\Twinsen\LBA2\` |
| macOS | `~/Library/Application Support/Twinsen/LBA2/` |

### Screenshots (`F9`)

Saved to `save/shoot/LBA00000.PCX`, `LBA00001.PCX`, etc. (original PCX format).  
For PNG screenshots without the debug overlay, use the **debug console**: see [CONSOLE.md](CONSOLE.md) and the `screenshot` command.

### Bug Saves (`G` key)

Saved to `save/bugs/*.lba`

The name "bugs" comes from the original French development team - these were used to save game states when reproducing bug reports.

### Regular Saves

Saved to `save/*.lba`

## Cheat Codes

Cheat codes are entered by **typing the letters during normal gameplay** - no special key combination needed. Just type the word while playing:

| Code | Short Version | Effect |
|------|---------------|--------|
| `LIFE` | `IFE` | Restores full health |
| `MAGIC` | — | Restores magic points |
| `FULL` | — | Restores health + magic + clovers |
| `GOLD` | — | Adds 50 kashes (or zlitos on Zeelich) |
| `SPEED` | — | Toggles FPS display |
| `CLOVER` | — | Fills clover leaves |
| `BOX` | — | Adds a clover box |
| `PINGOUIN` | — | Adds 5 meca-penguins |

The "Short Version" column shows the shorter codes used in DEBUG_TOOLS builds (e.g. `IFE` instead of `LIFE`). When a cheat activates, you'll see a confirmation message like "Life Found" on screen.

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

If F9 screenshots are not being saved, check:
1. The `save/shoot/` directory exists and is writable
2. File permissions on your user data directory

If the directory was created by running the game as root, you may need to fix permissions:
```bash
sudo chown -R $USER:$USER ~/.local/share/Twinsen/
```

### F9 / F12 not working on Windows

On some Windows configurations, F9 and F12 may not be detected by the game. This can be caused by:
- Windows Terminal or other terminal emulators intercepting function keys
- System-wide keyboard hooks (GeForce Experience, Xbox Game Bar, etc.)
- WSL2 not passing certain keys through to Linux applications

Try running the native Windows build instead of WSL2, or check your terminal emulator's key binding settings.

## Historical Context

These debug tools give us a window into how Adeline Software worked in the mid-1990s.

### Trade Show Demo Mode

The codebase contains a commented-out `/NOKEY` flag for trade show demonstrations:

```cpp
// Pour un SlideShow sur un salon, il ne faut pas que les p'tits gars
// puissent jouer avec le clavier !!!!!
```

> "For a SlideShow at a trade show, the little guys shouldn't be able to play with the keyboard!!!!!"

When LBA2 was shown at gaming conventions, Adeline would run the game in a demo mode where keyboard input was disabled to prevent visitors from interfering with the presentation.

### Bug Tracking with Codenames

The code contains traps for specific bugs tracked by codename:

```cpp
// DEBUG pour trouver bug Alpha

        // debug patch Gamma
        if( ptrobj->Obj.Alpha!=0
        OR  ptrobj->Obj.Gamma!=0 )
        {
                Message( "STOP: 'j'ai trouvé le Patch_Alpha' !", TRUE ) ;
        }
```

When triggered, a message pops up: "I found the Patch_Alpha!" This shows they had an internal system for tracking and hunting specific bugs.

### TEST_TOOLS vs DEBUG_TOOLS

The codebase has two separate debug defines:

- **DEBUG_TOOLS** - Full developer tools (all debug keys, cheat codes, F9 PCX screenshots, bug saves)
- **TEST_TOOLS** - Limited QA tester tools (only debug overlay and FPS counter)

This separation ensured QA testers could see debug information without having access to features that might let them "cheat" past bugs they should be finding. Code guarded by `#if defined(DEBUG_TOOLS)||defined(TEST_TOOLS)` was available to both teams.

## Related Files

- `SOURCES/DEFINES.H` - Contains the `DEBUG_TOOLS` define
- `SOURCES/PERSO.CPP` - Main debug key handlers, screenshot functions, debug overlay, cube selection
- `SOURCES/PERSO.H` - Debug function declarations
- `SOURCES/GAMEMENU.CPP` - Bug save/load menu integration. See [MENU.md](MENU.md) for the full menu flow.
- `SOURCES/SAVEGAME.CPP` - Save/load implementation. See [SAVEGAME.md](SAVEGAME.md) for format and lifecycle.
- `SOURCES/CHEATCOD.CPP` - Cheat code handling
- `SOURCES/DIRECTORIES.CPP` - File path resolution (screenshots, saves, bugs)
- `SOURCES/DISKFUNC.CPP` - Directory creation (save, shoot, bugs)
