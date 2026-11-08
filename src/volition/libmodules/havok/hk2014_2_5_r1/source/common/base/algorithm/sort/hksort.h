// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h
//
// Hand-written (havok_impl.md HK1): the hkAlgorithm sorting templates (hksort.h and hksort.inl in the SDK; the PDB
// only has instances). Checked against the 2014.2.5 instances (quickSortRecursive<unsigned int> 0x00D46460,
// <hkFreeList::Element*> 0x00D12820, sortList<hkFreeList::Element> 0x00D124D0) and Havok 2014.1 hkSort.inl:
// - quickSortRecursive: Hoare partition around the middle element, recursion on the left part and a loop on the
//   right part.
// - sortList: bottom-up merge sort of a singly linked list (runs of 1, 2, 4, ... merged until one run is left);
//   equal elements keep their order.
// - heapSort, insertionSort: the textbook forms of Havok 2014.1.
#pragma once

namespace hkAlgorithm
{
template<typename T>
inline void swap(T& x, T& y)
{
    T t = x;
    x = y;
    y = t;
}

// size 0x1
template<typename T>
class less
{
public:
    bool operator()(const T& a, const T& b) const { return a < b; }
};

// size 0x1
template<typename T>
class greater
{
public:
    bool operator()(const T& a, const T& b) const { return a > b; }
};

// hksort.inl:91 (instances listed in analysis/havok_closure.tsv)
template<typename T, typename L>
void quickSortRecursive(T* pArr, int d, int h, L cmpLess)
{
    for (;;)
    {
        int i = h;
        int j = d;
        const T pivot = pArr[(d + h) >> 1];
        do
        {
            while (cmpLess(pArr[j], pivot))
            {
                j++;
            }
            while (cmpLess(pivot, pArr[i]))
            {
                i--;
            }
            if (i < j)
            {
                break;
            }
            if (i != j)
            {
                swap(pArr[i], pArr[j]);
            }
            i--;
            j++;
        } while (j <= i);
        if (d < i)
        {
            quickSortRecursive(pArr, d, i, cmpLess);
        }
        if (j >= h)
        {
            return;
        }
        d = j;
    }
}

template<typename T, typename L>
inline void quickSort(T* pArr, int iSize, L cmpLess)
{
    if (iSize > 1)
    {
        quickSortRecursive(pArr, 0, iSize - 1, cmpLess);
    }
}

template<typename T>
inline void quickSort(T* pArr, int iSize)
{
    quickSort(pArr, iSize, less<T>());
}

// The heap property for pArr[k..n] (1-based), given that it holds for pArr[k+1..n].
template<typename T, typename L>
void downHeap(T* pArr, int k, int n, L cmpLess)
{
    T temp = pArr[k - 1];
    while (k <= n / 2)
    {
        int child = 2 * k;
        if (child < n && cmpLess(pArr[child - 1], pArr[child]))
        {
            child++;
        }
        if (!cmpLess(temp, pArr[child - 1]))
        {
            break;
        }
        pArr[k - 1] = pArr[child - 1];
        k = child;
    }
    pArr[k - 1] = temp;
}

template<typename T, typename L>
void heapSort(T* pArr, int iSize, L cmpLess)
{
    int n = iSize;
    for (int k = n / 2; k > 0; k--)
    {
        downHeap(pArr, k, n, cmpLess);
    }
    while (n >= 1)
    {
        swap(pArr[0], pArr[n - 1]);
        n--;
        downHeap(pArr, 1, n, cmpLess);
    }
}

template<typename T, typename L>
inline void insertionSort(T* pArr, int size, L cmpLess)
{
    for (int i = 0; i < size; i++)
    {
        T key = pArr[i];
        int j = i;
        while (j > 0 && cmpLess(key, pArr[j - 1]))
        {
            pArr[j] = pArr[j - 1];
            j--;
        }
        pArr[j] = key;
    }
}

// hksort.inl:274
// getNext(e) returns a reference to e's link; cmpLess compares two elements. Returns the new head.
template<typename T, typename GETNEXT, typename L>
T* sortList(T* headPtr, GETNEXT getNext, L cmpLess)
{
    if (headPtr == nullptr)
    {
        return nullptr;
    }
    T* list = headPtr;
    int insize = 1;
    for (;;)
    {
        T head; // the merged list hangs off this element's link
        getNext(&head) = nullptr;
        T* tail = &head;
        int nmerges = 0;
        T* p = list;
        while (p != nullptr)
        {
            nmerges++;
            T* q = p;
            int psize = 0;
            for (int i = 0; i < insize && q != nullptr; i++)
            {
                q = getNext(q);
                psize++;
            }
            int qsize = insize;
            while (psize > 0 || (qsize > 0 && q != nullptr))
            {
                T* e;
                if (psize == 0)
                {
                    e = q;
                    q = getNext(q);
                    qsize--;
                }
                else if (qsize == 0 || q == nullptr || !cmpLess(*q, *p))
                {
                    e = p;
                    p = getNext(p);
                    psize--;
                }
                else
                {
                    e = q;
                    q = getNext(q);
                    qsize--;
                }
                getNext(tail) = e;
                tail = e;
            }
            p = q;
        }
        getNext(tail) = nullptr;
        if (nmerges < 2)
        {
            return getNext(&head);
        }
        list = getNext(&head);
        insize *= 2;
    }
}
}
