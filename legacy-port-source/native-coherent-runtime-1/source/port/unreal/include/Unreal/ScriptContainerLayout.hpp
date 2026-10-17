#pragma once

#include <algorithm>

#include <Unreal/Common.hpp>
#include <Unreal/Core/Containers/Map.hpp>
#include <Unreal/Core/Containers/Set.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>

namespace RC::Unreal
{
    /**
     * Computes TSet/TMap storage layouts from reflection instead of reading the layout cached on the
     * property.
     *
     * FScriptSetLayout, FScriptMapLayout and FScriptSparseArrayLayout are mirrored in this module as
     * plain C++ structs matching current engine versions, where a leading offset field is omitted
     * because it is always zero. Verified against engine source, that omission only holds from
     * roughly UE 4.22 onward:
     *
     *   UE 4.16 - 4.20:  FScriptSetLayout          { int32 ElementOffset; ... }
     *                    FScriptMapLayout          { int32 KeyOffset; int32 ValueOffset; ... }
     *                    FScriptSparseArrayLayout  { int32 ElementOffset; int32 Alignment; int32 Size; }
     *   UE 4.24+ / UE5:  all three leading fields commented out, which is what this module mirrors.
     *
     * (The change lands somewhere in 4.21 - 4.23; those versions were not available to check.)
     *
     * On the older versions every field therefore shifts by one int32 when the cached layout is read
     * through this module's structs: Size picks up HashIndexOffset, the sparse array layout picks up
     * neighbouring fields, and the resulting element stride is meaningless. Iterating a container with
     * it walks into unrelated memory, which typically surfaces as a fault deep inside the engine
     * rather than at the point of the mistake.
     *
     * These helpers reconstruct the same numbers from values that are correct on every version --
     * the key/value properties' element sizes, alignments and offsets -- which is how TArray elements
     * are already walked.
     */
    namespace ScriptContainerLayout
    {
        FORCEINLINE int32 AlignUp(int32 Value, int32 Alignment)
        {
            if (Alignment <= 1)
            {
                return Value;
            }
            return (Value + Alignment - 1) & ~(Alignment - 1);
        }

        /**
         * Stride of one TSetElement, i.e. the sparse array's element stride.
         * TSetElement<T> stores T followed by FSetElementId HashNextId and int32 HashIndex.
         * Pass a null ValueProperty for a TSet; pass both for a TMap's TPair.
         */
        inline int32 ComputeElementStride(const FProperty* KeyProperty, const FProperty* ValueProperty)
        {
            // Trailing FSetElementId HashNextId + int32 HashIndex
            constexpr int32 HashFieldsSize = sizeof(int32) * 2;
            // TSparseArray stores a union of the element and a free-list link of two int32s
            constexpr int32 FreeListLinkSize = sizeof(int32) * 2;
            constexpr int32 FreeListLinkAlignment = alignof(int32);

            if (!KeyProperty)
            {
                return 0;
            }

            const int32 KeySize = const_cast<FProperty*>(KeyProperty)->GetElementSize();
            int32 ElementAlignment = std::max(static_cast<int32>(const_cast<FProperty*>(KeyProperty)->GetMinAlignment()), 1);
            int32 ElementSize = KeySize;

            if (ValueProperty)
            {
                auto* MutableValue = const_cast<FProperty*>(ValueProperty);
                const int32 ValueAlignment = std::max(static_cast<int32>(MutableValue->GetMinAlignment()), 1);
                ElementAlignment = std::max(ElementAlignment, ValueAlignment);
                // The engine records the value's offset within the pair on the property itself; fall
                // back to the natural offset if it is unset.
                const int32 ValueOffset = std::max(MutableValue->GetOffset_Internal(), AlignUp(KeySize, ValueAlignment));
                ElementSize = ValueOffset + MutableValue->GetElementSize();
            }

            ElementSize = AlignUp(ElementSize, ElementAlignment);

            const int32 Stride = AlignUp(ElementSize + HashFieldsSize, std::max(ElementAlignment, FreeListLinkAlignment));
            return std::max(Stride, FreeListLinkSize);
        }

        inline int32 ComputeElementAlignment(const FProperty* KeyProperty, const FProperty* ValueProperty)
        {
            constexpr int32 FreeListLinkAlignment = alignof(int32);
            if (!KeyProperty)
            {
                return FreeListLinkAlignment;
            }
            int32 Alignment = std::max(static_cast<int32>(const_cast<FProperty*>(KeyProperty)->GetMinAlignment()), 1);
            if (ValueProperty)
            {
                Alignment = std::max(Alignment, static_cast<int32>(const_cast<FProperty*>(ValueProperty)->GetMinAlignment()));
            }
            return std::max(Alignment, FreeListLinkAlignment);
        }

        /** Layout for iterating a TSet's storage. Only the sparse array stride is consulted by GetData. */
        inline FScriptSetLayout ComputeSetLayout(const FProperty* ElementProperty)
        {
            FScriptSetLayout Layout{};
            Layout.Size = ComputeElementStride(ElementProperty, nullptr);
            Layout.SparseArrayLayout.Size = Layout.Size;
            Layout.SparseArrayLayout.Alignment = ComputeElementAlignment(ElementProperty, nullptr);
            return Layout;
        }

        /** Layout for iterating a TMap's storage. */
        inline FScriptMapLayout ComputeMapLayout(const FProperty* KeyProperty, const FProperty* ValueProperty)
        {
            FScriptMapLayout Layout{};
            Layout.ValueOffset = ValueProperty ? const_cast<FProperty*>(ValueProperty)->GetOffset_Internal() : 0;
            Layout.SetLayout.Size = ComputeElementStride(KeyProperty, ValueProperty);
            Layout.SetLayout.SparseArrayLayout.Size = Layout.SetLayout.Size;
            Layout.SetLayout.SparseArrayLayout.Alignment = ComputeElementAlignment(KeyProperty, ValueProperty);
            return Layout;
        }
    } // namespace ScriptContainerLayout
} // namespace RC::Unreal
