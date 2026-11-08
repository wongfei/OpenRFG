// Original: volition/vlib/code/util/priority_queue_linear.h
// Implementation: volition/vlib/code/util/priority_queue_linear.cpp
//
// priority_queue_linear, a priority queue kept in a linear array.
#pragma once


class mempool;

// size 0x18
class priority_queue_linear
{
public:
    enum compare_results
    {
        HIGHER_THAN = 0,
        EQUAL = 1,
        LOWER_THAN = 2,
    };

    typedef unsigned int handle;
    typedef compare_results (*priority_item_compare_function)(const void*, const void*);

    // size 0xC
    struct priority_item
    {
        void* m_item_data; // 0x00
        priority_item* next; // 0x04
        priority_item* prev; // 0x08
    };

    priority_queue_linear(); // 0x0058FF00 (:87)
    void initialize(mempool* mempool_to_use, unsigned int max_items, compare_results (*compare_function)(const void*, const void*)); // 0x005B1B30 (:99)
    void clear(); // 0x005B1B70 (:119)
    unsigned int add(void* item_data); // 0x0058FF20 (:133)
    void* remove(unsigned int h_item); // 0x0058FFC0 (:178)
    void* remove_highest(); // 0x00590050 (:228)
    bool is_empty() const;
private:
    void initialize_lists(); // 0x005900C0 (:285)
    unsigned int pointer_to_handle(const priority_item* p_item) const; // 0x0058FEE0 (.h:118)

    unsigned int m_num_items; // 0x00
    unsigned int m_max_items; // 0x04
    priority_item* m_priority_items; // 0x08
    priority_item* m_in_use_list; // 0x0C
    priority_item* m_free_list; // 0x10
    compare_results (*m_compare_function)(const void*, const void*); // 0x14
};
