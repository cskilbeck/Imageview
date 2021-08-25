//////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////

void thread_pool_t::increment_thread_count()
{
    InterlockedIncrement(&thread_count);
}

//////////////////////////////////////////////////////////////////////

HRESULT thread_pool_t::decrement_thread_count()
{
    // The _ONLY_ thing you can do after decrementing thread_count is set thread_exit_event
    InterlockedDecrement(&thread_count);

    // also can't really check if this fails because that would involve other things
    // happening in case of failure. Also, it won't fail. FLW.
    SetEvent(thread_exit_event);
    return S_OK;
}

//////////////////////////////////////////////////////////////////////

HRESULT thread_pool_t::init()
{
    CHK_NULL(thread_exit_event = CreateEvent(null, true, true, null));
    return S_OK;
}

//////////////////////////////////////////////////////////////////////

void thread_pool_t::cleanup()
{
    while(InterlockedCompareExchange(&thread_count, 0, 0) != 0) {

        WaitForSingleObject(thread_exit_event, INFINITE);
    }

    CloseHandle(thread_exit_event);
}
