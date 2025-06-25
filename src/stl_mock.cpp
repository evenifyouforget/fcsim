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

std::string std::to_string(uint64_t value) {
    if(value == 0)return "0";
    //bool neg = value < 0;
    //if(neg)value = -value;
    char buffer[21] = {0};
    int i;
    for(i = 0;value;++i) {
        buffer[i] = (char)('0' + value % 10);
        value /= 10;
    }
    std::string result;
    //if(neg)result.append('-');
    for(int j = i-1; j >= 0; --j) {
        result.append(buffer[j]);
    }
    return result;
}

std::string std::to_string(int32_t v)    { return std::to_string((int64_t)v); }
std::string std::to_string(int16_t v)    { return std::to_string((int64_t)v); }
std::string std::to_string(int8_t v)     { return std::to_string((int64_t)v); }

std::string std::to_string(uint32_t v)   { return std::to_string((uint64_t)v); }
std::string std::to_string(uint16_t v)   { return std::to_string((uint64_t)v); }
std::string std::to_string(uint8_t v)    { return std::to_string((uint64_t)v); }

std::string std::to_string(double value) {
    // Handle special cases
    if (value != value) return "nan";
    if (value == 1.0 / 0.0) return "inf";
    if (value == -1.0 / 0.0) return "-inf";

    std::string result;
    if (value < 0) {
        result.append('-');
        value = -value;
    }

    // Use scientific notation for very large or very small values
    int exp = 0;
    double abs_val = value;
    if ((abs_val != 0.0 && (abs_val >= 1e10 || abs_val < 1e-6))) {
        // Normalize to [1,10)
        while (abs_val >= 10.0) {
            abs_val /= 10.0;
            ++exp;
        }
        while (abs_val > 0.0 && abs_val < 1.0) {
            abs_val *= 10.0;
            --exp;
        }
        // Print mantissa with 6 decimal digits
        int64_t int_part = (int64_t)abs_val;
        double frac_part = abs_val - int_part;
        result = result + std::to_string(int_part);
        result.append('.');
        for (int i = 0; i < 6; ++i) {
            frac_part *= 10;
            int digit = (int)frac_part;
            result.append('0' + digit);
            frac_part -= digit;
        }
        result.append('e');
        // Add exponent sign if needed
        if (exp >= 0) result.append('+');
        result = result + std::to_string(exp);
        return result;
    } else {
        // Standard fixed-point for "normal" numbers
        int64_t int_part = (int64_t)value;
        double frac_part = value - int_part;
        result = result + std::to_string(int_part);
        result.append('.');
        for (int i = 0; i < 6; ++i) {
            frac_part *= 10;
            int digit = (int)frac_part;
            result.append('0' + digit);
            frac_part -= digit;
        }
        return result;
    }
}

std::string to_string(float v)     { return std::to_string((double)v); }

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

std::string operator+(const std::string& lhs, const std::string& rhs) {
    std::string result = lhs;
    for (size_t i = 0; i < rhs.size(); ++i) {
        result.append(rhs._data[i]);
    }
    return result;
}