#ifdef __wasm__
#include "stl_mock.h"
#else
#include <unordered_map>
#endif
#include "stl_compat.h"
extern "C" {
#include "graph.h"
}

#define CLEAN_COPY(dest, src, attribute_name) (dest).attribute_name = _clean_copy(pointer_map, (src).attribute_name);
#define CHECK_RETURN(ptr_type, old_obj, new_obj) if(old_obj==nullptr)return nullptr;if(pointer_map.count(old_obj)){return (ptr_type*)pointer_map.at(old_obj);}ptr_type* new_obj = _new<ptr_type>();pointer_map.insert(std::make_pair<void*, void*>(old_obj, new_obj));

attach_node* _clean_copy(std::unordered_map<void*, void*> pointer_map, attach_node* old_obj);
block* _clean_copy(std::unordered_map<void*, void*> pointer_map, block* old_obj);
joint* _clean_copy(std::unordered_map<void*, void*> pointer_map, joint* old_obj);
design* _clean_copy(std::unordered_map<void*, void*> pointer_map, design* old_obj);

attach_node* _clean_copy(std::unordered_map<void*, void*> pointer_map, attach_node* old_obj) {
    CHECK_RETURN(attach_node, old_obj, new_obj);
    CLEAN_COPY(*new_obj, *old_obj, prev);
    CLEAN_COPY(*new_obj, *old_obj, next);
    CLEAN_COPY(*new_obj, *old_obj, block);
    return new_obj;
}

block* _clean_copy(std::unordered_map<void*, void*> pointer_map, block* old_obj) {
    CHECK_RETURN(block, old_obj, new_obj);
    CLEAN_COPY(*new_obj, *old_obj, prev);
    CLEAN_COPY(*new_obj, *old_obj, next);
    new_obj->shape = old_obj->shape;
    new_obj->material = old_obj->material; // intentionally not deep copying
    new_obj->goal = old_obj->goal;
    new_obj->overlap = old_obj->overlap;
    new_obj->visited = old_obj->visited;
    new_obj->id = old_obj->id;
    new_obj->type_id = old_obj->type_id;
    new_obj->body = nullptr; // intentionally clearing
    return new_obj;
}

joint* _clean_copy(std::unordered_map<void*, void*> pointer_map, joint* old_obj) {
    CHECK_RETURN(joint, old_obj, new_obj);
    CLEAN_COPY(*new_obj, *old_obj, prev);
    CLEAN_COPY(*new_obj, *old_obj, next);
    CLEAN_COPY(*new_obj, *old_obj, gen);
    new_obj->x = old_obj->x;
    new_obj->y = old_obj->y;
    CLEAN_COPY(*new_obj, *old_obj, att.head);
    CLEAN_COPY(*new_obj, *old_obj, att.tail);
    new_obj->visited = old_obj->visited;
    return new_obj;
}

design* _clean_copy(std::unordered_map<void*, void*> pointer_map, design* old_obj) {
    CHECK_RETURN(design, old_obj, new_obj);
    CLEAN_COPY(*new_obj, *old_obj, joints.head);
    CLEAN_COPY(*new_obj, *old_obj, joints.tail);
    CLEAN_COPY(*new_obj, *old_obj, level_blocks.head);
    CLEAN_COPY(*new_obj, *old_obj, level_blocks.tail);
    CLEAN_COPY(*new_obj, *old_obj, player_blocks.head);
    CLEAN_COPY(*new_obj, *old_obj, player_blocks.tail);
    new_obj->build_area = old_obj->build_area;
    new_obj->goal_area = old_obj->goal_area;
    new_obj->level_id = old_obj->level_id;
    new_obj->modcount = old_obj->modcount;
    return new_obj;
}

extern "C" design* clean_copy_design(design* old_design) {
    std::unordered_map<void*, void*> pointer_map;
    return _clean_copy(pointer_map, old_design);
}