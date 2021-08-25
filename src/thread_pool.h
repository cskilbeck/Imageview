//////////////////////////////////////////////////////////////////////
// noddy thread pool functions

#pragma once

//////////////////////////////////////////////////////////////////////

struct thread_pool_t
{
    HRESULT init();
    void cleanup();

    // NOTE: these create_thread... functions pass the args by value!!!

    //////////////////////////////////////////////////////////////////////
    // fire and forget a thread

    template <class FUNC, class... Args> HRESULT create_thread(FUNC function, Args... arguments)
    {
        increment_thread_count();

        std::thread(

            [this, function](Args... args) {

                function(args...);

                decrement_thread_count();
            },
            arguments...)
            .detach();

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // force a message pump to be created before returning the thread_id
    // so you can PostMessage to it safely as soon as this function returns

    template <class FUNC, class... Args> HRESULT create_thread_with_message_pump(uint *thread_id, FUNC function, Args... arguments)
    {
        if(thread_id == null) {
            return E_INVALIDARG;
        }

        HANDLE msg_q_created;
        CHK_NULL(msg_q_created = CreateEvent(null, false, false, null));

        defer(CloseHandle(msg_q_created));

        increment_thread_count();

        std::thread thread = std::thread(

            [this, function](HANDLE ev, Args... args) {

                MSG msg;
                PeekMessage(&msg, (HWND)-1, 0, 0, PM_NOREMOVE);

                SetEvent(ev);

                function(args...);

                decrement_thread_count();
            },
            msg_q_created, arguments...);

        uint id;
        CHK_BOOL(id = GetThreadId(thread.native_handle()));

        if(WaitForSingleObject(msg_q_created, INFINITE) != WAIT_OBJECT_0) {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        thread.detach();

        *thread_id = id;

        return S_OK;
    }

private:
    void increment_thread_count();
    HRESULT decrement_thread_count();

    LONG thread_count;
    HANDLE thread_exit_event;
};
