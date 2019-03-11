// copyright defined in LICENSE.txt

// todo: remove or replace everything in this file

#pragma once

extern "C" void print_range(const char* begin, const char* end);

template <typename T>
T& lvalue(T&& v) {
    return v;
}
