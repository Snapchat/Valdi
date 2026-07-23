//
//  AttributesBindingContextWrapper.hpp
//  valdi-android
//
//  Created by Simon Corsin on 8/4/21.
//

#pragma once

#include "valdi/runtime/Attributes/AttributesBindingContext.hpp"

#include "valdi_core/AssetOutputType.hpp"
#include "valdi_core/jni/JavaValue.hpp"

namespace ValdiAndroid {

class DeferredViewOperations;
class ViewManager;

class AttributesBindingContextWrapper : public Valdi::SimpleRefCountable {
public:
    AttributesBindingContextWrapper(ViewManager& viewManager, Valdi::AttributesBindingContext& bindingContext);
    ~AttributesBindingContextWrapper() override;

    jint bindAttributes(jint type, jstring name, jboolean invalidateLayoutOnChange, jobject delegate, jobject parts);

    void bindTransformAttributes(jobject delegate);

    void bindScrollAttributes();

    void bindAssetAttributes(snap::valdi_core::AssetOutputType assetOutputType);

    void registerAttributePreprocessor(jstring name, jboolean enableCache, jobject preprocessor);

    void setPlaceholderViewMeasureDelegate(jobject legacyMeasureDelegate);
    void setMeasureDelegate(jobject measureDelegate);

private:
    ViewManager& _viewManager;
    Valdi::AttributesBindingContext& _bindingContext;
};

} // namespace ValdiAndroid
