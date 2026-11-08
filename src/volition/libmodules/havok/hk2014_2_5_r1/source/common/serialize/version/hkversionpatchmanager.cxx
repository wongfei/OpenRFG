// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/version/hkversionpatchmanager.cxx
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SG): the patch list expander, as the 2014.1 SDK's (same macros; 2014.2.5
// has no PATCH_GROUP). A register<Product>Patches_<version> function defines HK_PATCHES_FILE and includes this file
// inside its body; the list is expanded three times: the default values (static d<line>), the patch structs (static
// p<line>), then one block per patch with its null-terminated component array and a static PatchInfo, registered with
// man.addPatch(&pinfo). rfg.exe builds the PatchInfos on the first call (the class names are local `const char*`
// variables, so the PatchInfo is initialised dynamically, guarded by $S1 bits).

#ifndef HK_PATCHES_FILE
#error You must define HK_PATCHES_FILE before including this file
#endif

#ifndef HK_COUNT_OF
#define HK_COUNT_OF(x) (sizeof(x) / sizeof(x[0]))
#endif

{
#define HK_CLASS_ADDED -1
#define HK_CLASS_REMOVED -1

//
// Defaults
//
#define HK_PATCH_BEGIN(CLASSNAME, OLDVER, NEWNAME, NEWVER)
#define HK_PATCH_CAST(CNAME)
#define HK_PATCH_DEPENDS(CNAME, VERSION)
#define HK_PATCH_PARENT_SET(OLDNAME, NEWNAME)

#define HK_PATCH_MEMBER_ADDED(NAME, TYPE, TYPE_NAME, TUPLES)
#define HK_PATCH_MEMBER_ADDED_INT(NAME, VALUE) static const hkInt64 HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__) = hkInt64(VALUE);
#define HK_PATCH_MEMBER_ADDED_REAL(NAME, VALUE) static const hkReal HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__) = hkReal(VALUE);
#define HK_PATCH_MEMBER_ADDED_BOOL(NAME, VALUE) static const hkInt64 HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__) = hkInt64(VALUE);
#define HK_PATCH_MEMBER_ADDED_BYTE(NAME, VALUE) static const hkInt64 HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__) = hkInt64(VALUE);
#define HK_PATCH_MEMBER_ADDED_VEC_4(NAME, vx, vy, vz, vw) static const hkReal HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)[] = {vx, vy, vz, vw};
#define HK_PATCH_MEMBER_ADDED_VEC_4_IDENTITY(NAME)
#define HK_PATCH_MEMBER_ADDED_VEC_12(NAME, tx, ty, tz, tw, rix, riy, riz, rr, sx, sy, sz, sw) static const hkReal HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)[] = {tx, ty, tz, tw, rix, riy, riz, rr, sx, sy, sz, sw};
#define HK_PATCH_MEMBER_ADDED_VEC_12_IDENTITY(NAME)
#define HK_PATCH_MEMBER_ADDED_VEC_16(NAME, ax, ay, az, aw, bx, by, bz, bw, cx, cy, cz, cw, dx, dy, dz, dw) static const hkReal HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)[] = {ax, ay, az, aw, bx, by, bz, bw, cx, cy, cz, cw, dx, dy, dz, dw};
#define HK_PATCH_MEMBER_ADDED_VEC_16_IDENTITY(NAME)
#define HK_PATCH_MEMBER_ADDED_CSTRING(NAME, VALUE) static const char* HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__) = VALUE;
#define HK_PATCH_MEMBER_ADDED_POINTER(NAME, SUBTYPE, VALUE) static const void* HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__) = VALUE;

#define HK_PATCH_MEMBER_DEFAULT_SET_INT(NAME, VALUE) static const hkInt64 HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__) = hkInt64(VALUE);
#define HK_PATCH_MEMBER_DEFAULT_SET_REAL(NAME, VALUE) static const hkReal HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__) = hkReal(VALUE);
#define HK_PATCH_MEMBER_DEFAULT_SET_BOOL(NAME, VALUE) static const hkInt64 HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__) = hkInt64(VALUE);
#define HK_PATCH_MEMBER_DEFAULT_SET_BYTE(NAME, VALUE) static const hkInt64 HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__) = hkInt64(VALUE);
#define HK_PATCH_MEMBER_DEFAULT_SET_VEC_4(NAME, vx, vy, vz, vw) static const hkReal HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)[] = {vx, vy, vz, vw};
#define HK_PATCH_MEMBER_DEFAULT_SET_VEC_12(NAME, tx, ty, tz, tw, rix, riy, riz, rr, sx, sy, sz, sw) static const hkReal HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)[] = {tx, ty, tz, tw, rix, riy, riz, rr, sx, sy, sz, sw};
#define HK_PATCH_MEMBER_DEFAULT_SET_VEC_16(NAME, ax, ay, az, aw, bx, by, bz, bw, cx, cy, cz, cw, dx, dy, dz, dw) static const hkReal HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)[] = {ax, ay, az, aw, bx, by, bz, bw, cx, cy, cz, cw, dx, dy, dz, dw};
#define HK_PATCH_MEMBER_DEFAULT_SET_CSTRING(NAME, VALUE) static const char* HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__) = VALUE;
#define HK_PATCH_MEMBER_DEFAULT_SET_POINTER(NAME, VALUE) static const void* HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__) = VALUE;

#define HK_PATCH_MEMBER_RENAMED(OLDNAME, NEWNAME)
#define HK_PATCH_FUNCTION(NAME)
#define HK_PATCH_MEMBER_REMOVED(NAME, TYPE, TYPE_NAME, TUPLES)

#define HK_PATCH_END()
#include HK_PATCHES_FILE
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/version/hkversionpatchmanagerundef.inl"

//
// Patches
//
#define HK_PATCH_BEGIN(CLASSNAME, OLDVER, NEWNAME, NEWVER)
#define HK_PATCH_CAST(CNAME) static const hkVersionPatchManager::CastPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {CNAME};
#define HK_PATCH_DEPENDS(CNAME, VERSION) static const hkVersionPatchManager::DependsPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {CNAME, VERSION};
#define HK_PATCH_PARENT_SET(OLDNAME, NEWNAME) static const hkVersionPatchManager::SetParentPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {OLDNAME, NEWNAME};

#define HK_PATCH_MEMBER_ADDED(NAME, TYPE, TYPE_NAME, TUPLES) static const hkVersionPatchManager::MemberAddedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, hkLegacyType::TYPE, TYPE_NAME, TUPLES, HK_NULL};
#define HK_PATCH_MEMBER_ADDED_INT(NAME, VALUE) static const hkVersionPatchManager::MemberAddedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, hkLegacyType::TYPE_INT, HK_NULL, 0, &HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)};
#define HK_PATCH_MEMBER_ADDED_REAL(NAME, VALUE) static const hkVersionPatchManager::MemberAddedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, hkLegacyType::TYPE_REAL, HK_NULL, 0, &HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)};
#define HK_PATCH_MEMBER_ADDED_BOOL(NAME, VALUE) static const hkVersionPatchManager::MemberAddedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, hkLegacyType::TYPE_BYTE, HK_NULL, 0, &HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)};
#define HK_PATCH_MEMBER_ADDED_BYTE(NAME, VALUE) static const hkVersionPatchManager::MemberAddedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, hkLegacyType::TYPE_BYTE, HK_NULL, 0, &HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)};
#define HK_PATCH_MEMBER_ADDED_VEC_4(NAME, vx, vy, vz, vw) static const hkVersionPatchManager::MemberAddedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, hkLegacyType::TYPE_VEC_4, HK_NULL, 0, reinterpret_cast<const hkVector4*>(&HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__))};
#define HK_PATCH_MEMBER_ADDED_VEC_4_IDENTITY(NAME) static const hkVersionPatchManager::MemberAddedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, hkLegacyType::TYPE_VEC_4, HK_NULL, 0, reinterpret_cast<const hkVector4*>(&hkQuaternion::getIdentity())};
#define HK_PATCH_MEMBER_ADDED_VEC_12(NAME, tx, ty, tz, tw, rix, riy, riz, rr, sx, sy, sz, sw) static const hkVersionPatchManager::MemberAddedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, hkLegacyType::TYPE_VEC_12, HK_NULL, 0, reinterpret_cast<const hkQsTransform*>(&HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__))};
#define HK_PATCH_MEMBER_ADDED_VEC_12_IDENTITY(NAME) static const hkVersionPatchManager::MemberAddedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, hkLegacyType::TYPE_VEC_12, HK_NULL, 0, reinterpret_cast<const hkQsTransform*>(&hkQsTransform::getIdentity())};
#define HK_PATCH_MEMBER_ADDED_VEC_16(NAME, ax, ay, az, aw, bx, by, bz, bw, cx, cy, cz, cw, dx, dy, dz, dw) static const hkVersionPatchManager::MemberAddedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, hkLegacyType::TYPE_VEC_16, HK_NULL, 0, reinterpret_cast<const hkTransform*>(&HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__))};
#define HK_PATCH_MEMBER_ADDED_VEC_16_IDENTITY(NAME) static const hkVersionPatchManager::MemberAddedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, hkLegacyType::TYPE_VEC_16, HK_NULL, 0, reinterpret_cast<const hkTransform*>(&hkTransform::getIdentity())};
#define HK_PATCH_MEMBER_ADDED_CSTRING(NAME, VALUE) static const hkVersionPatchManager::MemberAddedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, hkLegacyType::TYPE_CSTRING, HK_NULL, 0, &HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)};
#define HK_PATCH_MEMBER_ADDED_POINTER(NAME, SUBTYPE, VALUE) static const hkVersionPatchManager::MemberAddedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, hkLegacyType::TYPE_OBJECT, SUBTYPE, 0, &HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)};

#define HK_PATCH_MEMBER_DEFAULT_SET_INT(NAME, VALUE) static const hkVersionPatchManager::DefaultChangedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, &HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)};
#define HK_PATCH_MEMBER_DEFAULT_SET_REAL(NAME, VALUE) static const hkVersionPatchManager::DefaultChangedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, &HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)};
#define HK_PATCH_MEMBER_DEFAULT_SET_BOOL(NAME, VALUE) static const hkVersionPatchManager::DefaultChangedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, &HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)};
#define HK_PATCH_MEMBER_DEFAULT_SET_BYTE(NAME, VALUE) static const hkVersionPatchManager::DefaultChangedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, &HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)};
#define HK_PATCH_MEMBER_DEFAULT_SET_VEC_4(NAME, vx, vy, vz, vw) static const hkVersionPatchManager::DefaultChangedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, &HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)};
#define HK_PATCH_MEMBER_DEFAULT_SET_VEC_12(NAME, tx, ty, tz, tw, rix, riy, riz, rr, sx, sy, sz, sw) static const hkVersionPatchManager::DefaultChangedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, &HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)};
#define HK_PATCH_MEMBER_DEFAULT_SET_VEC_16(NAME, ax, ay, az, aw, bx, by, bz, bw, cx, cy, cz, cw, dx, dy, dz, dw) static const hkVersionPatchManager::DefaultChangedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, &HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)};
#define HK_PATCH_MEMBER_DEFAULT_SET_CSTRING(NAME, VALUE) static const hkVersionPatchManager::DefaultChangedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, &HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)};
#define HK_PATCH_MEMBER_DEFAULT_SET_POINTER(NAME, VALUE) static const hkVersionPatchManager::DefaultChangedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, &HK_PREPROCESSOR_JOIN_TOKEN(d, __LINE__)};

#define HK_PATCH_MEMBER_RENAMED(OLDNAME, NEWNAME) static const hkVersionPatchManager::MemberRenamedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {OLDNAME, NEWNAME};
#define HK_PATCH_FUNCTION(NAME) static const hkVersionPatchManager::FunctionPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {#NAME, NAME};
#define HK_PATCH_MEMBER_REMOVED(NAME, TYPE, TYPE_NAME, TUPLES) static const hkVersionPatchManager::MemberRemovedPatch HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__) = {NAME, hkLegacyType::TYPE, TYPE_NAME, TUPLES};
#define HK_PATCH_END()
#include HK_PATCHES_FILE
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/version/hkversionpatchmanagerundef.inl"

//
// PatchInfos (wrapping several logical patches)
//
#define HK_PATCH_BEGIN(CLASSNAME, OLDVER, NEWCLASSNAME, NEWVER) { const char* classname = CLASSNAME; const char* newclassname = NEWCLASSNAME; const int oldver = OLDVER; const int newver = NEWVER; static const hkVersionPatchManager::PatchInfo::Component patch[] = {
#define HK_PATCH_CAST(CNAME) {hkVersionPatchManager::PATCH_CAST, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_DEPENDS(CNAME, VERSION) {hkVersionPatchManager::PATCH_DEPENDS, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_PARENT_SET(OLDNAME, NEWNAME) {hkVersionPatchManager::PATCH_PARENT_SET, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},

#define HK_PATCH_MEMBER_ADDED(NAME, TYPE, TYPE_NAME, TUPLES) {hkVersionPatchManager::PATCH_MEMBER_ADDED, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_ADDED_INT(NAME, VALUE) {hkVersionPatchManager::PATCH_MEMBER_ADDED, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_ADDED_REAL(NAME, VALUE) {hkVersionPatchManager::PATCH_MEMBER_ADDED, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_ADDED_BOOL(NAME, VALUE) {hkVersionPatchManager::PATCH_MEMBER_ADDED, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_ADDED_BYTE(NAME, VALUE) {hkVersionPatchManager::PATCH_MEMBER_ADDED, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_ADDED_VEC_4(NAME, vx, vy, vz, vw) {hkVersionPatchManager::PATCH_MEMBER_ADDED, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_ADDED_VEC_4_IDENTITY(NAME) {hkVersionPatchManager::PATCH_MEMBER_ADDED, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_ADDED_VEC_12(NAME, tx, ty, tz, tw, rix, riy, riz, rr, sx, sy, sz, sw) {hkVersionPatchManager::PATCH_MEMBER_ADDED, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_ADDED_VEC_12_IDENTITY(NAME) {hkVersionPatchManager::PATCH_MEMBER_ADDED, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_ADDED_VEC_16(NAME, ax, ay, az, aw, bx, by, bz, bw, cx, cy, cz, cw, dx, dy, dz, dw) {hkVersionPatchManager::PATCH_MEMBER_ADDED, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_ADDED_VEC_16_IDENTITY(NAME) {hkVersionPatchManager::PATCH_MEMBER_ADDED, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_ADDED_CSTRING(NAME, VALUE) {hkVersionPatchManager::PATCH_MEMBER_ADDED, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_ADDED_POINTER(NAME, SUBTYPE, VALUE) {hkVersionPatchManager::PATCH_MEMBER_ADDED, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},

#define HK_PATCH_MEMBER_DEFAULT_SET_INT(NAME, VALUE) {hkVersionPatchManager::PATCH_MEMBER_DEFAULT_SET, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_DEFAULT_SET_REAL(NAME, VALUE) {hkVersionPatchManager::PATCH_MEMBER_DEFAULT_SET, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_DEFAULT_SET_BOOL(NAME, VALUE) {hkVersionPatchManager::PATCH_MEMBER_DEFAULT_SET, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_DEFAULT_SET_BYTE(NAME, VALUE) {hkVersionPatchManager::PATCH_MEMBER_DEFAULT_SET, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_DEFAULT_SET_VEC_4(NAME, vx, vy, vz, vw) {hkVersionPatchManager::PATCH_MEMBER_DEFAULT_SET, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_DEFAULT_SET_VEC_12(NAME, tx, ty, tz, tw, rix, riy, riz, rr, sx, sy, sz, sw) {hkVersionPatchManager::PATCH_MEMBER_DEFAULT_SET, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_DEFAULT_SET_VEC_16(NAME, ax, ay, az, aw, bx, by, bz, bw, cx, cy, cz, cw, dx, dy, dz, dw) {hkVersionPatchManager::PATCH_MEMBER_DEFAULT_SET, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_DEFAULT_SET_CSTRING(NAME, VALUE) {hkVersionPatchManager::PATCH_MEMBER_DEFAULT_SET, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_DEFAULT_SET_POINTER(NAME, VALUE) {hkVersionPatchManager::PATCH_MEMBER_DEFAULT_SET, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},

#define HK_PATCH_MEMBER_RENAMED(OLDNAME, NEWNAME) {hkVersionPatchManager::PATCH_MEMBER_RENAMED, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_FUNCTION(NAME) {hkVersionPatchManager::PATCH_FUNCTION, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_MEMBER_REMOVED(NAME, TYPE, TYPE_NAME, TUPLES) {hkVersionPatchManager::PATCH_MEMBER_REMOVED, &HK_PREPROCESSOR_JOIN_TOKEN(p, __LINE__)},
#define HK_PATCH_END() {hkVersionPatchManager::PATCH_INVALID, HK_NULL} }; static const hkVersionPatchManager::PatchInfo pinfo = {classname, newclassname, oldver, newver, patch, int(HK_COUNT_OF(patch)) - 1}; man.addPatch(&pinfo); }
#include HK_PATCHES_FILE
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/version/hkversionpatchmanagerundef.inl"

#undef HK_CLASS_ADDED
#undef HK_CLASS_REMOVED
}
