#ifndef STL_MOCK_H
#define STL_MOCK_H

#include "stdlib.h"
#include "stdint.h"
#include "string.h"
#include "stl_compat.h"

// issues:
// * wrong behaviour for address-sensitive types
// * * new
// * * vector

namespace std {

// <algorithm>

template <class T>
T abs(const T& a){
    return (a < 0) ? -a : a;
}

template <class T>
const T& min(const T& a, const T& b){
    return !(b < a) ? a : b;
}

    template <class T>
const T& max(const T& a, const T& b){
    return (a < b) ? b : a;
}

// <vector>

template <typename T>
struct vector {
    T* _storage;
    size_t _capacity;
    size_t _size;
    vector() {
        _capacity = 8;
        _size = 0;
        _storage = (T*)malloc(_capacity * sizeof(T));
    }
    vector(const vector<T>& other) {
        _capacity = other._capacity;
        _size = other._size;
        _storage = (T*)malloc(_capacity * sizeof(T));
        for(size_t i = 0; i < _size; ++i) {
            _new<T>(_storage + i);
            _storage[i] = other[i];
        }
    }
    ~vector() {
        for(size_t i = 0; i < _size; ++i) {
            _storage[i].~T();
        }
        free(_storage);
    }
    size_t size() {
        return _size;
    }
    void _expand() {
        size_t _new_capacity = _capacity << 1;
        T* _new_storage = (T*)malloc(_new_capacity * sizeof(T));
        for(size_t i = 0; i < _size; ++i) {
            _new<T>(_new_storage + i);
            _new_storage[i] = _storage[i];
            _storage[i].~T();
        }
        free(_storage);
        _storage = _new_storage;
        _capacity = _new_capacity;
    }
    void _ensure_capacity(size_t _new_size) {
        if(_new_size > _capacity) {
            _expand();
        }
    }
    void push_back(const T& value) {
        T value_copy = value; // copy before alloc in case it's our own data
        _ensure_capacity(_size + 1);
        _size++;
        _new<T>(_storage + _size - 1);
        _storage[_size - 1] = value_copy;
    }
    void emplace_back() {
        push_back(T());
    }
    T& operator [] (size_t index) const {
        return _storage[index];
    }
    void clear() {
        for(size_t i = 0; i < _size; ++i) {
            _storage[i].~T();
        }
        _size = 0;
    }
    T* begin() {
        return _storage;
    }
    T* end() {
        return _storage + _size;
    }
    vector<T>& operator=(const vector<T>& other) {
        for(size_t i = 0; i < _size; ++i) {
            _storage[i].~T();
        }
        free(_storage);
        _capacity = other._capacity;
        _size = other._size;
        _storage = (T*)malloc(_capacity * sizeof(T));
        for(size_t i = 0; i < _size; ++i) {
            _new<T>(_storage + i);
            _storage[i] = other[i];
        }
        return *this;
    }
};

// <string>

struct string {
    vector<char> _data;
    size_t _length;
    string();
    string(const string&);
    string(string&&);
    char* c_str();
    void append(char);
    string& operator=(const string& other);
};

string to_string(int64_t);

}

#endif