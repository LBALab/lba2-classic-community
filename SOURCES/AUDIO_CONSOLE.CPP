/* The console verbs the audio module owns: `playsample` and the `audio`
 * subcommand tree.
 *
 * These live here rather than in CONSOLE_CMD.CPP for the reason CODESTYLE gives
 * under "Features and surfaces": the console is a surface, generic and with no
 * opinion about who uses it, so it should not carry a feature's commands. It
 * offers Console_RegisterCommandEx and the feature fills it in, which is the
 * shape FOLLOWCAM.CPP already uses for camtrace.
 *
 * Separate from AMBIANCE.CPP because this is bindings rather than mechanism: it
 * parses argv, prints, and calls the module. That is the line SOURCES/CONSOLE/
 * already draws between its core and CONSOLE_CMD.CPP. */
#include "AUDIO_CONSOLE.H"

#include "AMBIANCE.H"
#include "CONSOLE/CONSOLE.H"
#include "MUSIC.H"

#include <AIL/SAMPLE.H>
#include <SYSTEM/ADELINE_TYPES.H>

#include <stdlib.h>
#include <string.h>

static void cmd_playsample(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Usage: playsample <num> [freq] [decal] [repeat] [volume] [pan]");
        Console_Print("  freq: pitchbend (default 0x1000)");
        Console_Print("  decal: random pitch range (default 0)");
        Console_Print("  repeat: 0=loop, >0 repeat count (default 1)");
        Console_Print("  volume: 0-127 (default 127)");
        Console_Print("  pan: 0-127 (default 64)");
        return;
    }
    S32 num = (S32)strtol(argv[1], NULL, 0);
    S32 freq = (argc >= 3 && argv[2][0]) ? (S32)strtol(argv[2], NULL, 0) : 0x1000;
    S32 decal = (argc >= 4 && argv[3][0]) ? (S32)strtol(argv[3], NULL, 0) : 0;
    S32 repeat = (argc >= 5 && argv[4][0]) ? (S32)strtol(argv[4], NULL, 0) : 1;
    S32 volume = (argc >= 6 && argv[5][0]) ? (S32)strtol(argv[5], NULL, 0) : 127;
    S32 pan = (argc >= 7 && argv[6][0]) ? (S32)strtol(argv[6], NULL, 0) : 64;

    HQ_MixSample(num, freq, decal, repeat, pan, volume);
    Console_Print("Sample %d (freq=%d decal=%d repeat=%d vol=%d pan=%d)",
                  num, freq, decal, repeat, volume, pan);
}

static void cmd_audio_help_sample(void) {
    Console_Print("audio sample commands:");
    Console_Print("  audio sample play <id> [freq] [decal] [repeat] [volume] [pan]");
    Console_Print("  audio sample stop_all");
}

static void cmd_audio_help_music(void) {
    Console_Print("audio music commands:");
    Console_Print("  audio music play <track> [loop 0|1]");
    Console_Print("  audio music stop");
    Console_Print("  audio music pause [fade 0|1]");
    Console_Print("  audio music resume [fade 0|1]");
}

static void cmd_audio_help_global(void) {
    Console_Print("audio global commands:");
    Console_Print("  audio global pause");
    Console_Print("  audio global resume");
    Console_Print("  audio global stop_all");
    Console_Print("  audio global reset");
    Console_Print("  audio global reverse_stereo <0|1>");
    Console_Print("  audio global log <0|1>");
    Console_Print("  audio global status");
}

static void cmd_audio_help_root(void) {
    Console_Print("audio commands:");
    Console_Print("  audio sample play <id> [freq] [decal] [repeat] [volume] [pan]");
    Console_Print("  audio sample stop_all");
    Console_Print("  audio music  play <track> [loop 0|1]");
    Console_Print("  audio music  stop");
    Console_Print("  audio music  pause [fade 0|1]");
    Console_Print("  audio music  resume [fade 0|1]");
    Console_Print("  audio global pause");
    Console_Print("  audio global resume");
    Console_Print("  audio global stop_all");
    Console_Print("  audio global reset");
    Console_Print("  audio global reverse_stereo <0|1>");
    Console_Print("  audio global log <0|1>");
}

static void cmd_audio(int argc, const char *argv[]) {
    if (argc < 2 || strcmp(argv[1], "help") == 0) {
        cmd_audio_help_root();
        return;
    }

    const char *domain = argv[1];

    /* audio sample ... */
    if (strcmp(domain, "sample") == 0) {
        if (argc < 3 || strcmp(argv[2], "help") == 0) {
            cmd_audio_help_sample();
            return;
        }
        const char *action = argv[2];
        if (strcmp(action, "play") == 0) {
            if (argc < 4) {
                cmd_audio_help_sample();
                return;
            }
            S32 num = (S32)strtol(argv[3], NULL, 0);
            S32 freq = (argc >= 5 && argv[4][0]) ? (S32)strtol(argv[4], NULL, 0) : 0x1000;
            S32 decal = (argc >= 6 && argv[5][0]) ? (S32)strtol(argv[5], NULL, 0) : 0;
            S32 repeat = (argc >= 7 && argv[6][0]) ? (S32)strtol(argv[6], NULL, 0) : 1;
            S32 volume = (argc >= 8 && argv[7][0]) ? (S32)strtol(argv[7], NULL, 0) : 127;
            S32 pan = (argc >= 9 && argv[8][0]) ? (S32)strtol(argv[8], NULL, 0) : 64;
            HQ_MixSample(num, freq, decal, repeat, pan, volume);
            Console_Print("Sample %d (freq=%d decal=%d repeat=%d vol=%d pan=%d)",
                          num, freq, decal, repeat, volume, pan);
            return;
        }
        if (strcmp(action, "stop_all") == 0) {
            HQ_StopSample();
            Console_Print("All samples stopped");
            return;
        }
        cmd_audio_help_sample();
        return;
    }

    /* audio music ... */
    if (strcmp(domain, "music") == 0) {
        if (argc < 3 || strcmp(argv[2], "help") == 0) {
            cmd_audio_help_music();
            return;
        }
        const char *action = argv[2];
        if (strcmp(action, "play") == 0) {
            if (argc < 4) {
                cmd_audio_help_music();
                return;
            }
            S32 num = (S32)strtol(argv[3], NULL, 0);
            S32 loop = 1;
            if (argc >= 5 && argv[4][0]) {
                loop = (S32)strtol(argv[4], NULL, 0) ? 1 : 0;
            }
            PlayMusic(num, loop);
            Console_Print("Music %d (loop=%d)", num, loop);
            return;
        }
        if (strcmp(action, "stop") == 0) {
            StopMusic();
            Console_Print("Music stopped");
            return;
        }
        if (strcmp(action, "pause") == 0) {
            S32 fade = 0;
            if (argc >= 4 && argv[3][0]) {
                fade = (S32)strtol(argv[3], NULL, 0) ? 1 : 0;
            }
            PauseMusic(fade);
            Console_Print("Music paused (fade=%d)", fade);
            return;
        }
        if (strcmp(action, "resume") == 0) {
            S32 fade = 0;
            if (argc >= 4 && argv[3][0]) {
                fade = (S32)strtol(argv[3], NULL, 0) ? 1 : 0;
            }
            ResumeMusic(fade);
            Console_Print("Music resumed (fade=%d)", fade);
            return;
        }
        cmd_audio_help_music();
        return;
    }

    /* audio global ... */
    if (strcmp(domain, "global") == 0) {
        if (argc < 3 || strcmp(argv[2], "help") == 0) {
            cmd_audio_help_global();
            return;
        }
        const char *action = argv[2];
        if (strcmp(action, "pause") == 0) {
            HQ_PauseSamples();
            Console_Print("Audio paused (samples)");
            return;
        }
        if (strcmp(action, "resume") == 0) {
            HQ_ResumeSamples();
            Console_Print("Audio resumed (samples)");
            return;
        }
        if (strcmp(action, "stop_all") == 0) {
            HQ_StopSample();
            Console_Print("All samples stopped");
            return;
        }
        if (strcmp(action, "reset") == 0) {
            HQ_StopSample();
            StopMusic();
            Audio_SetReverseStereo(ReverseStereo);
            Console_Print("Audio reset (samples stopped, music stopped, stereo reapplied)");
            return;
        }
        if (strcmp(action, "reverse_stereo") == 0) {
            if (argc < 4) {
                Console_Print("Usage: audio global reverse_stereo <0|1>");
                return;
            }
            S32 flag = (S32)strtol(argv[3], NULL, 0) ? 1 : 0;
            Audio_SetReverseStereo(flag);
            Console_Print("Reverse stereo: %s", flag ? "ON" : "OFF");
            return;
        }
        if (strcmp(action, "status") == 0) {
            Console_Print("samplesPaused=%d  Sample_Driver_Enabled=%d",
                          GetSamplesPausedCount(), Sample_Driver_Enabled);
            Console_Print("Audio backend logging: %s",
                          SfxLogIsEnabled() ? "ON" : "OFF");
            return;
        }
        if (strcmp(action, "log") == 0) {
            if (argc < 4) {
                Console_Print("Audio backend logging: %s",
                              SfxLogIsEnabled() ? "ON" : "OFF");
                Console_Print("Usage: audio global log <0|1>");
                return;
            }
            int flag = (int)strtol(argv[3], NULL, 0);
            SfxLogEnable(flag);
            Console_Print("Audio backend logging: %s",
                          SfxLogIsEnabled() ? "ON" : "OFF");
            return;
        }
        cmd_audio_help_global();
        return;
    }

    cmd_audio_help_root();
}

void Audio_RegisterCommands(void) {
    Console_RegisterCommandEx("playsample", cmd_playsample, "Play sample by index (optional params)",
                              "playsample <num> [freq] [decal] [repeat] [volume] [pan]",
                              "Defaults: freq=0x1000, decal=0, repeat=1, vol=127, pan=64.");
    Console_RegisterCommandEx("audio", cmd_audio, "Audio commands (sample/music/global)", "audio ...",
                              "Use: audio (no args) for subcommand list.");
}
