// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/copier/hkobjectcopier.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer; inline members from the 2014.1 SDK
// in SE).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkstructurelayout.h"

class hkClass;
class hkClassMember;
class hkOArchive;
class hkRelocationInfo;
class hkStreamWriter;

// size 0x18
class hkObjectCopier : public hkReferencedObject
{
public:
    enum ObjectCopierFlagBits
    {
        FLAG_NONE = 0,
        FLAG_APPLY_DEFAULT_IF_SERIALIZE_IGNORED = 1,
        FLAG_RESPECT_SERIALIZE_IGNORED = 2,
    };

    typedef hkFlags<enum ObjectCopierFlagBits,unsigned int> ObjectCopierFlags;

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00F72C70, delete(void* p, unsigned int nbytes) 0x00F7DAB0
    hkObjectCopier(const hkStructureLayout& layoutIn, const hkStructureLayout& layoutOut, hkFlags<enum ObjectCopierFlagBits,unsigned int> flags); // 0x00F7B850
    virtual ~hkObjectCopier(); // vtbl+0x00 0x00F7B890
    virtual hkResult copyObject(const void* dataIn, const hkClass& klassIn, hkStreamWriter* dataOut, const hkClass& klassOut, hkRelocationInfo& reloc); // vtbl+0x10 0x00F7D470
    const hkStructureLayout& getLayoutOut() const { return m_layoutOut; } // 0x00F72770 (.h:59)
    const hkStructureLayout& getLayoutIn() const { return m_layoutIn; }
public:
    virtual unsigned int areMembersCompatible(const hkClassMember& src, const hkClassMember& dst); // vtbl+0x14 0x00F7BA20
public:
    void writeZero(hkOArchive& oa, const hkClassMember& memberOut, hkLocalArray<char>& zeroArray); // 0x00F7BB70
    void saveBody(const void* dataIn, const hkClass& klassIn, hkOArchive& dataOut, const hkClass& klassOut, hkLocalArray<char>& zeroArray); // 0x00F7BE60
    void saveExtras(const void* dataIn, const hkClass& klassIn, hkOArchive& dataOut, const hkClass& klassOut, int classStart, int extrasStart, hkRelocationInfo& fixups, hkLocalArray<char>& zeroArray, int level); // 0x00F7C970
    virtual const hkClass* lookupClass(const hkClass& klass) { return &klass; } // vtbl+0x18 0x00F7D770 (.h:79)

public:
    hkStructureLayout m_layoutIn; // 0x08
    hkStructureLayout m_layoutOut; // 0x0C
    hkFlags<enum ObjectCopierFlagBits,unsigned int> m_flags; // 0x10
    hkBool m_byteSwap; // 0x14
};
