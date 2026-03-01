#include "stl_mock.h"

std::string::string() { _length = 0; }

std::string::string(const std::string &other) {
  _data = other._data;
  _length = other._length;
}

std::string::string(std::string &&other) {
  _data = other._data;
  _length = other._length;
}

std::string::string(const char *other) {
  for (_length = 0; other[_length]; ++_length) {
    _data.push_back(other[_length]);
  }
}

void std::string::append(char c) {
  if (_data.size() == _length) {
    _data.push_back(c);
  } else {
    _data[_length] = c;
  }
  _length++;
}

std::string &std::string::operator=(const std::string &other) {
  _data = other._data;
  _length = other._length;
  return *this;
}

std::string &std::string::operator=(const char *other) {
  _data.clear();
  for (_length = 0; other[_length]; ++_length) {
    _data.push_back(other[_length]);
  }
  return *this;
}

std::string &std::string::operator+=(char c) {
  append(c);
  return *this;
}

std::string std::to_string(int64_t value) {
  if (value == 0)
    return "0";
  bool neg = value < 0;
  if (neg)
    value = -value;
  char buffer[21] = {0};
  int i;
  for (i = 0; value; ++i) {
    buffer[i] = (char)('0' + value % 10);
    value /= 10;
  }
  std::string result;
  if (neg)
    result.append('-');
  for (int j = i - 1; j >= 0; --j) {
    result.append(buffer[j]);
  }
  return result;
}

std::string std::to_string(uint64_t value) {
  if (value == 0)
    return "0";
  char buffer[21] = {0};
  int i = 0;
  while (value) {
    buffer[i++] = (char)('0' + value % 10);
    value /= 10;
  }
  std::string result;
  for (int j = i - 1; j >= 0; --j) {
    result.append(buffer[j]);
  }
  return result;
}

std::string std::to_string(double value) {
  if (value != value) // NaN
    return "nan";
  bool neg = value < 0.0;
  double absval = neg ? -value : value;

  int64_t intpart = (int64_t)absval;
  double frac = absval - (double)intpart;

  // round to 5 decimal places
  int64_t frac5 = (int64_t)(frac * 100000.0 + 0.5);
  if (frac5 >= 100000) {
    intpart += 1;
    frac5 = 0;
  }

  std::string result;
  if (neg)
    result.append('-');

  std::string intstr = std::to_string(intpart);
  for (size_t i = 0; i < intstr.size(); ++i)
    result.append(intstr[i]);

  result.append('.');

  // fractional part with leading zeros to 5 digits
  char buf[6];
  buf[5] = 0;
  for (int i = 4; i >= 0; --i) {
    buf[i] = (char)('0' + (frac5 % 10));
    frac5 /= 10;
  }
  for (int i = 0; i < 5; ++i)
    result.append(buf[i]);

  return result;
}

// forwarding overloads for other integer types
std::string std::to_string(int32_t v) { return std::to_string((int64_t)v); }
std::string std::to_string(uint32_t v) { return std::to_string((uint64_t)v); }
std::string std::to_string(int16_t v) { return std::to_string((int64_t)v); }
std::string std::to_string(uint16_t v) { return std::to_string((uint64_t)v); }
std::string std::to_string(int8_t v) { return std::to_string((int64_t)v); }
std::string std::to_string(uint8_t v) { return std::to_string((uint64_t)v); }
std::string std::to_string(int v) { return std::to_string((int64_t)v); }
std::string std::to_string(unsigned int v) {
  return std::to_string((uint64_t)v);
}
std::string std::to_string(long v) { return std::to_string((int64_t)v); }
std::string std::to_string(unsigned long v) {
  return std::to_string((uint64_t)v);
}
std::string std::to_string(long long v) { return std::to_string((int64_t)v); }
std::string std::to_string(unsigned long long v) {
  return std::to_string((uint64_t)v);
}
std::string std::to_string(size_t v) { return std::to_string((uint64_t)v); }

// floating point forwarding
std::string std::to_string(float v) { return std::to_string((double)v); }
std::string std::to_string(long double v) { return std::to_string((double)v); }

char *std::string::c_str() {
  if (_data._capacity == _length) {
    _data.push_back((char)0);
  }
  _data[_length] = (char)0;
  return &(_data[0]);
}

size_t std::string::size() const { return _length; }

char &std::string::operator[](size_t pos) { return _data[pos]; }
