//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////

namespace imageview
{
    vec2 vec2::rotate(vec2 const &size, vec2 const &point, rotation_angle_t rotation)
    {
        switch(rotation) {

        default:
            return point;

        case rotation_angle_t::rotate_90:
            return { point.y, size.x - point.x };

        case rotation_angle_t::rotate_180:
            return { size.x - point.x, size.y - point.y };

        case rotation_angle_t::rotate_270:
            return { size.x - point.y, size.y - point.x };
        }
    }
}
