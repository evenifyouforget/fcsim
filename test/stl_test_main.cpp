#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTest/TestHarness.h"
#include "stl_mock.h"

// ── _new ─────────────────────────────────────────────────────────────────────

TEST_GROUP(NewTests){};

TEST(NewTests, DefaultValue) {
  int v;
  _new<int>(&v);
  CHECK_EQUAL(0, v);
}

// ── vector ───────────────────────────────────────────────────────────────────

TEST_GROUP(VectorTests){};

TEST(VectorTests, ConstructDestruct) { std::vector<int> vec; }

TEST(VectorTests, InitialSize) {
  std::vector<int> vec;
  CHECK_EQUAL(0, (int)vec.size());
}

TEST(VectorTests, PushBack) {
  for (int i = 0; i < 100; ++i) {
    std::vector<int> vec;
    for (int j = 0; j < i; ++j)
      vec.push_back(j);
    CHECK_EQUAL(i, (int)vec.size());
  }
}

TEST(VectorTests, Index) {
  for (int i = 0; i < 100; ++i) {
    std::vector<int> vec;
    for (int j = 0; j < i; ++j) {
      vec.push_back(j);
      CHECK_EQUAL(j, vec[j]);
    }
    CHECK_EQUAL(i, (int)vec.size());
  }
}

TEST(VectorTests, Clear) {
  for (int i = 0; i < 100; ++i) {
    std::vector<int> vec;
    for (int j = 0; j < i; ++j)
      vec.push_back(j);
    CHECK_EQUAL(i, (int)vec.size());
    vec.clear();
    for (int j = 0; j < i; ++j) {
      vec.push_back(2 * j);
      CHECK_EQUAL(2 * j, vec[j]);
    }
    CHECK_EQUAL(i, (int)vec.size());
  }
}

TEST(VectorTests, Copy) {
  for (int i = 0; i < 100; ++i) {
    std::vector<std::vector<int>> vec;
    vec.push_back(std::vector<int>());
    for (int j = 0; j < 100; ++j)
      vec[0].push_back(j);
    for (int j = 0; j < i; ++j) {
      vec.push_back(vec[j]);
      vec[j][0] = 2 * j;
    }
    for (int j = 0; j < i; ++j)
      CHECK_EQUAL(2 * j, vec[j][0]);
    CHECK_EQUAL(0, vec[i][0]);
  }
}

TEST(VectorTests, ExplicitCopy) {
  for (int i = 0; i < 100; ++i) {
    std::vector<std::vector<int>> vec;
    vec.push_back(std::vector<int>());
    for (int j = 0; j < 100; ++j)
      vec[0].push_back(j);
    for (int j = 0; j < i; ++j) {
      vec.push_back(std::vector<int>());
      vec[j + 1] = vec[j];
      vec[j][0] = 2 * j;
    }
    for (int j = 0; j < i; ++j)
      CHECK_EQUAL(2 * j, vec[j][0]);
    CHECK_EQUAL(0, vec[i][0]);
  }
}

TEST(VectorTests, ResizeSize) {
  std::vector<int> vec;
  for (int i = 0; i < 100; ++i) {
    vec.resize(i);
    CHECK_EQUAL(i, (int)vec.size());
  }
  for (int i = 100; i >= 0; --i) {
    vec.resize(i);
    CHECK_EQUAL(i, (int)vec.size());
  }
}

TEST(VectorTests, ResizeKeepsData) {
  std::vector<int> vec;
  for (int i = 1; i < 100; ++i) {
    vec.resize(i);
    CHECK_EQUAL(i, (int)vec.size());
    vec[i - 1] = i - 1;
    for (int j = 0; j < i; ++j)
      CHECK_EQUAL(j, vec[j]);
  }
}

TEST(VectorTests, ResizeDefaultElements) {
  int default_value;
  _new<int>(&default_value);
  std::vector<int> vec;
  vec.resize(10);
  for (int i = 0; i < 10; ++i)
    CHECK_EQUAL(default_value, vec[i]);
}

// ── string ───────────────────────────────────────────────────────────────────

TEST_GROUP(StringTests){};

TEST(StringTests, ConstructDestruct) { std::string str; }

TEST(StringTests, ToStringData) {
  std::string str = std::to_string(12345);
  for (int i = 0; i < 5; ++i)
    CHECK_EQUAL('0' + i + 1, str._data[i]);
  CHECK_EQUAL(5, (int)str._length);
}

TEST(StringTests, ToStringNegative) {
  std::string str = std::to_string(-12345);
  CHECK_EQUAL('-', str._data[0]);
  for (int i = 0; i < 5; ++i)
    CHECK_EQUAL('0' + i + 1, str._data[i + 1]);
  CHECK_EQUAL(6, (int)str._length);
}

TEST(StringTests, ToStringLargeValue) {
  std::to_string(9223372036854000000LL);
  std::to_string(-9223372036854000000LL);
}

TEST(StringTests, AssignByStringLiteral) {
  std::string str;
  str = "Hello world";
}

TEST(StringTests, ConstructByStringLiteral) { std::string str = "Hello world"; }

TEST(StringTests, Size) {
  std::string str;
  CHECK_EQUAL(0, (int)str.size());
  for (int i = 1; i < 10; ++i) {
    str.append('0' + i);
    CHECK_EQUAL(i, (int)str.size());
  }
}

TEST(StringTests, ToStringZero) {
  std::string str = std::to_string(0);
  CHECK_EQUAL(1, (int)str.size());
  CHECK_EQUAL('0', str[0]);
}

TEST(StringTests, ToStringSingleDigit) {
  std::string str;
  for (int i = 0; i < 10; ++i) {
    str = std::to_string(i);
    CHECK_EQUAL(1, (int)str.size());
  }
}

TEST(StringTests, ToStringSingleDigitNegative) {
  std::string str;
  for (int i = -9; i < 0; ++i) {
    str = std::to_string(i);
    CHECK_EQUAL(2, (int)str.size());
  }
}

TEST(StringTests, TwoDMemory) {
  for (int i = 0; i < 100; ++i) {
    std::vector<std::string> vec;
    for (int j = 0; j < i; ++j)
      vec.push_back(std::to_string(j));
    vec.clear();
    for (int j = 0; j < i; ++j)
      vec.push_back(std::to_string(2 * j));
  }
}

TEST(StringTests, TwoDMemoryOverwrite) {
  for (int i = 0; i < 100; ++i) {
    std::vector<std::string> vec;
    for (int j = 0; j < i; ++j)
      vec.push_back(std::to_string(j));
    for (int j = 0; j < i; ++j)
      vec[j] = std::to_string(2 * j);
  }
}

// ── unordered_map ─────────────────────────────────────────────────────────────

TEST_GROUP(UnorderedMapTests){};

TEST(UnorderedMapTests, EmptyCount) {
  std::unordered_map<int, int> map;
  CHECK_EQUAL(0, (int)map.count(1));
}

TEST(UnorderedMapTests, InsertAndCount) {
  std::unordered_map<int, int> map;
  map.insert(std::make_pair(1, 42));
  CHECK_EQUAL(1, (int)map.count(1));
  CHECK_EQUAL(0, (int)map.count(2));
}

TEST(UnorderedMapTests, AtReturnsCorrectValue) {
  std::unordered_map<int, int> map;
  map.insert(std::make_pair(1, 42));
  CHECK_EQUAL(42, map.at(1));
}

TEST(UnorderedMapTests, InsertDoesNotOverwrite) {
  std::unordered_map<int, int> map;
  map.insert(std::make_pair(1, 42));
  map.insert(std::make_pair(1, 99));
  CHECK_EQUAL(42, map.at(1));
}

TEST(UnorderedMapTests, AtReturnsReference) {
  std::unordered_map<int, int> map;
  map.insert(std::make_pair(1, 42));
  map.at(1) = 100;
  CHECK_EQUAL(100, map.at(1));
}

TEST(UnorderedMapTests, LargerDataTrivial) {
  std::unordered_map<int, int> map;
  for (int i = 0; i < 100; ++i)
    map.insert(std::make_pair(i, i * 2));
  for (int i = 0; i < 100; ++i)
    CHECK_EQUAL(i * 2, map.at(i));
}

TEST(UnorderedMapTests, LargerDataEvenlyDistributed) {
  std::unordered_map<int, int> map;
  int k = 1, v = 1;
  for (int i = 0; i < 100; ++i) {
    k = k * 5 % 107;
    v = v * 3 % 113;
    map.insert(std::make_pair(k, v));
  }
  k = 1;
  v = 1;
  for (int i = 0; i < 100; ++i) {
    k = k * 5 % 107;
    v = v * 3 % 113;
    CHECK_EQUAL(v, map.at(k));
  }
}

TEST(UnorderedMapTests, LargerDataUnevenlyDistributed) {
  std::unordered_map<int, int> map;
  for (int i = 0; i < 250; ++i)
    map.insert(std::make_pair(i * i * i * i, i * 2));
  for (int i = 0; i < 250; ++i) {
    int key = i * i * i * i;
    CHECK_EQUAL(i * 2, map.at(key));
    CHECK_EQUAL(1, (int)map.count(key));
    CHECK_EQUAL(0, (int)map.count(key + 2));
  }
}

// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
  return CommandLineTestRunner::RunAllTests(argc, argv);
}
