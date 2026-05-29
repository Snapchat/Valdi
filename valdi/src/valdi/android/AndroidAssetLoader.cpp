//
//  AndroidAssetLoader.cpp
//  valdi-android
//
//  Created by Simon Corsin on 8/13/21.
//

#include "valdi/android/AndroidAssetLoader.hpp"
#include "valdi/android/AndroidBitmap.hpp"
#include "valdi/android/AndroidBitmapFactory.hpp"
#include "valdi/android/ResourceLoader.hpp"
#include "valdi/runtime/Interfaces/IRemoteDownloader.hpp"
#include "valdi/runtime/Resources/AssetLoaderCompletion.hpp"
#include "valdi/svg/SVGRenderer.hpp"
#include "valdi_core/NativeCancelable.hpp"
#include "valdi_core/cpp/Attributes/ImageFilter.hpp"
#include "valdi_core/cpp/Resources/LoadedAsset.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Marshaller.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_core/jni/JavaCache.hpp"
#include "valdi_core/jni/JavaUtils.hpp"

namespace ValdiAndroid {

class AndroidLoadedAsset : public Valdi::LoadedAsset, public ValdiAndroid::GlobalRefJavaObjectBase {
public:
    AndroidLoadedAsset(JavaEnv env, jobject valdiAsset, jboolean isImage)
        : ValdiAndroid::GlobalRefJavaObjectBase(env, valdiAsset, "ValdiAsset"), _isImage(isImage) {}

    ~AndroidLoadedAsset() override {
        if (static_cast<bool>(_isImage)) {
            VALDI_TRACE("Valdi.releaseBitmapHandler");
            JavaEnv::getCache().getBitmapHandlerReleaseMethod().call(toObject());
        }
    }

    Valdi::Result<Valdi::BytesView> getBytesContent() override {
        Valdi::SimpleExceptionTracker exceptionTracker;
        Valdi::Marshaller errorHolder(exceptionTracker);

        auto ret = JavaEnv::getCache().getValdiImageOnRetrieveContentMethod().call(
            toObject(), reinterpret_cast<jlong>(&errorHolder));
        if (errorHolder.size() > 0) {
            return errorHolder.getOrUndefined(-1).getError();
        }

        auto value = ValdiAndroid::toValue(ret.getEnv(), ret.stealLocalRef());

        if (value.isString()) {
            Valdi::Path path(value.toStringBox().toStringView());
            auto loadResult = Valdi::DiskUtils::load(path);
            if (!loadResult) {
                return loadResult.moveError();
            }

            return Valdi::BytesView(loadResult.value());

        } else if (value.isTypedArray()) {
            return Valdi::BytesView(value.getTypedArray()->getBuffer());
        } else {
            return Valdi::LoadedAsset::getBytesContent();
        }
    }

    Valdi::Ref<Valdi::IBitmap> getBitmap() {
        if (static_cast<bool>(_isImage)) {
            auto javaObject = toObject();
            JavaEnv::getCache().getBitmapHandlerRetainMethod().call(javaObject);
            return Valdi::makeShared<AndroidBitmapHandler>(getEnv(), javaObject.getUnsafeObject(), true);
        }
        return NULL;
    }

    VALDI_CLASS_HEADER_IMPL(AndroidLoadedAsset)
private:
    jboolean _isImage;
};

static std::vector<Valdi::StringBox> getUrlSchemes(JavaEnv env, jobject supportedSchemes) {
    std::vector<Valdi::StringBox> urlSchemes;

    JavaObjectArray array(reinterpret_cast<jobjectArray>(supportedSchemes));

    auto length = array.size();
    for (size_t i = 0; i < length; i++) {
        auto element = array.getObject(i);
        urlSchemes.emplace_back(toInternedString(env, reinterpret_cast<jstring>(element.get())));
    }

    return urlSchemes;
}

static void loadAssetFromBytes(const Valdi::Ref<ResourceLoader>& resourceLoader,
                               const Valdi::BytesView& bytes,
                               int32_t preferredWidth,
                               int32_t preferredHeight,
                               const Valdi::Value& attachedData,
                               const Valdi::Ref<Valdi::AssetLoaderCompletion>& completion) {
    auto* completionHandle = Valdi::unsafeBridgeRetain(completion.get());

    const float* colorMatrixFilter = nullptr;
    float blurRadiusFilter = 0.0f;

    auto typedFilter = attachedData.getTypedRef<Valdi::ImageFilter>();
    if (typedFilter != nullptr) {
        if (!typedFilter->isIdentityColorMatrix()) {
            colorMatrixFilter = typedFilter->getColorMatrix();
        }
        blurRadiusFilter = typedFilter->getBlurRadius();
    }

    resourceLoader->loadAssetFromBytes(bytes,
                                       preferredWidth,
                                       preferredHeight,
                                       colorMatrixFilter,
                                       blurRadiusFilter,
                                       reinterpret_cast<jlong>(completionHandle));
}

static void loadAssetFromBitmap(const Valdi::Ref<ResourceLoader>& resourceLoader,
                                const Valdi::Ref<Valdi::IBitmap>& bitmap,
                                int32_t preferredWidth,
                                int32_t preferredHeight,
                                const Valdi::Value& attachedData,
                                const Valdi::Ref<Valdi::AssetLoaderCompletion>& completion) {
    auto androidBitmap = Valdi::castOrNull<AndroidBitmap>(bitmap);
    if (androidBitmap == nullptr) {
        completion->onLoadComplete(Valdi::Error("SVG rasterization did not return an Android bitmap"));
        return;
    }

    auto* completionHandle = Valdi::unsafeBridgeRetain(completion.get());

    const float* colorMatrixFilter = nullptr;
    float blurRadiusFilter = 0.0f;

    auto typedFilter = attachedData.getTypedRef<Valdi::ImageFilter>();
    if (typedFilter != nullptr) {
        if (!typedFilter->isIdentityColorMatrix()) {
            colorMatrixFilter = typedFilter->getColorMatrix();
        }
        blurRadiusFilter = typedFilter->getBlurRadius();
    }

    resourceLoader->loadAssetFromBitmap(androidBitmap->getJavaBitmap(),
                                        preferredWidth,
                                        preferredHeight,
                                        colorMatrixFilter,
                                        blurRadiusFilter,
                                        reinterpret_cast<jlong>(completionHandle));
}

class AndroidAssetLoaderWithDownloader : public Valdi::AssetLoader {
public:
    AndroidAssetLoaderWithDownloader(const Valdi::Ref<ResourceLoader>& resourceLoader,
                                     const std::vector<Valdi::StringBox>& urlSchemes,
                                     const Valdi::Ref<Valdi::IRemoteDownloader>& downloader,
                                     const Valdi::Ref<Valdi::DispatchQueue>& workerQueue)
        : Valdi::AssetLoader(urlSchemes),
          _resourceLoader(resourceLoader),
          _downloader(downloader),
          _workerQueue(workerQueue) {}
    ~AndroidAssetLoaderWithDownloader() override = default;

    snap::valdi_core::AssetOutputType getOutputType() const override {
        return snap::valdi_core::AssetOutputType::ImageAndroid;
    }

    Valdi::Result<Valdi::Value> requestPayloadFromURL(const Valdi::StringBox& url) override {
        return Valdi::Value(url);
    }

    Valdi::Shared<snap::valdi_core::Cancelable> loadAsset(
        const Valdi::Value& requestPayload,
        int32_t preferredWidth,
        int32_t preferredHeight,
        const Valdi::Value& attachedData,
        const Valdi::Ref<Valdi::AssetLoaderCompletion>& completion) override {
        return _downloader->downloadItem(
            requestPayload.toStringBox(),
            [resourceLoader = _resourceLoader,
             workerQueue = _workerQueue,
             preferredWidth,
             preferredHeight,
             attachedData,
             completion](auto result) {
                if (!result) {
                    completion->onLoadComplete(result.moveError());
                    return;
                }

                auto bytes = result.value();
                if (Valdi::SVGRenderer::isSVG(bytes)) {
                    workerQueue->async(
                        [resourceLoader, preferredWidth, preferredHeight, attachedData, completion, bytes]() {
                            auto bitmap = Valdi::SVGRenderer::rasterizeSVG(
                                bytes, AndroidBitmapFactory::getSharedInstance(), preferredWidth, preferredHeight);
                            if (!bitmap) {
                                completion->onLoadComplete(bitmap.moveError());
                                return;
                            }

                            loadAssetFromBitmap(resourceLoader,
                                                bitmap.moveValue(),
                                                preferredWidth,
                                                preferredHeight,
                                                attachedData,
                                                completion);
                        });
                    return;
                }

                loadAssetFromBytes(resourceLoader, bytes, preferredWidth, preferredHeight, attachedData, completion);
            });
    }

private:
    Valdi::Ref<ResourceLoader> _resourceLoader;
    Valdi::Ref<Valdi::IRemoteDownloader> _downloader;
    Valdi::Ref<Valdi::DispatchQueue> _workerQueue;
};

AndroidAssetLoaderFactory::AndroidAssetLoaderFactory(const Valdi::Ref<ResourceLoader>& resourceLoader,
                                                     const Valdi::Ref<Valdi::DispatchQueue>& workerQueue)
    : _resourceLoader(resourceLoader), _workerQueue(workerQueue) {}
AndroidAssetLoaderFactory::~AndroidAssetLoaderFactory() = default;

snap::valdi_core::AssetOutputType AndroidAssetLoaderFactory::getOutputType() const {
    return snap::valdi_core::AssetOutputType::ImageAndroid;
}

Valdi::Ref<Valdi::AssetLoader> AndroidAssetLoaderFactory::createAssetLoader(
    const std::vector<Valdi::StringBox>& urlSchemes, const Valdi::Ref<Valdi::IRemoteDownloader>& downloader) {
    return Valdi::makeShared<AndroidAssetLoaderWithDownloader>(_resourceLoader, urlSchemes, downloader, _workerQueue);
}

AndroidAssetLoader::AndroidAssetLoader(const Valdi::Ref<ResourceLoader>& resourceLoader,
                                       JavaEnv env,
                                       jobject imageLoader,
                                       jobject supportedSchemes,
                                       jint supportedOutputType)
    : Valdi::AssetLoader(ValdiAndroid::getUrlSchemes(env, supportedSchemes)),
      GlobalRefJavaObjectBase(env, imageLoader, "ImageLoader"),
      _resourceLoader(resourceLoader),
      _supportedOutputType(supportedOutputType) {}

AndroidAssetLoader::~AndroidAssetLoader() = default;

bool AndroidAssetLoader::canReuseLoadedAssets() const {
    return getOutputType() != snap::valdi_core::AssetOutputType::VideoAndroid;
}

snap::valdi_core::AssetOutputType AndroidAssetLoader::getOutputType() const {
    switch (_supportedOutputType) {
        case 0x000001:
            return snap::valdi_core::AssetOutputType::ImageAndroid;
        case 0x000010:
            return snap::valdi_core::AssetOutputType::Bytes;
        case 0x000100:
            return snap::valdi_core::AssetOutputType::VideoAndroid;
        default:
            // Something has gone horribly wrong
            return snap::valdi_core::AssetOutputType::Dummy;
    }
}

Valdi::Result<Valdi::Value> AndroidAssetLoader::requestPayloadFromURL(const Valdi::StringBox& url) {
    auto javaUrl = toJavaObject(getEnv(), url);

    Valdi::SimpleExceptionTracker exceptionTracker;
    Valdi::Marshaller errorHolder(exceptionTracker);
    auto payload = _resourceLoader->requestPayloadFromURL(toObject(), javaUrl, reinterpret_cast<jlong>(&errorHolder));
    if (errorHolder.size() > 0) {
        return errorHolder.getOrUndefined(-1).getError();
    }

    return Valdi::Value(valdiObjectFromJavaObject(payload, "AssetPayload"));
}

Valdi::Shared<snap::valdi_core::Cancelable> AndroidAssetLoader::loadAsset(
    const Valdi::Value& requestPayload,
    int32_t preferredWidth,
    int32_t preferredHeight,
    const Valdi::Value& attachedData,
    const Valdi::Ref<Valdi::AssetLoaderCompletion>& completion) {
    auto javaPayload = toJavaObject(getEnv(), requestPayload);
    auto* completionHandle = Valdi::unsafeBridgeRetain(completion.get());

    const float* colorMatrixFilter = nullptr;
    float blurRadiusFilter = 0.0f;

    auto typedFilter = attachedData.getTypedRef<Valdi::ImageFilter>();
    if (typedFilter != nullptr) {
        if (!typedFilter->isIdentityColorMatrix()) {
            colorMatrixFilter = typedFilter->getColorMatrix();
        }
        blurRadiusFilter = typedFilter->getBlurRadius();
    }

    auto cancelable = _resourceLoader->loadAsset(toObject(),
                                                 javaPayload,
                                                 preferredWidth,
                                                 preferredHeight,
                                                 colorMatrixFilter,
                                                 blurRadiusFilter,
                                                 _supportedOutputType,
                                                 reinterpret_cast<jlong>(completionHandle));
    if (cancelable.isNull()) {
        return nullptr;
    }

    return djinni_generated_client::valdi_core::NativeCancelable::toCpp(JavaEnv::getUnsafeEnv(),
                                                                        cancelable.getUnsafeObject());
}

Valdi::Ref<Valdi::LoadedAsset> AndroidAssetLoader::createLoadedAsset(JavaEnv env,
                                                                     jobject valdiAsset,
                                                                     jboolean isImage) {
    return Valdi::makeShared<AndroidLoadedAsset>(env, valdiAsset, isImage);
}

} // namespace ValdiAndroid
