#include "C_EXTERN.H"
#include "DIRECTORIES.H"

#include <SYSTEM/ADELINE.H>
#include <SYSTEM/LOG.H>

#include <SDL3/SDL_error.h>

#include "AMBIANCE.H"
#include "BOOT_EXIT.H"

U8 *PtrSce;
U8 *PtrScene;
S32 CubeMode = 0;     // 0-->Intérieur - 1-->Extérieur
S32 LastCubeMode = 0; // 0-->Intérieur - 1-->Extérieur

#define GET_S8 (*PtrSce++)
#define GET_S16       \
    (*(S16 *)PtrSce); \
    PtrSce += 2
#define GET_U32       \
    (*(U32 *)PtrSce); \
    PtrSce += 4

/*══════════════════════════════════════════════════════════════════════════*
	     █▀▀▀▄  █    ██▀▀▀ █  ▄▀       █▀▀▀▀ █   █ ██▄ █ █▀▀▀▀
	     ██  █  ██   ▀▀▀▀█ ██▀▄        ██▀▀  ██  █ ██▀██ ██
	     ▀▀▀▀   ▀▀   ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀ ▀▀    ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/
/*
void	GetDiskEnv( char *progpath )
{
	_splitpath( (char*)progpath, ProgDrive, ProgDir, Name, Ext ) ;
	getcwd( OrgDir, _MAX_DIR ) ;
	_dos_getdrive( &OrgDrive ) ;
}

void	RestoreDiskEnv()
{
	unsigned total ;

	_dos_setdrive( OrgDrive, &total ) ;
	chdir( OrgDir ) ;
}
*/

/*══════════════════════════════════════════════════════════════════════════*
	  ██▀▀▀ █▀▀▀▀ █▀▀▀▀ ██▄ █ █▀▀▀▀       █▀▀▀▀  █    █     █▀▀▀▀
	  ▀▀▀▀█ ██    ██▀▀  ██▀██ ██▀▀        ██▀▀   ██   ██    ██▀▀
	  ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀     ▀▀   ▀▀▀▀▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/
S32 LoadScene(S32 numscene) {
    T_OBJET *ptrobj;
    T_ZONE *ptrz;
    S32 sizetoload;
    S32 indexfile3d;
    U8 n;
    char tmpFilePath[ADELINE_MAX_PATH];
    //        U8	t ;

    // numscene+1 car en 0 se trouve SizeCube.MAX (taille du plus gros .SCC)
    GetResPath(tmpFilePath, ADELINE_MAX_PATH, SCENE_HQR_NAME);
    if (!Load_HQR(tmpFilePath, PtrScene, numscene + 1)) {
        TheEndCheckFile(tmpFilePath);
    }

    ManageSystem(); // for streaming

    PtrSce = PtrScene;

    // info world: INFO_WORLD

    NewCube = numscene;

    Island = GET_S8;

    Planet = TabArrow[Island].Planet;

    // Sert uniquement pour la 3DExt
    CurrentCubeX = GET_S8;
    CurrentCubeY = GET_S8;

    ShadowLevel = GET_S8;
    ModeLabyrinthe = GET_S8;

    CubeMode = GET_S8;

    if (!FlagLoadGame
            AND(!FlagDrawHorizon OR(CubeMode + LastCubeMode != 2))) {
        FadeToBlackAndSamples(PtrPal);
        HQ_StopSample();

        // The fade is now a pure presenter (it no longer recycles the moving
        // box list), so it leaves the old cube's actor boxes in pBoxMovingList.
        // Cls() blacks Log but does not touch that list, so the BoxUpdate below
        // would BoxClean stale old-cube rects (a Screen->Log restore at bogus
        // coordinates). Reset the lists first: Log is about to be all black, so
        // there is nothing dirty to preserve.
        BoxReset();
        Cls();
        BoxUpdate();
    }

    n = GET_S8;

    // ambiance: AMBIANCE

    AlphaLight = GET_S16;
    BetaLight = GET_S16;

    SampleAmbiance[0] = GET_S16;
    SampleRepeat[0] = GET_S16;
    SampleRnd[0] = GET_S16;
    SampleFreq[0] = GET_S16;
    SampleVol[0] = GET_S16;
    SampleAmbiance[1] = GET_S16;
    SampleRepeat[1] = GET_S16;
    SampleRnd[1] = GET_S16;
    SampleFreq[1] = GET_S16;
    SampleVol[1] = GET_S16;
    SampleAmbiance[2] = GET_S16;
    SampleRepeat[2] = GET_S16;
    SampleRnd[2] = GET_S16;
    SampleFreq[2] = GET_S16;
    SampleVol[2] = GET_S16;
    SampleAmbiance[3] = GET_S16;
    SampleRepeat[3] = GET_S16;
    SampleRnd[3] = GET_S16;
    SampleFreq[3] = GET_S16;
    SampleVol[3] = GET_S16;
    SecondMin = GET_S16;
    SecondEcart = GET_S16;

    CubeJingle = GET_S8;

    // hero inits: HERO_START

    ptrobj = ListObjet;

    CubeStartX = GET_S16;
    CubeStartY = GET_S16;
    CubeStartZ = GET_S16;

    sizetoload = GET_S16;
    ptrobj->PtrTrack = PtrSce;
    PtrSce += sizetoload;

    sizetoload = GET_S16;
    ptrobj->PtrLife = PtrSce;
    PtrSce += sizetoload;

    ptrobj++;

    // objets: OBJECT

    NbObjets = GET_S16;
    for (n = 1; n < NbObjets; n++, ptrobj++) {
        InitObject(n);

        ptrobj->Flags = GET_U32;

        indexfile3d = GET_S16;

        if (!(ptrobj->Flags & SPRITE_3D)) {
            if (!FlagLoadGame) {
                ptrobj->PtrFile3D = LoadFile3D(indexfile3d);
            }

            ptrobj->IndexFile3D = indexfile3d;
        }

        ManageSystem(); // for streaming

        ptrobj->GenBody = GET_S8;
        ptrobj->GenAnim = GET_S16;
        ptrobj->Sprite = GET_S16;

        ptrobj->OldPosX = ptrobj->Obj.X = GET_S16;
        ptrobj->OldPosY = ptrobj->Obj.Y = GET_S16;
        ptrobj->OldPosZ = ptrobj->Obj.Z = GET_S16;
        ptrobj->HitForce = GET_S8;
        ptrobj->OptionFlags = GET_S16;
        ptrobj->OptionFlags &= ~EXTRA_GIVE_NOTHING;
        ptrobj->Obj.Beta = GET_S16;
        ptrobj->SRot = GET_S16;
        ptrobj->Move = GET_S8;

        if (!(ptrobj->Flags & SPRITE_3D)
                 AND ptrobj->Move != MOVE_WAGON // cf TM_SPEED
                                         AND ptrobj->SRot) {
            // A Priori, ce test ne sert à rien ici
            if (ptrobj->SRot == 1) {
                ptrobj->SRot++;
            }

            ptrobj->SRot = (S16)(51200 / (S32)ptrobj->SRot); // 1024*1000/20
        }

        ptrobj->Info = GET_S16;
        ptrobj->Info1 = GET_S16;
        ptrobj->Info2 = GET_S16;
        ptrobj->Info3 = GET_S16;

        ptrobj->NbBonus = GET_S16;
        ptrobj->CoulObj = GET_S8;

        if (ptrobj->Flags & ANIM_3DS) {
            // num anim 3DS
            ptrobj->Coord.A3DS.Num = GET_U32;
            // NbFps
            ptrobj->Info3 = ptrobj->SizeSHit = GET_S16;
        }

        ptrobj->Armure = GET_S8;
        ptrobj->LifePoint = GET_S8;

        // Message chapter (a virer)
        /*			for( t=0; t<MAX_CHAPTER; t++ )
			{
				ptrobj->MessageChapter[t] = GET_S16 ;
			}
*/
        sizetoload = GET_S16;
        ptrobj->PtrTrack = PtrSce;
        PtrSce = PtrSce + sizetoload;

        sizetoload = GET_S16;
        ptrobj->PtrLife = PtrSce;
        PtrSce = PtrSce + sizetoload;
    }

    Checksum = GET_U32;

    // zone declechement: ZONE

    NbZones = GET_S16;
    ListZone = (T_ZONE *)PtrSce;
    PtrSce += NbZones * sizeof(T_ZONE);

    ptrz = ListZone;

    for (n = 0; n < NbZones; n++, ptrz++) {
        switch (ptrz->Type) {
        case 0: // change cube
            if (ptrz->Info5 & 1)
                ptrz->Info5 |= ZONE_TEST_BRICK;
            else
                ptrz->Info5 &= ~(ZONE_TEST_BRICK);

            if (ptrz->Info6 & 1)
                ptrz->Info6 |= ZONE_DONT_REAJUST_POS_TWINSEN;
            else
                ptrz->Info6 &= ~(ZONE_DONT_REAJUST_POS_TWINSEN);
        case 1: // camera
            if (ptrz->Info7 & 1)
                ptrz->Info7 |= ZONE_ON;
            else
                ptrz->Info7 &= ~(ZONE_ON);

            ptrz->Info7 &= ~(ZONE_ACTIVE);
            break;

        case 3: // zone grm
            ptrz->Info1 = 0;
            break;

        case 4:              /* zone giver */
            ptrz->Info2 = 0; /* pas encore pris */
            break;

        case 6: // echelle
        case 9: // Rail
            ptrz->Info1 = ptrz->Info0;
            break;

        case 8:              // zone Hit
            ptrz->Info3 = 0; // init TimerRef
            break;
        }
    }

    // point track: TRACK

    NbBrickTrack = GET_S16;
    ListBrickTrack = (T_TRACK *)PtrSce;
    PtrSce += NbBrickTrack * sizeof(T_TRACK);

    //--------- PATCHES ----------
    NbPatches = GET_U32;
    ListPatches = (T_PATCH *)PtrSce;
    //----------------------------

    if (NumCube == NUM_CUBE_PHANTOM) {
        Island = PhantomIsland;
        CurrentCubeX = PhantomX;
        CurrentCubeY = PhantomY;
    }

    ManageSystem(); // for streaming

    ChoicePalette();

    //		FlagFade = TRUE ;

    // Init suivant type de cube
    PtrInit3DGame();

    return TRUE;
}

/*──────────────────────────────────────────────────────────────────────────*/
// Pour le buggy pour recupérer les coordonnées 2D d'un cube extérieur dans
// la map
S32 LoadSceneCubeXY(S32 numcube, S32 *cubex, S32 *cubey) {
    S32 x, y;
    U8 island, shadowlvl, modelaby, cubemode;
    char tmpFilePath[ADELINE_MAX_PATH];

    // ATTENTION: chargée dans ScreenAux car normalement appelée dans le
    //	      comportement d'init du buggy

    // numscene+1 car en 0 se trouve SizeCube.MAX (taille du plus gros .SCC)
    GetResPath(tmpFilePath, ADELINE_MAX_PATH, SCENE_HQR_NAME);
    if (!Load_HQR(tmpFilePath, ScreenAux, numcube + 1)) {
        TheEndCheckFile(tmpFilePath);
    }

    *cubex = *cubey = 0;

    PtrSce = ScreenAux;

    // info world: INFO_WORLD

    island = GET_S8;

    // Sert uniquement pour la 3DExt
    x = GET_S8;
    y = GET_S8;

    shadowlvl = GET_S8;
    modelaby = GET_S8;
    cubemode = GET_S8;

    if (cubemode == CUBE_EXTERIEUR AND island == Island AND abs(x - CurrentCubeX) <= 1 AND abs(y - CurrentCubeY) <= 1) {
        *cubex = x;
        *cubey = y;

        return TRUE;
    } else
        return FALSE;
}

/*──────────────────────────────────────────────────────────────────────────*/

/* A folder the engine needs, made here or named here.

   Reporting it is the whole point of the wrapper. A folder that cannot be created is a
   failure the run survives and the player meets much later, at whatever first tried to
   write into it, in a message about a file. Saying it once at boot, next to the paths
   the banner has just printed, is the difference between "the recordings folder could
   not be created" and "unable to save game to <a path that reads as fine>". */
static void AddDirOrWarn(const char *path) {
    if (AddDirIfNot(path)) {
        return;
    }

    /* With the reason. "Could not create" alone sends a player to check permissions and
       free space whichever of them it was, and the two that actually happen -- a name
       already taken, and no permission to write there -- want opposite answers.

       Empty is itself one of the two failures rather than a missing message: AddDirIfNot
       answers a path that exists and is not a directory without reaching SDL, and clears
       the error on the way in so this cannot report an older one. */
    const char *why = SDL_GetError();
    Log_Warn("Folders    could not create %s: %s", path,
             (why[0] != '\0') ? why : "the name is taken by something that is not a folder");
}

/*──────────────────────────────────────────────────────────────────────────*/

void CreateLbaDirectories(void) {
    char tmpDirPath[ADELINE_MAX_PATH];

#ifndef DEMO
    /* Silent, unlike the rest, because this one is under the game data rather than the
       user directory and cannot tell a real failure from an ordinary install. The
       folder ships with the data, so this is a no-op wherever the voices exist; an
       install on read-only media is a normal thing to have; and with a disc image
       mounted, ExistsFileOrDir answers for this path out of the image (DISCIMG maps
       anything under the install dir), so a folder that is only in the image reads as
       present and not a directory. A warning here would fire on a healthy install. */
    GetVoicePath(tmpDirPath, ADELINE_MAX_PATH, NULL);
    AddDirIfNot(tmpDirPath);
#endif

    GetSavePath(tmpDirPath, ADELINE_MAX_PATH, NULL);
    AddDirOrWarn(tmpDirPath);

    GetShootPath(tmpDirPath, ADELINE_MAX_PATH, NULL);
    AddDirOrWarn(tmpDirPath);

    GetRecordingsPath(tmpDirPath, ADELINE_MAX_PATH, NULL);
    AddDirOrWarn(tmpDirPath);

#ifdef DEBUG_TOOLS
    GetBugPath(tmpDirPath, ADELINE_MAX_PATH, NULL);
    if (!ExistsFileOrDir(tmpDirPath)) {
        if (AddDirIfNot(tmpDirPath)) {
            LogPrintf("Created bugs directory: %s\n", tmpDirPath);
        }
    }
#endif

    GetLogPath(tmpDirPath, ADELINE_MAX_PATH, NULL);
    AddDirOrWarn(tmpDirPath);
}

/*══════════════════════════════════════════════════════════════════════════*/
/*══════════════════════════════════════════════════════════════════════════*/
