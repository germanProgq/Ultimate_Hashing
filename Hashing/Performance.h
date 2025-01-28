#ifndef PERFORMANCE_H
#define PERFORMANCE_H

#include "QuantumProtection.h"

// -----------------------------------------------------------------------------
//  speedOptimize(QFState &qs):
//    An optional function that attempts to optimize or re-mix the QFState
//    using vector instructions (SSE/AVX) or other techniques (unrolling, etc.).
// -----------------------------------------------------------------------------
void speedOptimize(QFState& qs);

#endif // PERFORMANCE_H
