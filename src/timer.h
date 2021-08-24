//////////////////////////////////////////////////////////////////////

#pragma once

#include <cmath>
#include <cstdint>

//////////////////////////////////////////////////////////////////////

struct timer_t
{
    //////////////////////////////////////////////////////////////////////

    timer_t() = default;

    //////////////////////////////////////////////////////////////////////

    void reset()
    {
        update();
        first_time = current_time;
    }

    //////////////////////////////////////////////////////////////////////

    void update()
    {
        LARGE_INTEGER now;
        LARGE_INTEGER frequency;

        QueryPerformanceCounter(&now);
        QueryPerformanceFrequency(&frequency);

        last_time = current_time;
        current_time = (double)now.QuadPart / (double)frequency.QuadPart;
    }

    //////////////////////////////////////////////////////////////////////

    double delta() const
    {
        return current_time - last_time;
    }

    //////////////////////////////////////////////////////////////////////

    double wall_time() const
    {
        return current_time - first_time;
    }

    //////////////////////////////////////////////////////////////////////

    double first_time;
    double last_time;
    double current_time;
};

struct stopwatch : timer_t
{
    wchar const *name;

    explicit stopwatch(wchar const *name) : timer_t(), name(name)
    {
        reset();
    }

    ~stopwatch()
    {
        double t = elapsed();
        (void)t;
        Log(L"Stopwatch %s : %f", name, t);
    }

    double elapsed()
    {
        update();
        return wall_time();
    }
};