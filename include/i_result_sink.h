#pragma once

// Minimal callback interface a worker uses to report results back.
// Workers hold IResultSink<TResult>* — they know nothing about the full
// manager interface, just that someone wants to receive their output.
template <typename TResult>
class IResultSink {
public:
    virtual ~IResultSink() = default;
    virtual void on_result(const TResult &r) = 0;
};
