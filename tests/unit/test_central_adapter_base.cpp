///////////////////////////////////////////////////////////////////////////////////////////////////
/// @file   test_central_adapter_base.cpp
/// @author CereLink Development Team
///
/// @brief  Unit tests for the version-agnostic array helpers in CentralAdapterBase
///         (copyArr / copyArr2D).
///
/// These helpers are the core "layout adaptation" primitive: whenever a Central
/// struct and its native equivalent have arrays of different dimensions, the
/// adapters copy the overlapping region and zero-fill the remainder. Getting the
/// truncation / zero-fill semantics wrong is exactly how a version bump silently
/// corrupts config data, so they are tested here in isolation from any concrete
/// version adapter.
///
/// copyArr / copyArr2D are `protected static` members of CentralAdapterBase. We
/// reach them through a derived helper that re-exports them with public access
/// via using-declarations. The helper is never instantiated (it stays abstract),
/// so the pure-virtual interface does not need to be implemented.
///
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <cbshm/central_adapters/base.h>
#include <cstdint>
#include <cstring>

using namespace cbshm;

namespace {

/// Re-exports the protected static array helpers with public access.
/// Abstract (pure virtuals left unimplemented) and never instantiated — we only
/// call its static members.
struct CopyArrAccess : public CentralAdapterBase {
    using CentralAdapterBase::copyArr;
    using CentralAdapterBase::copyArr2D;
};

/// A translator used to exercise the per-element translation-function overloads.
/// Doubles the source value into a wider destination type.
struct Doubler {
    void translate(int32_t& dst, const int16_t& src) const {
        dst = static_cast<int32_t>(src) * 2;
    }
};

} // namespace

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name copyArr — 1-D, same element type (plain memcpy path)
/// @{

TEST(CopyArr1D, EqualSize_CopiesAll) {
    int src[4] = {10, 20, 30, 40};
    int dst[4] = {0, 0, 0, 0};

    CopyArrAccess::copyArr(dst, src);

    for (int i = 0; i < 4; ++i) EXPECT_EQ(dst[i], src[i]) << "index " << i;
}

TEST(CopyArr1D, DestSmaller_Truncates) {
    // lhs_n (3) < rhs_n (5): only the first 3 source elements are copied.
    int src[5] = {1, 2, 3, 4, 5};
    int dst[3] = {-1, -1, -1};

    CopyArrAccess::copyArr(dst, src);

    EXPECT_EQ(dst[0], 1);
    EXPECT_EQ(dst[1], 2);
    EXPECT_EQ(dst[2], 3);
    // No overflow past dst[2] — verified implicitly by the fixed-size array.
}

TEST(CopyArr1D, DestLarger_CopiesThenZeroFills) {
    // lhs_n (5) > rhs_n (2): copy the 2 source elements, zero-fill the tail.
    int src[2] = {7, 8};
    int dst[5] = {99, 99, 99, 99, 99};

    CopyArrAccess::copyArr(dst, src);

    EXPECT_EQ(dst[0], 7);
    EXPECT_EQ(dst[1], 8);
    EXPECT_EQ(dst[2], 0) << "tail must be zero-filled";
    EXPECT_EQ(dst[3], 0);
    EXPECT_EQ(dst[4], 0);
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name copyArr — 1-D, per-element translation-function overload
/// @{

TEST(CopyArr1DTranslate, EqualSize_TranslatesEach) {
    Doubler d;
    int16_t src[3] = {1, 2, 3};
    int32_t dst[3] = {0, 0, 0};

    CopyArrAccess::copyArr(dst, src, &d, &Doubler::translate);

    EXPECT_EQ(dst[0], 2);
    EXPECT_EQ(dst[1], 4);
    EXPECT_EQ(dst[2], 6);
}

TEST(CopyArr1DTranslate, DestSmaller_TranslatesTruncated) {
    Doubler d;
    int16_t src[4] = {5, 6, 7, 8};
    int32_t dst[2] = {0, 0};

    CopyArrAccess::copyArr(dst, src, &d, &Doubler::translate);

    EXPECT_EQ(dst[0], 10);
    EXPECT_EQ(dst[1], 12);
}

TEST(CopyArr1DTranslate, DestLarger_TranslatesThenZeroFills) {
    Doubler d;
    int16_t src[2] = {3, 4};
    int32_t dst[4] = {99, 99, 99, 99};

    CopyArrAccess::copyArr(dst, src, &d, &Doubler::translate);

    EXPECT_EQ(dst[0], 6);
    EXPECT_EQ(dst[1], 8);
    EXPECT_EQ(dst[2], 0) << "tail must be zero-filled (raw memset)";
    EXPECT_EQ(dst[3], 0);
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name copyArr2D — matching inner dimension (fast memcpy path)
/// @{

TEST(CopyArr2D, EqualDims_CopiesAll) {
    int src[2][3] = {{1, 2, 3}, {4, 5, 6}};
    int dst[2][3] = {{0, 0, 0}, {0, 0, 0}};

    CopyArrAccess::copyArr2D(dst, src);

    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 3; ++x)
            EXPECT_EQ(dst[y][x], src[y][x]) << "(" << y << "," << x << ")";
}

TEST(CopyArr2D, FewerDestRows_SameInner_Truncates) {
    // lhs_ny (1) < rhs_ny (3), matching inner dim: memcpy of lhs_ny*lhs_nx.
    int src[3][2] = {{1, 2}, {3, 4}, {5, 6}};
    int dst[1][2] = {{-1, -1}};

    CopyArrAccess::copyArr2D(dst, src);

    EXPECT_EQ(dst[0][0], 1);
    EXPECT_EQ(dst[0][1], 2);
}

TEST(CopyArr2D, MoreDestRows_SameInner_ZeroFillsExtraRows) {
    // lhs_ny (3) > rhs_ny (1), matching inner dim: copy row 0, zero-fill rows 1..2.
    int src[1][2] = {{9, 8}};
    int dst[3][2] = {{5, 5}, {5, 5}, {5, 5}};

    CopyArrAccess::copyArr2D(dst, src);

    EXPECT_EQ(dst[0][0], 9);
    EXPECT_EQ(dst[0][1], 8);
    for (int x = 0; x < 2; ++x) {
        EXPECT_EQ(dst[1][x], 0) << "extra row 1 must be zero-filled";
        EXPECT_EQ(dst[2][x], 0) << "extra row 2 must be zero-filled";
    }
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name copyArr2D — mismatched inner dimension (per-row fallback path)
/// @{

TEST(CopyArr2D, MismatchedInner_DestInnerSmaller_TruncatesEachRow) {
    int src[2][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}};
    int dst[2][2] = {{0, 0}, {0, 0}};

    CopyArrAccess::copyArr2D(dst, src);

    EXPECT_EQ(dst[0][0], 1);
    EXPECT_EQ(dst[0][1], 2);
    EXPECT_EQ(dst[1][0], 5);
    EXPECT_EQ(dst[1][1], 6);
}

TEST(CopyArr2D, MismatchedInner_DestInnerLarger_ZeroFillsEachRowTail) {
    int src[2][2] = {{1, 2}, {3, 4}};
    int dst[2][4] = {{9, 9, 9, 9}, {9, 9, 9, 9}};

    CopyArrAccess::copyArr2D(dst, src);

    EXPECT_EQ(dst[0][0], 1);
    EXPECT_EQ(dst[0][1], 2);
    EXPECT_EQ(dst[0][2], 0) << "per-row tail must be zero-filled";
    EXPECT_EQ(dst[0][3], 0);
    EXPECT_EQ(dst[1][0], 3);
    EXPECT_EQ(dst[1][1], 4);
    EXPECT_EQ(dst[1][2], 0);
    EXPECT_EQ(dst[1][3], 0);
}

TEST(CopyArr2D, MismatchedInner_MoreDestRows_ZeroFillsExtraRows) {
    // lhs_ny (3) > rhs_ny (1) AND inner dims differ: per-row copy of row 0,
    // then the extra rows are zero-filled.
    int src[1][2] = {{1, 2}};
    int dst[3][3] = {{7, 7, 7}, {7, 7, 7}, {7, 7, 7}};

    CopyArrAccess::copyArr2D(dst, src);

    EXPECT_EQ(dst[0][0], 1);
    EXPECT_EQ(dst[0][1], 2);
    EXPECT_EQ(dst[0][2], 0);
    for (int y = 1; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            EXPECT_EQ(dst[y][x], 0) << "extra row " << y << " must be zero-filled";
}

/// @}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @name copyArr2D — per-element translation-function overload
/// @{

TEST(CopyArr2DTranslate, TranslatesEachElement) {
    Doubler d;
    int16_t src[2][2] = {{1, 2}, {3, 4}};
    int32_t dst[2][2] = {{0, 0}, {0, 0}};

    CopyArrAccess::copyArr2D(dst, src, &d, &Doubler::translate);

    EXPECT_EQ(dst[0][0], 2);
    EXPECT_EQ(dst[0][1], 4);
    EXPECT_EQ(dst[1][0], 6);
    EXPECT_EQ(dst[1][1], 8);
}

TEST(CopyArr2DTranslate, MoreDestRows_TranslatesThenZeroFills) {
    Doubler d;
    int16_t src[1][2] = {{5, 6}};
    int32_t dst[3][2] = {{9, 9}, {9, 9}, {9, 9}};

    CopyArrAccess::copyArr2D(dst, src, &d, &Doubler::translate);

    EXPECT_EQ(dst[0][0], 10);
    EXPECT_EQ(dst[0][1], 12);
    for (int y = 1; y < 3; ++y)
        for (int x = 0; x < 2; ++x)
            EXPECT_EQ(dst[y][x], 0) << "extra row " << y << " must be zero-filled";
}

/// @}
