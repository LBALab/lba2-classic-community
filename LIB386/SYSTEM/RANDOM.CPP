//──────────────────────────────────────────────────────────────────────────
#include <SYSTEM/RANDOM.H>

//──────────────────────────────────────────────────────────────────────────
// glibc's TYPE_3 generator, which is what its rand() has been for decades.
//
// State is a 31 entry ring. Each draw adds the entry 3 places behind the front
// into the front entry, wrapping at 32 bits, and returns that sum shifted right
// one place. The low bit is dropped because it has period 2 in an additive
// generator over a power of two modulus, which is why the result is 31 bits and
// RAND_MAX is 0x7FFFFFFF rather than 0xFFFFFFFF.
//
// Seeding fills the ring from a Lehmer generator (multiplier 16807, modulus
// 2^31-1) run through Schrage's factorisation, so every intermediate fits in 32
// bits and the arithmetic below is exact in S32 without a wider type. Then 310
// draws are discarded to let the additive recurrence mix the ring.
//
// Not thread-safe, where libc's rand() is: the state below carries no lock. That
// holds because every draw in the engine is on the simulation thread. The RNG
// consumers are all in SOURCES/, and neither thing that runs off it draws: SDL's
// audio callback (SAMPLE.CPP's SampleStreamCallback, which only mixes voices) and
// STREAM.CPP's OGG decode thread. A draw added from either needs a lock here, and
// would also break replay determinism, which is the louder of the two problems.
//
// The two index constants are glibc's degree and separation for TYPE_3.
#define RND_DEG 31
#define RND_SEP 3

// Schrage's factorisation of 2147483647 = 127773 * 16807 + 2836.
#define RND_Q 127773
#define RND_A 16807
#define RND_R 2836

static S32 s_state[RND_DEG];
static S32 s_front;
static S32 s_rear;
static S32 s_seeded;
static U32 s_draws;
static U32 s_lastSeed;

//──────────────────────────────────────────────────────────────────────────
void Rnd_Seed(U32 seed) {
    S32 word;
    S32 i;

    // glibc substitutes 1 for a zero seed, because a zero ring is a fixed point
    // of the recurrence and would return zero forever.
    if (seed == 0)
        seed = 1;

    // Seeds at or above 2^31 wrap to a negative word here, exactly as glibc's
    // `int32_t word = seed` does. Schrage's identities hold either way: the
    // quotient and remainder both take the sign of the dividend, so the two
    // products stay inside S32 and the result stays inside the modulus.
    s_lastSeed = seed;

    word = (S32)seed;
    s_state[0] = word;
    for (i = 1; i < RND_DEG; i++) {
        S32 hi = word / RND_Q;
        S32 lo = word % RND_Q;
        word = RND_A * lo - RND_R * hi;
        if (word < 0)
            word += 2147483647;
        s_state[i] = word;
    }

    s_front = RND_SEP;
    s_rear = 0;
    // Set before the discard loop: Rnd_Next seeds on demand, and without this it
    // would recurse back into here on its first call.
    s_seeded = 1;
    for (i = 0; i < RND_DEG * 10; i++)
        Rnd_Next();
}

//──────────────────────────────────────────────────────────────────────────
const char *Rnd_StreamName(void) {
    return "glibc-type3";
}

//──────────────────────────────────────────────────────────────────────────
U32 Rnd_Draws(void) {
    return s_draws;
}

//──────────────────────────────────────────────────────────────────────────
U32 Rnd_LastSeed(void) {
    return s_lastSeed;
}

//──────────────────────────────────────────────────────────────────────────
S32 Rnd_Next(void) {
    U32 val;

    if (!s_seeded)
        Rnd_Seed(1);

    s_draws++;

    // Unsigned so the sum wraps rather than overflowing a signed type.
    val = (U32)s_state[s_front] + (U32)s_state[s_rear];
    s_state[s_front] = (S32)val;

    // glibc advances the front first and only rewinds the rear when the front
    // did not wrap. The asymmetry is safe because the two indices stay exactly
    // RND_SEP apart, so they never reach the end of the ring on the same draw.
    s_front++;
    if (s_front >= RND_DEG) {
        s_front = 0;
        s_rear++;
    } else {
        s_rear++;
        if (s_rear >= RND_DEG)
            s_rear = 0;
    }

    return (S32)(val >> 1);
}
