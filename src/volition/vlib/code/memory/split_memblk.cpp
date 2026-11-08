// Original: volition/vlib/code/memory/split_memblk.cpp
//
// Blocks of split_memmgr's large allocations. All blocks (free and used) form an address-ordered list (next/prev);
// free blocks are also in an unbalanced binary tree ordered by usable_size (left smaller, right larger or equal;
// top is the parent). `in_use` / `track` is non-zero for used blocks (the debug name of the allocation).
//
// Notes (decompiled behaviour, kept as is):
// - alloc keeps the whole block when less than 4 KB + 1 byte would be left over.
// - remove() of the root with both children returns the new root with its old parent still in `top`.
// - free() returns the merged previous block after returning `this` to the pool.

#include "volition/vlib/code/memory/split_memblk.h"

#include "volition/vlib/code/memory/object_pool.h"

// 0x00589AA0 (:7)
split_memblk::split_memblk()
{
    info.size = 0;
    info.usable_size = 0;
    info.requested_size = 0;
    prev = nullptr;
    next = nullptr;
    top = nullptr;
    left = nullptr;
    right = nullptr;
    track = nullptr;
    mem_base = nullptr;
    mem_aligned = nullptr;
}

// 0x00589AD0 (:30)
void split_memblk::init(unsigned int size, char* addr, split_memblk* parent)
{
    info.size = size;
    info.usable_size = size;
    info.requested_size = 0;
    next = nullptr;
    top = nullptr;
    left = nullptr;
    right = nullptr;
    track = nullptr;
    prev = parent;
    mem_base = addr;
    mem_aligned = addr;
}

// 0x00589B10 (:55)
split_memblk* split_memblk::can_alloc(unsigned int size, unsigned int align)
{
    // Walk right until a block fits (aligned start and size, or size + align), then prefer the best fit on the left.
    split_memblk* blk = this;
    while ((((unsigned int)blk->mem_base & (align - 1)) != 0 || blk->info.size < size) && blk->info.size < size + align)
    {
        blk = blk->right;
        if (blk == nullptr)
        {
            return nullptr;
        }
    }
    if (blk->left != nullptr)
    {
        split_memblk* const smaller = blk->left->can_alloc(size, align);
        if (smaller != nullptr && smaller->info.size < blk->info.size)
        {
            return smaller;
        }
    }
    return blk;
}

// 0x005BC890 (:91)
split_memblk* split_memblk::alloc(unsigned int size, split_memblk*& root, dynamic_object_pool<split_memblk,1>& pool)
{
    root = remove(root);
    const unsigned int rest = info.size - size;
    info.requested_size = size;
    in_use = 1;
    if (rest < 0x1001)
    {
        return nullptr;
    }

    split_memblk* const old_next = next;
    info.size = size;
    info.usable_size = size;
    split_memblk* const blk = pool.alloc(0);
    if (blk == nullptr)
    {
        return nullptr;
    }
    blk->info.size = rest;
    blk->info.usable_size = rest;
    blk->info.requested_size = 0;
    blk->prev = this;
    blk->top = nullptr;
    blk->left = nullptr;
    blk->right = nullptr;
    blk->track = nullptr;
    blk->mem_base = mem_base + info.size;
    blk->mem_aligned = mem_base + info.size;
    next = blk;
    blk->next = old_next;
    if (old_next != nullptr)
    {
        old_next->prev = blk;
    }
    root = blk->insert(root);
    return blk;
}

// 0x005C4320 (:139)
split_memblk* split_memblk::free(split_memblk*& root, dynamic_object_pool<split_memblk,1>& pool)
{
    // Merge with a free next block, then into a free previous block.
    split_memblk* following = next;
    if (following != nullptr && following->track == nullptr)
    {
        root = following->remove(root);
        following = next;
        info.size += following->info.size;
        info.usable_size += following->info.size;
        next = following->next;
        if (next != nullptr)
        {
            next->prev = this;
        }
        pool.free(following);
    }
    if (prev != nullptr && prev->track == nullptr)
    {
        root = prev->remove(root);
        prev->next = next;
        prev->info.size += info.size;
        prev->info.usable_size += info.size;
        if (next != nullptr)
        {
            next->prev = prev;
        }
        root = prev->insert(root);
        split_memblk* const merged = prev;
        pool.free(this);
        return merged;
    }
    info.requested_size = 0;
    track = nullptr;
    root = insert(root);
    return this;
}

// 0x00589B60 (:195)
split_memblk* split_memblk::insert(split_memblk* root)
{
    left = nullptr;
    right = nullptr;
    top = nullptr;
    if (root == nullptr)
    {
        return this;
    }
    for (split_memblk* node = root; node != nullptr;)
    {
        if (info.usable_size < node->info.usable_size)
        {
            if (node->left == nullptr)
            {
                node->left = this;
                top = node;
                return root;
            }
            node = node->left;
        }
        else
        {
            if (node->right == nullptr)
            {
                node->right = this;
                top = node;
                return root;
            }
            node = node->right;
        }
    }
    top = nullptr;
    return root;
}

// 0x00589BD0 (:234)
split_memblk* split_memblk::remove(split_memblk* root)
{
    if (root != this)
    {
        // Replace this node in its parent.
        split_memblk** const parent_link = top->right == this ? &top->right : &top->left;
        if (left == nullptr && right == nullptr)
        {
            *parent_link = nullptr;
            return root;
        }
        if (right == nullptr)
        {
            *parent_link = left;
            left->top = top;
            return root;
        }
        if (left == nullptr)
        {
            *parent_link = right;
            right->top = top;
            return root;
        }
        if (left->right == nullptr)
        {
            // The left child takes this place and adopts the right subtree.
            left->right = right;
            right->top = left;
            left->top = top;
            *parent_link = left;
            return root;
        }
        // The largest node of the left subtree takes this place; the left subtree hangs below its smallest end.
        split_memblk* largest = left->right;
        while (largest->right != nullptr)
        {
            largest = largest->right;
        }
        largest->top->right = nullptr;
        *parent_link = largest;
        largest->top = top;
        largest->right = right;
        right->top = largest;
        split_memblk* smallest = largest;
        while (smallest->left != nullptr)
        {
            smallest = smallest->left;
        }
        left->top = smallest;
        smallest->left = left;
        return root;
    }

    // Removing the root: return the new root.
    if (left == nullptr && right == nullptr)
    {
        return nullptr;
    }
    if (right == nullptr)
    {
        left->top = nullptr;
        return left;
    }
    if (left == nullptr)
    {
        right->top = nullptr;
        return right;
    }
    if (left->right == nullptr)
    {
        left->right = right;
        right->top = left;
        left->top = nullptr;
        return left;
    }
    if (right->left == nullptr)
    {
        right->left = left;
        left->top = right;
        right->top = nullptr;
        return right;
    }
    // The smallest node of the right subtree becomes the root (its `top` is left as it was).
    split_memblk* smallest = right->left;
    while (smallest->left != nullptr)
    {
        smallest = smallest->left;
    }
    smallest->top->left = nullptr;
    left->top = smallest;
    smallest->left = left;
    split_memblk* largest = smallest;
    while (largest->right != nullptr)
    {
        largest = largest->right;
    }
    largest->right = right;
    right->top = largest;
    return smallest;
}
