///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   utils.h
/// @author Caden Shmookler
/// @date   2025-05-22
///
/// @brief  Common utilities for the Central-compatible object translators
///
/// lhs -> left-hand side (copy to)
/// rhs -> right-hand side (copy from)
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CBSHM_CENTRAL_TRANSLATORS_UTILS_H
#define CBSHM_CENTRAL_TRANSLATORS_UTILS_H

#include <cstring>
#include <cbshm/central_types/translators.h>

namespace cbshm {

template<typename T, size_t lhs_n, size_t rhs_n>
void copyArr(T(&lhs)[lhs_n], const T(&rhs)[rhs_n]) {
    if (lhs_n <= rhs_n) {
        std::memcpy(lhs, rhs, lhs_n * sizeof(T));
    } else {
        std::memcpy(lhs, rhs, rhs_n * sizeof(T));
        std::memset(lhs + rhs_n, 0, (lhs_n - rhs_n) * sizeof(T));
    }
}

template<typename LHS_T, size_t lhs_n, typename RHS_T, size_t rhs_n>
void copyArr(LHS_T(&lhs)[lhs_n], const RHS_T(&rhs)[rhs_n], LHS_T(*translation_func)(const RHS_T&, const TranslationContext&), const TranslationContext& ctx) {
    if (lhs_n <= rhs_n) {
        for (size_t i = 0; i < lhs_n; ++i) {
            lhs[i] = translation_func(rhs[i], ctx);
        }
    } else {
        for (size_t i = 0; i < rhs_n; ++i) {
            lhs[i] = translation_func(rhs[i], ctx);
        }
        std::memset(lhs + rhs_n, 0, (lhs_n - rhs_n) * sizeof(LHS_T));
    }
}

template<typename T, size_t lhs_ny, size_t lhs_nx, size_t rhs_ny, size_t rhs_nx>
void copyArr2D(T(&lhs)[lhs_ny][lhs_nx], const T(&rhs)[rhs_ny][rhs_nx]) {
    if (lhs_ny <= rhs_ny) {
        if (lhs_nx == rhs_nx) {
            std::memcpy(lhs, rhs, (lhs_ny * lhs_nx) * sizeof(T));
        } else {
            for (size_t i = 0; i < lhs_ny; ++i) {
                copyArr(lhs[i], rhs[i]);
            }
        }
    } else {
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

template<typename LHS_T, size_t lhs_ny, size_t lhs_nx, typename RHS_T, size_t rhs_ny, size_t rhs_nx>
void copyArr2D(LHS_T(&lhs)[lhs_ny][lhs_nx], const RHS_T(&rhs)[rhs_ny][rhs_nx], LHS_T(*translation_func)(const RHS_T&, const TranslationContext&), const TranslationContext& ctx) {
    if (lhs_ny <= rhs_ny) {
        for (size_t i = 0; i < lhs_ny; ++i) {
            copyArr(lhs[i], rhs[i], translation_func, ctx);
        }
    } else {
        for (size_t i = 0; i < rhs_ny; ++i) {
            copyArr(lhs[i], rhs[i], translation_func, ctx);
        }
        std::memset(lhs + rhs_ny, 0, ((lhs_ny - rhs_ny) * lhs_nx) * sizeof(LHS_T));
    }
}

} // namespace cbshm

#endif // CBSHM_CENTRAL_TRANSLATORS_UTILS_H
