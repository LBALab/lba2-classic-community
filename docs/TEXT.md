# Text and localization

How a string gets from `TEXT.HQR` (or from a table in our own source) onto the screen: the
on-disk format, the language model, id resolution, the fonts, and the dialogue machine.

Companion docs: [MENU.md](MENU.md) (which menu row uses which text source),
[ENGINE_FILE_FORMATS.md](ENGINE_FILE_FORMATS.md) (the HQR container and LZ codecs),
[CONFIG.md](CONFIG.md) (`Language` / `LanguageCD` persistence).

## The one thing to know

The engine has **two text sources, and only one of them is extensible**.

| | Retail `TEXT.HQR` | In-source tables |
|---|---|---|
| Holds | every string the 1997 game shipped | every string the community added |
| Reached by | `GetMultiText(id, dst)` / `Dial(id, …)` | `GetLocalizedMenuLabel(...)` |
| Encoding | CP850 bytes, used as-is | UTF-8 literals, folded to CP850 at draw time |
| Can we add a string? | **No** | Yes |

`TEXT.HQR` is a shipped binary asset we do not own and cannot regenerate. Adding one string to it
means rewriting two entries (an id bank and a text bank) in each of the six languages, and
reshipping a file the player already has on disc. So every new label the port introduces
("Advanced options", "Vsync ON", the resolution dialog) has to live in source, in
[SOURCES/MENU_LABELS.CPP](../SOURCES/MENU_LABELS.CPP). That is not a shortcut, it is the only door.

Everything below explains why the door is shut.

## TEXT.HQR

### Container

A standard Adeline HQR ([ENGINE_FILE_FORMATS.md](ENGINE_FILE_FORMATS.md)): a `U32` offset table
whose first element doubles as the table's own byte length, then one `COMPRESSED_HEADER` + payload
per entry. Parsed by `HQF_Init` / `HQF_LoadClose` in
[LIB386/SYSTEM/HQFILE.CPP](../LIB386/SYSTEM/HQFILE.CPP), decompressed by `ExpandLZ`
([LIB386/SYSTEM/LZ.CPP](../LIB386/SYSTEM/LZ.CPP)) with `MinBloc = CompressMethod + 1`.

Text is loaded straight from disk on demand. There is no in-memory HQR cache for it.

### Banks come in pairs

The only site that opens the file is `InitDial` at
[SOURCES/MESSAGE.CPP:1106](../SOURCES/MESSAGE.CPP#L1106):

```c
MaxText = (U16)(Load_HQR(FileText, BufOrder,
                         (Language * MAX_TEXT_LANG * 2) + (file * 2) + 0) / 2);
Load_HQR(FileText, BufText,
         (Language * MAX_TEXT_LANG * 2) + (file * 2) + 1);
```

That formula is the whole layout. With `NB_LANGUAGES 6` and `MAX_TEXT_LANG 15`
([MESSAGE.CPP:90-91](../SOURCES/MESSAGE.CPP#L90)), the file is 6 x 15 x 2 = **180 entries**:

```
entry (Language * 30) + (file * 2) + 0   ->  order bank   ("which ids, in what slot order")
entry (Language * 30) + (file * 2) + 1   ->  text bank    ("the strings themselves")
```

`file` is the bank index, 0..14, named by `ListFileText[]`
([MESSAGE.CPP:182](../SOURCES/MESSAGE.CPP#L182)):

| file | name | contents |
|---|---|---|
| 0 | `sys` | menus, system text, key names, behaviour names |
| 1 | `cre` | credits (**empty in retail data, all six languages**) |
| 2 | `gam` | generic game text, inventory item names and descriptions |
| 3..14 | `000`..`011` | per-island dialogue; loaded as `InitDial(START_FILE_ISLAND + Island)` |

`Language` selects the block of 30. This is why `TabLanguage[]` carries the original warning
*"Ne pas toucher l'ordre cause HQR"* ([MESSAGE.CPP:171](../SOURCES/MESSAGE.CPP#L171)): the array's
index order **is** the file's entry layout. Reordering it silently reads another language's banks.

### Inside the two banks

```
order bank :  U16 id[MaxText]                     MaxText = entry_size / 2
text  bank :  U16 offset[MaxText + 1]             offsets relative to the bank's own start
              <blob> <blob> ...                   blob = [U8 attr][CP850 bytes][NUL]
```

The two banks are parallel arrays keyed by the same slot number. There is no `{id, offset}` record
type on disk: slot `n` of the order bank names the id, slot `n` of the offset table locates the
string.

Resolution is two steps, `FindText` then `GetText`
([MESSAGE.CPP:419](../SOURCES/MESSAGE.CPP#L419) and
[MESSAGE.CPP:1411](../SOURCES/MESSAGE.CPP#L1411)):

```c
S32 FindText(S32 text) {            // linear scan, O(MaxText)
    pt = BufOrder;
    for (i = 0; i < MaxText; i++)
        if (*pt++ == text) return (i);
    return (-1);
}

S32 GetText(S32 text) {
    num = FindText(text);
    if (num == -1) return (0L);

    offset0 = *(U16 *)(BufText + ((num + 0) * 2L));
    offset1 = *(U16 *)(BufText + ((num + 1) * 2L));

    PtText   = BufText + offset0;
    FlagDial = *PtText++;                //  attribut pour GereFlagDial()
    SizeText = offset1 - offset0 - 1;    //  -1 pour virer octet d'attribut
    return (1L);
}
```

The scan is linear because **the ids are not sorted**. They are in authoring order. In the retail
file, 48 of the 84 populated banks have non-ascending id arrays, so a binary search would be wrong,
not merely unnecessary.

### The attribute byte

The first byte of every blob is not text. It is `FlagDial`, consumed by `GetText` and dispatched by
`GereFlagDial` ([MESSAGE.CPP:1340](../SOURCES/MESSAGE.CPP#L1340)) to pick the dialogue box style and
the voice reverb:

| Flag | Value | Meaning |
|---|---|---|
| `DIAL_DEF` | 1<<0 | normal box (`NormalWinDial`) |
| `DIAL_BIG` | 1<<1 | big box (`BigWinDial`) |
| `DIAL_FUL` | 1<<2 | big box, no frame (full screen) |
| `DIAL_SAY` | 1<<3 | over-head "say" text, no box |
| `DIAL_INT` / `DIAL_EXT` / `DIAL_CAV` | 1<<4/5/6 | voice reverb: interior / exterior / cave |

`MESSAGE.CPP` reinterprets the top bits for its own purposes
([MESSAGE.CPP:43-45](../SOURCES/MESSAGE.CPP#L43)): `DIAL_RAD` (1<<5) radio message, `DIAL_EXP`
(1<<6) inventory explain, `DIAL_DEM` (1<<7) demo text over a PCX. So bits 5 and 6 mean different
things depending on the call path. The base flags live in
[SOURCES/COMMON.H:1366](../SOURCES/COMMON.H#L1366).

The harness can override the byte with `Dial_ForceFlag`
([MESSAGE.H:25](../SOURCES/MESSAGE.H#L25)) to exercise a box style without hunting for a text id
authored with it.

### Verified against the retail asset

Decoding `Common/TEXT.HQR` (442,979 bytes) with the format above:

| Check | Result |
|---|---|
| Entry count | 180, exactly `NB_LANGUAGES * MAX_TEXT_LANG * 2` |
| Compression | 90 stored, 78 LZMIT, 12 empty |
| Empty entries | the `cre` bank pair, in all six languages |
| `offset[0] == (MaxText + 1) * 2` | holds in all 84 populated banks |
| `offset[MaxText] == entry size` | holds in all 84 populated banks |
| Id arrays ascending | only 36 of the 84 (hence the linear scan) |
| Largest `MaxText` | 347 (buffer `BIG_FILE_ORD` holds 512) |
| Largest text bank | 33,852 B (buffer `BIG_FILE_DIA` is 40,000 B) |

Both buffers are allocated once at boot in
[SOURCES/PERSO.CPP:3071](../SOURCES/PERSO.CPP#L3071), sized by `BIG_FILE_DIA` / `BIG_FILE_ORD`
([COMMON.H:39](../SOURCES/COMMON.H#L39)). Note the text-bank headroom is thin: about 15%. A fan
translation with longer strings, or any attempt to extend a bank, runs into a fixed 40 KB ceiling
with no bounds check in `Load_HQR`.

## The language model

```c
const char *TabLanguage[] = {         // Ne pas toucher l'ordre cause HQR
    "English", "Français", "Deutsch", "Español", "Italiano", "Portugues"};
const char *ListLanguage[] = {"EN_", "FR_", "DE_", "SP_", "IT_", "PO_"};
```

Index 0..5. `TabLanguage` is the config-file spelling and the on-screen name; `ListLanguage` is the
prefix for voice files (`FR_003.VOX`). Both orders are load-bearing.

Two globals: `Language` (UI text) and, in CDROM builds, `LanguageCD` (voice). They are set by, in
order:

1. `InitLanguage()` ([MESSAGE.CPP:342](../SOURCES/MESSAGE.CPP#L342)), matching the cfg's `Language`
   key against `TabLanguage[]`. The default is `TabLanguage[1]`, i.e. **Français**.
2. `--language <name>` on the command line ([SOURCES/CONTROL.CPP](../SOURCES/CONTROL.CPP)).
3. The in-game Options menu, `CycleTextLanguage()`
   ([GAMEMENU.CPP:484](../SOURCES/GAMEMENU.CPP#L484)), which calls `ReloadMultiTextFile(0)` so the
   change applies immediately.

`GetLanguageCount()` returns `NB_LANGUAGES`; `GetLanguageName()` maps an index to `TabLanguage[]`,
falling back to index 1 when out of range.

Two traps here:

- **`S32 Language = 1; // English` ([MESSAGE.CPP:168](../SOURCES/MESSAGE.CPP#L168)) is a stale
  comment.** Index 1 is Français. The initializer is overwritten by `InitLanguage()` at boot, so it
  is harmless, but do not trust the comment.
- **`NB_LANGUAGES` is a private `#define` in MESSAGE.CPP.** Nothing outside that translation unit
  can see it as a compile-time constant, so parallel per-language tables elsewhere in the codebase
  (see below) have to restate "6" as a literal and hope.

## Getting a string: `GetMultiText`

For anything that is not a full dialogue session, `GetMultiText(id, dst)`
([MESSAGE.CPP:2283](../SOURCES/MESSAGE.CPP#L2283)) is the API: it returns a plain C string for a
text id in the currently-loaded bank.

```c
char *GetMultiText(S32 text, char *dst) {
    if ((text == NumMultiText) AND(LastFileInit == FileMultiText)) {
        strcpy(dst, BufferMultiText);      // 1-entry memo cache
        return dst;
    }
    if (!GetText(text)) { *dst = 0; return 0L; }

    smax = SizeText - 1;                   // drop the NUL
    if (smax > 255) smax = 255;            // hard truncation
    memmove(dst, PtText, smax);
    dst[smax] = 0;
    ...
}
```

Three things to know:

- **Callers must pass a 256-byte buffer.** The result is silently truncated at 255 characters.
- **A one-entry memo cache** (`BufferMultiText`, keyed on id + bank) sits in front of the linear
  scan. `ReloadMultiTextFile` clears it and forces `InitDial` to re-read, which is what makes an
  in-game language change take effect.
- **A missing id returns `NULL` and writes an empty string.** It does not crash, it just renders
  nothing, which is why a wrong id is easy to miss.

`InitDial(file)` early-outs when the requested bank is already loaded (`LastFileInit == file`), so
calling it per-frame is cheap, but it also means **the "current bank" is global process state**. Ask
for an id that lives in another bank and you get whatever that id happens to mean in the bank you
are in, or nothing.

### Voice is coupled to the id's slot, not to the id

`Speak()` ([MESSAGE.CPP:1018](../SOURCES/MESSAGE.CPP#L1018)) reuses `FindText`'s slot number to index
a parallel seek table read from the `.VOX` file:

```c
num = FindText(text);
if ((num == -1) OR (num >= MaxVoice)) return (0L);
offset = BufMemoSeek[num];
```

So the order bank's slot ordering is also the voice file's record ordering. Reordering a bank
desynchronises every voice line after the edit. This is the strongest reason `TEXT.HQR` is
effectively frozen: it is not one file, it is one file plus 39 `.VOX` files that agree with it
positionally.

## Control codes in the text stream

| Byte | Meaning |
|---|---|
| first byte of blob | the `FlagDial` attribute, not text |
| `0x00` | end of entry |
| `0x01` | forced line break; also suppresses justification for that line |
| `@` | line break |
| `@P` | page break |
| `42` (`CODE_HARD_SPACE`) | fixed-width, non-justifiable space. **Not in the data**: `GetNextWord` injects it to glue punctuation (`! ? ) ; :`) to the preceding word, French-typography style |

There are no colour codes and no voice markers in the stream. Dialogue colour comes from the
speaking object (`TestCoulDial(ListObjet[NumObjDial].CoulObj)`), and voice comes from the slot
index as above.

## Rendering

Two fonts, two codepages, and they share nothing but the framebuffer.

| | Game font (`LbaFont`) | Debug font (`Font8x8`) |
|---|---|---|
| Source | `RESS.HQR` entry 1 (`RESS_FONT_GPM`) | hardcoded array in the binary |
| Codepage | **CP850** | **CP437** |
| Metrics | proportional | fixed 8x8 |
| Draw | `Font()` / `CarFont()` -> `AffMask()` | `AffString()` / `AffStringToBuffer()` |
| Used by | dialogue, menus, inventory, holomap, credits, over-head text | FPS and debug overlays, key-config screen, dev console, touch overlay |

### The game font

Loaded once at boot ([PERSO.CPP:3161](../SOURCES/PERSO.CPP#L3161)) and never reloaded. The bank is a
256-entry `U32` offset table indexed by **the raw byte value**, then per glyph a 4-byte header
(`deltaX`, `nbLine`, `hotX`, `hotY`) and an RLE 1-bit mask.

It is a monochrome *mask*, not a paletted sprite: every set pixel is filled with the single global
colour `ColMask`. That is why `ColorFont()` is free, and why the dialogue can afford to recolour
characters every frame.

The whole API is three functions in [LIB386/SVGA/FONT.CPP](../LIB386/SVGA/FONT.CPP): `SizeFont(str)`
(measure), `CarFont(x, y, c)` (one glyph), `Font(x, y, str)` (a whole string). Centring is always
the same idiom: `Font(x - SizeFont(s) / 2, y, s)`.

Byte 32 (space) is never looked up in the table; its width is the global `InterSpace`. Every other
byte indexes slot `c` directly, unsigned, 0..255. **There is no character mapping.** A CP850 byte
from `TEXT.HQR` indexes a glyph the artist drew at that CP850 position, which is exactly why é and ñ
render without anyone converting anything.

### Encoding: identity for game data, a fold for our strings

Retail text is CP850 bytes and goes to the glyph table untouched.

Strings we add are UTF-8 literals in `.CPP` files, so they must be folded first:
`CopyUtf8ToCp850()` and `FormatUtf8ToCp850()`
([LIB386/SYSTEM/STRING.CPP:244](../LIB386/SYSTEM/STRING.CPP#L244)) decode 1/2/3-byte UTF-8 and map
each codepoint to CP850. Unmapped codepoints become `'?'`; smart quotes, dashes and ellipsis are
transliterated to ASCII.

`CopyUtf8ToCp850` **has no NULL guard** and dereferences `src` immediately. A missing translation in
an in-source table is therefore a segfault, not a blank label. This is what
[tests/menu_labels](../tests/menu_labels) exists to prevent.

The dev console needs CP437 rather than CP850 (different font), so it carries its own separate fold
table, `utf8_to_cp437` in [SOURCES/CONSOLE/CONSOLE.CPP](../SOURCES/CONSOLE/CONSOLE.CPP).

### The dialogue machine

`MESSAGE.CPP` is not a text renderer, it is a small text *engine*, and everything except the menus
goes through it.

- **Box** setup: one of `NormalWinDial` / `BigWinDial` / `HoloWinDial` / `ExplainWinDial` /
  `DemoWinDial` sets `Dial_X0/Y0/X1/Y1`, `MaxLineDial` and `DialMaxSize`. `OpenDial` paints the
  frame and stashes a clean copy of the empty box in `Screen`, so a page turn can restore it
  without redrawing the scene.
- **Wrap and justify**: `GetNextWord` / `GetNextLine` ([MESSAGE.CPP:1582](../SOURCES/MESSAGE.CPP#L1582))
  greedily fill a line to `DialMaxSize`, then widen every space to justify, dribbling out the
  leftover pixels one at a time (`NbBigSpace`). A line ended by `0x01` or `@` is not justified.
- **Typewriter**: `NextDialCar()` ([MESSAGE.CPP:1678](../SOURCES/MESSAGE.CPP#L1678)) is driven by
  `TimerRefHR`, one character per `TIME_DELTA_CAR` (20 ms), so text speed is wall-clock, not
  per-frame. Returns 0 (done), 1 (running), 2 (page full, waiting for a key).
- **The glow**: characters are not drawn once. The last 14 are re-drawn every frame through a colour
  ramp (`StackCar`, `AffAllCar`, `TestCoulDial`), which is the trailing shimmer on Twinsen's speech.
  Each glyph is drawn twice, once in black at +2/+4 for the drop shadow.
- **Pages**: when a page fills, an arrow is drawn and the loop waits for input, then
  `NextDialWindow()` restores the stashed empty box.

`Dial(text, drawscene)` is the modal driver (dialogue, inventory descriptions); `MyDial` handles
choice bubbles; `HOLOGLOB.CPP` reuses `CommonOpenDial` + `NextDialCar` directly without the modal
loop.

### Menus

Menus do not use the dialogue machine. `DrawOneChoice`
([GAMEMENU.CPP:2067](../SOURCES/GAMEMENU.CPP#L2067)) calls `BuildCustomMenuText` to fill a
256-byte buffer, then `SizeFont` + `Font`. A row wider than the menu box scrolls back and forth
(the marquee). `BuildCustomMenuText` ([GAMEMENU.CPP:389](../SOURCES/GAMEMENU.CPP#L389)) is the
switch that decides which of the two text sources a row comes from, and is documented per-row in
[MENU.md](MENU.md).

## Adding text

### A new UI string (the normal case)

Add a row to `LocalizedMenuLabels` in [SOURCES/MENU_LABELS.CPP](../SOURCES/MENU_LABELS.CPP) with all
six translations, add its enum to [MENU_LABELS.H](../SOURCES/MENU_LABELS.H), and draw it with
`CopyUtf8ToCp850` (or `FormatUtf8ToCp850` if it takes arguments).
[tests/menu_labels](../tests/menu_labels) pins the row's shape: id equals index, no NULL or empty
translation, and identical printf specifiers across all six languages.

Do not reach for `GetMultiText` with a new id. There is no such id.

### A new language

This is a much bigger job than it looks:

1. `NB_LANGUAGES`, `TabLanguage[]`, `ListLanguage[]` in `MESSAGE.CPP`.
2. **30 new entries in `TEXT.HQR`**, in the right block, or `InitDial` reads off the end of the
   file's offset table.
3. 13 new `.VOX` files, positionally in sync with the new order banks, or voice desynchronises.
4. A seventh column in `LocalizedMenuLabels`, and in every other per-language table (below).

Steps 2 and 3 are the blockers, and they are why the port has six languages and will keep having
six.

### Gotchas worth knowing before you touch any of this

- **`SetFont` is global state and the dialogue box changes it permanently.** Boot calls
  `SetFont(LbaFont, 2, 8)` ([PERSO.CPP:3166](../SOURCES/PERSO.CPP#L3166)); `CommonOpenDial` calls
  `SetFont(LbaFont, INTER_LEAVE, INTER_SPACE)`, i.e. `(2, 7)`
  ([MESSAGE.CPP:1564](../SOURCES/MESSAGE.CPP#L1564)), and never restores it. `InterSpace` is a
  global, so **after the player's first line of dialogue, every `Font()` and `SizeFont()` in the
  process uses a 7px space instead of 8px**, menus included. The main menu at boot and the pause
  menu mid-game measure differently. This is original behaviour, not a port regression (the credits
  once re-set it, but that code is commented out at
  [CREDITS.CPP:209](../SOURCES/CREDITS.CPP#L209)).
- **Accented characters travel through signed `char`.** `MESSAGE.CPP` has a standing FIXME at
  [MESSAGE.CPP:1777](../SOURCES/MESSAGE.CPP#L1777): `*PtLine` is negative for any byte above 127.
  `GetDxDyMask` and `CarFont` take `U8`, so the conversion is well-defined today, but the whole
  accent path is one signed/unsigned slip away from a bad glyph lookup.
- **`GetMultiText` truncates at 255 characters, silently.**
- **`LocalizedMenuLabels` is not the only per-language table in source.** `footerOneLine[6]` in
  [SOURCES/CONFIG.CPP:526](../SOURCES/CONFIG.CPP#L526) (the key-config footer) is a second one, in
  the older one-string-per-language layout, untested, and bounds-checked against
  `GetLanguageCount()` rather than against its own size. If `NB_LANGUAGES` ever becomes 7, that
  indexes past the end.

## Where things live

| Concern | File |
|---|---|
| Bank loading, id resolution, dialogue engine | [SOURCES/MESSAGE.CPP](../SOURCES/MESSAGE.CPP) |
| Public text API | [SOURCES/MESSAGE.H](../SOURCES/MESSAGE.H) |
| `FlagDial` attribute flags, buffer sizes | [SOURCES/COMMON.H](../SOURCES/COMMON.H) |
| HQR container | [LIB386/SYSTEM/HQFILE.CPP](../LIB386/SYSTEM/HQFILE.CPP) |
| Game font (CP850) | [LIB386/SVGA/FONT.CPP](../LIB386/SVGA/FONT.CPP), [MASK.CPP](../LIB386/SVGA/MASK.CPP) |
| Debug font (CP437) | [LIB386/SVGA/AFFSTR.CPP](../LIB386/SVGA/AFFSTR.CPP) |
| UTF-8 to CP850 fold | [LIB386/SYSTEM/STRING.CPP](../LIB386/SYSTEM/STRING.CPP) |
| Community menu strings | [SOURCES/MENU_LABELS.CPP](../SOURCES/MENU_LABELS.CPP) |
| Menu text routing | `BuildCustomMenuText`, [SOURCES/GAMEMENU.CPP](../SOURCES/GAMEMENU.CPP) |

## Testing text

- `tests/menu_labels` pins the in-source table's contract (host test, links the real table).
- The console can drive the real dialogue path headlessly:
  `ui dialog <text-id> <path.png>` opens a bubble for an id in the current bank and screenshots it
  once the typewriter finishes a page ([CONSOLE.md](CONSOLE.md)).
- `Dial_ForceFlag` overrides the attribute byte so a given box style can be exercised without
  finding an id authored with it.
- Nothing currently covers `GetMultiText` id routing or `CopyUtf8ToCp850` itself.
