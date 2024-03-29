//////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

namespace imageview
{
    struct timer_t
    {
        //////////////////////////////////////////////////////////////////////

        timer_t()
        {
            reset();
        }

        //////////////////////////////////////////////////////////////////////

        void reset()
        {
            QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER *>(&frequency));

            update();
            first_time = current_time;
        }

        //////////////////////////////////////////////////////////////////////

        void update()
        {
            last_time = current_time;
            QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(&current_time));
        }

        //////////////////////////////////////////////////////////////////////

        double current() const
        {
            return static_cast<double>(current_time) / frequency;
        }

        //////////////////////////////////////////////////////////////////////

        double delta() const
        {
            return static_cast<double>(current_time - last_time) / frequency;
        }

        //////////////////////////////////////////////////////////////////////

        double wall_time() const
        {
            return static_cast<double>(current_time - first_time) / frequency;
        }

        //////////////////////////////////////////////////////////////////////

        uint64 first_time;
        uint64 last_time;
        uint64 current_time;
        uint64 frequency;
    };

    //////////////////////////////////////////////////////////////////////

    struct stopwatch : timer_t
    {
        LOG_CONTEXT("stopwatch");

        wchar const *name;

        explicit stopwatch(wchar const *name) : timer_t(), name(name)
        {
            reset();
        }

        void report(wchar_t const *event)
        {
            update();
            LOG_INFO(L"Stopwatch {} delta {:10.5f}, wall time {:10.5f} ({})", name, delta(), wall_time(), event);
        }

        ~stopwatch()
        {
            report(L"stopwatch ends");
        }
    };
}