// Host-only test for release identity in SOURCES/DISTRIB.CPP.
//
// Two inputs settle what release a run thinks it is: what the config declared,
// and what the assets measure as. The rules between them are small and the
// consequences are not, so they are pinned here rather than left to a boot log
// nobody reads.
//
// What must hold:
//   - a declaration always wins, whatever the data says, and always shows the
//     publisher splash it asked for;
//   - nothing declared never shows a splash, because which logo a release was
//     licensed to display is not a fact its assets carry;
//   - nothing declared over European or unrecognised data resolves exactly as
//     it did before anything was detected, so recognising a re-release changes
//     nothing about how it runs;
//   - nothing declared over American data resolves to the American arm, which
//     is the one case that renders wrong without detection;
//   - the demo's data is neither master and must not be read as either;
//   - a size the table does not hold is an ordinary answer, not a failure.

#include <cstdio>
#include <cstdlib>
#include <string>

#include "../../SOURCES/DEFINES.H"
#include "../../SOURCES/DISTRIB.H"

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, \
                         __LINE__, #cond);                              \
            std::abort();                                               \
        }                                                               \
    } while (0)

namespace {

struct Resolved {
    S32 version;
    bool splash;
};

Resolved Resolve(bool declared, S32 declaredVersion, T_DATA_MASTER master) {
    Resolved out = {-1, false};
    Distrib_Resolve(declared, declaredVersion, master, &out.version, &out.splash);
    return out;
}

const T_DATA_MASTER kAllMasters[] = {
    DATA_MASTER_UNRECOGNISED,
    DATA_MASTER_LBA2,
    DATA_MASTER_TWINSEN,
    DATA_MASTER_DEMO,
};

const S32 kAllVersions[] = {
    UNKNOWN_VERSION,
    ACTIVISION_VERSION,
    ACTIVISION_SUD_VERSION,
    EA_VERSION,
    VIRGIN_VERSION,
    VIRGIN_ASIA_VERSION,
};

void test_sizes(void) {
    CHECK(Distrib_MasterFromRessSize(582473) == DATA_MASTER_LBA2);
    CHECK(Distrib_MasterFromRessSize(582445) == DATA_MASTER_TWINSEN);
    CHECK(Distrib_MasterFromRessSize(289888) == DATA_MASTER_DEMO);

    // A size no sampled release has, including the two that sit either side of
    // a known one: sizes are a weak hash and near-misses are misses.
    CHECK(Distrib_MasterFromRessSize(582472) == DATA_MASTER_UNRECOGNISED);
    CHECK(Distrib_MasterFromRessSize(582474) == DATA_MASTER_UNRECOGNISED);
    CHECK(Distrib_MasterFromRessSize(1) == DATA_MASTER_UNRECOGNISED);

    // FileSize answers 0 for a file it could not open, so a bank that is not
    // there has to read as unrecognised rather than match anything.
    CHECK(Distrib_MasterFromRessSize(0) == DATA_MASTER_UNRECOGNISED);
}

void test_version_to_master(void) {
    // The split every consumer makes, which is what lets a declaration naming a
    // publisher be compared against a measurement that cannot see one.
    CHECK(Distrib_MasterForVersion(UNKNOWN_VERSION) == DATA_MASTER_LBA2);
    CHECK(Distrib_MasterForVersion(EA_VERSION) == DATA_MASTER_LBA2);
    CHECK(Distrib_MasterForVersion(ACTIVISION_VERSION) == DATA_MASTER_TWINSEN);
    CHECK(Distrib_MasterForVersion(ACTIVISION_SUD_VERSION) == DATA_MASTER_TWINSEN);
    CHECK(Distrib_MasterForVersion(VIRGIN_VERSION) == DATA_MASTER_TWINSEN);
    CHECK(Distrib_MasterForVersion(VIRGIN_ASIA_VERSION) == DATA_MASTER_TWINSEN);
}

void test_declaration_wins(void) {
    for (S32 want : kAllVersions) {
        for (T_DATA_MASTER master : kAllMasters) {
            const Resolved got = Resolve(true, want, master);
            CHECK(got.version == want);
            CHECK(got.splash);
        }
    }

    // Including the case the whole exercise exists to surface: a config naming
    // one master over data that measures as the other. The declaration still
    // stands, because the engine latches derived state from it at boot and a
    // size table is not grounds for overruling a human.
    const Resolved conflicted = Resolve(true, EA_VERSION, DATA_MASTER_TWINSEN);
    CHECK(conflicted.version == EA_VERSION);
}

void test_nothing_declared(void) {
    // European, demo and unrecognised data all resolve to the value a config
    // with no Version key has always produced. Nothing about how those installs
    // run changes; only the boot line gains a word about what the data is.
    const T_DATA_MASTER kUnchanged[] = {
        DATA_MASTER_LBA2,
        DATA_MASTER_DEMO,
        DATA_MASTER_UNRECOGNISED,
    };
    for (T_DATA_MASTER master : kUnchanged) {
        const Resolved got = Resolve(false, UNKNOWN_VERSION, master);
        CHECK(got.version == UNKNOWN_VERSION);
        CHECK(!got.splash);
    }

    // American data with no config is the case that renders as European today:
    // the wrong panel sprite, the wrong attract logo, the wrong disc label.
    const Resolved american = Resolve(false, UNKNOWN_VERSION, DATA_MASTER_TWINSEN);
    CHECK(american.version == ACTIVISION_VERSION);
    CHECK(Distrib_MasterForVersion(american.version) == DATA_MASTER_TWINSEN);

    // Still no splash. The value came from the assets, and a release shipping no
    // config ships no publisher branding to go with it.
    CHECK(!american.splash);
}

void test_declared_value_ignored_when_absent(void) {
    // Callers pass whatever they read into the out-parameter's slot; with
    // nothing declared that value must not leak through as the answer.
    const Resolved got = Resolve(false, VIRGIN_VERSION, DATA_MASTER_LBA2);
    CHECK(got.version == UNKNOWN_VERSION);
}

void test_names(void) {
    // Named for the game as it was sold, which is what the release line prints.
    CHECK(std::string(Distrib_MasterName(DATA_MASTER_LBA2)) == "LBA2");
    CHECK(std::string(Distrib_MasterName(DATA_MASTER_TWINSEN)) == "Twinsen's Odyssey");
    CHECK(std::string(Distrib_MasterName(DATA_MASTER_DEMO)) == "the 1997 demo");
    CHECK(std::string(Distrib_MasterName(DATA_MASTER_UNRECOGNISED)) == "unrecognised");
}

} // namespace

int main(void) {
    test_sizes();
    test_version_to_master();
    test_declaration_wins();
    test_nothing_declared();
    test_declared_value_ignored_when_absent();
    test_names();

    std::printf("test_distrib_resolve: OK\n");
    return 0;
}
