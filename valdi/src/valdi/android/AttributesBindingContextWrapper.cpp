//
//  AttributesBindingContextWrapper.cpp
//  valdi-android
//
//  Created by Simon Corsin on 8/4/21.
//

#include "valdi/android/AttributesBindingContextWrapper.hpp"
#include "valdi/android/AndroidViewHolder.hpp"
#include "valdi/android/AndroidViewTransaction.hpp"
#include "valdi/android/DeferredViewOperations.hpp"
#include "valdi/android/ViewManager.hpp"

#include "valdi/runtime/Attributes/ValueConverters.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/runtime/Views/DefaultMeasureDelegate.hpp"
#include "valdi/runtime/Views/PlaceholderViewMeasureDelegate.hpp"

#include "valdi_core/cpp/Utils/Format.hpp"

#include "valdi_core/NativeCompositeAttributePart.hpp"

#include "valdi_core/jni/JNIMethodUtils.hpp"
#include "valdi_core/jni/JavaCache.hpp"
#include "valdi_core/jni/JavaUtils.hpp"
#include "valdi_core/jni/ValueFunctionWithJavaFunction.hpp"

namespace ValdiAndroid {

static DeferredViewOperations& getViewOperationsFromViewTransactionScope(
    Valdi::ViewTransactionScope& viewTransactionScope) {
    auto& viewTransaction = dynamic_cast<AndroidViewTransaction&>(viewTransactionScope.transaction());
    return viewTransaction.getViewOperations();
}

class AndroidAttributeHandlerDelegate : public Valdi::AttributeHandlerDelegate {
public:
    explicit AndroidAttributeHandlerDelegate(jobject javaDelegate)
        : _javaDelegate(
              Valdi::makeShared<GlobalRefJavaObject>(JavaEnv(), javaDelegate, "AttributeHandlerDelegate", false)) {}

    void onReset(Valdi::ViewTransactionScope& viewTransactionScope,
                 Valdi::ViewNode& /*viewNode*/,
                 const Valdi::Ref<Valdi::View>& view,
                 const Valdi::StringBox& /*name*/,
                 const Valdi::Ref<Valdi::Animator>& animator) override {
        getViewOperationsFromViewTransactionScope(viewTransactionScope)
            .enqueueResetAttribute(view, _javaDelegate, animator);
    }

    Valdi::Result<Valdi::Void> onApply(Valdi::ViewTransactionScope& viewTransactionScope,
                                       Valdi::ViewNode& viewNode,
                                       const Valdi::Ref<Valdi::View>& view,
                                       const Valdi::StringBox& name,
                                       const Valdi::Value& value,
                                       const Valdi::Ref<Valdi::Animator>& animator) override {
        return onApply(
            getViewOperationsFromViewTransactionScope(viewTransactionScope), viewNode, view, name, value, animator);
    }

protected:
    virtual Valdi::Result<Valdi::Void> onApply(DeferredViewOperations& operations,
                                               Valdi::ViewNode& viewNode,
                                               const Valdi::Ref<Valdi::View>& view,
                                               const Valdi::StringBox& /*name*/,
                                               const Valdi::Value& value,
                                               const Valdi::Ref<Valdi::Animator>& animator) = 0;

    Valdi::ByteBuffer& enqueueApplyAttribute(DeferredViewOperations& operations,
                                             ViewOperation operation,
                                             const Valdi::Ref<Valdi::View>& view,
                                             const Valdi::Ref<Valdi::Animator>& animator) {
        return operations.enqueueApplyAttribute(operation, view, _javaDelegate, animator);
    }

private:
    Valdi::Value _javaDelegate;
};

class BooleanAttributeHandlerDelegate : public AndroidAttributeHandlerDelegate {
public:
    using AndroidAttributeHandlerDelegate::AndroidAttributeHandlerDelegate;

    Valdi::Result<Valdi::Void> onApply(DeferredViewOperations& operations,
                                       Valdi::ViewNode& /*viewNode*/,
                                       const Valdi::Ref<Valdi::View>& view,
                                       const Valdi::StringBox& /*name*/,
                                       const Valdi::Value& value,
                                       const Valdi::Ref<Valdi::Animator>& animator) override {
        auto& buffer = enqueueApplyAttribute(operations, ViewOperationApplyAttributeBool, view, animator);
        operations.write(buffer, static_cast<int32_t>(value.toBool()));
        return Valdi::Void();
    }
};

class IntAttributeHandlerDelegate : public AndroidAttributeHandlerDelegate {
public:
    using AndroidAttributeHandlerDelegate::AndroidAttributeHandlerDelegate;

    Valdi::Result<Valdi::Void> onApply(DeferredViewOperations& operations,
                                       Valdi::ViewNode& /*viewNode*/,
                                       const Valdi::Ref<Valdi::View>& view,
                                       const Valdi::StringBox& /*name*/,
                                       const Valdi::Value& value,
                                       const Valdi::Ref<Valdi::Animator>& animator) override {
        auto& buffer = enqueueApplyAttribute(operations, ViewOperationApplyAttributeInt, view, animator);
        operations.write(buffer, static_cast<int32_t>(value.toInt()));
        return Valdi::Void();
    }
};

class LongAttributeHandlerDelegate : public AndroidAttributeHandlerDelegate {
public:
    using AndroidAttributeHandlerDelegate::AndroidAttributeHandlerDelegate;

    Valdi::Result<Valdi::Void> onApply(DeferredViewOperations& operations,
                                       Valdi::ViewNode& /*viewNode*/,
                                       const Valdi::Ref<Valdi::View>& view,
                                       const Valdi::StringBox& /*name*/,
                                       const Valdi::Value& value,
                                       const Valdi::Ref<Valdi::Animator>& animator) override {
        auto& buffer = enqueueApplyAttribute(operations, ViewOperationApplyAttributeLong, view, animator);
        operations.write(buffer, value.toLong());
        return Valdi::Void();
    }
};

class FloatAttributeHandlerDelegate : public AndroidAttributeHandlerDelegate {
public:
    using AndroidAttributeHandlerDelegate::AndroidAttributeHandlerDelegate;

    Valdi::Result<Valdi::Void> onApply(DeferredViewOperations& operations,
                                       Valdi::ViewNode& /*viewNode*/,
                                       const Valdi::Ref<Valdi::View>& view,
                                       const Valdi::StringBox& /*name*/,
                                       const Valdi::Value& value,
                                       const Valdi::Ref<Valdi::Animator>& animator) override {
        auto& buffer = enqueueApplyAttribute(operations, ViewOperationApplyAttributeFloat, view, animator);
        operations.write(buffer, value.toFloat());
        return Valdi::Void();
    }
};

class ObjectAttributeHandlerDelegate : public AndroidAttributeHandlerDelegate {
public:
    using AndroidAttributeHandlerDelegate::AndroidAttributeHandlerDelegate;

    Valdi::Result<Valdi::Void> onApply(DeferredViewOperations& operations,
                                       Valdi::ViewNode& /*viewNode*/,
                                       const Valdi::Ref<Valdi::View>& view,
                                       const Valdi::StringBox& /*name*/,
                                       const Valdi::Value& value,
                                       const Valdi::Ref<Valdi::Animator>& animator) override {
        auto& buffer = enqueueApplyAttribute(operations, ViewOperationApplyAttributeObject, view, animator);
        operations.write(buffer, value);
        return Valdi::Void();
    }
};

class PercentAttributeHandlerDelegate : public AndroidAttributeHandlerDelegate {
public:
    using AndroidAttributeHandlerDelegate::AndroidAttributeHandlerDelegate;

    Valdi::Result<Valdi::Void> onApply(DeferredViewOperations& operations,
                                       Valdi::ViewNode& /*viewNode*/,
                                       const Valdi::Ref<Valdi::View>& view,
                                       const Valdi::StringBox& /*name*/,
                                       const Valdi::Value& value,
                                       const Valdi::Ref<Valdi::Animator>& animator) override {
        auto result = Valdi::ValueConverter::toPercent(value);
        if (!result) {
            return result.moveError();
        }

        auto percentValue = result.moveValue();

        auto& buffer = enqueueApplyAttribute(operations, ViewOperationApplyAttributePercent, view, animator);
        operations.write(buffer, static_cast<float>(percentValue.value));
        operations.write(buffer, static_cast<int32_t>(percentValue.isPercent ? 1 : 0));

        return Valdi::Void();
    }
};

class CornersAttributeHandlerDelegate : public AndroidAttributeHandlerDelegate {
public:
    using AndroidAttributeHandlerDelegate::AndroidAttributeHandlerDelegate;

    Valdi::Result<Valdi::Void> onApply(DeferredViewOperations& operations,
                                       Valdi::ViewNode& /*viewNode*/,
                                       const Valdi::Ref<Valdi::View>& view,
                                       const Valdi::StringBox& /*name*/,
                                       const Valdi::Value& value,
                                       const Valdi::Ref<Valdi::Animator>& animator) override {
        auto result = Valdi::ValueConverter::toBorderValues(value);
        if (!result) {
            return result.moveError();
        }

        auto& borderRadius = *result.value();

        auto& buffer = enqueueApplyAttribute(operations, ViewOperationApplyAttributeCorners, view, animator);

        int32_t borderFlags = 0;
        if (borderRadius.isTopLeftPercent()) {
            borderFlags |= 1 << 0;
        }
        if (borderRadius.isTopRightPercent()) {
            borderFlags |= 1 << 1;
        }
        if (borderRadius.isBottomRightPercent()) {
            borderFlags |= 1 << 2;
        }
        if (borderRadius.isBottomLeftPercent()) {
            borderFlags |= 1 << 3;
        }

        operations.write(buffer, borderFlags);
        operations.write(buffer, static_cast<float>(borderRadius.getTopLeftValue()));
        operations.write(buffer, static_cast<float>(borderRadius.getTopRightValue()));
        operations.write(buffer, static_cast<float>(borderRadius.getBottomRightValue()));
        operations.write(buffer, static_cast<float>(borderRadius.getBottomLeftValue()));

        return Valdi::Void();
    }
};

class AndroidLegacyMeasureDelegate : public Valdi::PlaceholderViewMeasureDelegate {
public:
    AndroidLegacyMeasureDelegate(jobject attributesBinder, ViewManager& viewManager)
        : _attributesBinder(JavaEnv(), attributesBinder, "AttributesBinder"), _viewManager(viewManager) {}
    ~AndroidLegacyMeasureDelegate() override = default;

    Valdi::Ref<Valdi::View> createPlaceholderView() override {
        auto result = _viewManager.getMeasurerPlaceholderView(_attributesBinder.toObject());
        if (result.isNull()) {
            return nullptr;
        }
        return ValdiAndroid::toValdiView(result, _viewManager);
    }

    Valdi::Size measureView(const Valdi::Ref<Valdi::View>& view,
                            float width,
                            Valdi::MeasureMode widthMode,
                            float height,
                            Valdi::MeasureMode heightMode) override {
        Valdi::CoordinateResolver coordinateResolver(_viewManager.getPointScale());
        auto intWidth = coordinateResolver.toPixels(width);
        auto intHeight = coordinateResolver.toPixels(height);

        auto javaView = fromValdiView(view);

        if (javaView.isNull()) {
            onJavaViewCastFail(view);
            return Valdi::Size(0.0f, 0.0f);
        }

        auto measureSpec = JavaCache::get().getViewRefMeasureMethod().call(
            javaView, intWidth, static_cast<int32_t>(widthMode), intHeight, static_cast<int32_t>(heightMode));

        return Valdi::Size::fromPackedPixels(measureSpec, coordinateResolver.getPointScale());
    }

private:
    GlobalRefJavaObjectBase _attributesBinder;
    ViewManager& _viewManager;

    static void onJavaViewCastFail(const Valdi::Ref<Valdi::View>& view) {
        if (view == nullptr) {
            SC_ASSERT_FAIL("Failed to cast View to Java: View was null");
        } else {
            std::string errorMessage =
                fmt::format("Failed to cast View to Java: View class was '{}' instead of 'AndroidViewHolder'",
                            view->getClassName());
            SC_ASSERT_FAIL(errorMessage.c_str());
        }
    }
};

class AndroidMeasureDelegate : public Valdi::DefaultMeasureDelegate {
public:
    AndroidMeasureDelegate(jobject measureDelegate, ViewManager& viewManager)
        : _measureDelegate(JavaEnv(), measureDelegate, "MeasureDelegate"), _viewManager(viewManager) {}
    ~AndroidMeasureDelegate() override = default;

    Valdi::Size onMeasure(const Valdi::Ref<Valdi::ValueMap>& attributes,
                          float width,
                          Valdi::MeasureMode widthMode,
                          float height,
                          Valdi::MeasureMode heightMode,
                          bool isRightToLeft) override {
        Valdi::CoordinateResolver coordinateResolver(_viewManager.getPointScale());

        auto cppAttributesPtr = bridgeRetain(attributes.get());

        auto measureSpec = _viewManager.measure(_measureDelegate.toObject(),
                                                cppAttributesPtr,
                                                coordinateResolver.toPixels(width),
                                                static_cast<int>(widthMode),
                                                coordinateResolver.toPixels(height),
                                                static_cast<int>(heightMode),
                                                isRightToLeft);

        return Valdi::Size::fromPackedPixels(measureSpec, coordinateResolver.getPointScale());
    }

private:
    GlobalRefJavaObjectBase _measureDelegate;
    ViewManager& _viewManager;
};

constexpr size_t kAttributeTypeUntyped = 1;
constexpr size_t kAttributeTypeInt = 2;
constexpr size_t kAttributeTypeBoolean = 3;
constexpr size_t kAttributeTypeDouble = 4;
constexpr size_t kAttributeTypeString = 5;
constexpr size_t kAttributeTypeColor = 6;
constexpr size_t kAttributeTypeBorder = 7;
constexpr size_t kAttributeTypePercent = 8;
constexpr size_t kAttributeTypeText = 9;
constexpr size_t kAttributeTypeComposite = 10;

AttributesBindingContextWrapper::AttributesBindingContextWrapper(ViewManager& viewManager,
                                                                 Valdi::AttributesBindingContext& bindingContext)
    : _viewManager(viewManager), _bindingContext(bindingContext) {}

AttributesBindingContextWrapper::~AttributesBindingContextWrapper() = default;

static std::vector<snap::valdi_core::CompositeAttributePart> unmarshallCompositeAttributeParts(JavaEnv /*env*/,
                                                                                               jobject javaParts) {
    auto* array = reinterpret_cast<jobjectArray>(javaParts);
    std::vector<snap::valdi_core::CompositeAttributePart> parts;

    JavaEnv::accessEnv([&](JNIEnv& jni) {
        auto length = jni.GetArrayLength(array);

        for (jsize i = 0; i < length; i++) {
            auto element = ::djinni::LocalRef<jobject>(jni.GetObjectArrayElement(array, i));

            parts.emplace_back(djinni_generated_client::valdi_core::NativeCompositeAttributePart::toCpp(&jni, element));
        }
    });

    return parts;
}

jint AttributesBindingContextWrapper::bindAttributes(
    jint type, jstring name, jboolean invalidateLayoutOnChange, jobject delegate, jobject parts) {
    JavaEnv env;
    auto attributeName = toInternedString(env, name);
    auto invalidateLayoutOnChangeCpp = static_cast<bool>(invalidateLayoutOnChange);

    Valdi::AttributeId attributeId;

    switch (static_cast<size_t>(type)) {
        case kAttributeTypeUntyped:
            attributeId =
                _bindingContext.bindUntypedAttribute(attributeName,
                                                     invalidateLayoutOnChangeCpp,
                                                     Valdi::makeShared<ObjectAttributeHandlerDelegate>(delegate));
            break;
        case kAttributeTypeInt:
            attributeId = _bindingContext.bindIntAttribute(
                attributeName, invalidateLayoutOnChangeCpp, Valdi::makeShared<IntAttributeHandlerDelegate>(delegate));
            break;
        case kAttributeTypeBoolean:
            attributeId =
                _bindingContext.bindBooleanAttribute(attributeName,
                                                     invalidateLayoutOnChangeCpp,
                                                     Valdi::makeShared<BooleanAttributeHandlerDelegate>(delegate));
            break;
        case kAttributeTypeDouble:
            attributeId = _bindingContext.bindDoubleAttribute(
                attributeName, invalidateLayoutOnChangeCpp, Valdi::makeShared<FloatAttributeHandlerDelegate>(delegate));
            break;
        case kAttributeTypeString:
            attributeId =
                _bindingContext.bindStringAttribute(attributeName,
                                                    invalidateLayoutOnChangeCpp,
                                                    Valdi::makeShared<ObjectAttributeHandlerDelegate>(delegate));
            break;
        case kAttributeTypeColor:
            attributeId = _bindingContext.bindColorAttribute(
                attributeName, invalidateLayoutOnChangeCpp, Valdi::makeShared<LongAttributeHandlerDelegate>(delegate));
            break;
        case kAttributeTypeBorder:
            attributeId =
                _bindingContext.bindBorderAttribute(attributeName,
                                                    invalidateLayoutOnChangeCpp,
                                                    Valdi::makeShared<CornersAttributeHandlerDelegate>(delegate));
            break;
        case kAttributeTypePercent:
            attributeId =
                _bindingContext.bindPercentAttribute(attributeName,
                                                     invalidateLayoutOnChangeCpp,
                                                     Valdi::makeShared<PercentAttributeHandlerDelegate>(delegate));
            break;
        case kAttributeTypeText:
            attributeId =
                _bindingContext.bindTextAttribute(attributeName,
                                                  invalidateLayoutOnChangeCpp,
                                                  Valdi::makeShared<ObjectAttributeHandlerDelegate>(delegate));
            break;
        case kAttributeTypeComposite: {
            auto cppParts = unmarshallCompositeAttributeParts(env, parts);

            attributeId = _bindingContext.bindCompositeAttribute(
                attributeName, cppParts, Valdi::makeShared<ObjectAttributeHandlerDelegate>(delegate));
        } break;
        default:
            throwJavaValdiException(JavaEnv::getUnsafeEnv(),
                                    Valdi::Error(STRING_FORMAT("Invalid attribute type '{}'", type)));
            return static_cast<jint>(0);
    }

    return static_cast<jint>(attributeId);
}

void AttributesBindingContextWrapper::bindTransformAttributes(jobject delegate) {
    _bindingContext.bindTransformAttributes(Valdi::makeShared<ObjectAttributeHandlerDelegate>(delegate));
}

void AttributesBindingContextWrapper::bindScrollAttributes() {
    _bindingContext.bindScrollAttributes();
}

void AttributesBindingContextWrapper::bindAssetAttributes(snap::valdi_core::AssetOutputType assetOutputType) {
    _bindingContext.bindAssetAttributes(assetOutputType);
}

void AttributesBindingContextWrapper::setPlaceholderViewMeasureDelegate(jobject legacyMeasureDelegate) {
    _bindingContext.setMeasureDelegate(
        Valdi::makeShared<AndroidLegacyMeasureDelegate>(legacyMeasureDelegate, _viewManager));
}

void AttributesBindingContextWrapper::setMeasureDelegate(jobject measureDelegate) {
    _bindingContext.setMeasureDelegate(Valdi::makeShared<AndroidMeasureDelegate>(measureDelegate, _viewManager));
}

void AttributesBindingContextWrapper::registerAttributePreprocessor(jstring name,
                                                                    jboolean enableCache,
                                                                    jobject preprocessor) {
    JavaEnv env;
    auto attributeName = toInternedString(env, name);
    auto enableCacheCpp = static_cast<bool>(enableCache);

    auto javaPreprocessor = Valdi::makeShared<ValueFunctionWithJavaFunction>(env, preprocessor);

    _bindingContext.registerPreprocessor(
        attributeName, enableCacheCpp, [javaPreprocessor](const auto& value) -> Valdi::Result<Valdi::Value> {
            return javaPreprocessor->call(Valdi::ValueFunctionFlagsNone, {value});
        });
}

} // namespace ValdiAndroid
