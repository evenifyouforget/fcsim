#ifndef STL_MOCK_HPP
#define STL_MOCK_HPP

#include "stddef.h"

namespace std {

// mock <algorithm>

template <typename T> T max(const T& lhs, const T& rhs) {
    if(lhs < rhs) {
        return rhs;
    }
    return lhs;
}

template <typename T> T min(const T& lhs, const T& rhs) {
    if(lhs > rhs) {
        return rhs;
    }
    return lhs;
}

// mock <vector>

template <typename T> struct vector {
    T* _data;
    size_t _size;
    size_t _capacity;
    vector() {
        _size = 0;
        _capacity = 8;
        _data = new T[_capacity];
    }
    ~vector() {
        delete[] _data;
    }
    void _expand() {
        _capacity *= 2;
        T* new_data = new T[_capacity];
        for(size_t i = 0; i < _size; ++i) {
            new_data[i] = _data[i];
        }
        delete[] _data;
        _data = new_data;
    }
    size_t size() {
        return _size;
    }
    void push_back(const T& value) {
        if(_size == _capacity) {
            _expand();
        }
        _data[_size++] = value;
    }
};

// mock <string>

}

#endif