#ifndef UNIVERSAL_DATA_H
#define UNIVERSAL_DATA_H

#include <string>
#include <vector>
#include <fstream>
#include <type_traits>
#include "QuantumProtection.h"

// ------------------------------------------------------------------
// 1) Basic “processString” + “processBytes” still included
// ------------------------------------------------------------------
void processString(QFState& qs, const std::string& str);
void processBytes(QFState& qs, const std::vector<uint8_t>& data);

// ------------------------------------------------------------------
// 2) Generic processing of raw memory
//    e.g. if you have a pointer + length
// ------------------------------------------------------------------
void processRaw(QFState& qs, const void* data, size_t length);

// ------------------------------------------------------------------
// 3) Template for processing a container of basic data types
//    (like std::vector<int>, std::array<double, 10>, etc.)
//    - We assume T is trivially copyable or “plain old data” (POD),
//      so we can safely reinterpret_cast and pass to qfAbsorb.
//
//    - If you want to handle non-trivial objects, you must either
//      define a serialization method or handle them differently.
// ------------------------------------------------------------------
template <class Container>
void processContainer(QFState& qs, const Container& c)
{
    // Ensure the value_type is a trivially copyable type
    static_assert(std::is_trivially_copyable<typename Container::value_type>::value,
        "Container must hold trivially copyable types.");

    // If c.size() is the number of elements, total bytes is:
    size_t totalBytes = c.size() * sizeof(typename Container::value_type);
    // Pass pointer + byte size to the raw processor
    processRaw(qs, c.data(), totalBytes);
}

// ------------------------------------------------------------------
// 4) Process an array of typed data
//    Same logic, but for a raw C-array or std::array
// ------------------------------------------------------------------
template <typename T, size_t N>
void processArray(QFState& qs, const T(&arr)[N])
{
    static_assert(std::is_trivially_copyable<T>::value,
        "Array must contain trivially copyable type.");
    processRaw(qs, arr, sizeof(T) * N);
}

// ------------------------------------------------------------------
// 5) Process user-defined structs or classes
//    - If the struct is trivially copyable, we can directly feed
//      its bytes into the absorber.
//    - Otherwise, you may need a custom “serialize” function.
// ------------------------------------------------------------------
template <typename T>
void processStruct(QFState& qs, const T& obj)
{
    static_assert(std::is_trivially_copyable<T>::value,
        "processStruct requires a trivially copyable type.");

    processRaw(qs, &obj, sizeof(T));
}

// ------------------------------------------------------------------
// 6) Process file data
//    - Reads file in chunks and feeds into the QFState
//    - Good for very large files or streaming
// ------------------------------------------------------------------
bool processFile(QFState& qs, const std::string& filename, size_t chunkSize = 4096);

// ------------------------------------------------------------------
// 7) (Optional) Overloads / specializations for specific data types
//    e.g. processInts, processDoubles, etc. – if you want 
//    specialized logic (endianness, etc.) – or rely on
//    processContainer() for typical usage.
// ------------------------------------------------------------------
//   void processInts(QFState &qs, const std::vector<int> &ints);
//   etc.

// ------------------------------------------------------------------
// 8) Additional domain-specific or advanced overloads could go here.
// ------------------------------------------------------------------

#endif // UNIVERSAL_DATA_H
