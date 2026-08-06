//
//  AttributesBindingContext.hpp
//  valdi
//
//  Created by Simon Corsin on 8/2/21.
//

#pragma once

#include "valdi_core/AssetOutputType.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

#include "valdi/runtime/Attributes/AttributeHandlerDelegate.hpp"
#include "valdi/runtime/Attributes/AttributeId.hpp"
#include "valdi/runtime/Attributes/AttributePreprocessor.hpp"

#include "valdi_core/CompositeAttributePart.hpp"

namespace Valdi {

class AttributeHandlerDelegate;
class MeasureDelegate;

class AttributesBindingContext {
public:
    virtual ~AttributesBindingContext() = default;

    virtual void registerPreprocessor(const StringBox& attribute,
                                      bool enableCache,
                                      AttributePreprocessor&& preprocessor) = 0;

    virtual AttributeId bindStringAttribute(const StringBox& attribute,
                                            bool invalidateLayoutOnChange,
                                            const Ref<AttributeHandlerDelegate>& delegate) = 0;
    virtual AttributeId bindDoubleAttribute(const StringBox& attribute,
                                            bool invalidateLayoutOnChange,
                                            const Ref<AttributeHandlerDelegate>& delegate) = 0;

    virtual AttributeId bindPercentAttribute(const StringBox& attribute,
                                             bool invalidateLayoutOnChange,
                                             const Ref<AttributeHandlerDelegate>& delegate) = 0;

    virtual AttributeId bindBooleanAttribute(const StringBox& attribute,
                                             bool invalidateLayoutOnChange,
                                             const Ref<AttributeHandlerDelegate>& delegate) = 0;

    virtual AttributeId bindIntAttribute(const StringBox& attribute,
                                         bool invalidateLayoutOnChange,
                                         const Ref<AttributeHandlerDelegate>& delegate) = 0;

    virtual AttributeId bindColorAttribute(const StringBox& attribute,
                                           bool invalidateLayoutOnChange,
                                           const Ref<AttributeHandlerDelegate>& delegate) = 0;

    virtual AttributeId bindBorderAttribute(const StringBox& attribute,
                                            bool invalidateLayoutOnChange,
                                            const Ref<AttributeHandlerDelegate>& delegate) = 0;

    virtual AttributeId bindUntypedAttribute(const StringBox& attribute,
                                             bool invalidateLayoutOnChange,
                                             const Ref<AttributeHandlerDelegate>& delegate) = 0;

    virtual AttributeId bindTextAttribute(const StringBox& attribute,
                                          bool invalidateLayoutOnChange,
                                          const Ref<AttributeHandlerDelegate>& delegate) = 0;

    virtual AttributeId bindCompositeAttribute(const StringBox& attribute,
                                               const std::vector<snap::valdi_core::CompositeAttributePart>& parts,
                                               const Ref<AttributeHandlerDelegate>& delegate) = 0;

    virtual AttributeId bindTransformAttributes(const Ref<AttributeHandlerDelegate>& delegate) = 0;

    virtual void bindScrollAttributes() = 0;

    virtual void bindAssetAttributes(snap::valdi_core::AssetOutputType assetOutputType) = 0;

    virtual void setDefaultDelegate(const Ref<AttributeHandlerDelegate>& delegate) = 0;

    virtual void setMeasureDelegate(const Ref<MeasureDelegate>& measureDelegate) = 0;
};

} // namespace Valdi
