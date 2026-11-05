#pragma once

#include <cstdlib>
#include <iostream>

// Unlike assert(), these checks remain active in Release builds.
#define CHECK(...) do { \
    if (!(__VA_ARGS__)) { \
        std::cerr << __FILE__ << ':' << __LINE__ << ": check failed: " << #__VA_ARGS__ << '\n'; \
        std::exit(EXIT_FAILURE); \
    } \
} while (false)
