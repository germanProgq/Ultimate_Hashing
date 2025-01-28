#ifndef QUANTIM_PROTECTION_H
#define QUANTIM_PROTECTION_H

#include <cstdint>
#include <cstddef>

// --------------------------------------------------------------------
// A 2048-bit internal state for “QuantumFortress” 
// (32 x 64-bit = 2048 bits).
// --------------------------------------------------------------------
struct QFState {
    static const int STATE_WORDS = 32; 
    uint64_t state[STATE_WORDS]; 
    uint64_t absorbedBytes; // track how many bytes we've absorbed
    // Possibly track other parameters if you want
};

// --------------------------------------------------------------------
// API
// --------------------------------------------------------------------

// Initialize the large quantum-fortress state
void qfInit(QFState &qs);

// Absorb data (in a sponge-like manner)
void qfAbsorb(QFState &qs, const uint8_t *data, size_t len);

// Finalize and produce a 512-bit (or bigger) digest
// For demonstration, we’ll produce 512 bits (64 bytes)
void qfSqueeze(const QFState &qs, uint8_t *out, size_t outLen);

// Optionally, a “permutation only” function if you want direct access
void qfPermutation(QFState &qs);

#endif // QUANTIM_PROTECTION_H
