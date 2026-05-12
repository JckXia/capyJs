#include "signal_context.h"

static void on_signal_cb(uv_signal_t *handle, int signum) {
    SigEntry *entry = (SigEntry *)handle->data;
    entry->active = false;
    uv_signal_stop(handle);
    uv_close((uv_handle_t *)handle, nullptr);
    uv_stop(handle->loop);
}

void SignalContext::register_daemon(uv_loop_t *loop, int signum) {
    SigEntry &entry = signals[signum];
    if (++entry.refcount == 1) {
        entry.handle.data = &entry;
        uv_signal_init(loop, &entry.handle);
        uv_signal_start(&entry.handle, on_signal_cb, signum);
        entry.active = true;
    }
}

void SignalContext::deregister_daemon(int signum) {
    auto it = signals.find(signum);
    if (it == signals.end()) return;

    SigEntry &entry = it->second;
    if (--entry.refcount == 0 && entry.active) {
        entry.active = false;
        uv_signal_stop(&entry.handle);
        uv_close((uv_handle_t *)&entry.handle, nullptr);
    }
}
