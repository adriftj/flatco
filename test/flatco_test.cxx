#include <coroutine>
#include <iostream>
#include <string.h>
#include <chrono>
#include "flatco.h"

struct task {
    struct promise_type {
        promise_type() noexcept {}
        task get_return_object() noexcept { return { std::coroutine_handle<task::promise_type>::from_promise(*this) }; }
        constexpr std::suspend_never initial_suspend() const noexcept { return {}; }
        constexpr std::suspend_never final_suspend() const noexcept { return {}; }
        constexpr void return_void() const noexcept {}
        constexpr void unhandled_exception() const noexcept {}
    };

    void resume() { handle_.resume(); }

    std::coroutine_handle<task::promise_type> handle_;
};

typedef void(*OnPacket)(const char* s);

struct Filter {
    Filter(OnPacket onPacket) : onPacket_(onPacket), texts_(NULL) {
        t_ = run();
    }

    void onData(const char* s) {
        texts_ = s;
        t_.resume();
    }

    task run();

    OnPacket onPacket_;
    task t_;
    const char* texts_;
};

struct GetText {
    GetText() : filter_(NULL), s_(NULL) {}
    GetText(Filter* filter) : filter_(filter), s_(NULL) {}
    GetText(const GetText&) = delete;
    GetText& operator=(const GetText&) = delete;
    GetText(GetText&& other) : filter_(other.filter_), s_(other.s_) { other.filter_ = NULL; other.s_ = NULL; }

    bool await_ready() { return s_!=NULL || filter_->texts_!=NULL; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<task::promise_type> continuation) {
        filter_->t_.handle_ = continuation;
        return std::noop_coroutine();
    }
    const char* await_resume() {
        const char* s = s_;
        if (s)
            s_ = NULL;
        else {
            s = filter_->texts_;
            filter_->texts_ = NULL;
            if (s && !strcmp(s, "do do"))
                throw "do do";
        }
        return s;
    }

    GetText& with_s(const char* s) {
        s_ = s;
        return *this;
    }

    Filter* filter_;
    const char* s_;
};

BL_func(task) const char* AsyncGetText(GetText& getText, const char* t) {
    GetText& gt = getText.with_s(/*a parameter*/t);
    BL_return(co_await gt);
}

task Filter::run() {
    GetText getText = GetText(this);
    for (;;) {
        try {
            const char* s = co_await getText;
            if (s == NULL)
                break;
            if (strstr(s, "you"))
                onPacket_(s);
            BL_call(s = AsyncGetText(getText, "Inline you"));
            if (strstr(s, "you"))
                onPacket_(s);
        }
        catch (const char* err) {
            printf("except: %s\n", err);
        }
    }
}

void MyOnPacket(const char* s) {
    printf("%s\n", s);
}

int main() {
    static const char* texts[] = {
        "Can you",
        "Find a",
        "you can",
        "do do",
        "ok"
    };

    Filter filter(MyOnPacket);
    int N = sizeof(texts)/sizeof(texts[0]);
    for (int i=0; i<N; ++i)
        filter.onData(texts[i]);
    filter.onData(NULL);
    return 0;
}
