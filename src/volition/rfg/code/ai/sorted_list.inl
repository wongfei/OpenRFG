// Original: volition/rfg/code/ai/sorted_list.inl (multiplayer.md MP8: the bullet hit lists of weapons.cpp)
//
// The elements live in two circular doubly linked lists, the free list and the in-use list (kept sorted by val). Ported
// from the <weapon_fire_info,float> and <human const*,float> instances; the other instances compile from the same code.
#pragma once

// 0x00A22CF0 (<weapon_fire_info,float>): only the fire info's hit handle and weapon info are set.
template<typename T, typename K>
inline sorted_list_element<T, K>::sorted_list_element()
{
}

// 0x00A244B0 (sorted_list.inl:16, <weapon_fire_info,float>)
// <turret*,int> 0x00A261F0
template<typename T, typename K>
sorted_list_base<T, K>::sorted_list_base(bool sort_ascending_, sorted_list_element<T, K>* buffer, int num)
    : sort_ascending(sort_ascending_)
    , num_elements(0)
    , free_list(nullptr)
    , in_use_list(nullptr)
{
    for (; num > 0; num--, buffer++)
    {
        if (free_list == nullptr)
        {
            buffer->next = buffer;
            buffer->prev = buffer;
            free_list = buffer;
        }
        else
        {
            buffer->prev = free_list->prev;
            buffer->next = free_list;
            free_list->prev->next = buffer;
            free_list->prev = buffer;
        }
    }
}

// 0x00BDB080 (.h:38)
template<typename T, typename K>
int sorted_list_base<T, K>::size() const
{
    return num_elements;
}

// 0x00A21E20 (.h:41)
template<typename T, typename K>
const T* sorted_list_base<T, K>::get_first() const
{
    return in_use_list != nullptr ? &in_use_list->data : nullptr;
}

// 0x00BDB090 (.h:44): the data is the element's first member.
template<typename T, typename K>
const T* sorted_list_base<T, K>::get_next(const T* prev) const
{
    if (prev == nullptr)
    {
        return nullptr;
    }
    sorted_list_element<T, K>* next = ((const sorted_list_element<T, K>*)prev)->next;
    return next != in_use_list ? &next->data : nullptr;
}

// 0x00BDB0B0 (.h:52)
template<typename T, typename K>
const T* sorted_list_base<T, K>::get_last() const
{
    return in_use_list != nullptr ? &in_use_list->prev->data : nullptr;
}

// 0x00BDB280 (sorted_list.inl:49, <weapon_fire_info,float>)
// <turret*,int> 0x00A26260
// Inserts before the first element the new value does not sort after. A full list reuses its last element: its data
// is overwritten and it moves to the insertion point (the count stays); a value that sorts after every element of a
// full list is dropped (false).
template<typename T, typename K>
bool sorted_list_base<T, K>::add(const T& data, K val)
{
    sorted_list_element<T, K>* first = in_use_list;
    if (first != nullptr)
    {
        sorted_list_element<T, K>* cur = first;
        do
        {
            const bool after = sort_ascending ? cur->val < val : val < cur->val;
            if (!after)
            {
                sorted_list_element<T, K>* elem = free_list;
                if (elem == nullptr)
                {
                    elem = first->prev;
                    elem->data = data;
                    elem->val = val;
                    if (elem == cur)
                    {
                        return true;
                    }
                    if (in_use_list == elem)
                    {
                        if (elem->next == elem)
                        {
                            elem->next = nullptr;
                            elem->prev = nullptr;
                            in_use_list = nullptr;
                            goto insert_reused;
                        }
                        in_use_list = elem->next;
                    }
                    if (elem->prev != nullptr)
                    {
                        elem->prev->next = elem->next;
                    }
                    if (elem->next != nullptr)
                    {
                        elem->next->prev = elem->prev;
                    }
                    elem->next = nullptr;
                    elem->prev = nullptr;
                insert_reused:
                    cur->prev->next = elem;
                    elem->prev = cur->prev;
                    cur->prev = elem;
                    elem->next = cur;
                    if (in_use_list == cur)
                    {
                        in_use_list = elem;
                    }
                    return true;
                }
                elem->data = data;
                elem->val = val;
                if (free_list == elem)
                {
                    if (elem->next == elem)
                    {
                        elem->next = nullptr;
                        elem->prev = nullptr;
                        free_list = nullptr;
                        goto insert_free;
                    }
                    free_list = elem->next;
                }
                if (elem->prev != nullptr)
                {
                    elem->prev->next = elem->next;
                }
                if (elem->next != nullptr)
                {
                    elem->next->prev = elem->prev;
                }
                elem->next = nullptr;
                elem->prev = nullptr;
            insert_free:
                cur->prev->next = elem;
                elem->prev = cur->prev;
                cur->prev = elem;
                elem->next = cur;
                if (in_use_list == cur)
                {
                    in_use_list = elem;
                }
                num_elements++;
                return true;
            }
            cur = cur->next;
        } while (cur != first && cur != nullptr);
    }
    sorted_list_element<T, K>* elem = free_list;
    if (elem == nullptr)
    {
        return false;
    }
    elem->data = data;
    elem->val = val;
    if (free_list == elem)
    {
        if (elem->next == elem)
        {
            elem->next = nullptr;
            elem->prev = nullptr;
            free_list = nullptr;
            goto append;
        }
        free_list = elem->next;
    }
    if (elem->prev != nullptr)
    {
        elem->prev->next = elem->next;
    }
    if (elem->next != nullptr)
    {
        elem->next->prev = elem->prev;
    }
    elem->next = nullptr;
    elem->prev = nullptr;
append:
    if (in_use_list != nullptr)
    {
        elem->prev = in_use_list->prev;
        elem->next = in_use_list;
        in_use_list->prev->next = elem;
        in_use_list->prev = elem;
    }
    else
    {
        elem->next = elem;
        elem->prev = elem;
        in_use_list = elem;
    }
    num_elements++;
    return true;
}

// 0x00A462C0 (.h:102, <weapon_fire_info,50,float>): the elements are constructed after the base links them.
template<typename T, int N, typename K>
sorted_list<T, N, K>::sorted_list(bool sort_ascending_)
    : sorted_list_base<T, K>(sort_ascending_, elements, N)
{
}

// 0x00648EE0 (.h:89, <human const*,float>)
template<typename T, typename K>
sorted_list_base_with_identity<T, K>::sorted_list_base_with_identity(bool sort_ascending_, sorted_list_element<T, K>* buffer, int num)
    : sorted_list_base<T, K>(sort_ascending_, buffer, num)
{
}

// 0x005FA790 (sorted_list.inl:98, <human const*,float>)
template<typename T, typename K>
sorted_list_element<T, K>* sorted_list_base_with_identity<T, K>::find(const T& data) const
{
    sorted_list_element<T, K>* first = this->in_use_list;
    if (first != nullptr)
    {
        sorted_list_element<T, K>* cur = first;
        do
        {
            if (cur->data == data)
            {
                return cur;
            }
            cur = cur->next;
        } while (cur != first && cur != nullptr);
    }
    return nullptr;
}

// 0x00649CA0 (sorted_list.inl:124, <human const*,float>): an element already in the list goes back on the free list and is
// added again with the new value, when the replace mode lets it.
template<typename T, typename K>
bool sorted_list_base_with_identity<T, K>::add_unique(const T& data, K val, add_unique_replace_modes replace_mode)
{
    sorted_list_element<T, K>* found = find(data);
    if (found != nullptr)
    {
        switch (replace_mode)
        {
        case NEVER_REPLACE:
            return false;
        case ALWAYS_REPLACE:
            if (val == found->val)
            {
                return true;
            }
            break;
        case REPLACE_IF_LESS_THAN:
            if (found->val <= val)
            {
                return false;
            }
            break;
        case REPLACE_IF_GREATER_THAN:
            if (val <= found->val)
            {
                return false;
            }
            break;
        default:
            break;
        }
        // Original bug kept: the count is not lowered here, and add raises it again, so a replaced element counts twice.
        if (this->in_use_list == found)
        {
            if (found->next == found)
            {
                found->next = nullptr;
                found->prev = nullptr;
                this->in_use_list = nullptr;
                goto unlinked;
            }
            this->in_use_list = found->next;
        }
        if (found->prev != nullptr)
        {
            found->prev->next = found->next;
        }
        if (found->next != nullptr)
        {
            found->next->prev = found->prev;
        }
        found->next = nullptr;
        found->prev = nullptr;
    unlinked:
        if (this->free_list == nullptr)
        {
            found->next = found;
            found->prev = found;
            this->free_list = found;
        }
        else
        {
            found->prev = this->free_list->prev;
            found->next = this->free_list;
            this->free_list->prev->next = found;
            this->free_list->prev = found;
        }
    }
    return this->add(data, val);
}

// 0x00671980 (.h:112, <human const*,3,float>)
template<typename T, int N, typename K>
sorted_list_with_identity<T, N, K>::sorted_list_with_identity(bool sort_ascending_)
    : sorted_list_base_with_identity<T, K>(sort_ascending_, elements, N)
{
}
