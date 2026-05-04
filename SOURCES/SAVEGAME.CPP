/*══════════════════════════════════════════════════════════════════════════*
	     ██▀▀▀ █▀▀▀█ █   █ █▀▀▀▀       █▀▀▀▀ █▀▀▀█ █▄ ▄█ █▀▀▀▀
	     ▀▀▀▀█ ██▀▀█ ██ ▄▀ ██▀▀        ██ ▀█ ██▀▀█ ██▀ █ ██▀▀
	     ▀▀▀▀▀ ▀▀  ▀ ▀▀▀   ▀▀▀▀▀       ▀▀▀▀▀ ▀▀  ▀ ▀▀  ▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

#include "C_EXTERN.H"

#include <stdint.h>

#include <SVGA/SCREEN.H>
#include "DIRECTORIES.H"
#include "INPUT.H"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SYSTEM/LOADSAVE.H>
#include <SYSTEM/LZ.H>
#include "SAVEGAME_LOAD_BOUNDS.H"

#include <OBJECT/AFF_OBJ.H>


extern char PlayerName[MAX_SIZE_PLAYER_NAME + 1];

extern S32 LastMyJoy;
extern S32 LastMyFire;
extern S32 LastJoyFlag;

#ifndef EDITLBA2
#ifdef __cplusplus
extern "C" U8 *PtrLib3DBufferAnim; // P_ANIM.ASM (lib_3D)
#else
extern U8 *PtrLib3DBufferAnim; // P_ANIM.ASM (lib_3D)
#endif

#pragma aux PtrLib3DBufferAnim "*"
#endif

extern S32 InvSelect; // GAMEMENU.C

U8 *PtrSave;

#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS) || defined(EDITLBA2)
typedef struct {
    S32 v[4];
} T_REAL_VALUE_HR;
T_REAL_VALUE_HR TempoRealAngle;
S32 LastStepFalling;
S32 LastStepShifting;
#endif

#ifdef DEMO
S32 CurrentDemoSave = 0;
S32 NewDemoSave = 0;

char *ListDemoSave[NB_DEMOSAVE] = {
    "demo0.lba",
    "demo1.lba",
    "demo2.lba"};
#endif

/*══════════════════════════════════════════════════════════════════════════*
    █▀▀█  █   █ █▀▀▀▀ █▀▀▀▀ █▀▀▀▀ █▀▀▀█       █▀▀▀▀ █   █ ██▄ █ █▀▀▀▀ ██▀▀▀
    ██▀▀█ ██  █ ██▀▀  ██▀▀  ██▀▀  ██▀█▀       ██▀▀  ██  █ ██▀██ ██    ▀▀▀▀█
    ▀▀▀▀▀ ▀▀▀▀▀ ▀▀    ▀▀    ▀▀▀▀▀ ▀▀  ▀       ▀▀    ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
#define PtrOpen() PtrSave = Screen

//-------- Lecture
#define LbaRead(ptr, size)      \
    memcpy(ptr, PtrSave, size); \
    PtrSave += size;

#define LbaReadByte(ptr) ptr = *PtrSave++;

#define LbaReadWord(ptr)   \
    ptr = *(S16 *)PtrSave; \
    PtrSave += 2;

#define LbaReadLong(ptr)   \
    ptr = *(S32 *)PtrSave; \
    PtrSave += 4;

//-------- Ecriture
#define LbaWrite(ptr, size)     \
    memcpy(PtrSave, ptr, size); \
    PtrSave += size;

#define LbaWriteByte(val) *PtrSave++ = val;

#define LbaWriteWord(val)  \
    *(S16 *)PtrSave = val; \
    PtrSave += 2;

#define LbaWriteLong(val)  \
    *(S32 *)PtrSave = val; \
    PtrSave += 4;

#ifndef EDITLBA2
static U8 *g_save_read_limit = NULL;

void SaveLoadSetReadLimit(U8 *end_exclusive) {
    g_save_read_limit = end_exclusive;
}

void SaveLoadClearReadLimit(void) {
    g_save_read_limit = NULL;
}

#define SAVELOAD_CTX_ERR ((S32)(-1))

static S32 SaveLoadRoom(size_t n) {
    if (!g_save_read_limit)
        return TRUE;
    if ((U8 *)(uintptr_t)((uintptr_t)PtrSave + n) < PtrSave)
        return FALSE;
    return (size_t)(g_save_read_limit - PtrSave) >= n;
}

static S32 SaveLoadReadPlayerName(void) {
    S32 i;
    for (i = 0; i <= MAX_SIZE_PLAYER_NAME; i++) {
        if (!SaveLoadRoom(1))
            return FALSE;
        {
            U8 b = *PtrSave++;
            if (i < MAX_SIZE_PLAYER_NAME)
                PlayerName[i] = (char)b;
            if (b == 0) {
                PlayerName[MAX_SIZE_PLAYER_NAME] = 0;
                return TRUE;
            }
        }
    }
    PlayerName[MAX_SIZE_PLAYER_NAME] = 0;
    return FALSE;
}

/* Returns bytes read, or 0 on failure. */
static S32 SaveLoadFileToScreen(char *path) {
    U32 cap = SaveLoadScreenBufferBytes();
    S32 n = LoadSize(path, (U8 *)Screen, (S32)cap);
    if (n <= 0 || n == (S32)INT_MIN)
        return 0;
    PtrSave = (U8 *)Screen;
    SaveLoadSetReadLimit((U8 *)Screen + cap);
    return n;
}

static S32 SaveLoadFileToBufSpeakSave(void) {
    U32 cap = SaveLoadSubbufferBytesFromLeadingSkip(50000u);
    U8 *base = BufSpeak + 50000L;
    S32 n = LoadSize(GamePathname, base, (S32)cap);
    if (n <= 0 || n == (S32)INT_MIN)
        return 0;
    PtrSave = base;
    SaveLoadSetReadLimit(base + cap);
    return n;
}

static S32 SaveLoadDecompressIfNeeded(U8 *buf_base, U32 buf_cap, S32 loaded_bytes) {
    U8 *ptrdecomp;
    S32 sizefile;
    S32 compressed_size;

    if (!(NumVersion & SAVE_COMPRESS))
        return TRUE;

    if (!SaveLoadRoom(sizeof(S32)))
        return FALSE;
    memcpy(&sizefile, PtrSave, sizeof(S32));
    PtrSave += sizeof(S32);

    compressed_size = (S32)((buf_base + (U32)loaded_bytes) - PtrSave);
    if (compressed_size < 0)
        return FALSE;

    if (!SaveLoadValidateCompressedStaging(buf_base, buf_cap, PtrSave, (U32)compressed_size, sizefile))
        return FALSE;

    ptrdecomp = PtrSave + sizefile + RECOVER_AREA;
    memcpy(ptrdecomp, PtrSave, (size_t)compressed_size);
    ExpandLZ(PtrSave, ptrdecomp, (U32)sizefile, 2);
    return TRUE;
}

#pragma pack(push, 1)
typedef struct {
    S32 X;
    S32 Y;
    S32 Z;
    S32 Alpha;
    S32 Beta;
    S32 Gamma;
    S32 BodyNum;
    S32 NextBodyNum;
    S32 AnimNum;
    U32 Texture;
    U32 NextTexture;
    U32 LastOfsIsPtr;
    S32 LastFrame;
    U32 LastOfsFrame;
    U32 LastTimer;
    U32 LastNbGroups;
    S32 NextFrame;
    U32 NextOfsFrame;
    U32 NextTimer;
    U32 NextNbGroups;
    S32 LoopFrame;
    U32 LoopOfsFrame;
    U32 NbFrames;
    S32 LastAnimStepX;
    S32 LastAnimStepY;
    S32 LastAnimStepZ;
    S32 LastAnimStepAlpha;
    S32 LastAnimStepBeta;
    S32 LastAnimStepGamma;
    U32 Interpolator;
    U32 Time;
    U32 Status;
    U32 Master;
    U32 NbGroups;
} T_OBJ_3D_WIRE32;
#pragma pack(pop)

static void SavegameObj3dFromWire32(const T_OBJ_3D_WIRE32 *s, T_OBJ_3D *d) {
    memset(d, 0, sizeof(*d));
    d->X = s->X;
    d->Y = s->Y;
    d->Z = s->Z;
    d->Alpha = s->Alpha;
    d->Beta = s->Beta;
    d->Gamma = s->Gamma;
    d->Body.Num = s->BodyNum;
    d->NextBody.Num = s->NextBodyNum;
    d->Anim.Num = s->AnimNum;
    d->Texture = NULL;
    d->NextTexture = NULL;
    d->LastOfsIsPtr = s->LastOfsIsPtr;
    d->LastFrame = s->LastFrame;
    d->LastOfsFrame = (PTR_U32)(uintptr_t)(size_t)s->LastOfsFrame;
    d->LastTimer = s->LastTimer;
    d->LastNbGroups = s->LastNbGroups;
    d->NextFrame = s->NextFrame;
    d->NextOfsFrame = (PTR_U32)(uintptr_t)(size_t)s->NextOfsFrame;
    d->NextTimer = s->NextTimer;
    d->NextNbGroups = s->NextNbGroups;
    d->LoopFrame = s->LoopFrame;
    d->LoopOfsFrame = s->LoopOfsFrame;
    d->NbFrames = s->NbFrames;
    d->LastAnimStepX = s->LastAnimStepX;
    d->LastAnimStepY = s->LastAnimStepY;
    d->LastAnimStepZ = s->LastAnimStepZ;
    d->LastAnimStepAlpha = s->LastAnimStepAlpha;
    d->LastAnimStepBeta = s->LastAnimStepBeta;
    d->LastAnimStepGamma = s->LastAnimStepGamma;
    d->Interpolator = s->Interpolator;
    d->Time = s->Time;
    d->Status = s->Status;
    d->Master = s->Master;
    d->NbGroups = s->NbGroups;
}
#endif /* !EDITLBA2 */

/*──────────────────────────────────────────────────────────────────────────*/
// Resultat pas terrible
#ifndef EDITLBA2
void RemapPicture(U8 *buf, U32 size) {
    S32 n;
    U8 newpal[256];
    U8 *ptr;
    U8 r, v, b;

    //	if( PtrPal==PtrPalNormal )	return ;
    // Mettre un moyen plus clean !!!!!!
    if (!memcmp(PtrPal, PtrPalNormal, 768))
        return;

    ptr = PtrPal;
    for (n = 0; n < 768; n++)
        *ptr++ >>= 2;

    ptr = PtrPalNormal;
    for (n = 0; n < 768; n++)
        *ptr++ >>= 2;

    //	ptr = PtrPal+10*3 ;
    ptr = PtrPal;
    //	for( n=10; n<245; n++ )
    for (n = 0; n < 255; n++) {
        // ATTENTION: ne pas passer les (*ptr++) directement en
        // parametre sinon, l'empilage du C passerait b,v,r
        r = *ptr++;
        v = *ptr++;
        b = *ptr++;

        newpal[n] = SearchBoundColRGB(r, v, b,
                                      PtrPalNormal, 0, 255);
        //		newpal[n] = SearchBoundColRGB( r, v, b,
        //					       PtrPalNormal, 10, 245 ) ;
    }

    for (n = 0; n < size; n++, buf++) {
        *buf = newpal[*buf];
    }

    ptr = PtrPal;
    for (n = 0; n < 768; n++)
        *ptr++ <<= 2;

    ptr = PtrPalNormal;
    for (n = 0; n < 768; n++)
        *ptr++ <<= 2;
}

#ifdef DEMO
S32 SetDemoSaveGame(S32 num) {
    NewDemoSave = CurrentDemoSave = num;

    if (num < NB_DEMOSAVE) {
        GetSavePath(GamePathname, ADELINE_MAX_PATH, ListDemoSave[CurrentDemoSave]);
        return TRUE;
    }

    return FALSE;
}
#endif // DEMO

/*══════════════════════════════════════════════════════════════════════════*
	  █▀▀▀▄  █    ██▀▀▀ █  ▄▀       █▀▀▀▀ █   █ ██▄ █ █▀▀▀▀ ██▀▀▀
	  ██  █  ██   ▀▀▀▀█ ██▀▄        ██▀▀  ██  █ ██▀██ ██    ▀▀▀▀█
	  ▀▀▀▀   ▀▀   ▀▀▀▀▀ ▀▀  ▀       ▀▀    ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/
// Version Jeu uniquement
void AutoSaveGame() {
#ifndef DEMO
    U8 memonumversion = NumVersion;
    char memoplayername[MAX_SIZE_PLAYER_NAME + 1];
    char memogamepathname[ADELINE_MAX_PATH];

    if (DemoSlide OR PLAY_THE_END)
        return;

    SaveTimer();

    strcpy(memoplayername, PlayerName);
    strcpy(memogamepathname, GamePathname);

    GetSavePath(GamePathname, ADELINE_MAX_PATH, AUTOSAVE_FILENAME);
    strcpy(PlayerName, AUTOSAVE_NAME);

#ifdef DEBUG_TOOLS
    {
        // ATTENTION : tempo uniquement interne a adeline
        char *pt;

        DefFileBufferInit("C:\\DOS\\EXTEND\\NAME.ME", BufSpeak, 640 * 480);

        pt = DefFileBufferReadString("Poste");

        if (pt AND * pt) {
            strcat(PlayerName, " ");
            strncat(PlayerName, pt, MAX_SIZE_PLAYER_NAME - 9);
        }
    }
#endif

    NumVersion = NUM_VERSION; // Sauvegarde non compactee

    SaveGame(FALSE);

    NumVersion = memonumversion;
    strcpy(PlayerName, memoplayername);
    strcpy(GamePathname, memogamepathname);

    RestoreTimer();
#endif // DEMO
}

/*──────────────────────────────────────────────────────────────────────────*/
void CurrentSaveGame() {
    U8 memonumversion = NumVersion;
    char memoplayername[MAX_SIZE_PLAYER_NAME + 1];
    char memogamepathname[ADELINE_MAX_PATH];

    if (DemoSlide OR PLAY_THE_END)
        return;

    SaveTimer();

    strcpy(memoplayername, PlayerName);
    strcpy(memogamepathname, GamePathname);

    GetSavePath(GamePathname, ADELINE_MAX_PATH, CURRENTSAVE_FILENAME);
    strcpy(PlayerName, CURRENTSAVE_NAME);

#ifdef DEBUG_TOOLS
    {
        // ATTENTION : tempo uniquement interne a adeline
        char *pt;

        DefFileBufferInit("C:\\DOS\\EXTEND\\NAME.ME", BufSpeak, 640 * 480);

        pt = DefFileBufferReadString("Poste");

        if (pt AND * pt) {
            strcat(PlayerName, " ");
            strncat(PlayerName, pt, MAX_SIZE_PLAYER_NAME - 9);
        }
    }
#endif

    NumVersion = NUM_VERSION; // Sauvegarde non compactee

    SaveGame(FALSE);

    NumVersion = memonumversion;
    strcpy(PlayerName, memoplayername);
    strcpy(GamePathname, memogamepathname);

    RestoreTimer();
}
#endif // EDITLBA2

/*──────────────────────────────────────────────────────────────────────────*/
void SaveGame(S32 flagmess) {
    U8 *sizeptr;
    U8 *memoptr;
    S32 sizefile = 0;
#ifndef EDITLBA2
    S32 savetimerrefhr;
    //	char string[30] ;
    //	S32	x0, y0, x1, y1 ;

    // Image 160x120
    ScaleBox(0, 0, 639, 479, Log, 0, 0, 159, 119, BufSpeak + 50000L); // old Screen
    SaveBlock(BufSpeak + 50000L, BufSpeak + 150000L, 0, 0, 159, 119);
    RemapPicture(BufSpeak + 150000L, 160 * 120);

    if (flagmess) {
/*		strcpy( string, "Sauvegarde en cours ..." ) ;
		ColorFont( LBAWHITE ) ;
		// cadre
		x0 = 320 - SizeFont( string )/2 - 50 ;
		x1 = 320 + SizeFont( string )/2 + 50 ;
		y0 = 240-25 ;
		y1 = 240+25 ;
		ShadeBoxBlk( 0, 0, 639, 479, SCREEN_SHADE_LVL ) ;
		BackupAngles( x0, y0, x1, y1 ) ;
		ShadeBoxBlk( x0, y0, x1, y1, MENU_SHADE_LVL ) ;
		DrawCadre( x0, y0, x1, y1, ALL_ANGLES ) ;
		RestoreAngles( x0, y0, x1, y1 ) ;
		Font( x0+50, y0+7, string ) ;
		BoxStaticFullflip() ;
*/	}

RestoreTimer();
savetimerrefhr = TimerRefHR;
SaveTimer();

PtrSave = BufSpeak + 50000L; // old Screen
#else
    PtrSave = (U8 *)Screen;
#endif

if (flagmess) // non AUTOSAVE ni CURRENTSAVE
{
    NumVersion &= SAVE_COMPRESS; // ecrase old num version
    NumVersion |= NUM_VERSION;
}

LbaWriteByte(NumVersion);

#ifdef EDITLBA2
LbaWriteLong(NewCube);
#else
    LbaWriteLong(NumCube);
#endif

LbaWrite(PlayerName, strlen(PlayerName) + 1);

if (NumVersion & SAVE_COMPRESS) {
    sizeptr = PtrSave;
    LbaWriteLong(sizefile);
    memoptr = PtrSave;
}

#ifndef EDITLBA2
LbaWrite(BufSpeak + 150000L, 160 * 120);
#else
    LbaWrite(PtrScreenSave, 160 * 120);
#endif

#ifdef EDITLBA2
SaveContexte(KeepTimerRefHR);
#else
    SaveContexte(savetimerrefhr);
#endif

// sauve position valide si nécessaire
LbaWriteLong(ValidePos);
LbaWriteLong(LastValidePos);

if (!ValidePos) {
    LbaWriteLong(ValideCube);
    LbaWriteLong(SizeOfBufferValidePos);
    LbaWrite(BufferValidePos, SizeOfBufferValidePos);
}

if (NumVersion & SAVE_COMPRESS) // Version compactee
{
    S32 size;

    sizefile = PtrSave - memoptr;

    size = Compress_LZSS((char *)memoptr, (char *)PtrSave, sizefile);
    memcpy(memoptr, PtrSave, size);

    PtrSave = sizeptr;
    LbaWriteLong(sizefile);
    PtrSave = memoptr + size;
}

#ifndef EDITLBA2
Save(GamePathname, BufSpeak + 50000L, PtrSave - (BufSpeak + 50000L));
#else
    Save(GamePathname, (U8 *)Screen, PtrSave - (U8 *)Screen);
#endif
}

// Version 4 : LBA II only !! (LoadSize + read limit via SaveLoadFileToScreen)

#ifndef EDITLBA2
/*──────────────────────────────────────────────────────────────────────────*/
void LoadGameNumCube() {
    U8 numversion;
    S32 loaded;

    loaded = SaveLoadFileToScreen(GamePathname);
    if (!loaded) {
        SaveLoadClearReadLimit();
        return;
    }

    LbaReadByte(numversion); // num version
    LbaReadLong(NewCube);

    if (!SaveLoadReadPlayerName()) {
        SaveLoadClearReadLimit();
        return;
    }

    NumVersion = numversion;
    if (!SaveLoadDecompressIfNeeded((U8 *)Screen, SaveLoadScreenBufferBytes(), loaded)) {
        SaveLoadClearReadLimit();
        return;
    }

    PtrSave += 160 * 120; // Skip Image

    // list Var game
    if (!SaveLoadRoom((size_t)MAX_VARS_GAME * sizeof(ListVarGame[0]))) {
        SaveLoadClearReadLimit();
        return;
    }
    LbaRead(ListVarGame, MAX_VARS_GAME * sizeof(ListVarGame[0]));

    SaveLoadClearReadLimit();
}

/*──────────────────────────────────────────────────────────────────────────*/
void LoadGamePlayerName() {
    U8 wbyte;
    S32 newcube;

#ifdef EDITLBA2
    PtrSave = BufTempo;
#else
    if (!SaveLoadFileToBufSpeakSave())
        return;
#endif

#ifdef EDITLBA2
    Load(GamePathname, PtrSave);
#endif

    LbaReadByte(wbyte); // num version
    LbaReadLong(newcube);

#ifndef EDITLBA2
    if (!SaveLoadReadPlayerName()) {
        SaveLoadClearReadLimit();
        return;
    }
    SaveLoadClearReadLimit();
#else
    {
        U8 *ptr = (U8 *)PlayerName;
        do {
            LbaReadByte(wbyte);
            *ptr++ = wbyte;

        } while (wbyte != 0);
    }
#endif
}
#endif

#define NB_GAME_CHOICE 5

/*──────────────────────────────────────────────────────────────────────────*/
U8 *LoadGameScreen() {
#ifdef EDITLBA2
    U8 wbyte;
    U8 *ptr;

    PtrSave = BufTempo;

    if (!Load(GamePathname, PtrSave))
        return (NULL);

    LbaReadByte(NumVersion); // num version
    LbaReadLong(NewCube);

    ptr = (U8 *)PlayerName;
    do {
        LbaReadByte(wbyte);
        *ptr++ = wbyte;

    } while (wbyte != 0);

    if (NumVersion & SAVE_COMPRESS) // Version compactee
    {
        U8 *ptrdecomp;
        S32 sizefile;
        S32 size;

        LbaReadLong(sizefile);
        size = FileSize(GamePathname) - (PtrSave - BufTempo);

        ptrdecomp = (U8 *)PtrSave + sizefile + RECOVER_AREA;
        memcpy(ptrdecomp, PtrSave, size);
        ExpandLZ(PtrSave, ptrdecomp, sizefile, 2);
    }

    return (PtrSave);
#else
    {
        S32 loaded;
        U8 *base = BufSpeak + 50000L;

        loaded = SaveLoadFileToBufSpeakSave();
        if (!loaded) {
            SaveLoadClearReadLimit();
            return (NULL);
        }

        if (!SaveLoadRoom(1 + 4))
            goto load_screen_fail;

        LbaReadByte(NumVersion); // num version
        LbaReadLong(NewCube);

        if (!SaveLoadReadPlayerName())
            goto load_screen_fail;

        if (!SaveLoadDecompressIfNeeded(base, SaveLoadSubbufferBytesFromLeadingSkip(50000u), loaded))
            goto load_screen_fail;

        SaveLoadClearReadLimit();
        return (PtrSave);
    }
load_screen_fail:
    SaveLoadClearReadLimit();
    return (NULL);
#endif
}

/*──────────────────────────────────────────────────────────────────────────*/
S32 LoadGame(void) {
#ifndef EDITLBA2
    U8 wbyte;
    S32 flaginit = FALSE;
    S32 savetimerrefhr;
    S32 loaded = 0;

    HQ_StopSample();


    loaded = SaveLoadFileToScreen(GamePathname);
    if (!loaded) {
        SaveLoadClearReadLimit();
        return FALSE;
    }


    LbaReadByte(NumVersion); // num version
    LbaReadLong(NewCube);

    if (!SaveLoadReadPlayerName()) {
        SaveLoadClearReadLimit();
        return FALSE;
    }


    if (!SaveLoadDecompressIfNeeded((U8 *)Screen, SaveLoadScreenBufferBytes(), loaded)) {
        SaveLoadClearReadLimit();
        return FALSE;
    }


    PtrSave += 160 * 120;

    flaginit = LoadContexte(&savetimerrefhr);
    if (flaginit == SAVELOAD_CTX_ERR) {
        SaveLoadClearReadLimit();
        return LOADGAME_ERR_CONTEXT;
    }

    // recupere position valide si nécessaire
    if (!SaveLoadRoom(8)) {
        SaveLoadClearReadLimit();
        return FALSE;
    }
    LbaReadLong(ValidePos);
    LbaReadLong(LastValidePos);

    if (!ValidePos) {
        if (!SaveLoadRoom(8))
            goto loadgame_tail_fail;
        LbaReadLong(ValideCube);
        LbaReadLong(SizeOfBufferValidePos);
        if (SizeOfBufferValidePos < 0
            OR (U32)SizeOfBufferValidePos > (U32)SIZE_BUFFER_VALIDE_POS)
            goto loadgame_tail_fail;
        if (!SaveLoadRoom((size_t)SizeOfBufferValidePos))
            goto loadgame_tail_fail;
        LbaRead(BufferValidePos, SizeOfBufferValidePos);
    }

    SaveLoadClearReadLimit();

    RestartMusic = TRUE;
    ChoicePalette();

    CameraCenter(0);
    SetTimerHR(savetimerrefhr);
    SaveTimer();


    return flaginit;

loadgame_tail_fail:
    SaveLoadClearReadLimit();
    return FALSE;
#else
    U8 *ptr;
    U8 wbyte;

    Load(GamePathname, (U8 *)Screen);
    PtrSave = (U8 *)Screen;

    LbaReadByte(NumVersion); // num version
    LbaReadLong(NewCube);

    ptr = (U8 *)PlayerName;
    do {
        LbaReadByte(wbyte);
        *ptr++ = wbyte;

    } while (wbyte != 0);

    if (NumVersion & SAVE_COMPRESS) // Version compactee
    {
        U8 *ptrdecomp;
        S32 sizefile;
        S32 size;

        LbaReadLong(sizefile);
        size = FileSize(GamePathname) - (PtrSave - (U8 *)Screen);

        ptrdecomp = (U8 *)PtrSave + sizefile + RECOVER_AREA;
        memcpy(ptrdecomp, PtrSave, size);
        ExpandLZ(PtrSave, ptrdecomp, sizefile, 2);

        SizeFile = sizefile + (PtrSave - (U8 *)Screen - sizeof(sizefile));
        CompressedSizeFile = size + (PtrSave - (U8 *)Screen);
    } else {
        SizeFile = FileSize(GamePathname);
        CompressedSizeFile = 0;
    }

    PtrSave += 160 * 120;

    LoadContexte(&KeepTimerRefHR);

    // recupere position valide si nécessaire
    LbaReadLong(ValidePos);
    LbaReadLong(LastValidePos);

    if (!ValidePos) {
        LbaReadLong(ValideCube);
        LbaReadLong(SizeOfBufferValidePos);
        LbaRead(BufferValidePos, SizeOfBufferValidePos);
    }

    return FALSE;
#endif
}

/*──────────────────────────────────────────────────────────────────────────*/
S32 LoadGameOldVersion(void) {
    T_INV_OBJ *ptri;
    T_ARROW *ptra;
    S32 n;
    U8 *ptr;
    S32 savetimerrefhr;
    S32 wlong;
    U8 wbyte;
#ifndef EDITLBA2
    S32 loaded_ov = 0;
#endif

#ifndef EDITLBA2
    loaded_ov = SaveLoadFileToScreen(GamePathname);
    if (!loaded_ov) {
        SaveLoadClearReadLimit();
        return FALSE;
    }
#else
    Load(GamePathname, (U8 *)Screen);
    PtrSave = (U8 *)Screen;
#endif

    LbaReadByte(NumVersion); // num version
    LbaReadLong(NewCube);

#ifndef EDITLBA2
    if (!SaveLoadReadPlayerName()) {
        SaveLoadClearReadLimit();
        return FALSE;
    }
#else
    ptr = (U8 *)PlayerName;
    do {
        LbaReadByte(wbyte);
        *ptr++ = wbyte;

    } while (wbyte != 0);
#endif

#ifndef EDITLBA2
    if (!SaveLoadDecompressIfNeeded((U8 *)Screen, SaveLoadScreenBufferBytes(), loaded_ov)) {
        SaveLoadClearReadLimit();
        return FALSE;
    }
#else
    if (NumVersion & SAVE_COMPRESS) // Version compactee
    {
        U8 *ptrdecomp;
        S32 sizefile;
        S32 size;

        LbaReadLong(sizefile);
        size = FileSize(GamePathname) - (PtrSave - (U8 *)Screen);

        ptrdecomp = (U8 *)PtrSave + sizefile + RECOVER_AREA;
        memcpy(ptrdecomp, PtrSave, size);
        ExpandLZ(PtrSave, ptrdecomp, sizefile, 2);

        SizeFile = sizefile + (PtrSave - (U8 *)Screen - sizeof(sizefile));
        CompressedSizeFile = size + (PtrSave - (U8 *)Screen);
    } else {
        SizeFile = FileSize(GamePathname);
        CompressedSizeFile = 0;
    }
#endif

    PtrSave += 160 * 120;

    // list Var game and cube
#ifdef EDITLBA2
    LbaRead(SaveListVarGame, MAX_VARS_GAME * sizeof(SaveListVarGame[0]));
    LbaRead(SaveListVarCube, MAX_VARS_CUBE * sizeof(SaveListVarCube[0]));
#else
    LbaRead(ListVarGame, MAX_VARS_GAME * sizeof(ListVarGame[0]));
    LbaRead(ListVarCube, MAX_VARS_CUBE * sizeof(ListVarCube[0]));
#endif

    LbaReadByte(Comportement);

#ifndef EDITLBA2
    SaveComportement = Comportement;
#endif

    LbaReadLong(wlong);

    NbGoldPieces = wlong & 0xFFFF;
    NbZlitosPieces = (wlong >> 16) & 0xFFFF;

    LbaReadByte(MagicLevel);
    LbaReadByte(MagicPoint);
    LbaReadByte(NbLittleKeys);
    LbaReadWord(NbCloverBox);

    LbaReadLong(SceneStartX);
    LbaReadLong(SceneStartY);
    LbaReadLong(SceneStartZ);

    LbaReadLong(StartXCube);
    LbaReadLong(StartYCube);
    LbaReadLong(StartZCube);

    LbaReadByte(Weapon);

    LbaReadLong(savetimerrefhr);

    LbaReadByte(NumObjFollow);

    LbaReadByte(SaveComportementHero);
    LbaReadByte(SaveBodyHero);

    ptra = TabArrow;

    for (n = 0; n < (MAX_OBJECTIF + MAX_CUBE); n++, ptra++) {
        LbaReadByte(wbyte);
        wbyte &= 3; // seuls les 2 bits de poids faible sont
                    // susceptibles de bouger
        ptra->FlagHolo |= wbyte;
    }

    // used inventory
    ptri = TabInv;

    for (n = 0; n < MAX_INVENTORY; n++, ptri++) {
        LbaReadLong(ptri->PtMagie);
        LbaReadLong(ptri->FlagInv);
        LbaReadWord(ptri->IdObj3D);
    }

    // Init position StartPos
#ifndef EDITLBA2
    //	SceneStartX = CubeStartX ;
    //	SceneStartY = CubeStartY ;
    //	SceneStartZ = CubeStartZ ;
    ListObjet[NUM_PERSO].Obj.X = SceneStartX;
    ListObjet[NUM_PERSO].Obj.Y = SceneStartY;
    ListObjet[NUM_PERSO].Obj.Z = SceneStartZ;

    StartXCube = ListObjet[NumObjFollow].Obj.X / SIZE_BRICK_XZ;
    StartYCube = (ListObjet[NumObjFollow].Obj.Y + SIZE_BRICK_Y) / SIZE_BRICK_Y;
    StartZCube = ListObjet[NumObjFollow].Obj.Z / SIZE_BRICK_XZ;

    if (FlagChgCube == 2 OR FlagChgCube == 0) {
        CameraCenter(1);
    } else {
        CameraCenter(0);
    }
#else
    SceneStartX = -1;
#endif

#ifndef EDITLBA2
    SaveLoadClearReadLimit();
#endif
    return TRUE;
}

/*──────────────────────────────────────────────────────────────────────────*/
// Sauvegarde de l'etat du jeu dans PtrSave
void SaveContexte(S32 savetimerrefhr) {
    T_OBJET *ptrobj;
    S_PART_FLOW *ptrf;
    T_INV_OBJ *ptri;
    T_DART *ptrd;
    T_ZONE *ptrz;
    T_ARROW *ptra;
    S32 n;
#ifndef EDITLBA2
    U8 wbyte;
    U8 wbyte2;
    U8 *saveptr;
    U8 *saveptr2;
    T_PATCH *ptrpatch;
    T_EXTRA *ptre;
    T_INCRUST_DISP *ptrdisp;
    S_ONE_DOT *ptrpt;
    S32 i;
#endif

    // list vars game et cube
#ifdef EDITLBA2
    LbaWrite(SaveListVarGame, MAX_VARS_GAME * sizeof(SaveListVarGame[0]));
    LbaWrite(SaveListVarCube, MAX_VARS_CUBE * sizeof(SaveListVarCube[0]));
#else
    LbaWrite(ListVarGame, MAX_VARS_GAME * sizeof(ListVarGame[0]));
    LbaWrite(ListVarCube, MAX_VARS_CUBE * sizeof(ListVarCube[0]));
#endif

    // Globales
    LbaWriteByte(Comportement);

    LbaWriteLong(((NbZlitosPieces << 16) + (NbGoldPieces & 0xFFFF)));
    LbaWriteByte(MagicLevel);
    LbaWriteByte(MagicPoint);
    LbaWriteByte(NbLittleKeys);
    LbaWriteWord(NbCloverBox);

    LbaWriteLong(SceneStartX);
    LbaWriteLong(SceneStartY);
    LbaWriteLong(SceneStartZ);

    LbaWriteLong(StartXCube);
    LbaWriteLong(StartYCube);
    LbaWriteLong(StartZCube);

    LbaWriteByte(Weapon);

    LbaWriteLong(savetimerrefhr);
    LbaWriteByte(NumObjFollow);

    LbaWriteByte(SaveComportementHero);
    LbaWriteByte(SaveBodyHero);

    ptra = TabArrow;

    for (n = 0; n < (MAX_OBJECTIF + MAX_CUBE); n++, ptra++) {
        LbaWriteByte(ptra->FlagHolo);
    }

    // used inventory
    ptri = TabInv;

    for (n = 0; n < MAX_INVENTORY; n++, ptri++) {
        LbaWriteLong(ptri->PtMagie);
        LbaWriteLong(ptri->FlagInv);
        LbaWriteWord(ptri->IdObj3D);
    }

    LbaWriteLong(Checksum);

    // Debut d'incompatibilité des sauvegardes

    LbaWriteLong(LastMyFire);
    LbaWriteLong(LastMyJoy);
    LbaWriteLong(LastInput);
    LbaWriteLong(LastJoyFlag);
    LbaWriteByte(Bulle);
    LbaWriteByte(ActionNormal);
    LbaWriteLong(InventoryAction);
    LbaWriteLong(MagicBall);
    LbaWriteByte(MagicBallType);
    LbaWriteByte(MagicBallCount);
    LbaWriteLong(MagicBallFlags);
    LbaWriteByte(FlagClimbing);
    LbaWriteLong(StartYFalling);
    LbaWriteByte(CameraZone);
    LbaWriteLong(InvSelect);
    LbaWriteLong(ExtraConque);
    LbaWriteByte(PingouinActif);

    LbaWriteLong((U64)PtrZoneClimb);

    // Flechettes
    ptrd = ListDart;

    for (n = 0; n < MAX_DARTS; n++, ptrd++) {
        LbaWriteLong(ptrd->PosX);
        LbaWriteLong(ptrd->PosY);
        LbaWriteLong(ptrd->PosZ);
        LbaWriteLong(ptrd->Alpha);
        LbaWriteLong(ptrd->Beta);
        LbaWriteLong(ptrd->NumCube);
        LbaWriteLong(ptrd->Flags);
    }
    //		LbaWrite( ptrd, sizeof( T_DART ) ) ;

    // Objets
    LbaWriteLong(NbObjets); // nb d'objets pour EditLba2

    ptrobj = ListObjet;

    for (n = 0; n < NbObjets; n++, ptrobj++) {
        LbaWriteByte(ptrobj->GenBody);
        LbaWriteByte(ptrobj->Col);
        LbaWriteWord(ptrobj->GenAnim);
        LbaWriteWord(ptrobj->NextGenAnim);
        LbaWriteLong(ptrobj->OldPosX);
        LbaWriteLong(ptrobj->OldPosY);
        LbaWriteLong(ptrobj->OldPosZ);
        LbaWriteLong(ptrobj->Info);
        LbaWriteLong(ptrobj->Info1);
        LbaWriteLong(ptrobj->Info2);
        LbaWriteLong(ptrobj->Info3);

        LbaWrite(&(ptrobj->Coord), sizeof(ptrobj->Coord));

        LbaWriteWord(ptrobj->SizeSHit);
        LbaWriteByte(ptrobj->HitBy);
        LbaWriteByte(ptrobj->HitForce);
        LbaWriteWord(ptrobj->LifePoint);
        LbaWriteWord(ptrobj->OptionFlags);

        //	U8	*PtrAnimAction ;

        LbaWriteWord(ptrobj->Sprite);
        LbaWriteWord(ptrobj->OffsetLabelTrack);

        //	T_OBJ_3D Obj	;
        //	U8	*PtrFile3D ;

        LbaWriteLong(ptrobj->IndexFile3D);

        //	S16	NbBonus ;
        LbaWriteByte(ptrobj->Armure);
        //	U8	CoulObj ;

        LbaWriteWord(ptrobj->XMin);
        LbaWriteWord(ptrobj->XMax);
        LbaWriteWord(ptrobj->YMin);
        LbaWriteWord(ptrobj->YMax);
        LbaWriteWord(ptrobj->ZMin);
        LbaWriteWord(ptrobj->ZMax);

        LbaWriteLong(ptrobj->OldBeta);
        LbaWrite(&(ptrobj->BoundAngle), sizeof(ptrobj->BoundAngle));
        //	U8	*PtrTrack ;
        LbaWriteWord(ptrobj->OffsetTrack);
        LbaWriteWord(ptrobj->SRot);
        LbaWriteWord(ptrobj->OffsetLife);
        LbaWriteWord(ptrobj->AnimDial);
        LbaWriteWord(ptrobj->CarryBy);
        LbaWriteByte(ptrobj->Move);
        LbaWriteByte(ptrobj->ObjCol);
        LbaWriteWord(ptrobj->ZoneSce);
        LbaWriteWord(ptrobj->LabelTrack);
        LbaWriteWord(ptrobj->MemoLabelTrack);
        LbaWriteWord(ptrobj->MemoComportement);
        LbaWriteLong(ptrobj->Flags);
        LbaWriteLong(ptrobj->WorkFlags);
        LbaWriteWord(ptrobj->DoorWidth);
        LbaWriteByte(ptrobj->FlagAnim);
        LbaWriteByte(ptrobj->CodeJeu);
        LbaWrite(&(ptrobj->ExeSwitch), sizeof(ptrobj->ExeSwitch));

        //	S16	MessageChapter[MAX_CHAPTER] ;

        LbaWriteLong(ptrobj->SampleAlways);
        LbaWriteByte(ptrobj->SampleVolume);

        //	T_ZONE	*PtrZoneRail ;

        // on ne veut pas sauver le champ CurrentFrame
        LbaWrite(&(ptrobj->Obj), sizeof(ptrobj->Obj) - sizeof(ptrobj->Obj.CurrentFrame));
    }

    //------------- PATCHES -------------
    LbaWriteLong(NbPatches);

#ifdef EDITLBA2
    LbaWriteLong(SizePatches);
    LbaWrite(BufferPatches, SizePatches);
#else
    saveptr = PtrSave;
    PtrSave += 4;

    ptrpatch = ListPatches;

    for (n = 0; n < NbPatches; n++, ptrpatch++) {
        switch (ptrpatch->Size) {
        case 1:
            LbaWriteByte(*(PtrScene + ptrpatch->Offset));
            break;

        case 2:
            LbaWriteWord(*(S16 *)(PtrScene + ptrpatch->Offset));
            break;

        case 4:
            LbaWriteLong(*(S32 *)(PtrScene + ptrpatch->Offset));
            break;

        default:
            LbaWrite(PtrScene + ptrpatch->Offset, ptrpatch->Size);
        }
    }

    *(U32 *)saveptr = (U32)(PtrSave - saveptr - 4);
#endif
    //-----------------------------------

    // Extras
#ifdef EDITLBA2
    LbaWriteByte(NbExtras);
    LbaWrite(ListExtra, sizeof(T_EXTRA) * NbExtras);
#else
    saveptr = PtrSave++;
    wbyte = 0;
    ptre = ListExtra;

    for (n = 0; n < MAX_EXTRAS; n++, ptre++) {
        if (ptre->Sprite != -1) {
            LbaWrite(ptre, sizeof(T_EXTRA));
            wbyte++;
        }
    }

    *saveptr = wbyte;
#endif

    // Zones
    LbaWriteLong(NbZones); // pour EditLba2

    ptrz = ListZone;

    for (n = 0; n < NbZones; n++, ptrz++) {
        LbaWriteLong(ptrz->Info1);
        LbaWriteLong(ptrz->Info2);
        LbaWriteLong(ptrz->Info3);
        LbaWriteLong(ptrz->Info7);
    }

    // Incrust
#ifdef EDITLBA2
    LbaWriteByte(NbIncrust);
    LbaWrite(ListIncrustDisp, sizeof(T_INCRUST_DISP) * NbIncrust);
#else
    saveptr = PtrSave++;
    wbyte = 0;
    ptrdisp = ListIncrustDisp;

    for (n = 0; n < MAX_INCRUST_DISP; n++, ptrdisp++) {
        if (ptrdisp->Num != -1) {
            LbaWrite(ptrdisp, sizeof(T_INCRUST_DISP));
            wbyte++;
        }
    }

    *saveptr = wbyte;
#endif

    // Flows
    ptrf = ListPartFlow;

#ifndef EDITLBA2
    saveptr = PtrSave++;
    wbyte = 0;

    for (n = 0; n < MAX_FLOWS; n++, ptrf++) {
        if (ptrf->NbDot > 0) {
            LbaWrite(ptrf, sizeof(S_PART_FLOW));

            ptrpt = ptrf->PtrListDot;
            wbyte2 = 0;
            saveptr2 = PtrSave++;

            for (i = 0; i < ptrf->NbDot; i++, ptrpt++) {
                if (ptrpt->Mode) {
                    LbaWrite(ptrpt, sizeof(S_ONE_DOT));
                    wbyte2++;
                }
            }

            *saveptr2 = wbyte2;
            wbyte++;
        }
    }

    *saveptr = wbyte;
#else
    LbaWriteByte(NbFlows);

    for (n = 0; n < NbFlows; n++, ptrf++) {
        LbaWrite(ptrf, sizeof(S_PART_FLOW));

        LbaWriteByte(NbDots[n]);
        LbaWrite(ListPtrListDot[n], sizeof(S_ONE_DOT) * NbDots[n]);
    }
#endif

    // Infos camera
    LbaWriteLong(VueDistance);
    LbaWriteLong(AlphaCam);
    LbaWriteLong(BetaCam);
    LbaWriteLong(GammaCam);
    LbaWriteLong(AddBetaCam);

    LbaWriteLong(VueOffsetX);
    LbaWriteLong(VueOffsetY);
    LbaWriteLong(VueOffsetZ);

    LbaWriteByte(CinemaMode);
    LbaWriteLong(TimerCinema);
    LbaWriteLong(LastYCinema);
    LbaWriteLong(DebCycleCinema);   // en pixels Y
    LbaWriteLong(DureeCycleCinema); // en TimerRefHR

#ifdef EDITLBA2
    LbaWriteLong(EditClipWindowYMin);
    LbaWriteLong(EditClipWindowYMax);
#else
    RestoreClipWindow();
    LbaWriteLong(ClipWindowYMin);
    LbaWriteLong(ClipWindowYMax);
#endif

    LbaWriteByte(AnimateTexture);

    LbaWriteLong(ParmSampleDecalage);
    LbaWriteLong(ParmSampleFrequence);
    LbaWriteLong(ParmSampleVolume);

    LbaWriteByte(NumBuggy);

    LbaWrite(ListBuggy, sizeof(S_BUGGY) * MAX_BUGGYS);

    // a integrer dans la prochaine version de sauvegarde
    LbaWrite(ListArdoise, MAX_ARDOISE * sizeof(S8));

    LbaWriteByte(CurrentArdoise);
    LbaWriteByte(NbArdoise);

    LbaWriteLong(VueCamera);
}

/*──────────────────────────────────────────────────────────────────────────*/
// Chargement d'une partie se trouvant dans PtrSave

#ifndef EDITLBA2
#undef LbaRead
#define LbaRead(ptr, size)                                                                 \
    do {                                                                                   \
        if (!SaveLoadRoom((size_t)(size)))                                                  \
            return SAVELOAD_CTX_ERR;                                                       \
        memcpy((ptr), PtrSave, (size_t)(size));                                            \
        PtrSave += (size_t)(size);                                                         \
    } while (0)

#undef LbaReadByte
#define LbaReadByte(ptr)                                                                   \
    do {                                                                                   \
        if (!SaveLoadRoom(1))                                                              \
            return SAVELOAD_CTX_ERR;                                                       \
        (ptr) = *PtrSave++;                                                                \
    } while (0)

#undef LbaReadWord
#define LbaReadWord(ptr)                                                                   \
    do {                                                                                   \
        if (!SaveLoadRoom(2))                                                              \
            return SAVELOAD_CTX_ERR;                                                       \
        (ptr) = *(S16 *)PtrSave;                                                           \
        PtrSave += 2;                                                                      \
    } while (0)

#undef LbaReadLong
#define LbaReadLong(ptr)                                                                   \
    do {                                                                                   \
        if (!SaveLoadRoom(4))                                                              \
            return SAVELOAD_CTX_ERR;                                                       \
        (ptr) = *(S32 *)PtrSave;                                                           \
        PtrSave += 4;                                                                      \
    } while (0)

/* Issue #62: read NbObjets objects from PtrSave at the configured stride and
 * validate the decoded ListObjet[] entries.  Returns 1 on read-ok + validate-pass,
 * 0 on validate-fail (typical of wrong-stride misalignment), or SAVELOAD_CTX_ERR (-1)
 * propagated out by the bounded LbaRead* macros on buffer overrun.  Caller may
 * rewind PtrSave and retry with the alternate stride; the bounded macros above
 * are scoped to the LoadContexte translation block, so this helper is placed
 * here to share them. */
static S32 LoadContexteReadObjectsAtStride(S32 use_wire32) {
    S32 n;
    T_OBJET *ptrobj = ListObjet;
#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS) || defined(EDITLBA2)
    extern S32 TempoRealAngle;
#endif
    for (n = 0; n < NbObjets; n++, ptrobj++) {
        LbaReadByte(ptrobj->GenBody);
        LbaReadByte(ptrobj->Col);
        LbaReadWord(ptrobj->GenAnim);
        LbaReadWord(ptrobj->NextGenAnim);
        LbaReadLong(ptrobj->OldPosX);
        LbaReadLong(ptrobj->OldPosY);
        LbaReadLong(ptrobj->OldPosZ);
        LbaReadLong(ptrobj->Info);
        LbaReadLong(ptrobj->Info1);
        LbaReadLong(ptrobj->Info2);
        LbaReadLong(ptrobj->Info3);
        LbaRead(&(ptrobj->Coord), sizeof(ptrobj->Coord));
        LbaReadWord(ptrobj->SizeSHit);
        LbaReadByte(ptrobj->HitBy);
        LbaReadByte(ptrobj->HitForce);
        LbaReadWord(ptrobj->LifePoint);
        LbaReadWord(ptrobj->OptionFlags);
        LbaReadWord(ptrobj->Sprite);
        LbaReadWord(ptrobj->OffsetLabelTrack);
        LbaReadLong(ptrobj->IndexFile3D);
        LbaReadByte(ptrobj->Armure);
        LbaReadWord(ptrobj->XMin);
        LbaReadWord(ptrobj->XMax);
        LbaReadWord(ptrobj->YMin);
        LbaReadWord(ptrobj->YMax);
        LbaReadWord(ptrobj->ZMin);
        LbaReadWord(ptrobj->ZMax);
        LbaReadLong(ptrobj->OldBeta);
#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS) || defined(EDITLBA2)
        if (NumVersion < 34) {
            LbaRead(&TempoRealAngle, sizeof(TempoRealAngle));
        } else
#endif
            LbaRead(&(ptrobj->BoundAngle), sizeof(ptrobj->BoundAngle));
        LbaReadWord(ptrobj->OffsetTrack);
        LbaReadWord(ptrobj->SRot);
        LbaReadWord(ptrobj->OffsetLife);
        LbaReadWord(ptrobj->AnimDial);
        LbaReadWord(ptrobj->CarryBy);
        LbaReadByte(ptrobj->Move);
        LbaReadByte(ptrobj->ObjCol);
        LbaReadWord(ptrobj->ZoneSce);
        LbaReadWord(ptrobj->LabelTrack);
        LbaReadWord(ptrobj->MemoLabelTrack);
        LbaReadWord(ptrobj->MemoComportement);
        LbaReadLong(ptrobj->Flags);
        LbaReadLong(ptrobj->WorkFlags);
        LbaReadWord(ptrobj->DoorWidth);
        LbaReadByte(ptrobj->FlagAnim);
        LbaReadByte(ptrobj->CodeJeu);
        LbaRead(&(ptrobj->ExeSwitch), sizeof(ptrobj->ExeSwitch));
#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS) || defined(EDITLBA2)
        if (NumVersion >= 35)
#endif
            LbaReadLong(ptrobj->SampleAlways);
#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS) || defined(EDITLBA2)
        if (NumVersion >= 36)
#endif
            LbaReadByte(ptrobj->SampleVolume);
        if (use_wire32) {
            T_OBJ_3D_WIRE32 w32;
            LbaRead(&w32, sizeof(w32));
            SavegameObj3dFromWire32(&w32, &ptrobj->Obj);
        } else {
            LbaRead(&(ptrobj->Obj), sizeof(ptrobj->Obj) - sizeof(ptrobj->Obj.CurrentFrame));
        }
    }
    /* Validate decoded objects.  A wrong-stride read misaligns the cursor by
     * ~28 bytes per object on 64-bit hosts; IndexFile3D ends up reading bytes
     * from another field and InitLoadedGame's LoadFile3D(garbage) deref segvs
     * (OBJECT.CPP InitLoadedGame).  IndexFile3D is the only field large
     * enough to catch this — GenAnim (U16) and GenBody (U8) cannot overflow
     * the 1M check the earlier H17 instrumentation used. */
    for (n = 0; n < NbObjets; n++) {
        if (ListObjet[n].IndexFile3D < -1 || ListObjet[n].IndexFile3D > 100000)
            return 0;
    }
    return 1;
}
#endif

S32 LoadContexte(S32 *savetimerrefhr) {
    T_OBJET *ptrobj;
    S_PART_FLOW *ptrf;
    T_INV_OBJ *ptri;
    T_DART *ptrd;
    T_ZONE *ptrz;
    T_ARROW *ptra;
    S32 n;
    S32 wlong;
#ifndef EDITLBA2
    S32 wlong2;
    T_EXTRA *ptrextra;
    T_PATCH *ptrpatch;
    S_ONE_DOT *ptrdot;
    S32 flaginit = FALSE;
    U8 wbyte2;
    S32 per_object_stride = 0;
    U8 use_wire32_obj = 0;
#endif
    U8 wbyte;

    // list Var game and cube
#ifdef EDITLBA2
    LbaRead(SaveListVarGame, MAX_VARS_GAME * sizeof(SaveListVarGame[0]));
    LbaRead(SaveListVarCube, MAX_VARS_CUBE * sizeof(SaveListVarCube[0]));
#else
    LbaRead(ListVarGame, MAX_VARS_GAME * sizeof(ListVarGame[0]));
    LbaRead(ListVarCube, MAX_VARS_CUBE * sizeof(ListVarCube[0]));
#endif

    // Globales
    LbaReadByte(Comportement);

#ifndef EDITLBA2
    SaveComportement = Comportement;
#endif

    LbaReadLong(wlong);

    NbGoldPieces = wlong & 0xFFFF;
    NbZlitosPieces = (wlong >> 16) & 0xFFFF;

    LbaReadByte(MagicLevel);
    LbaReadByte(MagicPoint);
    LbaReadByte(NbLittleKeys);
    LbaReadWord(NbCloverBox);

    LbaReadLong(SceneStartX);
    LbaReadLong(SceneStartY);
    LbaReadLong(SceneStartZ);

    LbaReadLong(StartXCube);
    LbaReadLong(StartYCube);
    LbaReadLong(StartZCube);

    LbaReadByte(Weapon);

    LbaReadLong(*savetimerrefhr);

    LbaReadByte(NumObjFollow);

    LbaReadByte(SaveComportementHero);
    LbaReadByte(SaveBodyHero);

    ptra = TabArrow;

    for (n = 0; n < (MAX_OBJECTIF + MAX_CUBE); n++, ptra++) {
        LbaReadByte(wbyte);
        wbyte &= 3; // seuls les 2 bits de poids faible sont
                    // susceptibles de bouger
        ptra->FlagHolo |= wbyte;
    }

    // used inventory
    ptri = TabInv;

    for (n = 0; n < MAX_INVENTORY; n++, ptri++) {
        LbaReadLong(ptri->PtMagie);
        LbaReadLong(ptri->FlagInv);
        LbaReadWord(ptri->IdObj3D);
    }

#ifdef EDITLBA2
    LbaReadLong(Checksum);
#else
    LbaReadLong(wlong);

    if (wlong != Checksum) {
#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS)
        Message("Warning: La sauvegarde ne correspond pas au scénario !", TRUE);
#endif
        SceneStartX = -1;
    }
#endif

#ifndef EDITLBA2
    if (SceneStartX == -1) {
        SceneStartX = CubeStartX;
        SceneStartY = CubeStartY;
        SceneStartZ = CubeStartZ;
        ListObjet[NUM_PERSO].Obj.X = SceneStartX;
        ListObjet[NUM_PERSO].Obj.Y = SceneStartY;
        ListObjet[NUM_PERSO].Obj.Z = SceneStartZ;

        StartXCube = ListObjet[NumObjFollow].Obj.X / SIZE_BRICK_XZ;
        StartYCube = (ListObjet[NumObjFollow].Obj.Y + SIZE_BRICK_Y) / SIZE_BRICK_Y;
        StartZCube = ListObjet[NumObjFollow].Obj.Z / SIZE_BRICK_XZ;

        if (FlagChgCube == 2 OR FlagChgCube == 0) {
            CameraCenter(1);
        } else {
            CameraCenter(0);
        }

        // Magouille : Pour récupérer le body du heros lors d'un
        // START_POS dans EDITLBA2, il est sauvé dans le dernier
        // VAR_CUBE par EDITLBA2

        ListObjet[NUM_PERSO].GenBody = ListVarCube[MAX_VARS_CUBE - 1];

        flaginit = TRUE;
    } else
#endif
    {
        // Debut incompatibilité des sauvegardes

#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS) || defined(EDITLBA2)
        if (NumVersion < 34) // Compatibilité sauvegardes
        {
            LbaReadLong(LastStepFalling);
            LbaReadLong(LastStepShifting);
        }
#endif

        LbaReadLong(LastMyFire);
        LbaReadLong(LastMyJoy);
        LbaReadLong(LastInput);
        LbaReadLong(LastJoyFlag);
        LbaReadByte(Bulle);
        LbaReadByte(ActionNormal);
        LbaReadLong(InventoryAction);
        LbaReadLong(MagicBall);
        LbaReadByte(MagicBallType);
        LbaReadByte(MagicBallCount);
        LbaReadLong(MagicBallFlags);
        LbaReadByte(FlagClimbing);
        LbaReadLong(StartYFalling);

#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS) || defined(EDITLBA2)
        if (NumVersion < 34) // Compatibilité sauvegardes
        {
            LbaRead(&RealFalling, sizeof(RealFalling));
            LbaReadLong(StepFalling);
            LbaRead(&RealShifting, sizeof(RealShifting));
            LbaReadLong(StepShifting);
        }
#endif

        LbaReadByte(CameraZone);
        LbaReadLong(InvSelect);
        LbaReadLong(ExtraConque);
        LbaReadByte(PingouinActif);

        PtrZoneClimb = (T_ZONE *)(uintptr_t)(*(U32 *)PtrSave);
        PtrSave += 4;

        // Flechettes
        ptrd = ListDart;

        for (n = 0; n < MAX_DARTS; n++, ptrd++) {
            LbaReadLong(ptrd->PosX);
            LbaReadLong(ptrd->PosY);
            LbaReadLong(ptrd->PosZ);
            LbaReadLong(ptrd->Alpha);
            LbaReadLong(ptrd->Beta);
            LbaReadLong(ptrd->NumCube);
            LbaReadLong(ptrd->Flags);
        }

        // Objets
        LbaReadLong(NbObjets); // nb d'objets pour EditLba2
#ifdef EDITLBA2
        ptrobj = ListObjet;
        for (n = 0; n < NbObjets; n++, ptrobj++) {
            LbaReadByte(ptrobj->GenBody);
            LbaReadByte(ptrobj->Col);
            LbaReadWord(ptrobj->GenAnim);
            LbaReadWord(ptrobj->NextGenAnim);
            LbaReadLong(ptrobj->OldPosX);
            LbaReadLong(ptrobj->OldPosY);
            LbaReadLong(ptrobj->OldPosZ);
            LbaReadLong(ptrobj->Info);
            LbaReadLong(ptrobj->Info1);
            LbaReadLong(ptrobj->Info2);
            LbaReadLong(ptrobj->Info3);
            LbaRead(&(ptrobj->Coord), sizeof(ptrobj->Coord));
            LbaReadWord(ptrobj->SizeSHit);
            LbaReadByte(ptrobj->HitBy);
            LbaReadByte(ptrobj->HitForce);
            LbaReadWord(ptrobj->LifePoint);
            LbaReadWord(ptrobj->OptionFlags);
            LbaReadWord(ptrobj->Sprite);
            LbaReadWord(ptrobj->OffsetLabelTrack);
            LbaReadLong(ptrobj->IndexFile3D);
            LbaReadByte(ptrobj->Armure);
            LbaReadWord(ptrobj->XMin);
            LbaReadWord(ptrobj->XMax);
            LbaReadWord(ptrobj->YMin);
            LbaReadWord(ptrobj->YMax);
            LbaReadWord(ptrobj->ZMin);
            LbaReadWord(ptrobj->ZMax);
            LbaReadLong(ptrobj->OldBeta);
            if (NumVersion < 34) {
                LbaRead(&TempoRealAngle, sizeof(TempoRealAngle));
            } else
                LbaRead(&(ptrobj->BoundAngle), sizeof(ptrobj->BoundAngle));
            LbaReadWord(ptrobj->OffsetTrack);
            LbaReadWord(ptrobj->SRot);
            LbaReadWord(ptrobj->OffsetLife);
            LbaReadWord(ptrobj->AnimDial);
            LbaReadWord(ptrobj->CarryBy);
            LbaReadByte(ptrobj->Move);
            LbaReadByte(ptrobj->ObjCol);
            LbaReadWord(ptrobj->ZoneSce);
            LbaReadWord(ptrobj->LabelTrack);
            LbaReadWord(ptrobj->MemoLabelTrack);
            LbaReadWord(ptrobj->MemoComportement);
            LbaReadLong(ptrobj->Flags);
            LbaReadLong(ptrobj->WorkFlags);
            LbaReadWord(ptrobj->DoorWidth);
            LbaReadByte(ptrobj->FlagAnim);
            LbaReadByte(ptrobj->CodeJeu);
            LbaRead(&(ptrobj->ExeSwitch), sizeof(ptrobj->ExeSwitch));
            if (NumVersion >= 35)
                LbaReadLong(ptrobj->SampleAlways);
            if (NumVersion >= 36)
                LbaReadByte(ptrobj->SampleVolume);
            LbaRead(&(ptrobj->Obj), sizeof(ptrobj->Obj) - sizeof(ptrobj->Obj.CurrentFrame));
        }
#else
        if (NbObjets < 0 OR NbObjets > MAX_OBJETS)
            return SAVELOAD_CTX_ERR;
        {
            /* Match writer at SAVEGAME.CPP SaveContexte: LbaWrite(&Obj, sizeof(Obj) - sizeof(Obj.CurrentFrame)).
             * 64-bit-host stride differs from 32-bit retail wire (278) by 28 bytes per object;
             * picking the wrong stride misaligns the cursor and IndexFile3D ends up reading garbage,
             * which makes InitLoadedGame's LoadFile3D(garbage) deref segv.  Strategy:
             *   1. Pre-check the buffer fits the larger of {278, native} so neither attempt overruns.
             *   2. Try the heuristic-picked stride; the helper validates IndexFile3D after the read.
             *   3. On validate-fail, rewind PtrSave and try the alternate stride.
             *   4. If both fail (or env-forced and forced fails), return SAVELOAD_CTX_ERR. */
            S32 stride64_host = 142 + (S32)(sizeof(T_OBJ_3D) - sizeof(((T_OBJ_3D *)0)->CurrentFrame));
            const char *abi_env = getenv("LBA2_SAVE_LOAD_ABI");
            S32 forced_32 = (abi_env AND !strcmp(abi_env, "32")) ? 1 : 0;
            S32 first_stride;
            S32 alt_stride;
            U8 *obj_checkpoint;
            S32 max_stride;
            S32 r;

            if (forced_32) {
                first_stride = 278;
            } else {
                U8 *stream_end = g_save_read_limit;
                if (!stream_end)
                    stream_end = (U8 *)Screen + SaveLoadScreenBufferBytes();
                S32 g = SaveLoadGuessObjectWireStride(NbObjets, PtrSave, stream_end,
                                                      stride64_host, NbPatches);
                first_stride = g ? g : stride64_host;
            }
            alt_stride = (first_stride == 278) ? stride64_host : 278;
            max_stride = (stride64_host > 278) ? stride64_host : 278;

            if (!SaveLoadRoom((size_t)NbObjets * (size_t)max_stride))
                return SAVELOAD_CTX_ERR;

            obj_checkpoint = PtrSave;
            r = LoadContexteReadObjectsAtStride(first_stride == 278);
            if (r == 0 && !forced_32) {
                /* Validate failed — likely wrong stride.  Rewind and retry the alternate. */
                PtrSave = obj_checkpoint;
                r = LoadContexteReadObjectsAtStride(alt_stride == 278);
                if (r == 1)
                    first_stride = alt_stride;
            }
            if (r != 1)
                return SAVELOAD_CTX_ERR;

            per_object_stride = first_stride;
            use_wire32_obj = (per_object_stride == 278) ? 1 : 0;
        }
#endif

        //------------- PATCHES -------------
        LbaReadLong(NbPatches);

#ifdef EDITLBA2
        LbaReadLong(SizePatches);
        LbaRead(BufferPatches, SizePatches);
#else
        if (NbPatches < 0 OR NbPatches > MAX_PATCHES)
            return SAVELOAD_CTX_ERR;

        LbaReadLong(wlong);

        ptrpatch = ListPatches;

        for (n = 0; n < NbPatches; n++, ptrpatch++) {
            if (ptrpatch->Offset < 0 OR ptrpatch->Size < 0
                OR ptrpatch->Offset > PtrSceneMem OR ptrpatch->Size > PtrSceneMem
                OR (S32)((S64)ptrpatch->Offset + (S64)ptrpatch->Size) > PtrSceneMem)
                return SAVELOAD_CTX_ERR;

            switch (ptrpatch->Size) {
            case 1:
                LbaReadByte(*(PtrScene + ptrpatch->Offset));
                break;

            case 2:
                LbaReadWord(*(S16 *)(PtrScene + ptrpatch->Offset));
                break;

            case 4:
                LbaReadLong(*(S32 *)(PtrScene + ptrpatch->Offset));
                break;

            default:
                LbaRead(PtrScene + ptrpatch->Offset, ptrpatch->Size);
            }
        }
#endif
        //-----------------------------------

        // Extras

        LbaReadByte(wbyte);

#ifdef EDITLBA2
        NbExtras = wbyte;
#else
        if (wbyte > MAX_EXTRAS)
            return SAVELOAD_CTX_ERR;
#endif

        LbaRead(ListExtra, sizeof(T_EXTRA) * wbyte);

#ifndef EDITLBA2
        // Magouille pour éviter de changer le format des sauvegardes
        // Pourrait etre changé en version finale en sauvant ProtectActif
        ProtectActif = 0;
        ptrextra = ListExtra;

        for (n = 0; n < wbyte; n++, ptrextra++) {
            if (ptrextra->Flags & EXTRA_PROTECT) {
                ProtectActif = 1;
                break; // 1 seul suffit pour qu'il soit actif
            }
        }
#endif

        // Zones
        LbaReadLong(NbZones); // pour EditLba2

#ifndef EDITLBA2
        if (NbZones < 0 OR NbZones > MAX_ZONES)
            return SAVELOAD_CTX_ERR;
#endif

        ptrz = ListZone;

        for (n = 0; n < NbZones; n++, ptrz++) {
            LbaReadLong(ptrz->Info1);
            LbaReadLong(ptrz->Info2);
            LbaReadLong(ptrz->Info3);
            LbaReadLong(ptrz->Info7);

#ifndef EDITLBA2
            switch (ptrz->Type) {
            case 3: // GRM
                if (ptrz->Info2) {
                    IncrustGrm(ptrz);
                }
                break;
            }
#endif
        }

        // Incrust
        LbaReadByte(wbyte);

#ifdef EDITLBA2
        NbIncrust = wbyte;
#else
        if (wbyte > MAX_INCRUST_DISP)
            return SAVELOAD_CTX_ERR;
#endif

        LbaRead(ListIncrustDisp, sizeof(T_INCRUST_DISP) * wbyte);

#ifndef EDITLBA2
        for (n = 0; n < wbyte; n++) {
            if ((ListIncrustDisp[n].Type & 0xFF) == INCRUST_PLUIE) {
                FlagRain = TRUE;
                RestartRainSample = TRUE;
            }
        }
#endif

        // Flows
        ptrf = ListPartFlow;

        LbaReadByte(wbyte);

#ifdef EDITLBA2
        NbFlows = wbyte;
#else
        if (wbyte > MAX_FLOWS)
            return SAVELOAD_CTX_ERR;
#endif

        for (n = 0; n < wbyte; n++, ptrf++) {
#ifdef EDITLBA2
            LbaRead(ptrf, sizeof(S_PART_FLOW));
            LbaReadByte(NbDots[n]);
            LbaRead(ListPtrListDot[n], sizeof(S_ONE_DOT) * NbDots[n]);
#else
            ptrdot = ptrf->PtrListDot;
            LbaRead(ptrf, sizeof(S_PART_FLOW));
            ptrf->PtrListDot = ptrdot;
            LbaReadByte(wbyte2);
            if (wbyte2 > MAX_FLOW_DOTS)
                return SAVELOAD_CTX_ERR;
            LbaRead(ptrf->PtrListDot, sizeof(S_ONE_DOT) * wbyte2);
#endif
        }

        // Infos camera
        LbaReadLong(VueDistance);
        LbaReadLong(AlphaCam);
        LbaReadLong(BetaCam);
        LbaReadLong(GammaCam);
        LbaReadLong(AddBetaCam);

        LbaReadLong(VueOffsetX);
        LbaReadLong(VueOffsetY);
        LbaReadLong(VueOffsetZ);

        LbaReadByte(CinemaMode);
        LbaReadLong(TimerCinema);
        LbaReadLong(LastYCinema);
        LbaReadLong(DebCycleCinema);   // en pixels Y
        LbaReadLong(DureeCycleCinema); // en TimerRefHR

#ifdef EDITLBA2
        LbaReadLong(EditClipWindowYMin);
        LbaReadLong(EditClipWindowYMax);
#else
        LbaReadLong(wlong);
        LbaReadLong(wlong2);
        SetClipWindow(ClipWindowXMin, wlong, ClipWindowXMax, wlong2);
        MemoClipWindow();
#endif

        LbaReadByte(AnimateTexture);

        LbaReadLong(ParmSampleDecalage);
        LbaReadLong(ParmSampleFrequence);
        LbaReadLong(ParmSampleVolume);

        LbaReadByte(NumBuggy);
        LbaRead(ListBuggy, sizeof(S_BUGGY) * MAX_BUGGYS);

        LbaRead(ListArdoise, MAX_ARDOISE * sizeof(S8));

        LbaReadByte(CurrentArdoise);
        LbaReadByte(NbArdoise);
        LbaReadLong(VueCamera);
    }

#ifndef EDITLBA2
    TimerAniPoly = 0; // reinit animation de l'eau

    InitRain(); // Init Pluie
    LastTimer = 0;

    return flaginit;
#else
    return FALSE;
#endif
}

#ifndef EDITLBA2
#undef LbaRead
#define LbaRead(ptr, size)      \
    memcpy(ptr, PtrSave, size); \
    PtrSave += size;

#undef LbaReadByte
#define LbaReadByte(ptr) ptr = *PtrSave++;

#undef LbaReadWord
#define LbaReadWord(ptr)   \
    ptr = *(S16 *)PtrSave; \
    PtrSave += 2;

#undef LbaReadLong
#define LbaReadLong(ptr)   \
    ptr = *(S32 *)PtrSave; \
    PtrSave += 4;
#endif
