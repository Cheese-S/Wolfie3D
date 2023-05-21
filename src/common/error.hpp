#pragma once

#include <cassert>
#include <stdexcept>

#if defined(__clang__)
// CLANG ENABLE/DISABLE WARNING DEFINITION
#define VKBP_DISABLE_WARNINGS()                                                    \
    _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wall\"") \
        _Pragma("clang diagnostic ignored \"-Wextra\"")                            \
            _Pragma("clang diagnostic ignored \"-Wtautological-compare\"")

#define VKBP_ENABLE_WARNINGS() _Pragma("clang diagnostic pop")
#endif