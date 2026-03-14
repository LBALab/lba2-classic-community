#include "CONSOLE.H"
#include "../C_EXTERN.H"
#include "../COMMON.H"
#include "../DIRECTORIES.H"
#include "../PLAYACF.H"
#include "../MUSIC.H"
#include "../AMBIANCE.H"
#include "../DISKFUNC.H"
#include "../SAVEGAME.H"
#include "../INVENT.H"
#include "../GAMEMENU.H"
#include <SYSTEM/FILES.H>
#include <SYSTEM/STRING.H>
#include <SVGA/SCREEN.H>
#include <SVGA/DIRTYBOX.H>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifdef CONSOLE_MODULE
#include <SYSTEM/TIMER.H>
#endif

/*──────────────────────────────────────────────────────────────────────────*/
/* Command handlers */
/*──────────────────────────────────────────────────────────────────────────*/

static void cmd_cube(int argc, const char *argv[])
{
	if (argc < 2) {
		Console_Print("Usage: cube <number>");
		return;
	}
	Console_Close();
	NewCube = (S32)strtol(argv[1], NULL, 0);
	Console_Print("Cube %d (change on next frame)", NewCube);
}

static void cmd_load(int argc, const char *argv[])
{
	if (argc < 2) {
		Console_Print("Usage: load <save_name>");
		return;
	}

	/* Reconstruct full name from all arguments (so names with spaces work). */
	char targetName[MAX_SIZE_PLAYER_NAME + 1];
	targetName[0] = '\0';
	for (int i = 1; i < argc; i++) {
		if (i > 1 && strlen(targetName) + 1 < sizeof(targetName) - 1)
			strcat(targetName, " ");
		if (strlen(targetName) + strlen(argv[i]) < sizeof(targetName) - 1)
			strcat(targetName, argv[i]);
	}

	/* First, try to resolve by player name as printed by listsaves. */
	char pathname[ADELINE_MAX_PATH];
	struct dirent *file;
	DIR *dir;
	S32 handle;
	U8 wbyte;
	S32 wlong;
	char playername[MAX_SIZE_PLAYER_NAME + 1];
	int foundByName = 0;

	GetSavePath(pathname, ADELINE_MAX_PATH, NULL);
	dir = opendir(pathname);
	if (dir) {
		while ((file = readdir(dir)) != NULL) {
			if (ade_strcasestr(file->d_name, ".LBA") == NULL)
				continue;
			GetSavePath(pathname, ADELINE_MAX_PATH, file->d_name);
			struct stat st;
			if (stat(pathname, &st) != 0 || !S_ISREG(st.st_mode))
				continue;
			if (ade_strcasestr(pathname, AUTOSAVE_FILENAME) != NULL
			    || ade_strcasestr(pathname, CURRENTSAVE_FILENAME) != NULL)
				continue;
			GetSavePath(pathname, ADELINE_MAX_PATH, file->d_name);
			handle = OpenRead(pathname);
			if (!handle)
				continue;
			Read(handle, &wbyte, 1);
			Read(handle, &wlong, sizeof(wlong));
			S32 i = 0;
			do {
				Read(handle, &wbyte, 1);
				playername[i++] = (char)wbyte;
			} while (wbyte != 0 && i < (S32)(sizeof(playername) - 1));
			playername[i] = '\0';
			Close(handle);
			if (playername[0] && strcasecmp(playername, targetName) == 0) {
				/* Found matching save by player name. */
				GetSavePath(GamePathname, ADELINE_MAX_PATH, file->d_name);
				foundByName = 1;
				break;
			}
		}
		closedir(dir);
	}

	/* If no player-name match, fall back to treating input as filename. */
	if (!foundByName) {
		char filename[ADELINE_MAX_PATH];
		size_t len = strlen(targetName);
		if (len >= sizeof(filename))
			len = sizeof(filename) - 1;
		memcpy(filename, targetName, len);
		filename[len] = '\0';

		/* Append .lba if not already present (case-insensitive). */
		const char *dot = strrchr(filename, '.');
		if (!dot || ade_strcasestr(dot, ".lba") != dot) {
			size_t cur = strlen(filename);
			if (cur + 4 < sizeof(filename)) {
				filename[cur + 0] = '.';
				filename[cur + 1] = 'l';
				filename[cur + 2] = 'b';
				filename[cur + 3] = 'a';
				filename[cur + 4] = '\0';
			}
		}

		GetSavePath(GamePathname, ADELINE_MAX_PATH, filename);
		if (!ExistsFileOrDir(GamePathname)) {
			Console_Print("Save not found: %s", targetName);
			return;
		}
	}

	Console_Close();
	FlagLoadGame = TRUE;
	LoadGameNumCube();
	Console_Print("Loading save: %s", targetName);
}

static void cmd_loadbug(int argc, const char *argv[])
{
	(void)argc;
	(void)argv;
#ifdef DEBUG_TOOLS
	Console_Print("Use debug menu to load bug save.");
#else
	Console_Print("Not available (build with DEBUG_TOOLS).");
#endif
}

#ifdef DEBUG_TOOLS
static void cmd_savebug(int argc, const char *argv[])
{
	if (NumCube == NUM_CUBE_PHANTOM || PtrScene == NULL || FlagPlayAcf) {
		Console_Print("savebug only available in-game when a cube is loaded.");
		return;
	}
	const char *name = (argc >= 2 && argv[1][0]) ? argv[1] : "bug";
	/* Sanitize name for filename: keep alphanumeric, dash, underscore. */
	char safeName[MAX_SIZE_PLAYER_NAME + 1];
	size_t i, j;
	for (i = 0, j = 0; name[i] && j < MAX_SIZE_PLAYER_NAME; i++) {
		char c = name[i];
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')
			safeName[j++] = c;
	}
	safeName[j] = '\0';
	if (j == 0) {
		safeName[0] = 'b'; safeName[1] = 'u'; safeName[2] = 'g'; safeName[3] = '\0';
	}
	char bugsDir[ADELINE_MAX_PATH];
	GetSavePath(bugsDir, ADELINE_MAX_PATH, "bugs");
	AddDirIfNot(bugsDir);
	char relPath[ADELINE_MAX_PATH];
	snprintf(relPath, sizeof(relPath), "bugs%c%s.lba", '/', safeName);
	GetSavePath(GamePathname, ADELINE_MAX_PATH, relPath);
	strncpy(PlayerName, safeName, MAX_SIZE_PLAYER_NAME);
	PlayerName[MAX_SIZE_PLAYER_NAME] = '\0';
	Console_Close();
	NumVersion = NUM_VERSION | SAVE_COMPRESS;
	SaveGame(TRUE);
	Console_Print("Bug save: %s", GamePathname);
}
#endif

static void cmd_timer(int argc, const char *argv[])
{
#ifdef CONSOLE_MODULE
	S32 delta = (argc >= 2 && argv[1][0]) ? (S32)strtol(argv[1], NULL, 0) : 200;
	SetTimerHR(TimerRefHR + (U32)delta);
	Console_Print("Timer +%d (now %u)", delta, (unsigned int)TimerRefHR);
#else
	(void)argc;
	(void)argv;
	Console_Print("timer requires CONSOLE_MODULE (timer API).");
#endif
}

static void cmd_status(int argc, const char *argv[])
{
	(void)argc;
	(void)argv;
	Console_Print("Island: %d  Cube: %d  Chapter: %d", Island, NumCube, (int)ListVarGame[FLAG_CHAPTER]);
	Console_Print("Obj: %d  Zones: %d  Tracks: %d", NbObjets, NbZones, NbBrickTrack);
#ifdef CONSOLE_MODULE
	{
		extern volatile S32 NbFramePerSecond;
		extern volatile U32 TimerRefHR;
		Console_Print("FPS: %d  TimerRefHR: %u", NbFramePerSecond, (unsigned int)TimerRefHR);
	}
#else
	Console_Print("(FPS/timer with CONSOLE_MODULE)");
#endif
	Console_Print("Pos: (%d,%d,%d)", Nxw, Nyw, Nzw);
}

static void cmd_screenshot(int argc, const char *argv[])
{
	(void)argc;
	(void)argv;
	char filename[64];
	char path[ADELINE_MAX_PATH];
	snprintf(filename, sizeof(filename), "shot_%lu.png", (unsigned long)time(NULL));
	GetShootPath(path, ADELINE_MAX_PATH, filename);
	Console_RequestScreenshot(path, 1); /* 1 = PNG via SavePNG in game */
	Console_Print("Screenshot (no console): %s", path);
}

static void cmd_give(int argc, const char *argv[])
{
	if (argc < 2) {
		Console_Print("Usage: give <inventory_index> (0..%d)", (int)(MAX_INVENTORY - 1));
		return;
	}
	/* Only meaningful when a real scene is loaded (not in menus, phantom cube, or during video). */
	if (NumCube == NUM_CUBE_PHANTOM || PtrScene == NULL || FlagPlayAcf) {
		Console_Print("give is only available in-game when a cube is loaded.");
		return;
	}
	Console_Close();
	S32 numvar = (S32)strtol(argv[1], NULL, 0);
	if (numvar < 0 || numvar >= (S32)MAX_INVENTORY) {
		Console_Print("Invalid inventory index %d (valid 0..%d)", numvar, (int)(MAX_INVENTORY - 1));
		return;
	}
	/* Match script behavior: run found-object cinematic with saved timer context. */
	SaveTimer();
	DoFoundObj(numvar);
	RestoreTimer();
	Console_Print("Gave inventory entry %d", numvar);
}

static void cmd_playvideo(int argc, const char *argv[])
{
	if (argc < 2) {
		Console_Print("Usage: playvideo <name>");
		return;
	}
	Console_Close();
	if (PlayAcf(argv[1]) >= 0)
		Console_Print("Playing video: %s", argv[1]);
	else
		Console_Print("Video not found: %s", argv[1]);
}

static void cmd_listvideos(int argc, const char *argv[])
{
	/* GetAcfName has no size limit; use a safe buffer for long names. */
	char buf[128];
	ResetAcfList();
	while (GetAcfName(buf))
		Console_Print("  %s", buf);
}

static void cmd_playjingle(int argc, const char *argv[])
{
	if (argc < 2) {
		Console_Print("Usage: playjingle <1-26>");
		return;
	}
	S32 num = (S32)strtol(argv[1], NULL, 0);
	PlayJingle(num);
	Console_Print("Jingle %d", num);
}

static void cmd_listjingles(int argc, const char *argv[])
{
	(void)argc;
	(void)argv;
}

static void cmd_playmusic(int argc, const char *argv[])
{
	if (argc < 2) {
		Console_Print("Usage: playmusic <num>");
		return;
	}
	S32 num = (S32)strtol(argv[1], NULL, 0);
	PlayMusic(num, 1);
	Console_Print("Music %d", num);
}

static void cmd_listmusic(int argc, const char *argv[])
{
	(void)argc;
	(void)argv;
}

static void cmd_playsample(int argc, const char *argv[])
{
	if (argc < 2) {
		Console_Print("Usage: playsample <num>");
		return;
	}
	S32 num = (S32)strtol(argv[1], NULL, 0);
	HQ_MixSample(num, 0x1000, 0, 1, 64, 127);
	Console_Print("Sample %d", num);
}

static void cmd_listsamples(int argc, const char *argv[])
{
	(void)argc;
	(void)argv;
}

static void cmd_buildinfo(int argc, const char *argv[])
{
	(void)argc;
	(void)argv;
	Console_Print("%s", BuildInfo ? BuildInfo : "(no build info)");
}

static void cmd_exit(int argc, const char *argv[])
{
	(void)argc;
	(void)argv;
	/* Immediate clean exit requested from console. */
	TheEnd(PROGRAM_OK, "Exit from console");
}

static void cmd_listsaves(int argc, const char *argv[])
{
	(void)argc;
	(void)argv;
	char pathname[ADELINE_MAX_PATH];
	struct dirent *file;
	DIR *dir;
	S32 handle;
	U8 wbyte;
	S32 wlong;
	char playername[MAX_SIZE_PLAYER_NAME + 1];
	S32 nb = 0;

	GetSavePath(pathname, ADELINE_MAX_PATH, NULL);
	dir = opendir(pathname);
	if (!dir) {
		Console_Print("Save directory not found.");
		return;
	}
	while ((file = readdir(dir)) != NULL) {
		if (ade_strcasestr(file->d_name, ".LBA") == NULL)
			continue;
		GetSavePath(pathname, ADELINE_MAX_PATH, file->d_name);
		struct stat st;
		if (stat(pathname, &st) != 0 || !S_ISREG(st.st_mode))
			continue;
		if (ade_strcasestr(pathname, AUTOSAVE_FILENAME) != NULL
		    || ade_strcasestr(pathname, CURRENTSAVE_FILENAME) != NULL)
			continue;
		GetSavePath(pathname, ADELINE_MAX_PATH, file->d_name);
		handle = OpenRead(pathname);
		if (!handle)
			continue;
		Read(handle, &wbyte, 1);
		Read(handle, &wlong, sizeof(wlong));
		S32 i = 0;
		do {
			Read(handle, &wbyte, 1);
			playername[i++] = (char)wbyte;
		} while (wbyte != 0 && i < (S32)(sizeof(playername) - 1));
		playername[i] = '\0';
		Close(handle);
		Console_Print("  %s", playername[0] ? playername : file->d_name);
		nb++;
	}
	closedir(dir);
	Console_Print("Saves: %d", nb);
}

static void cmd_listbugs(int argc, const char *argv[])
{
	(void)argc;
	(void)argv;
	struct dirent *file;
	DIR *dir;
	S32 nb = 0;
	char pathBugs[ADELINE_MAX_PATH];

	GetSavePath(pathBugs, ADELINE_MAX_PATH, "bugs");
	dir = opendir(pathBugs);
	if (!dir) {
		Console_Print("Bug save directory not found: %s", pathBugs);
		return;
	}
	while ((file = readdir(dir)) != NULL) {
		if (ade_strcasestr(file->d_name, ".LBA") != NULL) {
			Console_Print("  %s", file->d_name);
			nb++;
		}
	}
	closedir(dir);
	Console_Print("Bug saves: %d", nb);
}

/*──────────────────────────────────────────────────────────────────────────*/
/* Registration */
/*──────────────────────────────────────────────────────────────────────────*/

void Console_RegisterAll(void)
{
	Console_RegisterCommand("cube",   cmd_cube,   "Go to cube N (e.g. cube 5)");
	Console_RegisterCommand("load",  cmd_load,   "Load save by player name or file name");
	Console_RegisterCommand("loadbug", cmd_loadbug, "Load bug save (see listbugs)");
	Console_RegisterCommand("give",  cmd_give,   "Give inventory item by flag number");
	Console_RegisterCommand("playvideo", cmd_playvideo, "Play ACF video by name");
	Console_RegisterCommand("listvideos", cmd_listvideos, "List available ACF video names");
	Console_RegisterCommand("playjingle", cmd_playjingle, "Play jingle 1-26");
	Console_RegisterCommand("playmusic", cmd_playmusic, "Play music track by number");
	Console_RegisterCommand("playsample", cmd_playsample, "Play sample by index");
	Console_RegisterCommand("buildinfo", cmd_buildinfo, "Build timestamp and CMake flags");
	Console_RegisterCommand("exit", cmd_exit, "Exit the game immediately");
	Console_RegisterCommand("listsaves", cmd_listsaves, "List save games (names)");
	Console_RegisterCommand("listbugs", cmd_listbugs, "List bug saves in bugs directory");
	Console_RegisterCommand("timer", cmd_timer, "Advance timer by N ms (default 200)");
	Console_RegisterCommand("status", cmd_status, "Print island, cube, chapter, objs, FPS, pos");
	Console_RegisterCommand("screenshot", cmd_screenshot, "Save next frame as PPM (no console)");
#ifdef DEBUG_TOOLS
	Console_RegisterCommand("savebug", cmd_savebug, "Save current game to bugs dir (optional name)");
#endif

	Console_RegisterCvar("fps", &DebugFps, CONSOLE_CVAR_BOOL, "Frame rate display");
	Console_RegisterCvar("horizon", (void *)&FlagDrawHorizon, CONSOLE_CVAR_BOOL_U8, "Draw horizon/cubes around");
	Console_RegisterCvar("zv", &DrawZVObjets, CONSOLE_CVAR_BOOL, "Draw ZV boxes");
}
