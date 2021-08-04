//////////////////////////////////////////////////////////////////////

#pragma once

#include <sstream>

//////////////////////////////////////////////////////////////////////

struct rect : RECT
{
    rect() = default;

    rect(RECT const &r) : RECT(r)
    {
    }

    rect(int l, int t, int r, int b) : RECT{ l, t, r, b }
    {
    }

    inline static rect &as(RECT &r)
    {
        return (rect &)r;
    }

    int w() const
    {
        return right - left;
    };

    int h() const
    {
        return bottom - top;
    }
    int x() const
    {
        return left;
    }
    int y() const
    {
        return top;
    }
    int r() const
    {
        return right;
    }
    int b() const
    {
        return bottom;
    }

    std::basic_string<wchar_t> as_string()
    {
        std::basic_ostringstream<wchar_t> s;
        s << L"X:" << x() << L" Y:" << y() << L" W:" << w() << L" H:" << h();
        return s.str();
    }
};

//////////////////////////////////////////////////////////////////////

struct point_f
{
    float x;
    float y;

    point_f() = default;

    point_f(float x, float y) : x(x), y(y)
    {
    }

    explicit point_f(POINT p) : x((float)p.x), y((float)p.y)
    {
    }

    explicit point_f(POINTS p) : x((float)p.x), y((float)p.y)
    {
    }

    static point_f min(point_f const &a, point_f const &b)
    {
        return { std::min(a.x, b.x), std::min(a.y, b.y) };
    }

    static point_f max(point_f const &a, point_f const &b)
    {
        return { std::max(a.x, b.x), std::max(a.y, b.y) };
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

    point_f top_left() const
    {
        return point_f(x, y);
    }

    point_f bottom_right() const
    {
        return point_f(x + w - 1, y + h - 1);
    }
};

//////////////////////////////////////////////////////////////////////
