//
//  AndroidResourceLoader.hpp
//  valdi-android
//
//  Created by Simon Corsin on 10/1/19.
//

#pragma once

#include "valdi/runtime/Interfaces/IResourceLoader.hpp"
#include "valdi/runtime/Resources/AssetLoader.hpp"
#include "valdi_core/jni/GlobalRefJavaObject.hpp"
#include "valdi_core/jni/JavaEnv.hpp"
#include "valdi_core/jni/JavaMethod.hpp"

struct AAssetManager;

namespace ValdiAndroid {

class ResourceLoader : public Valdi::SharedPtrRefCountable, public Valdi::IResourceLoader {
public:
    ResourceLoader(JavaEnv env, jobject localResourceResolver, jobject assetManager, bool shouldDeferModuleLoadToJava);
    ~ResourceLoader() override;

    Valdi::Shared<Valdi::IResourceLoader> withCustomResourceResolver(jobject customResourceResolver) const;

    Valdi::Result<Valdi::BytesView> loadModuleContent(const Valdi::StringBox& module) override;

    Valdi::StringBox resolveLocalAssetURL(const Valdi::StringBox& moduleName,
                                          const Valdi::StringBox& resourcePath) override;

    JavaObject requestPayloadFromURL(const JavaObject& imageLoader, const String& url, int64_t marshallerHandle);
    JavaObject loadAsset(const JavaObject& imageLoader,
                         const JavaObject& payload,
                         int32_t preferredWidth,
                         int32_t preferredHeight,
                         const float* colorMatrixFilter,
                         float blurRadiusFilter,
                         int32_t outputType,
                         int64_t callbackHandle);

    JavaObject loadAssetFromBytes(const Valdi::BytesView& bytes,
                                  int32_t preferredWidth,
                                  int32_t preferredHeight,
                                  const float* colorMatrixFilter,
                                  float blurRadiusFilter,
                                  int64_t callbackHandle);

    JavaObject loadAssetFromBitmap(const JavaObject& bitmap,
                                   int32_t preferredWidth,
                                   int32_t preferredHeight,
                                   const float* colorMatrixFilter,
                                   float blurRadiusFilter,
                                   int64_t callbackHandle);

private:
    [[maybe_unused]] AAssetManager* _assetManager = nullptr;
    GlobalRefJavaObjectBase _localResourceResolver;
    djinni::GlobalRef<jobject> _assetManagerJava;
    JavaMethod<ByteArray, String, int64_t> _getCustomModuleDataMethod;
    JavaMethod<JavaObject, String, String> _resolveResourceMethod;
    JavaMethod<JavaObject, JavaObject, String, int64_t> _requestPayloadFromURLMethod;
    JavaMethod<JavaObject, JavaObject, JavaObject, int32_t, int32_t, JavaObject, float, int32_t, int64_t>
        _loadAssetMethod;
    JavaMethod<JavaObject, ByteArray, int32_t, int32_t, JavaObject, float, int64_t> _loadAssetFromBytes;
    JavaMethod<JavaObject, JavaObject, int32_t, int32_t, JavaObject, float, int64_t> _loadAssetFromBitmap;
    bool _shouldDeferModuleLoadToJava;
};

} // namespace ValdiAndroid
