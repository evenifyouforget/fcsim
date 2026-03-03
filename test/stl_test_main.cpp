#include "stl_mock.h"
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

TEST_GROUP(STLMock) {
};

TEST(STLMock, NewDefaultValue) {
  int default_value;
  _new<int>(&default_value);
  CHECK_EQUAL(0, default_value);
}

TEST_GROUP(Vector) {
};

TEST(Vector, ConstructDestruct) {
  std::vector<int> vec;
}

TEST(Vector, Size) {
  std::vector<int> vec;
  CHECK_EQUAL(0, vec.size());
}

TEST(Vector, PushBack) {
  for (int i = 0; i < 100; ++i) {
    std::vector<int> vec;
    for (int j = 0; j < i; ++j) {
      vec.push_back(j);
    }
    CHECK_EQUAL((size_t)i, vec.size());
  }
}

TEST(Vector, Index) {
  for (int i = 0; i < 100; ++i) {
    std::vector<int> vec;
    for (int j = 0; j < i; ++j) {
      vec.push_back(j);
      CHECK_EQUAL(j, vec[j]);
    }
    CHECK_EQUAL((size_t)i, vec.size());
  }
}

TEST(Vector, Clear) {
  for (int i = 0; i < 100; ++i) {
    std::vector<int> vec;
    for (int j = 0; j < i; ++j) {
      vec.push_back(j);
      CHECK_EQUAL(j, vec[j]);
    }
    CHECK_EQUAL((size_t)i, vec.size());
    vec.clear();
    for (int j = 0; j < i; ++j) {
      vec.push_back(2 * j);
      CHECK_EQUAL(2 * j, vec[j]);
    }
    CHECK_EQUAL((size_t)i, vec.size());
  }
}

TEST_GROUP(String) {
};

TEST(String, ConstructDestruct) {
  std::string str;
}

TEST(String, ToString) {
  std::string str = std::to_string(12345);
}

TEST(String, ToStringData) {
  std::string str = std::to_string(12345);
  for (int i = 0; i < 5; ++i) {
    CHECK_EQUAL('0' + i + 1, str._data[i]);
  }
  CHECK_EQUAL(5, str._length);
}

TEST(String, ToStringNegative) {
  std::string str = std::to_string(-12345);
  CHECK_EQUAL('-', str._data[0]);
  for (int i = 0; i < 5; ++i) {
    CHECK_EQUAL('0' + i + 1, str._data[i + 1]);
  }
  CHECK_EQUAL(6, str._length);
}

TEST(String, ToStringLarge) {
  std::to_string(9223372036854000000);
  std::to_string(-9223372036854000000);
}

TEST_GROUP(Memory) {
};

TEST(Memory, TwoDMemory) {
  for (int i = 0; i < 100; ++i) {
    std::vector<std::string> vec;
    for (int j = 0; j < i; ++j) {
      vec.push_back(std::to_string(j));
    }
    vec.clear();
    for (int j = 0; j < i; ++j) {
      vec.push_back(std::to_string(2 * j));
    }
  }
}

TEST(Memory, TwoDMemoryOverwrite) {
  for (int i = 0; i < 100; ++i) {
    std::vector<std::string> vec;
    for (int j = 0; j < i; ++j) {
      vec.push_back(std::to_string(j));
    }
    for (int j = 0; j < i; ++j) {
      vec[j] = std::to_string(2 * j);
    }
  }
}

TEST(Vector, VectorCopy) {
  for (int i = 0; i < 100; ++i) {
    std::vector<std::vector<int>> vec;
    vec.push_back(std::vector<int>());
    for (int j = 0; j < 100; ++j) {
      vec[0].push_back(j);
    }
    for (int j = 0; j < i; ++j) {
      vec.push_back(vec[j]);
      vec[j][0] = 2 * j;
    }
    for (int j = 0; j < i; ++j) {
      CHECK_EQUAL(2 * j, vec[j][0]);
    }
    CHECK_EQUAL(0, vec[i][0]);
  }
}

TEST(Vector, ExplicitCopy) {
  for (int i = 0; i < 100; ++i) {
    std::vector<std::vector<int>> vec;
    vec.push_back(std::vector<int>());
    for (int j = 0; j < 100; ++j) {
      vec[0].push_back(j);
    }
    for (int j = 0; j < i; ++j) {
      vec.push_back(std::vector<int>());
      vec[j + 1] = vec[j];
      vec[j][0] = 2 * j;
    }
    for (int j = 0; j < i; ++j) {
      CHECK_EQUAL(2 * j, vec[j][0]);
    }
    CHECK_EQUAL(0, vec[i][0]);
  }
}

TEST(Vector, ResizeUpDown) {
  std::vector<int> vec;
  for (int i = 0; i < 100; ++i) {
    vec.resize(i);
    CHECK_EQUAL((size_t)i, vec.size());
  }
  for (int i = 100; i >= 0; --i) {
    vec.resize(i);
    CHECK_EQUAL((size_t)i, vec.size());
  }
}

TEST(Vector, ResizeKeepsData) {
  std::vector<int> vec;
  for (int i = 1; i < 100; ++i) {
    vec.resize(i);
    CHECK_EQUAL((size_t)i, vec.size());
    vec[i - 1] = i - 1;
    for (int j = 0; j < i; ++j) {
      CHECK_EQUAL(j, vec[j]);
    }
  }
}

TEST(Vector, ResizeDefaultElements) {
  int default_value;
  _new<int>(&default_value);
  std::vector<int> vec;
  vec.resize(10);
  for (int i = 0; i < 10; ++i) {
    CHECK_EQUAL(default_value, vec[i]);
  }
}

TEST(String, AssignByStringLiteral) {
  std::string str;
  str = "Hello world";
}

TEST(String, ConstructByStringLiteral) {
  std::string str = "Hello world";
}

TEST(String, StringSize) {
  std::string str;
  CHECK_EQUAL(0, str.size());
  for (int i = 1; i < 10; ++i) {
    str.append('0' + i);
    CHECK_EQUAL((size_t)i, str.size());
  }
}

TEST(String, ToStringZero) {
  std::string str = std::to_string(0);
  CHECK_EQUAL(1, str.size());
  CHECK_EQUAL('0', str[0]);
}

TEST(String, ToStringSingleDigit) {
  std::string str;
  for (int i = 0; i < 10; ++i) {
    str = std::to_string(i);
    CHECK_EQUAL(1, str.size());
  }
}

TEST(String, ToStringSingleDigitNegative) {
  std::string str;
  for (int i = -9; i < 0; ++i) {
    str = std::to_string(i);
    CHECK_EQUAL(2, str.size());
  }
}

TEST_GROUP(UnorderedMap) {
};

TEST(UnorderedMap, EmptyMapCount) {
  std::unordered_map<int, int> map;
  CHECK_EQUAL(0, map.count(1));
}

TEST(UnorderedMap, InsertAndCount) {
  std::unordered_map<int, int> map;
  map.insert(std::make_pair(1, 42));
  CHECK_EQUAL(1, map.count(1));
  CHECK_EQUAL(0, map.count(2));
}

TEST(UnorderedMap, AtReturnsCorrectValue) {
  std::unordered_map<int, int> map;
  map.insert(std::make_pair(1, 42));
  CHECK_EQUAL(42, map.at(1));
}

TEST(UnorderedMap, InsertDoesNotOverwrite) {
  std::unordered_map<int, int> map;
  map.insert(std::make_pair(1, 42));
  map.insert(std::make_pair(1, 99));
  CHECK_EQUAL(42, map.at(1));
}

TEST(UnorderedMap, AtReturnsReference) {
  std::unordered_map<int, int> map;
  map.insert(std::make_pair(1, 42));
  map.at(1) = 100;
  CHECK_EQUAL(100, map.at(1));
}

TEST(UnorderedMap, LargerDataTrivial) {
  std::unordered_map<int, int> map;
  for (int i = 0; i < 100; ++i) {
    int k = i;
    int v = i * 2;
    map.insert(std::make_pair(k, v));
  }
  for (int i = 0; i < 100; ++i) {
    int k = i;
    int v = i * 2;
    CHECK_EQUAL(v, map.at(k));
  }
}

TEST(UnorderedMap, LargerDataEvenly) {
  std::unordered_map<int, int> map;
  int k = 1;
  int v = 1;
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

TEST(UnorderedMap, LargerDataUnevenly) {
  std::unordered_map<int, int> map;
  for (int i = 0; i < 250; ++i) {
    int k = i * i * i * i;
    int v = i * 2;
    map.insert(std::make_pair(k, v));
  }
  for (int i = 0; i < 250; ++i) {
    int k = i * i * i * i;
    int v = i * 2;
    CHECK_EQUAL(v, map.at(k));
    CHECK_EQUAL(1, map.count(k));
    CHECK_EQUAL(0, map.count(k + 2));
  }
}

int main(int ac, char** av) {
  return CommandLineTestRunner::RunAllTests(ac, av);
}
