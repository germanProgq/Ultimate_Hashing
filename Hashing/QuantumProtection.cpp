#include "QuantumProtection.h"
#include <cstring>     // for std::memcpy, etc.
#include <iostream>    // optional: for debugging

// ----------------------------------------------------
// Some constants/round keys for the permutation
// We’ll use 24 rounds, reminiscent of Keccak, 
// but these are random or arbitrary for demonstration
// ----------------------------------------------------
static const uint64_t ROUND_CONSTANTS[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL,
    0x800000000000808aULL, 0x8000000080008000ULL,
    0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008aULL, 0x0000000000000088ULL,
    0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL,
    0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL,
    0x8000000080008081ULL, 0x8000000000008080ULL,
    0x0000000080000001ULL, 0x8000000080008008ULL
};

// ----------------------------------------------------
// Helper: 64-bit rotation
// ----------------------------------------------------
static inline uint64_t rotl64(uint64_t x, unsigned n) {
    return (x << n) | (x >> (64 - n));
}

// ----------------------------------------------------
// 1) qfInit
//     - Clear or set some arbitrary starting constants
// ----------------------------------------------------
void qfInit(QFState& qs) {
    std::memset(qs.state, 0, sizeof(qs.state));
    // You could set them to random or "nothing-up-our-sleeves" constants
    // e.g. for demonstration:
    qs.state[0] = 0x6A09E667F3BCC908ULL;
    qs.state[1] = 0xBB67AE8584CAA73BULL;
    qs.state[2] = 0x3C6EF372FE94F82BULL;
    qs.state[3] = 0xA54FF53A5F1D36F1ULL;
    // etc. for all 32 words if you want
    qs.absorbedBytes = 0;
}

// ----------------------------------------------------
// A big, toy "permutation" that tries to mix the full 
// 2048-bit state with 24 rounds of shifts, xors, etc.
// (Heavily inspired by SHA-3/Keccak style, but not identical.)
// ----------------------------------------------------
void qfPermutation(QFState& qs) {
    // We'll treat qs.state as 32 words. 
    // For a fancier approach, you might arrange them in a 5x5 or 8x4 matrix, etc.
    // We'll do something simpler but still large.

    for (int round = 0; round < 24; round++) {
        // 1. XOR a round constant into one word
        qs.state[round % QFState::STATE_WORDS] ^= ROUND_CONSTANTS[round];

        // 2. Sub-rounds: rotate pairs, cross-couple
        for (int i = 0; i < 32; i += 2) {
            uint64_t a = qs.state[i];
            uint64_t b = qs.state[i + 1];
            // simple mixing
            a = rotl64(a ^ b, (i + round) % 63);
            b = rotl64(b ^ a, ((i * 3) + round) % 59);
            qs.state[i] = a;
            qs.state[i + 1] = b;
        }

        // 3. More cross-lane mixing
        for (int i = 0; i < 32; i++) {
            qs.state[i] ^= rotl64(qs.state[(i + 5) % 32], ((i + round) % 7) + 1);
        }
    }
}

// ----------------------------------------------------
// 2) qfAbsorb
//     - We’ll do a sponge-like approach with rate=1024 bits (128 bytes)
//     - capacity=1024 bits
// ----------------------------------------------------
void qfAbsorb(QFState& qs, const uint8_t* data, size_t len) {
    size_t rateBytes = 128; // 1024 bits
    qs.absorbedBytes += len;

    while (len > 0) {
        size_t toXor = (len < rateBytes) ? len : rateBytes;

        // XOR the input into the first 128 bytes of the state
        // state is 32 x 64-bit => 256 bytes total
        // the "rate" portion = first 128 bytes (16 words)
        for (size_t i = 0; i < toXor; i++) {
            reinterpret_cast<uint8_t*>(qs.state)[i] ^= data[i];
        }

        data += toXor;
        len -= toXor;

        // If we consumed a full rate block, apply the permutation
        if (toXor == rateBytes) {
            qfPermutation(qs);
        }
        else {
            // If partial block, we just do a short absorption. 
            // Wait for more or finalization to call next permutation
            break;
        }
    }
}

// ----------------------------------------------------
// 3) qfSqueeze
//    - If we haven’t processed a partial block, we do so with padding
//    - Then we read out from the "rate" portion up to outLen bytes
// ----------------------------------------------------
void qfSqueeze(const QFState& qsConst, uint8_t* out, size_t outLen) {
    // We need a mutable copy because we might do a final permutation.
    // Or we can do a final "partial" permutation on a copy to preserve original state 
    // if we want a multi-use approach. For demonstration, let's copy it.
    QFState qs = qsConst;

    // If we didn't fill the last rate block, pad
    // For a toy approach: just do a simple 0x80, then zero pad
    // then permute once more.
    {
        size_t rateBytes = 128;
        // find how many bytes are in the "rate" portion that are not fully permuted
        // we can compute it from (qs.absorbedBytes % rateBytes), but we didn't store partial offset.
        // Let's do a simpler approach: re-permute unconditionally at finalize.
        // Then no partial block remains.
        // -> In a real design, you'd track partial offsets carefully.
        qfPermutation(qs);
    }

    // Now read out from the first 128 bytes in increments, permuting between each block if needed
    size_t rateBytes = 128;
    size_t offset = 0;

    while (outLen > 0) {
        size_t toCopy = (outLen < rateBytes) ? outLen : rateBytes;
        // Copy from the rate portion
        std::memcpy(out + offset, reinterpret_cast<uint8_t*>(qs.state), toCopy);

        offset += toCopy;
        outLen -= toCopy;

        if (outLen > 0) {
            // We still have more to produce => permute again
            qfPermutation(qs);
        }
    }
}
