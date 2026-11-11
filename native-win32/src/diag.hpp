#pragma once
#include <string>

namespace diag {
void enable(bool on);
bool enabled();
void log(const char* msg);
struct Timer {
    explicit Timer(const char* name);
    ~Timer();
    const char* name;
    long long t0;
};
}
