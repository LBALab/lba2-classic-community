//──────────────────────────────────────────────────────────────────────────
#ifndef LIB_SYSTEM_RANDOM
#define LIB_SYSTEM_RANDOM

#include <SYSTEM/ADELINE_TYPES.H>

//──────────────────────────────────────────────────────────────────────────
// The engine's one pseudo-random stream.
//
// Everything random in the game draws from here: rain, ambient sound, animated
// textures, particle scatter, track AI, and the LF_RND Life-script opcode. One
// stream, seeded once per cube transition in ChangeCube, advancing one step per
// draw. That topology is the original engine's and is not changed here.
//
// What the engine does not do is call libc rand(), because libc rand() is not
// the same function on two platforms:
//
//   glibc (Linux)         srand(0) -> 1804289383, 846930886, ...  RAND_MAX 2147483647
//   UCRT  (Windows)       srand(0) -> 38, 7719, ...               RAND_MAX 32767
//
// The sequences differ, and so does the range, which skews the distribution of
// every `rand() % n` on top of reordering it. Drawing from libc therefore means
// the same build, the same data and the same input producing a different game on
// the two platforms, and a recorded session that can only replay on the platform
// that recorded it.
//
// Rnd_Next is glibc's TYPE_3 additive-feedback generator, reimplemented. That
// specific choice is deliberate: it costs Linux nothing, where every committed
// baseline, corpus save and reference recording was made, and puts every other
// platform on the sequence those baselines already describe. A nicer generator
// would be the same amount of code and would describe none of them.
//
// It is not the retail sequence. Watcom's rand() is a third implementation
// again, which no port of this engine reproduces and no golden here describes.
// See docs/BIT_EXACTNESS.md for which of the three kinds of bit-exactness that
// makes this, and tests/random for the oracle: a committed vector that runs
// everywhere, plus a live comparison against the system rand() wherever the
// system is glibc.

#ifdef __cplusplus
extern "C" {
#endif

// Reseeds the stream. Seed 0 and seed 1 give the same sequence, which is glibc's
// behaviour and is relied on by nothing, but reproducing it keeps the mapping
// from seed to stream total.
void Rnd_Seed(U32 seed);

// The next draw, in [0, 2147483647]. Before the first Rnd_Seed the stream reads
// as if seeded with 1, which is what a C program gets from rand() with no srand.
S32 Rnd_Next(void);

// Which generator this build draws from, for the recording header to declare and a
// replay to compare. A recording made against one stream cannot reproduce against
// another, and saying so by name turns that into a line in the mode diff rather
// than a divergence a thousand ticks in. Change it whenever the sequence changes.
const char *Rnd_StreamName(void);

// How many draws this stream has served, seeding discards included. Not the state,
// just the position: two runs that agree on every other field but disagree here have
// taken a different number of draws, which is the one thing that moves actor decisions
// without moving anything a digest already covers.
U32 Rnd_Draws(void);

// The value the stream was last seeded with, after the zero substitution, so a caller
// that has to reproduce a stream can record the seed rather than the state.
U32 Rnd_LastSeed(void);

#ifdef __cplusplus
}
#endif

//──────────────────────────────────────────────────────────────────────────
#endif
