#pragma once

//////////////////////////////////////////////////////////////////////
// defer / scoped

namespace imageview::defer
{
    template <typename F> class defer_finalizer
    {
        F f;
        bool moved;

    public:
        template <typename T> defer_finalizer(T &&f_) : f(std::forward<T>(f_)), moved(false)
        {
        }

        defer_finalizer(const defer_finalizer &) = delete;

        defer_finalizer(defer_finalizer &&other) : f(std::move(other.f)), moved(other.moved)
        {
            other.moved = true;
        }

        void cancel()
        {
            moved = true;
        }

        ~defer_finalizer()
        {
            if(!moved) {
                f();
            }
        }
    };

    template <typename F> defer_finalizer<F> deferred(F &&f)
    {
        return defer_finalizer<F>(std::forward<F>(f));
    }

    static struct
    {
        template <typename F> defer_finalizer<F> operator<<(F &&f)
        {
            return defer_finalizer<F>(std::forward<F>(f));
        }
    } deferrer;
}

#define _DEFER_TOKENPASTE(x, y) x##y
#define _DEFER_TOKENPASTE2(x, y) _DEFER_TOKENPASTE(x, y)

#define SCOPED imageview::defer::deferrer <<
#define DEFER(X) auto _DEFER_TOKENPASTE2(__deferred_lambda_call, __COUNTER__) = imageview::defer::deferrer << [=] { X; }
