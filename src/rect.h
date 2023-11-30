//////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

struct rect : RECT
{
    rect() = default;

    rect(RECT const &r) : RECT(r)
    {
    }

    rect(long l, long t, long r, long b) : RECT{ l, t, r, b }
    {
    }

    inline static rect &as(RECT &r)
    {
        return (rect &)r;
    }

    long w() const
    {
        return right - left;
    }

    long h() const
    {
        return bottom - top;
    }

    long x() const
    {
        return left;
    }

    long y() const
    {
        return top;
    }

    long r() const
    {
        return right;
    }

    long b() const
    {
        return bottom;
    }

    POINT top_left() const
    {
        return { left, top };
    }

    std::string to_string() const
    {
        return std::format("{},{} ({}x{})", x(), y(), w(), h());
    }
};

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

    static vec2 round(vec2 const &a)
    {
        return { roundf(a.x), roundf(a.y) };
    }

    static vec2 clamp(vec2 const &min, vec2 const &a, vec2 const &max)
    {
        return { ::clamp(min.x, a.x, max.x), ::clamp(min.y, a.y, max.y) };
    }

    static float length(vec2 const &a)
    {
        return sqrtf(a.x * a.x + a.y * a.y);
    }
};

//////////////////////////////////////////////////////////////////////

struct size : vec2
{
    size() = default;

    explicit size(float x, float y) : vec2(x, y)
    {
    }

    explicit size(vec2 v) : vec2(v)
    {
    }
};

//////////////////////////////////////////////////////////////////////

struct point_s : POINTS
{
    point_s() : POINTS()
    {
    }

    point_s(short x, short y) : POINTS{ x, y }
    {
    }

    // shady
    point_s(POINT const &p) : POINTS{ (SHORT)p.x, (SHORT)p.y }
    {
    }

    point_s(POINTS const &p) : POINTS(p)
    {
    }

    operator POINTS()
    {
        return *this;
    }

    operator POINT()
    {
        return { x, y };
    }
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

    rect_f(vec2 topleft, size wh) : x(topleft.x), y(topleft.y), w(wh.x), h(wh.y)
    {
    }

    rect_f(vec2 a, vec2 b)
    {
        vec2 min = vec2::min(a, b);
        vec2 max = vec2::max(a, b);
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
};
