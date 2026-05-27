///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   utils.h
/// @author Caden Shmookler
/// @date   2025-05-22
///
/// @brief  Common utilities for the Central-compatible object translators
///
/// leg -> Legacy Version
/// cur -> Current Version
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSHM_CENTRAL_TRANSLATORS_UTILS_H
#define CBSHM_CENTRAL_TRANSLATORS_UTILS_H

#include <cstring>

template<typename T, size_t lhs_n, size_t rhs_n>
void copyArr(T(&lhs)[lhs_n], const T(&rhs)[rhs_n]) {
    if (lhs_n <= rhs_n) {
        // Either the current array has fewer elements than the legacy array,
        // or they both have the same length. Either way, copy only as much
        // as the current array has space for.
        std::memcpy(lhs, rhs, lhs_n * sizeof(T));
    } else {
        // Current array has more entries! Copy everything from the legacy
        // array to the current array and zero out the remaining space.
        std::memcpy(lhs, rhs, rhs_n * sizeof(T));
        std::memset(lhs + rhs_n, 0, (lhs_n - rhs_n) * sizeof(T));
    }
}

template<typename LHS_T, typename RHS_T, size_t lhs_n, size_t rhs_n>
void copyArr(LHS_T(&lhs)[lhs_n], const RHS_T(&rhs)[rhs_n], LHS_T(*translation_func)(const RHS_T&)) {
    if (lhs_n <= rhs_n) {
        // Either the current array has fewer elements than the legacy array,
        // or they both have the same length. Either way, copy only as much
        // as the current array has space for.
        for (size_t i = 0; i < lhs_n; ++i) {
            lhs[i] = translation_func(rhs[i]);
        }
    } else {
        // Current array has more entries! Copy everything from the legacy
        // array to the current array and zero out the remaining space.
        for (size_t i = 0; i < rhs_n; ++i) {
            lhs[i] = translation_func(rhs[i]);
        }
        std::memset(lhs + rhs_n, 0, (lhs_n - rhs_n) * sizeof(LHS_T));
    }
}

template<typename T, size_t lhs_ny, size_t lhs_nx, size_t rhs_ny, size_t rhs_nx>
void copyArr2D(T(&lhs)[lhs_ny][lhs_nx], const T(&rhs)[rhs_ny][rhs_nx]) {
    if (lhs_ny <= rhs_ny) {
        // Either the current array has fewer elements than the legacy array,
        // or they both have the same length. Either way, copy only as much
        // as the current array has space for.
        if (lhs_nx == rhs_nx) {
            std::memcpy(lhs, rhs, (lhs_ny * lhs_nx) * sizeof(T));
        } else {
            for (size_t i = 0; i < lhs_ny; ++i) {
                copyArr(lhs[i], rhs[i]);
            }
        }
    } else {
        // Current array has more entries! Copy everything from the legacy
        // array to the current array and zero out the remaining space.
        if (lhs_nx == rhs_nx) {
            std::memcpy(lhs, rhs, (rhs_ny * rhs_nx) * sizeof(T));
        } else {
            for (size_t i = 0; i < rhs_ny; ++i) {
                copyArr(lhs[i], rhs[i]);
            }
        }
        std::memset(lhs + rhs_ny, 0, ((lhs_ny - rhs_ny) * lhs_nx) * sizeof(T));
    }
}

template<typename LHS_T, typename RHS_T, size_t lhs_ny, size_t lhs_nx, size_t rhs_ny, size_t rhs_nx>
void copyArr2D(LHS_T(&lhs)[lhs_ny][lhs_nx], const RHS_T(&rhs)[rhs_ny][rhs_nx], LHS_T(*translation_func)(const RHS_T&)) {
    if (lhs_ny <= rhs_ny) {
        // Either the current array has fewer elements than the legacy array,
        // or they both have the same length. Either way, copy only as much
        // as the current array has space for.
        for (size_t i = 0; i < lhs_ny; ++i) {
            copyArr(lhs[i], rhs[i], translation_func);
        }
    } else {
        // Current array has more entries! Copy everything from the legacy
        // array to the current array and zero out the remaining space.
        for (size_t i = 0; i < rhs_ny; ++i) {
            copyArr(lhs[i], rhs[i], translation_func);
        }
        std::memset(lhs + rhs_ny, 0, ((lhs_ny - rhs_ny) * lhs_nx) * sizeof(LHS_T));
    }
}

#endif // CBSHM_CENTRAL_TRANSLATORS_UTILS_H
