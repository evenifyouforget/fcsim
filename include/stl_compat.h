#ifndef STL_COMPAT_H
#define STL_COMPAT_H

#include "stdlib.h"

// no destruct

template <typename T> struct _nodestruct {
    union {
        T value;
    };
    _nodestruct(): value() {}
    ~_nodestruct() {}
};

// new interface functions

template <typename T>
void _new(T* addr) {
    #ifdef __wasm__
    // need to implement our own
    _nodestruct<T> value;
    memcpy(addr, &value, sizeof(T));
    #else
    // use builtin
    new(addr) T();
    #endif
}

template <typename T>
T* _new() {
    T* addr = (T*)malloc(sizeof(T));
    _new<T>(addr);
    return addr;
}

#endif