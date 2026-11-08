// Original: volition/rfg/code/havok/hkproductfeatures.inl
//
// The game's copy of the Havok feature list (Havok's hkProductFeatures.inl template): the products and the features it
// strips before compiling the SDK's hkproductfeatures.cxx into havok_cm_manager.cpp. rfg.exe strips nothing that
// hkProductFeatures::initialize looks at: it requests every Physics 2012 component (havok_impl.md HK5). The keycode
// (keycode.cxx, which normally selects the products) is S phase and not reimplemented; the product is set here.
#pragma once

#define HK_FEATURE_PRODUCT_PHYSICS_2012
