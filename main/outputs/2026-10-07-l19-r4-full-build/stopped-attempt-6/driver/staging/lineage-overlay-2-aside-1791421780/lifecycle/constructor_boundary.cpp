#include "lifecycle.h"

extern "C" int probe_call_constructor_catching(probe_callback callback) noexcept {
    try {
        callback();
        return 0;
    } catch (...) {
        return PROBE_CTOR_EXCEPTION;
    }
}
