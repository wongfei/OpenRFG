// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/staticmodelinstance.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/staticmodelinstance.cpp
//
// StaticModelInstance, an instance of an unskinned model (render objects plus one RenderInstance).
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/internallist.hpp"
#include "keen/math/matrix43.hpp"

namespace keen
{
    struct ModelHandleType;

    // size 0x3C
    class StaticModelInstance : public InternalListBase::Listable
    {
    public:
        StaticModelInstance(); // 0x00CC8950 (:10)
        void setModel(const ModelHandleType* model); // 0x00CC89B0 (:16)
        const ModelHandleType* getModel() const;
        void setWorldTransform(const Matrix43& transform); // 0x00CC89C0 (:23)
        const Matrix43& getWorldTransform() const;

    private:
        const ModelHandleType* m_model; // 0x08
        Matrix43 m_worldTranform; // 0x0C
    };
}
