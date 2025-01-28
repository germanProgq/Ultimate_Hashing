#include "SelfHeal.h"
#include <cstring>     // std::memcpy
#include <iostream>
#include <random>      // for std::mt19937_64 & random_device
#include "QuantumProtection.h"

// Forward declare from QuantumSafe.cpp if needed
extern void qfInit(QFState& qs);

/* ------------------------------------------------------
   1) Utility: A simple 64-bit "mini-hash" function
      We mix an ephemeral key to hamper trivial forging.
   ------------------------------------------------------ */
static uint64_t miniHash(const uint64_t* data, size_t count, uint64_t key) {
    // Example: simple FNV-like mixing
    static const uint64_t FNV_PRIME = 0x100000001B3ULL;
    uint64_t hash = 0xcbf29ce484222325ULL ^ key; // start with key

    for (size_t i = 0; i < count; i++) {
        uint64_t val = data[i];
        for (int b = 0; b < 8; b++) {
            uint8_t byteVal = static_cast<uint8_t>(val & 0xFF);
            hash ^= byteVal;
            hash *= FNV_PRIME;
            val >>= 8;
        }
    }
    return hash;
}

/* ------------------------------------------------------
   2) Fill the partialChecks and fullChecksum for a snapshot
   ------------------------------------------------------ */
static void fillChecksums(StateSnapshot& snap) {
    // Per-word partial checks
    for (int i = 0; i < 8; i++) {
        snap.partialChecks[i] = miniHash(&snap.state[i], 1, snap.ephemeralKey);
    }

    // Combine the entire state + totalLen to compute fullChecksum
    // We'll treat (state + totalLen) as 9 64-bit words
    uint64_t temp[9];
    for (int i = 0; i < 8; i++) {
        temp[i] = snap.state[i];
    }
    temp[8] = snap.totalLen;
    snap.fullChecksum = miniHash(temp, 9, snap.ephemeralKey);
}

/* ------------------------------------------------------
   3) Create a snapshot from the current QState
   ------------------------------------------------------ */
static StateSnapshot createSnapshot(const QFState& qs) {
    // Generate an ephemeral key
    // For demonstration, we use a random 64-bit number
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    uint64_t ephemeral = gen();

    StateSnapshot snap;
    std::memcpy(snap.state, qs.state, sizeof(qs.state));
    snap.totalLen = qs.absorbedBytes;
    snap.ephemeralKey = ephemeral;

    fillChecksums(snap);
    return snap;
}

/* ------------------------------------------------------
   4) Validate a snapshot matches the current QState
   ------------------------------------------------------ */
static bool validateSnapshot(const QFState& qs, const StateSnapshot& snap) {
    // Recompute partial and full checksums from qs, using the snapshot's ephemeralKey
    // Then compare with stored partialChecks / fullChecksum
    // If everything matches, we trust that QState = snapshot
    // (We skip the ephemeralKey check for forging in this toy example.)

    // Step A: Recompute partial checks for each word
    for (int i = 0; i < 8; i++) {
        uint64_t wordHash = miniHash(&qs.state[i], 1, snap.ephemeralKey);
        if (wordHash != snap.partialChecks[i]) {
            return false;
        }
    }

    // Step B: Recompute full checksum
    uint64_t temp[9];
    for (int i = 0; i < 8; i++) {
        temp[i] = qs.state[i];
    }
    temp[8] = qs.absorbedBytes;
    uint64_t fullHash = miniHash(temp, 9, snap.ephemeralKey);
    return (fullHash == snap.fullChecksum);
}

// ------------------------------------------------------
// 5) Initialize the SelfHealContext from an existing QState
// ------------------------------------------------------
void selfHealInit(SelfHealContext& ctx, const QFState& qs) {
    // Clear counters
    ctx.partialRepairs = 0;
    ctx.fullReverts = 0;
    ctx.totalReinits = 0;
    ctx.consecutiveAnomalies = 0;

    // Create an initial snapshot
    StateSnapshot initialSnap = createSnapshot(qs);

    // Put it in index = 0, copy it to shadow
    ctx.currentIndex = 0;
    ctx.snapshots[0] = initialSnap;
    ctx.shadowCopy = initialSnap;

    // For the rest of the ring, set ephemeralKey=0 or something
    for (int i = 1; i < SelfHealContext::MAX_SNAPSHOTS; i++) {
        ctx.snapshots[i].ephemeralKey = 0;
        ctx.snapshots[i].fullChecksum = 0;
        std::memset(ctx.snapshots[i].partialChecks, 0, sizeof(ctx.snapshots[i].partialChecks));
    }
}

// ------------------------------------------------------
// 6) Save a new snapshot into the ring buffer
// ------------------------------------------------------
void selfHealSaveSnapshot(SelfHealContext& ctx, const QFState& qs) {
    // Create a fresh snapshot
    StateSnapshot newSnap = createSnapshot(qs);

    // Advance ring index
    ctx.currentIndex = (ctx.currentIndex + 1) % SelfHealContext::MAX_SNAPSHOTS;
    ctx.snapshots[ctx.currentIndex] = newSnap;

    // Update shadow copy as well (the "last known good")
    ctx.shadowCopy = newSnap;
}

// ------------------------------------------------------
// 7) Detect anomalies
// ------------------------------------------------------
bool selfHealDetect(const QFState& qs, SelfHealContext& ctx) {
    // If the QState matches the shadow copy exactly, that’s definitely good
    if (validateSnapshot(qs, ctx.shadowCopy)) {
        // No anomaly
        return false;
    }

    // Additional check: totalLen not exceeding some huge boundary
    static const uint64_t MAX_LEN = 1ULL << 48; // e.g. 281TB
    if (qs.absorbedBytes > MAX_LEN) {
        std::cerr << "[SelfHealDetect] totalLen way too large.\n";
        return true;
    }

    // If the state does NOT match the shadow, check if it matches
    // any snapshot in the ring buffer
    for (int i = 0; i < SelfHealContext::MAX_SNAPSHOTS; i++) {
        if (validateSnapshot(qs, ctx.snapshots[i])) {
            // The state matches a ring snapshot => no anomaly
            return false;
        }
    }

    // If we got here => no match found => anomaly
    std::cerr << "[SelfHealDetect] State does not match shadow or ring snapshots.\n";
    return true;
}

// ------------------------------------------------------
// 8) Attempt Recovery
// ------------------------------------------------------
bool selfHealAttemptRecovery(QFState& qs, SelfHealContext& ctx) {
    ctx.consecutiveAnomalies++;

    // PART A) Attempt "partial healing":
    // Compare each 64-bit word of QState with the shadow's partial checks.
    // If only 1 or 2 words are corrupted, fix them in place.
    // Then see if it now validates with the shadow or any snapshot.

    // We'll do a naive approach: for each word i, check partial hash vs shadow's partialChecks[i].
    // If mismatch, we revert just that word from the shadow.
    int wordsFixed = 0;
    {
        // For partial healing, we re-check each word’s partial hash
        for (int i = 0; i < 8; i++) {
            uint64_t wordHash = miniHash(&qs.state[i], 1, ctx.shadowCopy.ephemeralKey);
            if (wordHash != ctx.shadowCopy.partialChecks[i]) {
                // Word i is suspect => revert from shadow
                qs.state[i] = ctx.shadowCopy.state[i];
                wordsFixed++;
            }
        }
        // After partial fixes, check if we now match shadow or ring
        if (wordsFixed > 0) {
            if (!selfHealDetect(qs, ctx)) {
                // Partial repair successful
                ctx.partialRepairs++;
                std::cerr << "[SelfHeal] Partial repair fixed " << wordsFixed << " word(s).\n";
                // Re-snapshot
                selfHealSaveSnapshot(ctx, qs);
                ctx.consecutiveAnomalies = 0;
                return true;
            }
        }
    }

    // PART B) If partial healing failed or nothing to fix, revert fully to the most recent valid snapshot
    // Start from currentIndex, go backward in ring buffer
    for (int i = 0; i < SelfHealContext::MAX_SNAPSHOTS; i++) {
        int idx = (ctx.currentIndex - i + SelfHealContext::MAX_SNAPSHOTS)
            % SelfHealContext::MAX_SNAPSHOTS;
        const StateSnapshot& snap = ctx.snapshots[idx];
        // If ephemeralKey=0, it likely was never set => skip
        if (snap.ephemeralKey == 0) {
            continue;
        }
        // Re-validate the snapshot itself (in case ring was partially corrupted)
        if (validateSnapshot(*(reinterpret_cast<const QFState*>(&snap.state)), snap)) {
            // Looks good => revert
            std::memcpy(qs.state, snap.state, sizeof(qs.state));
            qs.absorbedBytes = snap.totalLen;
            ctx.shadowCopy = snap; // update shadow
            ctx.fullReverts++;
            std::cerr << "[SelfHeal] Full revert to ring snapshot index " << idx << ".\n";
            // Re-snapshot so ring moves forward from this recovered state
            selfHealSaveSnapshot(ctx, qs);
            ctx.consecutiveAnomalies = 0;
            return true;
        }
    }

    // PART C) If we still haven’t succeeded, do a full re-init of the entire QState
    std::cerr << "[SelfHeal] No valid snapshot found or ring is corrupted. Force re-init!\n";
    qfInit(qs);
    // Overwrite everything in context
    selfHealInit(ctx, qs);
    ctx.totalReinits++;
    ctx.consecutiveAnomalies = 0;

    return false; // indicates a full re-init was necessary
}
