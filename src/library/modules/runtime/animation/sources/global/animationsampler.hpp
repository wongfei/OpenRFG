// Original: library/modules/runtime/animation/sources/global/animationsampler.hpp
//
// Key decompression and lookup helpers of the animation sampler.
#pragma once

#include "keen/base/types.hpp"
#include <xmmintrin.h>
#include <stdint.h>

namespace keen
{
    struct JointAnimationData;
    struct JointAnimationHeader;
    struct LveAnimationData;
    struct Quaternion;
    struct Vector3;

    namespace animation
    {
        float getJointAnimationTime(uint32* pExactFrame, const JointAnimationData& animData, float time); // 0x00CBB7A0 (.h:42)
        bool getLveAnimationTime(float* pStartFrame, float* pEndFrame, const LveAnimationData& animData, float startTime, float endTime); // 0x00CBEC30 (.h:66)
        const JointAnimationHeader& getAnimationHeaderAtTime(const JointAnimationData& animData, uint32 exactFrame); // 0x00CBB860 (.h:107)
        uint32 getCountLeftBit(uint32* pLeftBits, uint64_t mask, uint32 bit); // 0x00CBB890 (.h:132)
        uint32 getCountRightBit(uint32* pRightBits, uint64_t mask, uint32 bit); // 0x00CBBA20 (.h:140)
        void decompressOrientation(Quaternion& target, const short* pData); // 0x00CBBBB0 (.h:148)
        void decompressPosition(Vector3& target, const short* pData, const __m128& factor); // 0x00CBBC20 (.h:161)
        void decompressScale(Vector3& target, const short* pData, const __m128& factor); // 0x00CBBC80 (.h:173)
    }
}
