//////////////////////////////////////////////////////////////////////

#pragma once

namespace imageview
{
    //////////////////////////////////////////////////////////////////////

    enum rotation_angle_t : uint;

    //////////////////////////////////////////////////////////////////////

    struct vec2
    {
        float x;
        float y;

        vec2() = default;

        vec2(float x, float y) : x(x), y(y)
        {
        }

        explicit vec2(POINT p) : x((float)p.x), y((float)p.y)
        {
        }

        explicit vec2(POINTS p) : x((float)p.x), y((float)p.y)
        {
        }

        static vec2 min(vec2 const &a, vec2 const &b)
        {
            return { std::min(a.x, b.x), std::min(a.y, b.y) };
        }

        static vec2 max(vec2 const &a, vec2 const &b)
        {
            return { std::max(a.x, b.x), std::max(a.y, b.y) };
        }

        static vec2 mod(vec2 const &a, vec2 const &b)
        {
            return { fmodf(a.x, b.x), fmodf(a.y, b.y) };
        }

        static vec2 floor(vec2 const &a)
        {
            return { floorf(a.x), floorf(a.y) };
        }

        static vec2 ceil(vec2 const &a)
        {
            return { ceilf(a.x), ceilf(a.y) };
        }

        static vec2 round(vec2 const &a)
        {
            return { roundf(a.x), roundf(a.y) };
        }

        static vec2 clamp(vec2 const &min, vec2 const &a, vec2 const &max)
        {
            return { std::clamp(a.x, min.x, max.x), std::clamp(a.y, min.y, max.y) };
        }

        static float length(vec2 const &a)
        {
            return sqrtf(a.x * a.x + a.y * a.y);
        }

        static vec2 rotate(vec2 const &size, vec2 const &point, rotation_angle_t rotation);
    };

    //////////////////////////////////////////////////////////////////////

    struct rect_f
    {
        float x;
        float y;
        float w;
        float h;

        rect_f() = default;

        rect_f(float _x, float _y, float _w, float _h) : x(_x), y(_y), w(_w), h(_h)
        {
        }

        rect_f(vec2 corner1, vec2 corner2)
        {
            vec2 min = vec2::min(corner1, corner2);
            vec2 max = vec2::max(corner1, corner2);
            vec2 diff = sub_point(max, min);
            x = min.x;
            y = min.y;
            w = diff.x;
            h = diff.y;
        }

        vec2 top_left() const
        {
            return { x, y };
        }

        vec2 bottom_right() const
        {
            return { x + w, y + h };
        }

        vec2 size() const
        {
            return { w, h };
        }

        bool contains(vec2 p) const
        {
            vec2 br = bottom_right();
            return p.x >= x && p.y >= y && p.x <= br.x && p.y <= br.y;
        }

        rect_f rotate90() const
        {
            float hw = w / 2;
            float hh = h / 2;
            float mx = x + hw;
            float my = y + hh;
            return rect_f{ mx - hh, my - hw, h, w };
        }
    };
}