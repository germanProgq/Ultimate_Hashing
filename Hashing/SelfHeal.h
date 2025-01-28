#ifndef SELF_HEAL_H
#define SELF_HEAL_H

#include <cstdint>
#include <cstddef>
#include "QuantumProtection.h"

// --------------------------------------------------
//  A snapshot of the QFState, plus extras
//  (for a 2048-bit state = 32 x 64-bit words)
// --------------------------------------------------
struct StateSnapshot {
    static const int SNAP_WORDS = 32;

    // Full copy of the 2048-bit state
    uint64_t state[SNAP_WORDS];

    // If you track total length in QFState
    uint64_t totalLen;

    // A partial checksum for each 64-bit word (32 entries)
    uint64_t partialChecks[SNAP_WORDS];

    // A full checksum of the entire snapshot
    uint64_t fullChecksum;

    // Optional ephemeral "key" for verifying authenticity
    uint64_t ephemeralKey;
};

// --------------------------------------------------
//  Our "ultimate" self-healing context
// --------------------------------------------------
struct SelfHealContext {
    static const int MAX_SNAPSHOTS = 5;

    // A ring buffer of snapshots
    StateSnapshot snapshots[MAX_SNAPSHOTS];
    int currentIndex;         // which snapshot is "most recent"

    // A "shadow copy" storing the last verified good state
    // for faster revert or partial repair
    StateSnapshot shadowCopy;

    // Counters for how many times partial or full repairs occurred
    int partialRepairs;
    int fullReverts;
    int totalReinits;

    // Track repeated anomaly detection in short succession
    int consecutiveAnomalies;
};

// --------------------------------------------------
//  PUBLIC FUNCTION DECLARATIONS
// --------------------------------------------------

// Initialize the SelfHealContext with an initial known-good (2048-bit) QFState
void selfHealInit(SelfHealContext& ctx, const QFState& qs);

// Save a new snapshot (periodically or after big updates).
// Also updates the shadow copy.
void selfHealSaveSnapshot(SelfHealContext& ctx, const QFState& qs);

// Check whether the given QFState (2048-bit) is anomalous
// (e.g., corrupted memory).
bool selfHealDetect(const QFState& qs, SelfHealContext& ctx);

// Attempt partial or full healing. 
// Returns `true` if recovery was successful,
// or `false` if we had to do a full re-init.
bool selfHealAttemptRecovery(QFState& qs, SelfHealContext& ctx);

#endif // SELF_HEAL_H