//
//  AndroidResourceLoader.cpp
//  valdi-android
//
//  Created by Simon Corsin on 10/1/19.
//

#include "valdi/android/ResourceLoader.hpp"
#include "valdi_core/cpp/Attributes/ImageFilter.hpp"
#include "valdi_core/jni/JavaUtils.hpp"

#include "utils/base/NonCopyable.hpp"

#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#endif

namespace ValdiAndroid {

#ifdef __ANDROID__
class ManagedAsset : public Valdi::SimpleRefCountable, public snap::NonCopyable {
public:
    explicit ManagedAsset(AAsset* asset) : _asset(asset) {}

    ~ManagedAsset() override {
        AAsset_close(_asset);
    }

private:
    AAsset* _asset;
};
#endif

ResourceLoader::ResourceLoader(JavaEnv env,
                               jobject localResourceResolver,
                               jobject assetManager,
                               bool shouldDeferModuleLoadToJava)
    : _localResourceResolver(env, localResourceResolver, "ResourceResolver"),
      _assetManagerJava(JavaEnv::newGlobalRef(assetManager)),
      _shouldDeferModuleLoadToJava(shouldDeferModuleLoadToJava) {
    auto localResourceResolverRef = _localResourceResolver.toObject();
    auto cls = localResourceResolverRef.getClass();
    cls.getMethod("getCustomModuleData", _getCustomModuleDataMethod);
    cls.getMethod("resolveResource", _resolveResourceMethod);
    cls.getMethod("requestPayloadFromURL", _requestPayloadFromURLMethod);
    cls.getMethod("loadAsset", _loadAssetMethod);
    cls.getMethod("loadAssetFromBytes", _loadAssetFromBytes);
    cls.getMethod("loadAssetFromBitmap", _loadAssetFromBitmap);

#ifdef __ANDROID__
    _assetManager = AAssetManager_fromJava(JavaEnv::getUnsafeEnv(), _assetManagerJava.get());
#endif
}

ResourceLoader::~ResourceLoader() = default;

Valdi::Shared<Valdi::IResourceLoader> ResourceLoader::withCustomResourceResolver(jobject customResourceResolver) const {
    return Valdi::makeShared<ResourceLoader>(JavaEnv(), customResourceResolver, _assetManagerJava.get(), true)
        .toShared();
}

Valdi::Result<Valdi::BytesView> ResourceLoader::loadModuleContent(const Valdi::StringBox& module) {
    if (_shouldDeferModuleLoadToJava) {
        auto modulePathJava = toJavaObject(JavaEnv(), module);
        Valdi::SimpleExceptionTracker exceptionTracker;
        Valdi::Marshaller errorHolder(exceptionTracker);

        auto moduleData = _getCustomModuleDataMethod.call(
            _localResourceResolver.toObject(), modulePathJava, reinterpret_cast<jlong>(&errorHolder));

        if (errorHolder.size() > 0) {
            return errorHolder.getOrUndefined(-1).getError();
        }

        if (!moduleData.isNull()) {
            return toByteArray(JavaEnv(), reinterpret_cast<jbyteArray>(moduleData.getUnsafeObject()));
        }
    }

#ifdef __ANDROID__
    auto* asset = AAssetManager_open(_assetManager, module.getCStr(), AASSET_MODE_BUFFER);
    if (asset == nullptr) {
        return Valdi::Error("Unable to open asset");
    }
    auto managedAsset = Valdi::makeShared<ManagedAsset>(asset);

    const auto* buffer = AAsset_getBuffer(asset);
    auto length = AAsset_getLength(asset);

    return Valdi::BytesView(managedAsset, reinterpret_cast<const Valdi::Byte*>(buffer), static_cast<size_t>(length));
#else
    return Valdi::Error("Not implemented");
#endif
}

Valdi::StringBox ResourceLoader::resolveLocalAssetURL(const Valdi::StringBox& moduleName,
                                                      const Valdi::StringBox& resourcePath) {
    auto env = JavaEnv();
    auto javaModuleName = ValdiAndroid::toJavaObject(env, moduleName);
    auto javaResourcePath = ValdiAndroid::toJavaObject(env, resourcePath);
    auto resolvedResource =
        _resolveResourceMethod.call(_localResourceResolver.toObject(), javaModuleName, javaResourcePath);
    auto value = ValdiAndroid::toValue(env, resolvedResource.stealLocalRef());

    if (!value.isString()) {
        return Valdi::StringBox();
    }

    return value.toStringBox();
}

JavaObject ResourceLoader::requestPayloadFromURL(const JavaObject& imageLoader,
                                                 const String& url,
                                                 int64_t marshallerHandle) {
    return _requestPayloadFromURLMethod.call(_localResourceResolver.toObject(), imageLoader, url, marshallerHandle);
}

static JavaObject colorMatrixToFloatArray(const float* colorMatrixFilter) {
    if (colorMatrixFilter == nullptr) {
        return JavaObject(JavaEnv());
    }

    return toJavaFloatArray(JavaEnv(), colorMatrixFilter, Valdi::ImageFilter::kColorMatrixSize);
}

JavaObject ResourceLoader::loadAsset(const JavaObject& imageLoader,
                                     const JavaObject& payload,
                                     int32_t preferredWidth,
                                     int32_t preferredHeight,
                                     const float* colorMatrixFilter,
                                     float blurRadiusFilter,
                                     int32_t outputType,
                                     int64_t callbackHandle) {
    auto jColorMatrixFilter = colorMatrixToFloatArray(colorMatrixFilter);

    return _loadAssetMethod.call(_localResourceResolver.toObject(),
                                 imageLoader,
                                 payload,
                                 preferredWidth,
                                 preferredHeight,
                                 jColorMatrixFilter,
                                 blurRadiusFilter,
                                 outputType,
                                 callbackHandle);
}

JavaObject ResourceLoader::loadAssetFromBytes(const Valdi::BytesView& bytes,
                                              int32_t preferredWidth,
                                              int32_t preferredHeight,
                                              const float* colorMatrixFilter,
                                              float blurRadiusFilter,
                                              int64_t callbackHandle) {
    auto jColorMatrixFilter = colorMatrixToFloatArray(colorMatrixFilter);
    auto jBytes = toJavaObject(JavaEnv(), bytes);

    return _loadAssetFromBytes.call(_localResourceResolver.toObject(),
                                    jBytes,
                                    preferredWidth,
                                    preferredHeight,
                                    jColorMatrixFilter,
                                    blurRadiusFilter,
                                    callbackHandle);
}

JavaObject ResourceLoader::loadAssetFromBitmap(const JavaObject& bitmap,
                                               int32_t preferredWidth,
                                               int32_t preferredHeight,
                                               const float* colorMatrixFilter,
                                               float blurRadiusFilter,
                                               int64_t callbackHandle) {
    auto jColorMatrixFilter = colorMatrixToFloatArray(colorMatrixFilter);

    return _loadAssetFromBitmap.call(_localResourceResolver.toObject(),
                                     bitmap,
                                     preferredWidth,
                                     preferredHeight,
                                     jColorMatrixFilter,
                                     blurRadiusFilter,
                                     callbackHandle);
}

} // namespace ValdiAndroid
