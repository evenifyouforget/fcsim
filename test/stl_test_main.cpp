#include "stl_mock.h"
#include <cassert>

int main() {
    // test new default value
    {
        int default_value;
        _new<int>(&default_value);
        assert(default_value == 0);
    }
    // test vector construct destruct
    {
        std::vector<int> vec;
    }
    // test vector size
    {
        std::vector<int> vec;
        assert(vec.size() == 0);
    }
    // test vector push_back
    for(int i = 0; i < 100; ++i) {
        std::vector<int> vec;
        for(int j = 0; j < i; ++j) {
            vec.push_back(j);
        }
        assert(vec.size() == (size_t)i);
    }
    // test vector index
    for(int i = 0; i < 100; ++i) {
        std::vector<int> vec;
        for(int j = 0; j < i; ++j) {
            vec.push_back(j);
            assert(vec[j] == j);
        }
        assert(vec.size() == (size_t)i);
    }
    // test vector clear
    for(int i = 0; i < 100; ++i) {
        std::vector<int> vec;
        for(int j = 0; j < i; ++j) {
            vec.push_back(j);
            assert(vec[j] == j);
        }
        assert(vec.size() == (size_t)i);
        vec.clear();
        for(int j = 0; j < i; ++j) {
            vec.push_back(2 * j);
            assert(vec[j] == 2 * j);
        }
        assert(vec.size() == (size_t)i);
    }
    // test string construct destruct
    {
        std::string str;
    }
    // test string to_string
    {
        std::string str = std::to_string(12345);
    }
    // test string to_string data
    {
        std::string str = std::to_string(12345);
        for(int i = 0; i < 5; ++i) {
            assert(str._data[i] == '0' + i + 1);
        }
        assert(str._length == 5);
    }
    // test string to_string negative
    {
        std::string str = std::to_string(-12345);
        assert(str._data[0] == '-');
        for(int i = 0; i < 5; ++i) {
            assert(str._data[i+1] == '0' + i + 1);
        }
        assert(str._length == 6);
    }
    // test string to_string large value
    {
        std::to_string(9223372036854000000);
        std::to_string(-9223372036854000000);
    }
    // test 2d memory
    for(int i = 0; i < 100; ++i) {
        std::vector<std::string> vec;
        for(int j = 0; j < i; ++j) {
            vec.push_back(std::to_string(j));
        }
        vec.clear();
        for(int j = 0; j < i; ++j) {
            vec.push_back(std::to_string(2 * j));
        }
    }
    // test 2d memory overwrite
    for(int i = 0; i < 100; ++i) {
        std::vector<std::string> vec;
        for(int j = 0; j < i; ++j) {
            vec.push_back(std::to_string(j));
        }
        for(int j = 0; j < i; ++j) {
            vec[j] = std::to_string(2 * j);
        }
    }
    // test vector copy
    for(int i = 0; i < 100; ++i) {
        std::vector<std::vector<int>> vec;
        vec.push_back(std::vector<int>());
        for(int j = 0; j < 100; ++j) {
            vec[0].push_back(j);
        }
        for(int j = 0; j < i; ++j) {
            vec.push_back(vec[j]);
            vec[j][0] = 2 * j;
        }
        for(int j = 0; j < i; ++j) {
            assert(vec[j][0] == 2 * j);
        }
        assert(vec[i][0] == 0);
    }
    // test vector explicit copy
    for(int i = 0; i < 100; ++i) {
        std::vector<std::vector<int>> vec;
        vec.push_back(std::vector<int>());
        for(int j = 0; j < 100; ++j) {
            vec[0].push_back(j);
        }
        for(int j = 0; j < i; ++j) {
            vec.push_back(std::vector<int>());
            vec[j+1] = vec[j];
            vec[j][0] = 2 * j;
        }
        for(int j = 0; j < i; ++j) {
            assert(vec[j][0] == 2 * j);
        }
        assert(vec[i][0] == 0);
    }
    // test vector resize size (both up and down)
    {
        std::vector<int> vec;
        for(int i = 0; i < 100; ++i) {
            vec.resize(i);
            assert(vec.size() == (size_t)i);
        }
        for(int i = 100; i >= 0; --i) {
            vec.resize(i);
            assert(vec.size() == (size_t)i);
        }
    }
    // test vector resize keeps data
    {
        std::vector<int> vec;
        for(int i = 1; i < 100; ++i) {
            vec.resize(i);
            assert(vec.size() == (size_t)i);
            vec[i-1] = i - 1;
            for(int j = 0; j < i; ++j) {
                assert(vec[j] == j);
            }
        }
    }
    // test vector resize default elements
    {
        int default_value;
        _new<int>(&default_value);
        std::vector<int> vec;
        vec.resize(10);
        for(int i = 0; i < 10; ++i) {
            assert(vec[i] == default_value);
        }
    }
    // test string assign by string literal
    {
        std::string str;
        str = "Hello world";
    }
    // test string construct by string literal
    {
        std::string str = "Hello world";
    }
    // test string size
    {
        std::string str;
        assert(str.size() == 0);
        for(int i = 1; i < 10; ++i) {
            str.append('0' + i);
            assert(str.size() == (size_t)i);
        }
    }
    // test to_string 0
    {
        std::string str = std::to_string(0);
        assert(str.size() == 1);
        assert(str[0] == '0');
    }
    // test string size with to_string single digit
    {
        std::string str;
        for(int i = 0; i < 10; ++i) {
            str = std::to_string(i);
            assert(str.size() == 1);
        }
    }
    // test string size with to_string single digit negative
    {
        std::string str;
        for(int i = -9; i < 0; ++i) {
            str = std::to_string(i);
            assert(str.size() == 2);
        }
    }
    // unordered_map: empty map, count
    {
        std::unordered_map<int, int> map;
        assert(map.count(1) == 0);
    }
    // unordered_map: insert and count
    {
        std::unordered_map<int, int> map;
        map.insert(std::make_pair(1, 42));
        assert(map.count(1) == 1);
        assert(map.count(2) == 0);
    }
    // unordered_map: at returns correct value
    {
        std::unordered_map<int, int> map;
        map.insert(std::make_pair(1, 42));
        assert(map.at(1) == 42);
    }
    // unordered_map: insert does not overwrite existing value
    {
        std::unordered_map<int, int> map;
        map.insert(std::make_pair(1, 42));
        map.insert(std::make_pair(1, 99));
        assert(map.at(1) == 42);
    }
    // unordered_map: at returns reference, assignment works
    {
        std::unordered_map<int, int> map;
        map.insert(std::make_pair(1, 42));
        map.at(1) = 100;
        assert(map.at(1) == 100);
    }
    // unordered_map: larger data (trivial)
    {
        std::unordered_map<int, int> map;
        for(int i = 0; i < 100; ++i) {
            int k = i;
            int v = i * 2;
            map.insert(std::make_pair(k, v));
        }
        for(int i = 0; i < 100; ++i) {
            int k = i;
            int v = i * 2;
            assert(map.at(k) == v);
        }
    }
    // unordered_map: larger data (non-trivial, evenly distributed)
    {
        std::unordered_map<int, int> map;
        int k = 1;
        int v = 1;
        for(int i = 0; i < 100; ++i) {
            k = k * 5 % 107;
            v = v * 3 % 113;
            map.insert(std::make_pair(k, v));
        }
        k = 1;
        v = 1;
        for(int i = 0; i < 100; ++i) {
            k = k * 5 % 107;
            v = v * 3 % 113;
            assert(map.at(k) == v);
        }
    }
    // unordered_map: larger data (non-trivial, unevenly distributed)
    {
        std::unordered_map<int, int> map;
        for(int i = 0; i < 250; ++i) {
            int k = i * i * i * i;
            int v = i * 2;
            map.insert(std::make_pair(k, v));
        }
        for(int i = 0; i < 250; ++i) {
            int k = i * i * i * i;
            int v = i * 2;
            assert(map.at(k) == v);
            assert(map.count(k) == 1);
            assert(map.count(k + 2) == 0); // impossible gap
        }
    }
}
