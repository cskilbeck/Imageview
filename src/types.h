#pragma once

//////////////////////////////////////////////////////////////////////

// remove some nonsense

constexpr nullptr_t null = nullptr;

using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;

using uint = uint32;

using wchar = wchar_t;

// share some types with the HLSL header

namespace imageview
{
    struct vec2;
}

using matrix = XMMATRIX;
using vec4 = XMVECTOR;
using float4 = vec4;
using float2 = imageview::vec2;
