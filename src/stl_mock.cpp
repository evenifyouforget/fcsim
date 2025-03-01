#include "stl_mock.h"

std::string::string() {
    _length = 0;
}

std::string::string(const std::string& other) {
    _data = other._data;
    _length = other._length;
}

std::string::string(std::string&& other) {
    _data = other._data;
    _length = other._length;
}

std::string::string(const char* other) {
    for(_length = 0; other[_length]; ++_length) {
        _data.push_back(other[_length]);
    }
}

void std::string::append(char c) {
    if(_data.size() == _length) {
        _data.push_back(c);
    } else {
        _data[_length] = c;
    }
    _length++;
}

std::string& std::string::operator=(const std::string& other) {
    _data = other._data;
    _length = other._length;
    return *this;
}

std::string& std::string::operator=(const char* other) {
    _data.clear();
    for(_length = 0; other[_length]; ++_length) {
        _data.push_back(other[_length]);
    }
    return *this;
}

std::string std::to_string(int64_t value) {
    if(value == 0)return "0";
    bool neg = value < 0;
    if(neg)value = -value;
    char buffer[21] = {0};
    int i;
    for(i = 0;value;++i) {
        buffer[i] = (char)('0' + value % 10);
        value /= 10;
    }
    std::string result;
    if(neg)result.append('-');
    for(int j = i-1; j >= 0; --j) {
        result.append(buffer[j]);
    }
    return result;
}

char* std::string::c_str() {
    if(_data._capacity == _length) {
        _data.push_back((char)0);
    }
    _data[_length] = (char)0;
    return &(_data[0]);
}

size_t std::string::size() const {
    return _length;
}

char& std::string::operator[](size_t pos) {
    return _data[pos];
}