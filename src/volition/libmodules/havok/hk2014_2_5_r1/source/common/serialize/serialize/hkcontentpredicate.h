// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/hkcontentpredicate.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

struct hkVariant;

// size 0x10
class hkContentPredicate
{
public:
    enum PredicateIdEnum
    {
        NO_PREDICATE = 0,
        USER_PREDICATE_1 = 1,
        USER_PREDICATE_2 = 2,
        USER_PREDICATE_3 = 3,
        USER_PREDICATE_4 = 4,
        USER_PREDICATE_5 = 5,
        USER_PREDICATE_6 = 6,
        USER_PREDICATE_7 = 7,
        USER_PREDICATE_8 = 8,
        USER_PREDICATE_9 = 9,
        USER_PREDICATE_10 = 10,
        USER_PREDICATE_11 = 11,
        USER_PREDICATE_12 = 12,
        USER_PREDICATE_13 = 13,
        USER_PREDICATE_14 = 14,
        USER_PREDICATE_15 = 15,
        USER_PREDICATE_16 = 16,
        USER_PREDICATE_17 = 17,
        USER_PREDICATE_18 = 18,
        USER_PREDICATE_19 = 19,
        HAS_ROOT_LEVEL_CONTAINER = 20,
        MAX_PREDICATE_ID = 21,
    };

    typedef hkEnum<enum PredicateIdEnum,unsigned short> PredicateId;
    typedef bool (*PredicateFunctionType)(hkVariant);

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkContentPredicate);
    hkContentPredicate(hkEnum<enum PredicateIdEnum,unsigned short> id, const char* predicateName, bool (*func)(hkVariant)); // 0x00F53240
    static hkContentPredicate* getListHead(); // 0x00F429F0
    static hkContentPredicate* findPredicate(unsigned short id); // 0x00F5D550
    static hkContentPredicate* findPredicate(hkEnum<enum PredicateIdEnum,unsigned short> id); // 0x00F53270
    static void mergePredicateList(hkContentPredicate* otherListHead); // 0x00F53290
    bool applyPredicate(hkVariant v) const; // 0x00F42A00
    hkContentPredicate* getNext() const; // 0x00F42A20
    hkEnum<enum PredicateIdEnum,unsigned short> getId() const; // 0x00F42A30
    const char* getName() const; // 0x00F5C5F0

public:
    hkEnum<enum PredicateIdEnum,unsigned short> m_id; // 0x00
    const char* m_predicateName; // 0x04
    bool (*m_predicate)(hkVariant); // 0x08
    hkContentPredicate* m_next; // 0x0C
    static hkContentPredicate* m_head; // 0x03A38174
};
