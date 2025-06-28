#ifndef STL_MOCK_H
#define STL_MOCK_H

#include "stdlib.h"
#include "stdint.h"
#include "string.h"
#include "stl_compat.h"

#ifndef VECTOR_DEFAULT_CAPACITY
#define VECTOR_DEFAULT_CAPACITY 8
#endif

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
        _capacity = VECTOR_DEFAULT_CAPACITY;
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
    size_t size() const {
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
    void resize(size_t new_size) {
        if (new_size < _size) {
            // Destroy elements past new_size
            for (size_t i = new_size; i < _size; ++i) {
                _storage[i].~T();
            }
            _size = new_size;
        } else if (new_size > _size) {
            _ensure_capacity(new_size);
            for (size_t i = _size; i < new_size; ++i) {
                _new<T>(_storage + i);
            }
            _size = new_size;
        }
    }
};

// <string>

struct string {
    vector<char> _data;
    size_t _length;
    string();
    string(const string&);
    string(string&&);
    string(const char*);
    char* c_str();
    void append(char);
    string& operator=(const string& other);
    string& operator=(const char* other);
    size_t size() const;
    char& operator[](size_t pos);
};

string to_string(int64_t);
string to_string(uint64_t);

string to_string(int32_t);
string to_string(int16_t);
string to_string(int8_t);

string to_string(uint32_t);
string to_string(uint16_t);
string to_string(uint8_t);

string to_string(double);

string to_string(float);

template <typename A, typename B> struct pair {
    A first;
    B second;
};

template <typename A, typename B> pair<A, B> make_pair(A first, B second) {
    return {first, second};
};

// <unordered_map> mock

enum class EntryState {
    EMPTY,
    OCCUPIED,
    DELETED
};

// BasicHasher: Deviation from std::unordered_map which uses std::hash.
// User must provide specializations for custom types if default cast is not suitable.
template <typename K>
struct BasicHasher {
    size_t operator()(const K& key) const {
        return static_cast<size_t>(key);
    }
};

template <>
struct BasicHasher<void*> {
    size_t operator()(void* key) const {
        return reinterpret_cast<size_t>(key);
    }
};

template <typename K, typename V, typename Hasher = BasicHasher<K>>
struct unordered_map {
private:
    struct Entry {
        K key;
        V value;
        EntryState state;

        Entry() : state(EntryState::EMPTY) {}
        Entry(const K& k, const V& v) : key(k), value(v), state(EntryState::OCCUPIED) {}
    };

    vector<Entry> buckets;
    size_t num_elements;
    size_t num_buckets;
    const float max_load_factor = 0.75f;

    size_t _find_slot(const K& key) const {
        if (num_elements == 0) {
            return num_buckets;
        }

        Hasher hasher;
        size_t index = hasher(key) % num_buckets;
        size_t start_index = index;

        while (buckets[index].state != EntryState::EMPTY) {
            if (buckets[index].state == EntryState::OCCUPIED && buckets[index].key == key) {
                return index;
            }
            index = (index + 1) % num_buckets;
            if (index == start_index) {
                break;
            }
        }
        return num_buckets;
    }

    size_t _find_insert_slot(const K& key) const {
        Hasher hasher;
        size_t index = hasher(key) % num_buckets;
        size_t start_index = index;
        size_t first_deleted_slot = num_buckets;

        while (buckets[index].state != EntryState::EMPTY) {
            if (buckets[index].state == EntryState::OCCUPIED) {
                if (buckets[index].key == key) {
                    return index;
                }
            } else { // EntryState::DELETED
                if (first_deleted_slot == num_buckets) {
                    first_deleted_slot = index;
                }
            }
            index = (index + 1) % num_buckets;
            if (index == start_index) {
                return num_buckets;
            }
        }
        return (first_deleted_slot != num_buckets) ? first_deleted_slot : index;
    }

    void _rehash(size_t new_bucket_count) {
        vector<Entry> old_buckets = buckets;

        buckets.clear();
        buckets.resize(new_bucket_count);
        for (size_t i = 0; i < new_bucket_count; ++i) {
            buckets[i].state = EntryState::EMPTY;
        }

        num_buckets = new_bucket_count;
        num_elements = 0;

        for (size_t i = 0; i < old_buckets.size(); ++i) {
            if (old_buckets[i].state == EntryState::OCCUPIED) {
                pair<K,V> data_to_insert;
                data_to_insert.first = old_buckets[i].key;
                data_to_insert.second = old_buckets[i].value;
                insert(data_to_insert);
            }
        }
    }

    void _check_and_rehash() {
        if (num_buckets == 0 || static_cast<float>(num_elements + 1) / num_buckets > max_load_factor) {
            size_t new_size = (num_buckets == 0) ? 16 : num_buckets * 2;
            _rehash(new_size);
        }
    }

public:
    unordered_map() : num_elements(0), num_buckets(0) {
        _check_and_rehash();
    }

    // Compliant with std::unordered_map::count.
    size_t count(const K& key) const {
        return (_find_slot(key) != num_buckets) ? 1 : 0;
    }

    // Compliant with std::unordered_map::insert return type.
    pair<bool, V&> insert(const pair<K, V>& data) {
        _check_and_rehash();

        size_t slot_index = _find_insert_slot(data.first);

        if (buckets[slot_index].state == EntryState::OCCUPIED && buckets[slot_index].key == data.first) {
            return {false, buckets[slot_index].value};
        } else {
            buckets[slot_index].key = data.first;
            buckets[slot_index].value = data.second;
            buckets[slot_index].state = EntryState::OCCUPIED;
            num_elements++;
            return {true, buckets[slot_index].value};
        }
    }

    // Deviation from std::unordered_map::at: Does not throw std::out_of_range.
    // Returns a reference to a static dummy value if key is not found.
    V& at(const K& key) {
        size_t index = _find_slot(key);
        if (index != num_buckets) {
            return buckets[index].value;
        }
        static V dummy_value_for_not_found;
        return dummy_value_for_not_found;
    }
};

}

// additional functions that have to be outside the std namespace

std::string operator+(const std::string& lhs, const std::string& rhs);

#endif