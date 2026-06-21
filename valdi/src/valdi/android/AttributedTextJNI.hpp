#include "valdi_core/cpp/Attributes/TextAttributeValue.hpp"
#include "valdi_core/jni/JNIMethodUtils.hpp"
#include "valdi_core/jni/JavaUtils.hpp"
#include <fbjni/fbjni.h>

namespace fbjni = facebook::jni;

namespace ValdiAndroid {

class AttributedTextJNI : public fbjni::JavaClass<AttributedTextJNI> {
public:
    static constexpr auto kJavaDescriptor = "Lcom/snap/valdi/attributes/impl/richtext/AttributedTextCpp;";

    static Valdi::Ref<Valdi::TextAttributeValue> getAttributedText(jlong ptr) {
        return valueFromJavaCppHandle(ptr).getTypedRef<Valdi::TextAttributeValue>();
    }

    // NOLINTNEXTLINE
    static jint nativeGetPartsSize(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr) {
        auto attributedText = getAttributedText(ptr);
        return static_cast<jint>(attributedText->getPartsSize());
    }

    // NOLINTNEXTLINE
    static jstring nativeGetContent(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr, jint index) {
        auto attributedText = getAttributedText(ptr);
        const auto& str = attributedText->getContentAtIndex(index);

        auto javaStr = toJavaObject(JavaEnv(), str);
        return reinterpret_cast<jstring>(javaStr.releaseObject());
    }

    // NOLINTNEXTLINE
    static jstring nativeGetFont(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr, jint index) {
        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));

        if (!style.font) {
            return nullptr;
        }

        auto javaStr = toJavaObject(JavaEnv(), style.font.value());
        return reinterpret_cast<jstring>(javaStr.releaseObject());
    }

    // NOLINTNEXTLINE
    static jint nativeGetTextDecoration(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr, jint index) {
        static constexpr jint kTextDecorationUnset = static_cast<jint>(std::numeric_limits<int32_t>::min());
        static constexpr jint kTextDecorationNone = static_cast<jint>(0);
        static constexpr jint kTextDecorationUnderline = static_cast<jint>(1);
        static constexpr jint kTextDecorationStrikethrough = static_cast<jint>(2);
        static constexpr jint kTextDecorationDashedUnderline = static_cast<jint>(3);
        static constexpr jint kTextDecorationDottedUnderline = static_cast<jint>(4);

        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        switch (style.textDecoration) {
            case Valdi::TextDecoration::Unset:
                return kTextDecorationUnset;
            case Valdi::TextDecoration::None:
                return kTextDecorationNone;
            case Valdi::TextDecoration::Underline:
                return kTextDecorationUnderline;
            case Valdi::TextDecoration::Strikethrough:
                return kTextDecorationStrikethrough;
            case Valdi::TextDecoration::DashedUnderline:
                return kTextDecorationDashedUnderline;
            case Valdi::TextDecoration::DottedUnderline:
                return kTextDecorationDottedUnderline;
        }
    }

    // NOLINTNEXTLINE
    static jlong nativeGetColor(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr, jint index) {
        static constexpr jlong kColorUnset = static_cast<jlong>(std::numeric_limits<int64_t>::min());

        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        if (!style.color) {
            return kColorUnset;
        }

        return static_cast<jlong>(style.color.value().value);
    }

    // NOLINTNEXTLINE
    static jlong nativeGetBackgroundColor(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr, jint index) {
        static constexpr jlong kColorUnset = static_cast<jlong>(std::numeric_limits<int64_t>::min());

        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        if (style.background == nullptr || !style.background->color) {
            return kColorUnset;
        }

        return static_cast<jlong>(style.background->color.value().value);
    }

    // NOLINTNEXTLINE
    static jobject nativeGetOnTap(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr, jint index) {
        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        if (style.onTap == nullptr) {
            return nullptr;
        }

        return toJavaObject(JavaEnv(), style.onTap).releaseObject();
    }

    // NOLINTNEXTLINE
    static jobject nativeGetOnLayout(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr, jint index) {
        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        if (style.onLayout == nullptr) {
            return nullptr;
        }

        return toJavaObject(JavaEnv(), style.onLayout).releaseObject();
    }

    // NOLINTNEXTLINE
    static jlong nativeGetOutlineColor(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr, jint index) {
        static constexpr jlong kColorUnset = static_cast<jlong>(std::numeric_limits<int64_t>::min());

        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        if (!style.outlineColor) {
            return kColorUnset;
        }

        return static_cast<jlong>(style.outlineColor.value().value);
    }

    // NOLINTNEXTLINE
    static jdouble nativeGetOutlineWidth(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr, jint index) {
        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        return style.outlineWidth.has_value() ? static_cast<jdouble>(style.outlineWidth.value()) : 0.0;
    }

    // NOLINTNEXTLINE
    static jdouble nativeGetAnimationTranslationY(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr, jint index) {
        static constexpr jdouble kUnset = static_cast<jdouble>(std::numeric_limits<double>::quiet_NaN());

        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        if (!style.animationTransform) {
            return kUnset;
        }

        return static_cast<jdouble>(style.animationTransform->translationY);
    }

    // NOLINTNEXTLINE
    static jdouble nativeGetAnimationScale(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr, jint index) {
        static constexpr jdouble kUnset = static_cast<jdouble>(std::numeric_limits<double>::quiet_NaN());

        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        if (!style.animationTransform) {
            return kUnset;
        }

        return static_cast<jdouble>(style.animationTransform->scale);
    }

    // NOLINTNEXTLINE
    static jdouble nativeGetAnimationOpacity(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr, jint index) {
        static constexpr jdouble kUnset = static_cast<jdouble>(std::numeric_limits<double>::quiet_NaN());

        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        if (!style.animationTransform) {
            return kUnset;
        }

        return static_cast<jdouble>(style.animationTransform->opacity);
    }

    // NOLINTNEXTLINE
    static jfloat nativeGetImageAttachmentWidth(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr, jint index) {
        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        if (!style.imageAttachment) {
            return 0.0f;
        }
        return static_cast<jfloat>(style.imageAttachment->width);
    }

    // NOLINTNEXTLINE
    static jfloat nativeGetImageAttachmentHeight(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr, jint index) {
        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        if (!style.imageAttachment) {
            return 0.0f;
        }
        return static_cast<jfloat>(style.imageAttachment->height);
    }

    // NOLINTNEXTLINE
    static jbyteArray nativeGetImageAttachmentData(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr, jint index) {
        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        if (!style.imageAttachment || style.imageAttachment->imageData.empty()) {
            return nullptr;
        }

        JNIEnv* env = fbjni::Environment::current();
        const auto& data = style.imageAttachment->imageData;
        jbyteArray result = env->NewByteArray(static_cast<jsize>(data.size()));
        if (result != nullptr) {
            env->SetByteArrayRegion(
                result, 0, static_cast<jsize>(data.size()), reinterpret_cast<const jbyte*>(data.data()));
        }
        return result;
    }

    // NOLINTNEXTLINE
    static jint nativeGetInlineViewAttachmentChildIndex(fbjni::alias_ref<fbjni::JClass> /* clazz */,
                                                        jlong ptr,
                                                        jint index) {
        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        if (style.inlineViewAttachment == nullptr) {
            return -1;
        }
        return static_cast<jint>(style.inlineViewAttachment->getChildIndex());
    }

    // NOLINTNEXTLINE
    static jint nativeGetInlineViewAttachmentVerticalAlignment(fbjni::alias_ref<fbjni::JClass> /* clazz */,
                                                              jlong ptr,
                                                              jint index) {
        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        if (style.inlineViewAttachment == nullptr) {
            return static_cast<jint>(Valdi::InlineViewVerticalAlignment::Center);
        }
        return static_cast<jint>(style.inlineViewAttachment->getVerticalAlignment());
    }

    // NOLINTNEXTLINE
    static jfloat nativeGetInlineViewAttachmentWidth(fbjni::alias_ref<fbjni::JClass> /* clazz */,
                                                     jlong ptr,
                                                     jint index) {
        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        if (style.inlineViewAttachment == nullptr) {
            return 0.0f;
        }
        return static_cast<jfloat>(style.inlineViewAttachment->getSize().width);
    }

    // NOLINTNEXTLINE
    static jfloat nativeGetInlineViewAttachmentHeight(fbjni::alias_ref<fbjni::JClass> /* clazz */,
                                                      jlong ptr,
                                                      jint index) {
        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        if (style.inlineViewAttachment == nullptr) {
            return 0.0f;
        }
        return static_cast<jfloat>(style.inlineViewAttachment->getSize().height);
    }

    // NOLINTNEXTLINE
    static jboolean nativeHasAnimationTransform(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr, jint index) {
        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        return style.animationTransform.has_value();
    }

    // NOLINTNEXTLINE
    static jint nativeGetAnimationTransformsSize(fbjni::alias_ref<fbjni::JClass> /* clazz */, jlong ptr) {
        auto attributedText = getAttributedText(ptr);
        return static_cast<jint>(attributedText->getAnimationTransformsSize());
    }

    static const Valdi::TextAnimationTransform* getAnimationTransformOrThrow(jlong ptr, jint index) {
        auto attributedText = getAttributedText(ptr);
        const auto& style = attributedText->getStyleAtIndex(static_cast<size_t>(index));
        if (!style.animationTransform.has_value()) {
            throwJavaValdiException(
                JavaEnv::getUnsafeEnv(), "AttributedText part does not have an animation transform");
            return nullptr;
        }
        return &style.animationTransform.value();
    }

    // NOLINTNEXTLINE
    static jstring nativeGetAnimationTransformKey(fbjni::alias_ref<fbjni::JClass> /* clazz */,
                                                  jlong ptr,
                                                  jint index) {
        const auto* animationTransform = getAnimationTransformOrThrow(ptr, index);
        if (animationTransform == nullptr || !animationTransform->key) {
            return nullptr;
        }

        auto javaStr = toJavaObject(JavaEnv(), animationTransform->key.value());
        return reinterpret_cast<jstring>(javaStr.releaseObject());
    }

    // NOLINTNEXTLINE
    static jfloat nativeGetAnimationTransformTranslationY(fbjni::alias_ref<fbjni::JClass> /* clazz */,
                                                         jlong ptr,
                                                         jint index) {
        const auto* animationTransform = getAnimationTransformOrThrow(ptr, index);
        if (animationTransform == nullptr) {
            return 0.0f;
        }
        return static_cast<jfloat>(animationTransform->translationY);
    }

    // NOLINTNEXTLINE
    static jfloat nativeGetAnimationTransformScale(fbjni::alias_ref<fbjni::JClass> /* clazz */,
                                                   jlong ptr,
                                                   jint index) {
        const auto* animationTransform = getAnimationTransformOrThrow(ptr, index);
        if (animationTransform == nullptr) {
            return 0.0f;
        }
        return static_cast<jfloat>(animationTransform->scale);
    }

    // NOLINTNEXTLINE
    static jfloat nativeGetAnimationTransformOpacity(fbjni::alias_ref<fbjni::JClass> /* clazz */,
                                                     jlong ptr,
                                                     jint index) {
        const auto* animationTransform = getAnimationTransformOrThrow(ptr, index);
        if (animationTransform == nullptr) {
            return 0.0f;
        }
        return static_cast<jfloat>(animationTransform->opacity);
    }

    // NOLINTNEXTLINE
    static jdouble nativeGetAnimationTransformDuration(fbjni::alias_ref<fbjni::JClass> /* clazz */,
                                                       jlong ptr,
                                                       jint index) {
        const auto* animationTransform = getAnimationTransformOrThrow(ptr, index);
        if (animationTransform == nullptr) {
            return 0.0;
        }
        return static_cast<jdouble>(animationTransform->duration);
    }

    // NOLINTNEXTLINE
    static jdouble nativeGetAnimationTransformTimeOffsetBetweenParts(fbjni::alias_ref<fbjni::JClass> /* clazz */,
                                                                     jlong ptr,
                                                                     jint index) {
        const auto* animationTransform = getAnimationTransformOrThrow(ptr, index);
        if (animationTransform == nullptr) {
            return 0.0;
        }
        return static_cast<jdouble>(animationTransform->timeOffsetBetweenParts);
    }

    // NOLINTNEXTLINE
    static jint nativeGetAnimationTransformGroupIndex(fbjni::alias_ref<fbjni::JClass> /* clazz */,
                                                      jlong ptr,
                                                      jint index) {
        const auto* animationTransform = getAnimationTransformOrThrow(ptr, index);
        if (animationTransform == nullptr) {
            return 0;
        }
        return static_cast<jint>(animationTransform->groupIndex);
    }

    // NOLINTNEXTLINE
    static jint nativeGetAnimationTransformPartIndexInGroup(fbjni::alias_ref<fbjni::JClass> /* clazz */,
                                                            jlong ptr,
                                                            jint index) {
        const auto* animationTransform = getAnimationTransformOrThrow(ptr, index);
        if (animationTransform == nullptr) {
            return 0;
        }
        return static_cast<jint>(animationTransform->partIndexInGroup);
    }

    // NOLINTNEXTLINE
    static jstring nativeGetAnimationTransformPartPattern(fbjni::alias_ref<fbjni::JClass> /* clazz */,
                                                          jlong ptr,
                                                          jint index) {
        const auto* animationTransform = getAnimationTransformOrThrow(ptr, index);
        if (animationTransform == nullptr || animationTransform->partPattern.isEmpty()) {
            return nullptr;
        }

        auto javaStr = toJavaObject(JavaEnv(), animationTransform->partPattern);
        return reinterpret_cast<jstring>(javaStr.releaseObject());
    }

    static void registerNatives() {
        javaClassStatic()->registerNatives({
            makeNativeMethod("nativeGetPartsSize", AttributedTextJNI::nativeGetPartsSize),
            makeNativeMethod("nativeGetContent", AttributedTextJNI::nativeGetContent),
            makeNativeMethod("nativeGetFont", AttributedTextJNI::nativeGetFont),
            makeNativeMethod("nativeGetTextDecoration", AttributedTextJNI::nativeGetTextDecoration),
            makeNativeMethod("nativeGetColor", AttributedTextJNI::nativeGetColor),
            makeNativeMethod("nativeGetBackgroundColor", AttributedTextJNI::nativeGetBackgroundColor),
            makeNativeMethod("nativeGetOnTap", AttributedTextJNI::nativeGetOnTap),
            makeNativeMethod("nativeGetOnLayout", AttributedTextJNI::nativeGetOnLayout),
            makeNativeMethod("nativeGetOutlineColor", AttributedTextJNI::nativeGetOutlineColor),
            makeNativeMethod("nativeGetOutlineWidth", AttributedTextJNI::nativeGetOutlineWidth),
            makeNativeMethod("nativeGetAnimationTranslationY", AttributedTextJNI::nativeGetAnimationTranslationY),
            makeNativeMethod("nativeGetAnimationScale", AttributedTextJNI::nativeGetAnimationScale),
            makeNativeMethod("nativeGetAnimationOpacity", AttributedTextJNI::nativeGetAnimationOpacity),
            makeNativeMethod("nativeGetImageAttachmentWidth", AttributedTextJNI::nativeGetImageAttachmentWidth),
            makeNativeMethod("nativeGetImageAttachmentHeight", AttributedTextJNI::nativeGetImageAttachmentHeight),
            makeNativeMethod("nativeGetImageAttachmentData", AttributedTextJNI::nativeGetImageAttachmentData),
            makeNativeMethod("nativeGetInlineViewAttachmentChildIndex",
                             AttributedTextJNI::nativeGetInlineViewAttachmentChildIndex),
            makeNativeMethod("nativeGetInlineViewAttachmentVerticalAlignment",
                             AttributedTextJNI::nativeGetInlineViewAttachmentVerticalAlignment),
            makeNativeMethod("nativeGetInlineViewAttachmentWidth",
                             AttributedTextJNI::nativeGetInlineViewAttachmentWidth),
            makeNativeMethod("nativeGetInlineViewAttachmentHeight",
                             AttributedTextJNI::nativeGetInlineViewAttachmentHeight),
            makeNativeMethod("nativeGetAnimationTransformsSize", AttributedTextJNI::nativeGetAnimationTransformsSize),
            makeNativeMethod("nativeHasAnimationTransform", AttributedTextJNI::nativeHasAnimationTransform),
            makeNativeMethod("nativeGetAnimationTransformKey", AttributedTextJNI::nativeGetAnimationTransformKey),
            makeNativeMethod("nativeGetAnimationTransformTranslationY",
                             AttributedTextJNI::nativeGetAnimationTransformTranslationY),
            makeNativeMethod("nativeGetAnimationTransformScale", AttributedTextJNI::nativeGetAnimationTransformScale),
            makeNativeMethod("nativeGetAnimationTransformOpacity",
                             AttributedTextJNI::nativeGetAnimationTransformOpacity),
            makeNativeMethod("nativeGetAnimationTransformDuration",
                             AttributedTextJNI::nativeGetAnimationTransformDuration),
            makeNativeMethod("nativeGetAnimationTransformTimeOffsetBetweenParts",
                             AttributedTextJNI::nativeGetAnimationTransformTimeOffsetBetweenParts),
            makeNativeMethod("nativeGetAnimationTransformGroupIndex",
                             AttributedTextJNI::nativeGetAnimationTransformGroupIndex),
            makeNativeMethod("nativeGetAnimationTransformPartIndexInGroup",
                             AttributedTextJNI::nativeGetAnimationTransformPartIndexInGroup),
            makeNativeMethod("nativeGetAnimationTransformPartPattern",
                             AttributedTextJNI::nativeGetAnimationTransformPartPattern),
        });
    }
};

} // namespace ValdiAndroid
