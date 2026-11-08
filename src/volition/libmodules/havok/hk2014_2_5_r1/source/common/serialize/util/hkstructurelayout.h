// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkstructurelayout.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"

class hkBool;
class hkClass;
struct hkContainerHeapAllocator;

// size 0x4
class hkStructureLayout
{
public:
    // size 0x4
    struct LayoutRules
    {
        // 0x00F44B00 (.h:51)
        hkBool operator==(const LayoutRules& other) const
        {
            return (m_bytesInPointer == other.m_bytesInPointer) && (m_littleEndian == other.m_littleEndian) &&
                   (m_reusePaddingOptimization == other.m_reusePaddingOptimization) &&
                   (m_emptyBaseClassOptimization == other.m_emptyBaseClassOptimization);
        }
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(LayoutRules);

    public:
        unsigned char m_bytesInPointer; // 0x00
        unsigned char m_littleEndian; // 0x01
        unsigned char m_reusePaddingOptimization; // 0x02
        unsigned char m_emptyBaseClassOptimization; // 0x03
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkStructureLayout);
    hkStructureLayout(const LayoutRules& rules); // 0x00F65AB0
    hkStructureLayout(); // 0x00F65AA0
    void computeMemberOffsetsInplace(hkClass& klass, hkPointerMap<hkClass const*,int,hkContainerHeapAllocator>& classesDone, unsigned int recurse) const; // 0x00F65FF0
    const LayoutRules& getRules() const { return m_rules; } // 0x00F59760 (.h:74)

    static const LayoutRules HostLayoutRules; // 0x016FA9E4
    static const LayoutRules MsvcWin32LayoutRules; // 0x01373B7C
    static const LayoutRules MsvcXboxLayoutRules; // 0x01373B80
    static const LayoutRules MsvcAmd64LayoutRules; // 0x01373B84
    static const LayoutRules CwPs2LayoutRules; // 0x01373B88
    static const LayoutRules CwNgcLayoutRules; // 0x01373B8C
    static const LayoutRules CwPspLayoutRules; // 0x01373B90
    static const LayoutRules CwWiiLayoutRules; // 0x01373B94
    static const LayoutRules Gcc32Ps2LayoutRules; // 0x01373B98
    static const LayoutRules Gcc151PspLayoutRules; // 0x01373B9C
    static const LayoutRules Gcc295LinuxLayoutRules; // 0x01373BA0
    static const LayoutRules Gcc33LinuxLayoutRules; // 0x01373BA4
    static const LayoutRules GccAmd64LinuxLayoutRules; // 0x01373BA8
    static const LayoutRules GccAndroidLayoutRules; // 0x01373BAC
    static const LayoutRules Sn31Ps2LayoutRules; // 0x01373BB0
    static const LayoutRules Sn10PspLayoutRules; // 0x01373BB4
    static const LayoutRules Sn393NgcLayoutRules; // 0x01373BB8
    static const LayoutRules SnPsVitaLayoutRules; // 0x01373BBC
    static const LayoutRules Xbox360LayoutRules; // 0x01373BC0
    static const LayoutRules GccPs3LayoutRules; // 0x01373BC4
    static const LayoutRules Gcc40MacPpcLayoutRules; // 0x01373BC8
    static const LayoutRules Gcc40MacIntelLayoutRules; // 0x01373BCC
    static const LayoutRules Gcc40iOSLayoutRules; // 0x01373BD0
    static const LayoutRules Rvct3dsLayoutRules; // 0x01373BD4
    static const LayoutRules GhsWiiULayoutRules; // 0x01373BD8
    static const LayoutRules Ps4LayoutRules; // 0x01373BDC
    static const LayoutRules XboxOneLayoutRules; // 0x01373BE0
public:
    LayoutRules m_rules; // 0x00
};
