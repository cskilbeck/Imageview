//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////

namespace imageview
{
    vec2 vec2::rotate(vec2 const &size, vec2 const &point, rotation_angle_t rotation)
    {
        vec2 t = sub_point(size, { 1, 1 });

        switch(rotation) {

        default:
            return point;

        case rotation_angle_t::rotate_90:
            return { point.y, t.x - point.x };

        case rotation_angle_t::rotate_180:
            return { t.x - point.x, t.y - point.y };

        case rotation_angle_t::rotate_270:
            return { t.x - point.y, t.y - point.x };
        }
    }
}
