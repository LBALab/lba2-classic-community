# Music state machine

How the game decides *what music plays and when*: the track routing, the
`PlayMusic` decision, the deferred-switch queue, and the stream backend that
turns a CD/jingle request into audio. This is the layer that produced #353-#355
and the resume-silence regression.

For the retail assets, the full AIL contract, SFX/ambience/voice scripting, and
diagnostic logging, see [AUDIO.md](AUDIO.md). For the clocks the fades and the
defer window run on, see [TIMING.md](TIMING.md).

## Two eras, one stream

The 1997 engine mixed **redbook CD tracks** with short **streamed ADPCM jingles**.
The SDL port keeps the `CDROM` build define on (it is set in
`SOURCES/CMakeLists.txt`), so the CD code paths are compiled in, but there is no
CD hardware: `LIB386/AIL/SDL/CD.CPP` maps `PlayCD(track)` to `MUSIC/Track%02d.wav`
and hands it to `PlayStream`. So in practice **every track, CD or jingle, is one
SDL audio stream** (a WAV or its OGG transcode). There is a single stream device
at a time.

Layers:

| Layer | File | Role |
|-------|------|------|
| Game state machine | `SOURCES/MUSIC.CPP` | routing, the `PlayMusic` decision, the `NextMusic` queue, fades, pause/resume orchestration |
| Redbook shim | `LIB386/AIL/SDL/CD.CPP` | `PlayCD` -> `Track%02d.wav` -> `PlayStream` |
| Stream backend | `LIB386/AIL/SDL/STREAM.CPP` | one `SDL_AudioStream`, WAV/OGG decode, the park/resume model |
| Park decision seam | `LIB386/H/AIL/STREAM_PARK.H` | the pure pause/resume/focus decisions, host-tested |

## Track routing

`PtrTrackCD[num]` maps a logical track index to a byte: the top bit `JINGLE`
(`0x80`) marks a streamed jingle, the low 7 bits (`MUSIC` mask, `0x7F`) are the
track/jingle number. Two layouts, selected by `DistribVersion` in `InitTabTracks`:

- **EU** (`EA_VERSION`/`UNKNOWN_VERSION`) -> `TrackCD`: every entry is `JINGLE`,
  so everything streams. `FirstCDTrack = 6`.
- **US** (default) -> `TrackCDUS`: the first entries (`Track01`-`Track06`) are
  plain CD tracks, the rest are jingles. `FirstCDTrack` differs.

`PlayMusic` reads `PtrTrackCD[num]`: `JINGLE` set -> `PlayJingle`, else (CDROM)
`PlayCD`. `PlayJingle(n)` streams `ListJingle[n - FIRST_JINGLE + 1] + ".WAV"`
(`FIRST_JINGLE = 2`); the `ListJingle` table holds the `JADPCM*` / `TADPCM*`
basenames. `GetNumJingle` reverses it: it strips the path and extension from the
playing stream name and matches it back to a `ListJingle` index, so `GetMusic`
can report which jingle is playing.

## The PlayMusic decision

`PlayMusic(num, playit)` is the heart of the state machine. `num` is the track
index; `playit` (a.k.a. force) chooses immediate replacement over deferral.

```
NextMusic = -1
cur = (playit OR StopLastMusic) ? 0 : GetMusic()   // what is playing now (0 = nothing)
StopLastMusic = playit
if cur == 0:                                        // nothing playing (or forced)
    if !playit OR GetMusic() != this-track:         // don't restart the exact same track
        StopMusic(); PlayJingle/PlayCD(this-track)  // play now
else:                                               // something else is playing
    if cur != this-track:
        NextMusic = num                             // DEFER (queue for later)
NextMusicTimer = now + TEST_MUSIC_TEMPO             // 2000 ms
```

The matrix that falls out of this (all covered by tests, see below):

| Situation | Result |
|-----------|--------|
| Nothing playing, soft request | plays now |
| Same track re-requested | no-op (never restarts) |
| Different track, soft request | **deferred** into `NextMusic` |
| Force (`playit=TRUE`), different track | replaces immediately |
| Force, same track already playing | no-op |
| `StopLastMusic` set (call right after a force) | next soft request plays now, not deferred |

## The deferred-switch queue

A soft request for a *different* track does not cut the current one; it parks the
request in `NextMusic` with a `now + TEST_MUSIC_TEMPO` (2 s) deadline. The purpose
is to ride out a quick pass-through of an area instead of chopping its music.

`CheckNextMusic()` (called every frame from `PERSO.CPP`) drains it:

```
if NextMusic != -1 AND now > NextMusicTimer:
    PlayMusic(NextMusic, TRUE)   // force the switch
```

The `TRUE` is the #355 fix. The original `FALSE` re-deferred whenever the current
track was still playing, so with music that outlasts the 2 s window the queued
track never played (you kept hearing the old one until a menu toggle nudged the
stream). Forcing takes over once the window elapses.

**Consequence: scene-music switches lag by ~2 s.** On a cube change the old jingle
keeps playing; the new one is queued and only takes over after the defer window.
That delay is the queue, not the codec. Levers if it feels too long: lower
`TEST_MUSIC_TEMPO`, or make the cube-entry call in `PERSO.CPP` force
(`PlayMusic(CubeJingle, TRUE)`) at the cost of chopping music on quick
pass-throughs.

## GetMusic, StopMusic, fades

- `GetMusic()` reports what is playing: `IsCDPlaying()` (returns the track + it
  is offset by `FirstCDTrack`), otherwise `GetNumJingle(StreamName())`, otherwise
  `0`. The decision above depends on this round-trip being correct.
- `StopMusic()` = `StopCD` + `StopStream`.
- `FadeOutVolumeMusic` / `FadeInVolumeMusic` ramp the jingle (and CD) volume over
  `FADE_MUSIC_TIME` timer units by spinning the virtual clock (`ManageTime`),
  driving to 0 / back to target. Volume flows through the `SetVolumeJingle(v)`
  macro = `ChangeVolumeStream(RegleTrois(0, MasterVolume, 127, v))`, so
  `MasterVolume` scales everything. See AUDIO.md for the fade asymmetry note.

## Pause and resume: two layers

Pause/resume is a reversible pair and **must never `StopMusic`** first: a Stop is
destructive on the SDL stream (it tears the device stream down), so a resume would
have nothing to unpause. This is the resume-silence regression AUDIO.md records.

There are two independent pause layers, both driving the one stream device:

1. **Logical** `PauseStream` / `ResumeStream` (the game's `PauseMusic` /
   `ResumeMusic`, for the Options menu and dialogue). `PauseStream` parks
   `{path, frame}` so a resume can restore even if a different stream (the menu
   theme) played and tore this one down in between. `ResumeStream` then either
   fast-unpauses the still-live parked stream, or re-opens the parked file and
   seeks back (mirrors the MILES redbook `PauseCD`/`ResumeCD`).
2. **Device** `SuspendStreamOutput` / `ResumeStreamOutput` (window focus loss, in
   `AMBIANCE.CPP`). A device-only pause that does **not** touch the parked
   position, and `ResumeStreamOutput` un-pauses the device only if the stream is
   not *logically* paused. That nesting invariant keeps a focus blip during a
   menu/dialogue pause from un-pausing the music.

The decision logic for both (fast-unpause vs restore-from-park vs nothing; and the
focus nesting rule) is extracted, SDL-free, into `LIB386/H/AIL/STREAM_PARK.H`
(`Stream_ShouldPark`, `Stream_ResumeAction`, `Stream_ShouldResumeDeviceOnFocus`).
`STREAM.CPP`'s `PauseStream`/`ResumeStream`/`ResumeStreamOutput` are thin wrappers
that call these and run the SDL side effects, so the invariants are unit-testable.

## Stream backend: WAV vs OGG

The game always requests `.WAV`; `PlayStreamInternal` (`STREAM.CPP`) resolves it,
trying filesystem WAV, then the `.ogg` transcode, then a mounted disc image, then
cue audio. WAV and OGG then reach the stream very differently:

| | WAV (`SetupWavStream`) | OGG (`PlayOggFile`) |
|---|---|---|
| Decode | `SDL_LoadWAV` decodes the whole file up front (synchronous) | `stb_vorbis` on a **background decode thread**, incrementally |
| Push | all PCM in one call, before the device resumes | chunks pushed as decoded |
| Cache | none | one-slot decoded-PCM cache (`oggCache`), keyed by path |
| Seek/restore | byte offset into the loaded PCM | `stb_vorbis_seek_frame` (a seeked decode is not cached), or offset on a cache hit |

Two consequences worth knowing:

- **Cold-OGG first-audio gap.** `FinishStream` resumes the device right after
  `PlayOggFile` launches the decode thread, so the device can run dry for tens of
  ms until the first chunk is pushed. WAV and OGG-cache-hit push all their PCM
  first, so they have no such gap. A scene switch is always a different track (a
  cache miss), so it always takes the cold OGG path.
- **The cache and the queue do not compose.** The OGG cache is a single slot for
  *same-track* replay/restore. The `NextMusic` queue exists to switch to a
  *different* track, which is a cache miss that also evicts the previous track.
  So the cache buys nothing on queue-driven area changes, and toggling between two
  adjacent areas re-decodes each time. This is an efficiency gap, not a bug (the
  cache is path-keyed; partial/seeked decodes are correctly not cached).

## Testing

The music state machine is host-tested; the stream *decode/threading* is not (that
is SDL and library territory). Two targets under `tests/music/`:

- **`test_music_pause_resume.cpp`** compiles the real `MUSIC.CPP` against a spy AIL
  backend (records ordered calls; a small model of "what is playing" that the test
  controls). Covers pause/resume-never-Stop (CD and jingle layouts), faded resume,
  the #355 queue drain, the full decision matrix (nothing/same/different/force/
  `StopLastMusic`), `CheckNextMusic` timing and empty-queue edges, the `GetMusic`
  round-trip, and fade convergence.
- **`test_music_stream_park.cpp`** unit-tests the pure `STREAM_PARK.H` decisions:
  fast-unpause vs restore-from-park vs nothing, and the focus-nesting invariant.

Why a mock rather than the real backend: the runtime control harness (`--exec`,
`--tick`) cannot reproduce music-state bugs, because its headless audio never
reports a stream as playing (`IsStreamPlaying()` stays 0), so `PlayMusic` always
takes the clean "nothing playing" path. The spy mock lets a test *set* "track X is
playing" and assert what the state machine does. Grow these files scenario by
scenario as behaviour is pinned down.

## Debugging on a live game

- `audio global log 1` (console) turns on the `[MUSIC]`/`[CD]` trace channel.
- `cube <N>` jumps to a scene, which runs the cube-entry music path.
- Traces land in `adeline.log` (the terminal log sink is TTY-gated, so grep the
  file, not piped stdout).

See AUDIO.md for the full logging reference and `audio global status`.
