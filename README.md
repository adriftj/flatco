# flatco

A C++ coroutine co_await preprocessor

This project is a preprocessor for C++20 coroutine functions. In the current C++20 coroutine standard, the main
coroutine function can only call/wait for other coroutines through co_await, which will have a coroutine creation
overhead (memory allocation, etc.) far exceeding that of ordinary function calls; or call ordinary functions, but
the function cannot use co_await because it is not a coroutine function. This project uses a preprocessing method
to allow the main coroutine to call other functions that call co_await internally. This avoids the main coroutine
function being too complicated due to the use of co_await only in the coroutine main function to improve performance,
and makes the main coroutine function call other functions that use co_await with almost no overhead.
