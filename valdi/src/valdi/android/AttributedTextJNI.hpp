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

    static void registerNatives() {
        javaClassStatic()->registerNatives({
            makeNativeMethod("nativeGetPartsSize", AttributedTextJNI::nativeGetPartsSize),
            makeNativeMethod("nativeGetContent", AttributedTextJNI::nativeGetContent),
            makeNativeMethod("nativeGetFont", AttributedTextJNI::nativeGetFont),
            makeNativeMethod("nativeGetTextDecoration", AttributedTextJNI::nativeGetTextDecoration),
            makeNativeMethod("nativeGetColor", AttributedTextJNI::nativeGetColor),
            makeNativeMethod("nativeGetOnTap", AttributedTextJNI::nativeGetOnTap),
            makeNativeMethod("nativeGetOnLayout", AttributedTextJNI::nativeGetOnLayout),
            makeNativeMethod("nativeGetOutlineColor", AttributedTextJNI::nativeGetOutlineColor),
            makeNativeMethod("nativeGetOutlineWidth", AttributedTextJNI::nativeGetOutlineWidth),
            makeNativeMethod("nativeGetImageAttachmentWidth", AttributedTextJNI::nativeGetImageAttachmentWidth),
            makeNativeMethod("nativeGetImageAttachmentHeight", AttributedTextJNI::nativeGetImageAttachmentHeight),
            makeNativeMethod("nativeGetImageAttachmentData", AttributedTextJNI::nativeGetImageAttachmentData),
        });
    }
};

} // namespace ValdiAndroid
