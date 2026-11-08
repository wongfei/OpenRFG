// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/config/hkproductfeatures.h
//
// Seam only (Havok 2014.2.5): the optional feature registration functions Volition calls.
#pragma once

void hkFeature_serializeDeprecatedPre700(); // enables loading packfiles older than Havok 7.0

// size 0x1
class hkProductFeatures
{
public:
    static void initialize(); // 0x00819910 (hkproductfeatures.cxx:51)

};
