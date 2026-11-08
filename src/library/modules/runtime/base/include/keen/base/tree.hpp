// Original: library/modules/runtime/base/include/keen/base/tree.hpp
// Implementation: library/modules/runtime/base/sources/global/tree.inl
#pragma once

namespace keen
{
    // Intrusive tree links; T derives from TreeNode<T>.
    // size 0x10
    template<typename T>
    struct TreeNode
    {
        TreeNode() // <FileSystemMountPoint> 0x00BF31E0 (tree.inl)
        {
            pNextSibling = nullptr;
            pPrevSibling = nullptr;
            pParent = nullptr;
            pFirstChild = nullptr;
        }

        T* pNextSibling; // 0x00
        T* pPrevSibling; // 0x04
        T* pParent; // 0x08
        T* pFirstChild; // 0x0C
    };
}
