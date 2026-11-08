// Original: volition/rfg/code/util/generic_heap.h
// Implementation: volition/rfg/code/util/generic_heap.cpp
//
// generic_heap, a binary min-heap of heap_item (an item with a cost and its heap index) allocated from a mempool for one
// session; used by path searches. Placement of heap_item inferred.
#pragma once


class mempool;

// size 0xC
struct heap_item
{
    float cost; // 0x00
    unsigned char* item; // 0x04
    int heap_index; // 0x08
};

// size 0x18
class generic_heap
{
    void heapify(int index); // 0x00B620C0 (:39)
    void percolate(int index); // 0x00B621A0 (:111)
    void validate(int index, heap_item* parent); // 0x00B61210 (:158)
public:
    generic_heap(); // 0x00B61280 (:193)
    ~generic_heap(); // 0x00B62210 (:200)
    void start_session(mempool* p, int max_items); // 0x00B61290 (:210)
    void end_session(); // 0x00B612E0 (:227)
    void insert_item(heap_item* hi); // 0x00B62230 (:240)
    void adjust_item(heap_item* hi); // 0x00B62260 (:257)
    heap_item* remove_best_item(); // 0x00B62270 (:267)
    bool is_empty();

private:
    heap_item** items; // 0x00
    int num_items; // 0x04
    mempool* pool; // 0x08
    unsigned int mark; // 0x0C
    bool in_session; // 0x10
    unsigned int session_id; // 0x14
};
