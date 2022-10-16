[![unit tests](https://github.com/DanglingPointer/crhandle/actions/workflows/cmake.yml/badge.svg)](https://github.com/DanglingPointer/crhandle/actions/workflows/cmake.yml)

# crhandle
Simple coroutine library for C++20. Cancelation is implemented using exceptions rather than cancelation tokens.`TaskHandle` is not thread safe, but `Unichannel` is.
