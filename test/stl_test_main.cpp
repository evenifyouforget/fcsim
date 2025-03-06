#include "stl_mock.h"
#include <cassert>

typedef unsigned int GLuint;


template <typename T> void append_vector(std::vector<T>& a, std::vector<T>& b) {
    for(auto it = b.begin(); it != b.end(); ++it) {
        a.push_back(*it);
    }
}

struct color {
    float a,r,g,b;
};

struct ui_button_id {
    int group, index;
    bool operator==(const ui_button_id& other) {
        return group == other.group && index == other.index;
    }
};

struct ui_button_text {
    std::string text;
    float scale = 2;
    float relative_x = 0, relative_y = 0, align_x = 0.5f, align_y = 0.5f;
};

struct ui_button_single {
    ui_button_id id = {0, 0};
    float x = 0, y = 0, w = 0, h = 0;
    int z_offset = 0;
    bool enabled = true, clickable = true, highlighted = false;
    std::vector<ui_button_text> texts;
};

struct ui_button_collection {
    std::vector<ui_button_single> buttons;
};

struct block_graphics_layer {
    std::vector<uint16_t> indices;
    std::vector<float> coords;
    std::vector<float> colors;
    uint16_t push_vertex(float x, float y, float r, float g, float b);
    uint16_t push_vertex(float x, float y, color col);
    void push_triangle(uint16_t v1, uint16_t v2, uint16_t v3);
};

struct block_graphics {
    // flags
    bool simple_graphics = false;
    bool wireframe = false;
    // working values
    std::vector<block_graphics_layer> layers;
    // final values
    std::vector<uint16_t> _indices;
    std::vector<float> _coords;
    std::vector<float> _colors;
	GLuint _index_buffer;
	GLuint _coord_buffer;
	GLuint _color_buffer;
	int _triangle_cnt;
	int _vertex_cnt;
    // methods
    void clear();
    void ensure_layer(int z_offset);
    uint16_t push_vertex(float x, float y, color col, int z_offset);
    void push_triangle(uint16_t v1, uint16_t v2, uint16_t v3, int z_offset);
    void push_all_layers();
};


uint16_t block_graphics_layer::push_vertex(float x, float y, float r, float g, float b) {
    uint16_t offset = coords.size() / 2;
    coords.push_back(x);
    coords.push_back(y);
    colors.push_back(r);
    colors.push_back(g);
    colors.push_back(b);
    return offset;
}

uint16_t block_graphics_layer::push_vertex(float x, float y, color col) {
    return push_vertex(x, y, col.r, col.g, col.b);
}

void block_graphics::ensure_layer(int z_offset) {
    while(layers.size() <= z_offset) {
        layers.emplace_back();
    }
}

uint16_t block_graphics::push_vertex(float x, float y, color col, int z_offset) {
    ensure_layer(z_offset);
    return layers[z_offset].push_vertex(x, y, col);
}

void block_graphics_layer::push_triangle(uint16_t v1, uint16_t v2, uint16_t v3) {
    indices.push_back(v1);
    indices.push_back(v2);
    indices.push_back(v3);
}

void block_graphics::push_triangle(uint16_t v1, uint16_t v2, uint16_t v3, int z_offset) {
    ensure_layer(z_offset);
    return layers[z_offset].push_triangle(v1, v2, v3);
}

void block_graphics::push_all_layers() {
    _indices.clear();
    _coords.clear();
    _colors.clear();
    for(auto it = layers.begin(); it != layers.end(); ++it) {
        uint16_t offset = _coords.size() / 2;
        for(auto jt = it->indices.begin(); jt != it->indices.end(); ++jt) {
            _indices.push_back(offset + *jt);
        }
        append_vector(_coords, it->coords);
        append_vector(_colors, it->colors);
    }
    _triangle_cnt = _indices.size() / 3;
    _vertex_cnt = _coords.size() / 2;
}

void block_graphics::clear() {
    layers.clear();
}

int main() {
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
        assert(vec.size() == i);
    }
    // test vector index
    for(int i = 0; i < 100; ++i) {
        std::vector<int> vec;
        for(int j = 0; j < i; ++j) {
            vec.push_back(j);
            assert(vec[j] == j);
        }
        assert(vec.size() == i);
    }
    // test vector clear
    for(int i = 0; i < 100; ++i) {
        std::vector<int> vec;
        for(int j = 0; j < i; ++j) {
            vec.push_back(j);
            assert(vec[j] == j);
        }
        assert(vec.size() == i);
        vec.clear();
        for(int j = 0; j < i; ++j) {
            vec.push_back(2 * j);
            assert(vec[j] == 2 * j);
        }
        assert(vec.size() == i);
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
            assert(str.size() == i);
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
    // vector/string stress test
    {
        std::vector<ui_button_collection> vec;
        std::vector<block_graphics> bec;
        for(int k = 0; k < 10; ++k) {
            for(int i = 0; i < 10; ++i) {
                vec.push_back(ui_button_collection());
                vec[i].buttons.push_back(ui_button_single());
                vec[i].buttons[0].texts.push_back(ui_button_text{std::to_string(k+i*993245)});
                bec.push_back(block_graphics());
                bec[i].push_vertex(0,0,color(),2);
                bec[i].push_vertex(1,0,color(),2);
                bec[i].push_vertex(0,1,color(),2);
                bec[i].push_triangle(0,1,2,2);
                bec[i].push_all_layers();
            }
            for(int n = 0; n < 100; ++n) {
                for(int i = 0; i < 9; ++i) {
                    if(i % 2) {
                        vec[i].buttons[0].texts.clear();
                        vec[i].buttons.clear();
                    }
                    vec[i] = vec[i+1];
                    if(i % 3) {
                        bec[i].clear();
                    }
                    bec[i] = bec[i+1];
                }
                for(int i = 0; i < 8; ++i) {
                    if(n % 3)vec[i] = vec[i+2];
                    if(n % 2)bec[i] = bec[i + 2];
                }
                for(int i = 0; i < 3; ++i) {
                    vec[9].buttons.push_back(ui_button_single());
                    vec[9].buttons[0].texts.push_back(ui_button_text{std::to_string(k+n*987165+i*89871273)});
                    bec[9].push_vertex(0,0,color(),2);
                    bec[9].push_vertex(1,0,color(),2);
                    bec[9].push_vertex(0,1,color(),2);
                    bec[9].push_triangle(0,1,2,2);
                    bec[9].push_all_layers();
                }
            }
            if(k%3==0)vec.clear();
            if(k%2==0)bec.clear();
        }
    }
}