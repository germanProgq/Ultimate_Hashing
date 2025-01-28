#include "UniversalData.h"
#include "QuantumProtection.h"
#include <cstring>      // for std::memcpy
#include <iostream>     // for I/O, logging
#include <fstream>
#include <algorithm>    // for std::min

// Uncomment the following line to enable debug prints
// #define UNIVERSALDATA_DEBUG

#ifdef UNIVERSALDATA_DEBUG
#define UDATA_LOG(msg) std::cerr << "[UniversalData] " << msg << "\n"
#else
#define UDATA_LOG(msg) /* no-op */
#endif

// --------------------------------------------------------------------
// Optional endianness conversion function
// If you want consistent hashing across platforms, 
// you can convert all multi-byte data to little-endian or big-endian
// before absorption.
// --------------------------------------------------------------------
static inline uint64_t toLittleEndian64(uint64_t x) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    // Convert from big-endian to little-endian
    x = ((x & 0x00000000000000FFULL) << 56) |
        ((x & 0x000000000000FF00ULL) << 40) |
        ((x & 0x0000000000FF0000ULL) << 24) |
        ((x & 0x00000000FF000000ULL) << 8) |
        ((x & 0x000000FF00000000ULL) >> 8) |
        ((x & 0x0000FF0000000000ULL) >> 24) |
        ((x & 0x00FF000000000000ULL) >> 40) |
        ((x & 0xFF00000000000000ULL) >> 56);
#endif
    return x;
}

// --------------------------------------------------------------------
// If you want to do endianness transform for an arbitrary buffer,
// you'd do something like this (we keep it optional):
// --------------------------------------------------------------------
static void ensureLittleEndianBuffer(const void* srcData, size_t lengthBytes, std::vector<uint8_t>& out) {
    out.resize(lengthBytes);
    std::memcpy(out.data(), srcData, lengthBytes);

    // Example: if we consider the input as an array of 64-bit words,
    // convert each to little-endian.
    // This only makes sense if lengthBytes is a multiple of 8,
    // or if you want partial word handling.
    // We'll keep it simple: only handle multiples of 8.
    if ((lengthBytes % 8) == 0) {
        size_t wordCount = lengthBytes / 8;
        uint64_t* words = reinterpret_cast<uint64_t*>(out.data());
        for (size_t i = 0; i < wordCount; i++) {
            words[i] = toLittleEndian64(words[i]);
        }
    }
}

// --------------------------------------------------------------------
// The core function that actually calls qfAbsorb
// with optional endianness transform
// --------------------------------------------------------------------
void processRaw(QFState& qs, const void* data, size_t length) {
    UDATA_LOG("processRaw: absorbing " << length << " bytes.");

    // If you want to *skip* endianness conversion, just do:
    // qfAbsorb(qs, static_cast<const uint8_t*>(data), length);
    // Otherwise, do:

    std::vector<uint8_t> buffer;
    ensureLittleEndianBuffer(data, length, buffer);

    // Now feed 'buffer' to qfAbsorb
    qfAbsorb(qs, buffer.data(), buffer.size());
}

// --------------------------------------------------------------------
// processString
//   - Optionally absorb the length of the string first,
//     then the string bytes, ensuring we won't have collisions
//     between "abc" and "abc\0def" or other ambiguities.
// --------------------------------------------------------------------
void processString(QFState& qs, const std::string& str) {
    UDATA_LOG("processString: string length = " << str.size());

    // (Optional) absorb a 64-bit length field
    uint64_t strLen = static_cast<uint64_t>(str.size());
    processRaw(qs, &strLen, sizeof(strLen));

    // absorb the string characters
    processRaw(qs, str.data(), str.size());
}

// --------------------------------------------------------------------
// processBytes
//   - a simple vector of bytes
// --------------------------------------------------------------------
void processBytes(QFState& qs, const std::vector<uint8_t>& data) {
    UDATA_LOG("processBytes: vector.size = " << data.size());
    // If you want, you can absorb the length first:
    uint64_t vsize = static_cast<uint64_t>(data.size());
    processRaw(qs, &vsize, sizeof(vsize));

    // Then absorb the actual bytes
    processRaw(qs, data.data(), data.size());
}

// --------------------------------------------------------------------
// processFile
//   - Reads the file in chunks, calls qfAbsorb for each chunk
//   - Returns false if file can't be opened, true otherwise
// --------------------------------------------------------------------
bool processFile(QFState& qs, const std::string& filename, size_t chunkSize) {
    UDATA_LOG("processFile: reading " << filename << " in chunks of " << chunkSize << " bytes.");

    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "[processFile] Failed to open file: " << filename << "\n";
        return false;
    }

    // (Optional) incorporate filename or file size if you want
    // to differentiate files that happen to have the same content.
    // For example:
    // processString(qs, filename);

    // read in chunked manner
    std::vector<uint8_t> buffer(chunkSize);
    while (true) {
        file.read(reinterpret_cast<char*>(buffer.data()), chunkSize);
        std::streamsize bytesRead = file.gcount();
        if (bytesRead <= 0) {
            break; // done or error
        }

        // Optionally do endianness transform here, if desired
        // For large files, we might skip it for performance. 
        // We'll just call processRaw:
        processRaw(qs, buffer.data(), static_cast<size_t>(bytesRead));

        if (!file) {
            // might be EOF or some error
            if (file.eof()) break;
            if (file.fail() && !file.eof()) {
                std::cerr << "[processFile] Reading error before EOF.\n";
                break;
            }
        }
    }
    file.close();
    return true;
}

// --------------------------------------------------------------------
// The templated functions (processContainer, processArray, processStruct)
// remain in the header, but if you want to provide explicit instantiations,
// you can do them here for specific types.
//
// e.g. 
// template void processContainer<std::vector<int>>(QFState&, const std::vector<int>&);
// ...
//
// Usually, though, we keep them inline in the header.
// --------------------------------------------------------------------
