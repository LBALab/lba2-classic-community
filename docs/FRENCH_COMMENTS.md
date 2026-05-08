# French comments

Little Big Adventure 2 was developed by Adeline Software International in Lyon, France. The developers wrote their comments, debug messages, and variable names in French throughout the codebase. Many of these go beyond dry technical notes -- they show real personality, humor, frustration, and the everyday culture of a 1990s game studio under deadline pressure.

This document preserves a curated selection of the most interesting French comments, with English translations that try to capture the original tone and vibe.

**Attribution:** The French comments and strings documented here are from the original Adeline / lba2-classic codebase. They appear in C++ files that existed in the original release (e.g. PERSO.CPP, WAGON.CPP, SAVEGAME.CPP, OBJECT.CPP, BEZIER.CPP) or were preserved when porting ASM to C++. They reflect the historical 1990s Adeline team, not the community.

---

## "Magouille" -- the team's favorite word

The word **magouille** (roughly: hack, kludge, sneaky workaround) appears at least six times across the codebase. It was clearly the team's go-to term for code they knew wasn't clean but needed to ship. In French, "magouille" carries more personality than the English "hack" -- it implies something slightly underhanded, like a dodgy shortcut you hope nobody notices.

**`SOURCES/WAGON.CPP:80`**
```
// ATTENTION : Grosse Magouille :
// Le champ Sprite des objets de type Wagon (forcement des
// objets 3D) est utilisé dans les aiguillages pour savoir
// dans quel sens on le prend. Cela repare un bug lorqu'on
// tournait dans un aiguillage et qu'on modifiait en meme
// temps le sens de l'aiguillage !
```
> "WARNING: Big Hack:
> The Sprite field of Wagon objects (which are always 3D objects) is repurposed in railway switches to know which direction you're taking. This fixes a bug where you'd turn at a switch while simultaneously changing the switch direction!"

**`SOURCES/WAGON.CPP:9`**
```
// Magouille pour ne pas ecraser les groupes dans AffOneObjet()
```
> "Hack so we don't overwrite the groups in AffOneObjet()"

**`SOURCES/PERSO.CPP:357`**
```
// Magouille pour ne pas avoir la gestion des extras, pof ...
```
> "Hack to skip the extras management, poof..."

The "pof" at the end is an onomatopoeia -- the sound of something vanishing, like a dismissive hand wave.

**`SOURCES/SAVEGAME.CPP:1408`**
```
// Magouille pour éviter de changer le format des sauvegardes
// Pourrait etre changé en version finale en sauvant ProtectActif
```
> "Hack to avoid changing the save file format. Could be changed in the final version by saving ProtectActif..."

Spoiler: the final version shipped with the hack.

**`SOURCES/SAVEGAME.CPP:1200`**
```
// Magouille : Pour récupérer le body du heros lors d'un
// START_POS dans EDITLBA2, il est sauvé dans le dernier
```
> "Hack: To recover the hero's body during a START_POS in EDITLBA2, it's saved in the last..."

---

## Developer frustration

**`SOURCES/PERSO.CPP:1953`**
```
// Trouver le moyen de cleaner cette @#&? de fonction !!!!!!
```
> "Find a way to clean up this @#&? function!!!!!!"

A developer censoring their own profanity in a comment -- the grawlix (`@#&?`) speaks volumes.

**`SOURCES/PERSO.CPP:2074-2075`**
```
// pour virer warnings !!!! Baaahhhh ! Que j'aime pas ça, c'est pas
// beau !!! mais ça marche et ça ne génère pas de code !!!
```
> "to get rid of warnings!!!! Baaahhhh! I really hate this, it's ugly!!! but it works and doesn't generate any code!!!"

The "Baaahhhh!" is a French exclamation of disgust -- imagine a developer throwing their hands up at the screen.

**`SOURCES/OBJECT.CPP:432-433`**
```
ObjectClear( &(ptrobj->Obj) ) ;     // init structure 3D: TRES
                                     // IMPORTANT, A NE PAS VIRER !!!!
```
> "init 3D structure: VERY IMPORTANT, DO NOT REMOVE!!!!"

**`SOURCES/OBJECT.CPP:405-406`**
```
// Passer tout ca en DEFINE !!!!!!!!!! Impossible, ce sont des pointeurs
// de fonctions (sauf peut-etre GivePtrObjFix et GivePtrSample
```
> "Convert all this to DEFINEs!!!!!!!!!! Impossible, they're function pointers (except maybe GivePtrObjFix and GivePtrSample)"

The exclamation count (ten!) followed by immediate self-defeat.

**`SOURCES/PERSO.CPP:2274`**
```
// a Faire Imperativement avant LoadFicPerso() !!!!!!!!
```
> "MUST be done before LoadFicPerso()!!!!!!!!"

**`SOURCES/PERSO.CPP:89`**
```
// Pour eviter d'ecraser les anciennes copies d'ecran !!!!
```
> "To avoid overwriting the old screenshots!!!!"

**`SOURCES/SAVEGAME.CPP:93`**
```
// Mettre un moyen plus clean !!!!!!
```
> "Find a cleaner way!!!!!!"

**`SOURCES/FIRE.CPP:26`** (in ASM)
```
; grossse optimisations à faire ......
```
> "huuuge optimizations to do......"

The triple-s in "grossse" mirrors the exhaustion of the ellipsis. This comment sits right after the FIRE banner, as if the developer stopped to sigh before writing the function.

---

## Colloquial French / humor

**`SOURCES/PERSO.CPP:30`**
```
extern S32  PersoFlashTimer ;  // pour quand le perso eh ben y meurt !!!
```
> "for when the character, well, he just up and dies!!!"

"Eh ben y meurt" is very informal spoken French -- the kind of thing you'd say to a colleague across the desk, not write in documentation. The "eh ben" is like "well, y'know."

**`SOURCES/PERSO.CPP:508`**
```
// au cas ou le mec meurre avant de faire <ESC>
```
> "in case the dude croaks before pressing ESC"

"Le mec" is casual slang for "the guy/dude." "Meurre" is a misspelling of "meurt" (dies) -- writing too fast to spell-check.

**`SOURCES/OBJECT.CPP:3797`**
```
else  /* pas SPRITE_3D donc obj articulés (Avez Vous Donc Une Ame? ;)*/
```
> "not SPRITE_3D, so articulated objects (So Do You Have A Soul? ;)"

A philosophical aside wondering whether 3D articulated objects have souls. The phrasing echoes formal literary French.

**`SOURCES/PERSO.CPP:593`**
```
// Faut-il le laisser ? (non, à mon avis !)
```
> "Should we keep it? (no, in my opinion!)"

A developer voting in a comment. The parenthetical feels like a whispered aside.

**`SOURCES/OBJECT.CPP:562-564`**
```
// Faut-il le laisser ?
// Cela sert a virer le compteur du pingouin si on meurt
// pendant qu'il est actif
```
> "Should we keep it? This removes the penguin counter if you die while it's active."

The recurring "faut-il le laisser?" suggests this was an actual team discussion pattern -- leaving questions in the code for colleagues.

---

## In-game French strings

Some debug and warning messages were left in French, giving us a window into the developer-facing side of the game.

**`SOURCES/OBJECT.CPP:1496`**
```
Message( "Warning: Sauvegarde trop ancienne. Je vais tenter de la charger, mais méfie !!", TRUE ) ;
```
> "Warning: Save file too old. I'll try to load it, but watch out!!"

The casual "méfie" (watch yourself) gives this system message unexpected personality.

**`LIB386/MENU/SELECTOR.CPP:544-545`**
```
strcat(workstring, " existe déjà !");
if (Confirm(workstring, "Ecrase", "Oups") == 1)
```
> "[filename] already exists!" / buttons: "Overwrite" / "Oops"

"Oups" as the cancel button label is delightful.

**`SOURCES/VALIDPOS.CPP:37`**
```
Message( "Warning: BufferValidePos trop petit (ça a dû patché) !", TRUE ) ;
```
> "Warning: BufferValidePos too small (must've been patched)!"

"Ca a du patche" mixes French grammar with the English loanword "patche" -- classic bilingual developer speak.

**`SOURCES/PERSO.CPP:458`**
```
GraphPrintf( TRUE, 0, 0, "STOP: Patch Inputs trouvé !!!", TRUE ) ;
```
> "STOP: Input Patch found!!!"

---

## Cultural references

**`SOURCES/BEZIER.CPP:10`**
```
(merci Time Commando !!!)
```
> "(thanks Time Commando!!!)"

Time Commando (1996) was another game by Adeline Software. This credit, nestled inside the BEZIER banner, acknowledges that the Bezier curve shadow code was shared (or borrowed) from that project.

---

## Detail Level: Machine de Bof to Machine de Folie

The Options menu's Detail Level slider (graphics quality) is annotated with informal French that maps each tier to 1997-era hardware. The progression reads like a developer ranking their players' PCs:

**`SOURCES/GAMEMENU.CPP:462-490`** (`SetDetailLevel` switch)
```
case 0: // machine bof !!!!!!
	case 1: // 486 ?
	case 2: // Pentium de base ?
	default:// machine de folie !!!!!
```

> "case 0: crap machine!!!!!!"  
> "case 1: 486?"  
> "case 2: base Pentium?"  
> "default: crazy machine!!!!!"

*Machine bof* is slang for a crummy machine (*bof* = "meh"). *Machine de folie* (crazy machine) is the opposite — a beast of a PC. The question marks on 486 and Pentium suggest the devs weren't 100% sure which CPUs would hit those tiers; the exclamation marks on "bof" and "folie" leave no doubt about the emotional extremes. See [MENU.md](MENU.md) for the menu structure.

---

## Notable French variable and function names

| Name | File | Translation |
|------|------|-------------|
| `Comportement` | COMPORTE.CPP | "Behaviour" -- the AI scripting system |
| `Magouille` | various | "Hack/kludge" (see above) |
| `TabVirgule` | PLASMA.CPP | "Comma table" -- interpolation lookup values |
| `MécaPingouin` | CONFIG/ | "Mechanical Penguin" -- key binding for a rideable vehicle |
| `AdjustEssieuWagonAvant` | WAGON.CPP | "Adjust front wagon axle" |
| `AdjustEssieuWagonArriere` | WAGON.CPP | "Adjust rear wagon axle" |
| `Fléchettes` | DART.CPP / PERSO.CPP | "Darts" -- the blowgun weapon |
| `Sarbacane` / `Sarbatron` | CLAVIER.TXT | "Blowpipe" / "Sarbatron" (sci-fi blowpipe) |
| `Grille` | GRILLE.CPP | "Grid" -- the brick/tile system |
| `Conque` | CLAVIER.TXT | "Conch shell" -- a magic item |
| `Gant` | CLAVIER.TXT | "Glove" -- the magic glove weapon |
| `PistoLaser` | CLAVIER.TXT | "Laser Pistol" |
| `Protopack` / `Jetpack` | CLAVIER.TXT | Twinsen's jetpack |
| `Esquives` | CLAVIER.TXT | "Dodges" -- evasion moves |
| `CubeMode` | DISKFUNC.CPP | With comment: `0-->Intérieur - 1-->Extérieur` ("Interior / Exterior") |
| `Tri Intérieur` | SORT.CPP | "Interior Sort" -- the depth sorting algorithm |
| `GereLife` | GERELIFE.CPP | "Manage Life" -- the life script interpreter |
| `GereTrack` | GERETRAK.CPP | "Manage Track" -- the track/path system |
| `Décors` | DECORS.CPP | "Scenery/Decor" -- background rendering |

---

## Abbreviated French

The developers frequently abbreviated words in a way that reflects rapid informal writing:

| Abbreviation | Full French | Translation | File |
|-------------|-------------|-------------|------|
| `tjrs` | toujours | "always" | GERELIFE.CPP:1193 |
| `cond` | condition | "condition" | GERELIFE.CPP:1193 |
| `dep` | départ/déplacement | "start/movement" | EXTFUNC.CPP |
| `perso` | personnage | "character" | everywhere |
| `anim` | animation | "animation" | everywhere |
| `ptr` | pointeur | "pointer" | everywhere |
| `swif` | switch (English) | misspelling | GERELIFE.CPP:1193 |

The line `/* swif inversé tjrs jump jusqu'a cond inverse */` in GERELIFE.CPP packs an impressive amount of abbreviation into a single comment: *"reversed switch, always jump until inverse condition."*
