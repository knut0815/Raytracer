#pragma once

#include "Vector4.h"

namespace rt {
namespace math {


const VectorInt4 VectorInt4::Zero()
{
    return _mm_setzero_si128();
}

VectorInt4::VectorInt4(const __m128i& m)
    : v(m)
{}

VectorInt4::VectorInt4(const VectorInt4& other)
    : v(other.v)
{}

VectorInt4::VectorInt4(const VectorBool4& other)
    : v(_mm_castps_si128(other.v))
{}

const VectorInt4 VectorInt4::Cast(const Vector4& v)
{
    return _mm_castps_si128(v);
}

const Vector4 VectorInt4::CastToFloat() const
{
    return _mm_castsi128_ps(v);
}

VectorInt4::VectorInt4(const int32 x, const int32 y, const int32 z, const int32 w)
    : v(_mm_set_epi32(w, z, y, x))
{}

VectorInt4::VectorInt4(const int32 i)
    : v(_mm_set1_epi32(i))
{}

VectorInt4::VectorInt4(const uint32 u)
    : v(_mm_set1_epi32(u))
{}

const VectorInt4 VectorInt4::Convert(const Vector4& v)
{
    return _mm_cvtps_epi32(v);
}

const VectorInt4 VectorInt4::TruncateAndConvert(const Vector4& v)
{
    return _mm_cvttps_epi32(v);
}

const Vector4 VectorInt4::ConvertToFloat() const
{
    return _mm_cvtepi32_ps(v);
}

//////////////////////////////////////////////////////////////////////////

const VectorInt4 VectorInt4::Select(const VectorInt4& a, const VectorInt4& b, const VectorBool4& sel)
{
    return _mm_blendv_epi8(a, b, _mm_castps_si128(sel.v));
}

template<uint32 ix, uint32 iy, uint32 iz, uint32 iw>
const VectorInt4 VectorInt4::Swizzle() const
{
    static_assert(ix < 4, "Invalid X element index");
    static_assert(iy < 4, "Invalid Y element index");
    static_assert(iz < 4, "Invalid Z element index");
    static_assert(iw < 4, "Invalid W element index");

    if (ix == 0 && iy == 1 && iz == 2 && iw == 3)
    {
        return *this;
    }
    else if (ix == 0 && iy == 0 && iz == 1 && iw == 1)
    {
        return _mm_unpacklo_epi32(v, v);
    }
    else if (ix == 2 && iy == 2 && iz == 3 && iw == 3)
    {
        return _mm_unpackhi_epi32(v, v);
    }
    else if (ix == 0 && iy == 1 && iz == 0 && iw == 1)
    {
        return _mm_unpacklo_epi64(v, v);
    }
    else if (ix == 2 && iy == 3 && iz == 2 && iw == 3)
    {
        return _mm_unpackhi_epi64(v, v);
    }

    return _mm_shuffle_epi32(v, _MM_SHUFFLE(iw, iz, iy, ix));
}

//////////////////////////////////////////////////////////////////////////

const VectorInt4 VectorInt4::operator & (const VectorInt4& b) const
{
    return _mm_and_si128(v, b.v);
}

const VectorInt4 VectorInt4::AndNot(const VectorInt4& a, const VectorInt4& b)
{
    return _mm_andnot_si128(a.v, b.v);
}

const VectorInt4 VectorInt4::operator | (const VectorInt4& b) const
{
    return _mm_or_si128(v, b.v);
}

const VectorInt4 VectorInt4::operator ^ (const VectorInt4& b) const
{
    return _mm_xor_si128(v, b.v);
}

VectorInt4& VectorInt4::operator &= (const VectorInt4& b)
{
    v = _mm_and_si128(v, b.v);
    return *this;
}

VectorInt4& VectorInt4::operator |= (const VectorInt4& b)
{
    v = _mm_or_si128(v, b.v);
    return *this;
}

VectorInt4& VectorInt4::operator ^= (const VectorInt4& b)
{
    v = _mm_xor_si128(v, b.v);
    return *this;
}

//////////////////////////////////////////////////////////////////////////

const VectorInt4 VectorInt4::operator - () const
{
    return VectorInt4::Zero() - (*this);
}

const VectorInt4 VectorInt4::operator + (const VectorInt4& b) const
{
    return _mm_add_epi32(v, b);
}

const VectorInt4 VectorInt4::operator - (const VectorInt4& b) const
{
    return _mm_sub_epi32(v, b);
}

const VectorInt4 VectorInt4::operator * (const VectorInt4& b) const
{
    return _mm_mullo_epi32(v, b);
}

VectorInt4& VectorInt4::operator += (const VectorInt4& b)
{
    v = _mm_add_epi32(v, b);
    return *this;
}

VectorInt4& VectorInt4::operator -= (const VectorInt4& b)
{
    v = _mm_sub_epi32(v, b);
    return *this;
}

VectorInt4& VectorInt4::operator *= (const VectorInt4& b)
{
    v = _mm_mullo_epi32(v, b);
    return *this;
}

const VectorInt4 VectorInt4::operator + (int32 b) const
{
    return _mm_add_epi32(v, _mm_set1_epi32(b));
}

const VectorInt4 VectorInt4::operator - (int32 b) const
{
    return _mm_sub_epi32(v, _mm_set1_epi32(b));
}

const VectorInt4 VectorInt4::operator * (int32 b) const
{
    return _mm_mullo_epi32(v, _mm_set1_epi32(b));
}

VectorInt4& VectorInt4::operator += (int32 b)
{
    v = _mm_add_epi32(v, _mm_set1_epi32(b));
    return *this;
}

VectorInt4& VectorInt4::operator -= (int32 b)
{
    v = _mm_sub_epi32(v, _mm_set1_epi32(b));
    return *this;
}

VectorInt4& VectorInt4::operator *= (int32 b)
{
    v = _mm_mullo_epi32(v, _mm_set1_epi32(b));
    return *this;
}

//////////////////////////////////////////////////////////////////////////

const VectorInt4 VectorInt4::operator << (const VectorInt4& b) const
{
#ifdef RT_USE_AVX2
    return _mm_sllv_epi32(v, b);
#else
    return { x << b.x, y << b.y, z << b.z, w << b.w };
#endif
}

const VectorInt4 VectorInt4::operator >> (const VectorInt4& b) const
{
#ifdef RT_USE_AVX2
    return _mm_srlv_epi32(v, b);
#else
    return { x >> b.x, y >> b.y, z >> b.z, w >> b.w };
#endif
}

VectorInt4& VectorInt4::operator <<= (const VectorInt4& b)
{
#ifdef RT_USE_AVX2
    v = _mm_sllv_epi32(v, b);
#else
    x <<= b.x;
    y <<= b.y;
    z <<= b.z;
    w <<= b.w;
#endif
    return *this;
}

VectorInt4& VectorInt4::operator >>= (const VectorInt4& b)
{
#ifdef RT_USE_AVX2
    v = _mm_srlv_epi32(v, b);
#else
    x >>= b.x;
    y >>= b.y;
    z >>= b.z;
    w >>= b.w;
#endif
    return *this;
}

const VectorInt4 VectorInt4::operator << (int32 b) const
{
    return _mm_slli_epi32(v, b);
}

const VectorInt4 VectorInt4::operator >> (int32 b) const
{
    return _mm_srli_epi32(v, b);
}

VectorInt4& VectorInt4::operator <<= (int32 b)
{
    v = _mm_slli_epi32(v, b);
    return *this;
}

VectorInt4& VectorInt4::operator >>= (int32 b)
{
    v = _mm_srli_epi32(v, b);
    return *this;
}

//////////////////////////////////////////////////////////////////////////

const VectorBool4 VectorInt4::operator == (const VectorInt4& b) const
{
    return _mm_castsi128_ps(_mm_cmpeq_epi32(v, b.v));
}

const VectorBool4 VectorInt4::operator != (const VectorInt4& b) const
{
    return _mm_castsi128_ps(_mm_xor_si128(_mm_set1_epi32(0xFFFFFFFF), _mm_cmpeq_epi32(v, b.v)));
}

const VectorBool4 VectorInt4::operator < (const VectorInt4& b) const
{
    return _mm_castsi128_ps(_mm_cmplt_epi32(v, b.v));
}

const VectorBool4 VectorInt4::operator > (const VectorInt4& b) const
{
    return _mm_castsi128_ps(_mm_cmpgt_epi32(v, b.v));
}

const VectorBool4 VectorInt4::operator >= (const VectorInt4& b) const
{
    return _mm_castsi128_ps(_mm_xor_si128(_mm_set1_epi32(0xFFFFFFFF), _mm_cmplt_epi32(v, b.v)));
}

const VectorBool4 VectorInt4::operator <= (const VectorInt4& b) const
{
    return _mm_castsi128_ps(_mm_xor_si128(_mm_set1_epi32(0xFFFFFFFF), _mm_cmpgt_epi32(v, b.v)));
}

//////////////////////////////////////////////////////////////////////////

const VectorInt4 VectorInt4::Min(const VectorInt4& a, const VectorInt4& b)
{
    return _mm_min_epi32(a, b);
}

const VectorInt4 VectorInt4::Max(const VectorInt4& a, const VectorInt4& b)
{
    return _mm_max_epi32(a, b);
}


} // namespace math
} // namespace rt
