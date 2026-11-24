//
//  ANativeWindowGraphicsContext.cpp
//  snap_drawing
//
//  Created by Simon Corsin on 1/20/22.
//

#if __ANDROID__

#include "snap_drawing/cpp/Drawing/GraphicsContext/ANativeWindowGraphicsContext.hpp"
#include "snap_drawing/cpp/Drawing/GraphicsContext/EGLUtils.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include <GLES/glext.h>

#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/egl/GrGLMakeEGLInterface.h"

namespace snap::drawing {

class ANativeWindowDrawableSurface : public DrawableSurface {
public:
    ANativeWindowDrawableSurface(const Ref<ANativeWindowGraphicsContext>& context, ANativeWindow* nativeWindow)
        : _context(context), _nativeWindow(nativeWindow) {
        ANativeWindow_acquire(nativeWindow);
        _width = ANativeWindow_getWidth(nativeWindow);
        _height = ANativeWindow_getHeight(nativeWindow);
    }

    ~ANativeWindowDrawableSurface() override {
        if (_eglContext.getDrawSurface() != EGL_NO_SURFACE) {
            eglDestroySurface(_eglContext.getDisplay(), _eglContext.getDrawSurface());

            _eglContext.setReadSurface(EGL_NO_SURFACE);
            _eglContext.setDrawSurface(EGL_NO_SURFACE);
        }

        if (_nativeWindow) {
            ANativeWindow_release(_nativeWindow);
            _nativeWindow = nullptr;
        }
    }

    GraphicsContext* getGraphicsContext() override {
        return _context.get();
    }

    Valdi::Result<DrawableSurfaceCanvas> prepareCanvas() override {
        auto glObjectsResult = _context->getGLObjects();
        if (!glObjectsResult) {
            SC_ASSERT_FAIL(glObjectsResult.error().toString().c_str());
            return glObjectsResult.moveError();
        }

        auto glObjects = glObjectsResult.moveValue();

        _eglContext.setDisplay(glObjects.eGLDisplay);
        _eglContext.setContext(glObjects.eGLContext);

        if (_eglContext.getDrawSurface() == EGL_NO_SURFACE) {
            auto* surfaceAndroid =
                eglCreateWindowSurface(_eglContext.getDisplay(), glObjects.eGLConfig, _nativeWindow, nullptr);

            if (surfaceAndroid == EGL_NO_SURFACE) {
                return EGLContextWrapper::getLastError("Could not create GL surface");
            }

            _eglContext.setDrawSurface(surfaceAndroid);
            _eglContext.setReadSurface(surfaceAndroid);
        }

        auto result = makeCurrent();
        if (!result) {
            return result.moveError();
        }

        // The window resizes with its view, while the wrapped framebuffer keeps the size it was wrapped at.
        const auto width = ANativeWindow_getWidth(_nativeWindow);
        const auto height = ANativeWindow_getHeight(_nativeWindow);
        const auto isValidSize = width > 0 && height > 0;
        if (isValidSize && (width != _width || height != _height)) {
            _width = width;
            _height = height;
            _surface = nullptr;
        }

        if (_surface == nullptr) {
            int fbBOID;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING_OES, &fbBOID);

            GrGLFramebufferInfo fbInfo;
            fbInfo.fFBOID = static_cast<GrGLint>(fbBOID);
            fbInfo.fFormat = GL_RGBA8_OES;

            auto backendRT =
                GrBackendRenderTargets::MakeGL(_width, _height, glObjects.sampleCount, glObjects.stencilBits, fbInfo);

            SkSurfaceProps surfaceProps;

            _surface = SkSurfaces::WrapBackendRenderTarget(glObjects.grContext.get(),
                                                           backendRT,
                                                           kBottomLeft_GrSurfaceOrigin,
                                                           kRGBA_8888_SkColorType,
                                                           nullptr,
                                                           &surfaceProps);
        }

        return DrawableSurfaceCanvas(_surface.get(), _width, _height);
    }

    void flush() override {
        if (_surface != nullptr) {
            GLDrawableSurface::flushSurface(_surface.get());
        }

        swapBuffers();

        releaseCurrent();
    }

    Valdi::Result<Valdi::Void> makeCurrent() {
        if (!_eglContext.makeCurrent()) {
            return EGLContextWrapper::getLastError("Failed to make EGL context current");
        }
        return Valdi::Void();
    }

    bool releaseCurrent() {
        return _eglContext.releaseCurrent();
    }

    void swapBuffers() {
        if (_eglContext.getDrawSurface() != EGL_NO_SURFACE) {
            eglSwapBuffers(_eglContext.getDisplay(), _eglContext.getDrawSurface());
        }
    }

private:
    Ref<ANativeWindowGraphicsContext> _context;
    ANativeWindow* _nativeWindow;
    EGLContextWrapper _eglContext;
    sk_sp<SkSurface> _surface;

    int _width;
    int _height;
};

ANativeWindowGraphicsContext::ANativeWindowGraphicsContext(const DisplayParams& displayParams,
                                                           const Ref<GrGraphicsContextOptions>& options)
    : GraphicsContext(), _displayParams(displayParams), _options(options) {}

ANativeWindowGraphicsContext::~ANativeWindowGraphicsContext() {
    std::lock_guard<Valdi::Mutex> guard(_mutex);

    if (_glObjects) {
        auto& glObjects = _glObjects.value();

        if (glObjects.grContext != nullptr) {
            glObjects.grContext->abandonContext();
            glObjects.grContext = nullptr;
        }

        if (glObjects.eGLContext != EGL_NO_CONTEXT) {
            eglDestroyContext(glObjects.eGLDisplay, glObjects.eGLContext);
            glObjects.eGLContext = EGL_NO_CONTEXT;
        }

        if (glObjects.eGLDefaultSurface != EGL_NO_SURFACE) {
            eglDestroySurface(glObjects.eGLDisplay, glObjects.eGLDefaultSurface);
            glObjects.eGLDefaultSurface = EGL_NO_SURFACE;
        }

        if (glObjects.eGLDisplay != EGL_NO_DISPLAY) {
            eglTerminate(glObjects.eGLDisplay);
            glObjects.eGLDisplay = EGL_NO_DISPLAY;
        }
    }
}

void ANativeWindowGraphicsContext::setResourceCacheLimit(size_t resourceCacheLimitBytes) {
    auto currentContext = makeContextCurrent();
    if (currentContext) {
        GrGraphicsContext::setGrResourceCacheLimit(_glObjects->grContext, resourceCacheLimitBytes);
        currentContext->releaseCurrent();
    }
}

void ANativeWindowGraphicsContext::performCleanup(bool shouldPurgeScratchResources,
                                                  std::chrono::seconds secondsNotUsed) {
    auto currentContext = makeContextCurrent();
    if (currentContext) {
        GrGraphicsContext::performGrCleanup(_glObjects->grContext, shouldPurgeScratchResources, secondsNotUsed);
        currentContext->releaseCurrent();
    }
}

std::optional<EGLContextWrapper> ANativeWindowGraphicsContext::makeContextCurrent() {
    auto glObjectsResult = getGLObjects();
    if (!glObjectsResult) {
        return std::nullopt;
    }

    auto newGLContext = EGLContextWrapper(
        glObjectsResult.value().eGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, glObjectsResult.value().eGLContext);

    if (!newGLContext.makeCurrent()) {
        return std::nullopt;
    }

    return {newGLContext};
}

Valdi::Error ANativeWindowGraphicsContext::onGLObjectsError(GLObjects& glObjects, const char* message) {
    auto error = EGLContextWrapper::getLastError(message);

    if (glObjects.eGLContext != EGL_NO_CONTEXT) {
        eglDestroyContext(glObjects.eGLDisplay, glObjects.eGLContext);
        glObjects.eGLContext = EGL_NO_CONTEXT;
    }

    if (glObjects.eGLDefaultSurface != EGL_NO_SURFACE) {
        eglDestroySurface(glObjects.eGLDisplay, glObjects.eGLDefaultSurface);
        glObjects.eGLDefaultSurface = EGL_NO_SURFACE;
    }

    if (glObjects.eGLDisplay != EGL_NO_DISPLAY) {
        eglTerminate(glObjects.eGLDisplay);
        glObjects.eGLDisplay = EGL_NO_DISPLAY;
    }

    return error;
}

Valdi::Error ANativeWindowGraphicsContext::onBindAPIError(GLObjects& glObjects) {
    return onGLObjectsError(glObjects, "Failed to bind GL API");
}

Valdi::Error ANativeWindowGraphicsContext::onGetDefaultDisplayError(GLObjects& glObjects) {
    return onGLObjectsError(glObjects, "Failed to retrieve default display");
}

Valdi::Error ANativeWindowGraphicsContext::onInitializeError(GLObjects& glObjects) {
    return onGLObjectsError(glObjects, "Failed to initialize eGLDisplay");
}

Valdi::Error ANativeWindowGraphicsContext::onChooseConfigError(GLObjects& glObjects) {
    return onGLObjectsError(glObjects, "Could not resolve GL config");
}

Valdi::Error ANativeWindowGraphicsContext::onCreateContextError(GLObjects& glObjects) {
    return onGLObjectsError(glObjects, "Could not create GL context");
}

Valdi::Error ANativeWindowGraphicsContext::onCreateDefaultSurfaceError(GLObjects& glObjects) {
    return onGLObjectsError(glObjects, "Could not create default surface during initialization");
}

Valdi::Error ANativeWindowGraphicsContext::onMakeCurrentError(GLObjects& glObjects) {
    return onGLObjectsError(glObjects, "Could not make GL current during initialization");
}

Valdi::Error ANativeWindowGraphicsContext::onGrContextError(GLObjects& glObjects) {
    return onGLObjectsError(glObjects, "Failed to create GrContext");
}

Valdi::Result<ANativeWindowGraphicsContext::GLObjects> ANativeWindowGraphicsContext::getGLObjects() {
    std::lock_guard<Valdi::Mutex> guard(_mutex);

    if (_glObjects) {
        return _glObjects.value();
    }

    GLObjects glObjects;

    if (eglBindAPI(EGL_OPENGL_ES_API) == 0) {
        return onBindAPIError(glObjects);
    }

    glObjects.eGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if (glObjects.eGLDisplay == EGL_NO_DISPLAY) {
        return onGetDefaultDisplayError(glObjects);
    }

    EGLint majorVersion;
    EGLint minorVersion;
    if (eglInitialize(glObjects.eGLDisplay, &majorVersion, &minorVersion) != EGL_TRUE) {
        return onInitializeError(glObjects);
    }

    EGLint numConfigs = 0;
    EGLint eglSampleCnt = _displayParams.mSAASampleCount > 1 ? static_cast<EGLint>(_displayParams.mSAASampleCount) : 0;

    const EGLint configAttribs[] = {EGL_SURFACE_TYPE,
                                    EGL_WINDOW_BIT,
                                    EGL_RENDERABLE_TYPE,
                                    EGL_OPENGL_ES2_BIT,
                                    EGL_RED_SIZE,
                                    8,
                                    EGL_GREEN_SIZE,
                                    8,
                                    EGL_BLUE_SIZE,
                                    8,
                                    EGL_ALPHA_SIZE,
                                    8,
                                    EGL_STENCIL_SIZE,
                                    8,
                                    EGL_SAMPLE_BUFFERS,
                                    (eglSampleCnt > 1) ? 1 : 0,
                                    EGL_SAMPLES,
                                    eglSampleCnt,
                                    EGL_NONE};

    eglChooseConfig(glObjects.eGLDisplay, configAttribs, &glObjects.eGLConfig, 1, &numConfigs);
    if (numConfigs == 0) {
        return onChooseConfigError(glObjects);
    }

    static const EGLint kEGLContextAttribsForOpenGLES[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    glObjects.eGLContext =
        eglCreateContext(glObjects.eGLDisplay, glObjects.eGLConfig, EGL_NO_CONTEXT, kEGLContextAttribsForOpenGLES);
    if (glObjects.eGLContext == EGL_NO_CONTEXT) {
        return onCreateContextError(glObjects);
    }

    eglGetConfigAttrib(glObjects.eGLDisplay, glObjects.eGLConfig, EGL_STENCIL_SIZE, &glObjects.stencilBits);
    eglGetConfigAttrib(glObjects.eGLDisplay, glObjects.eGLConfig, EGL_SAMPLES, &glObjects.sampleCount);
    glObjects.sampleCount = std::max(glObjects.sampleCount, 1);

    static const EGLint kEGLPBufferAttributes[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    glObjects.eGLDefaultSurface =
        eglCreatePbufferSurface(glObjects.eGLDisplay, glObjects.eGLConfig, kEGLPBufferAttributes);
    if (glObjects.eGLDefaultSurface == EGL_NO_SURFACE) {
        return onCreateDefaultSurfaceError(glObjects);
    }

    eglSwapInterval(glObjects.eGLDisplay, _displayParams.disableVsync ? 0 : 1);

    if (eglMakeCurrent(
            glObjects.eGLDisplay, glObjects.eGLDefaultSurface, glObjects.eGLDefaultSurface, glObjects.eGLContext) ==
        EGL_FALSE) {
        return onMakeCurrentError(glObjects);
    }

    sk_sp<const GrGLInterface> glInterface = GrGLInterfaces::MakeEGL();
    if (!glInterface) {
        eglMakeCurrent(glObjects.eGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        return Valdi::Error("Failed to create EGL interface");
    }

    glObjects.grContext = GrDirectContexts::MakeGL(std::move(glInterface), _options->getGrContextOptions());

    eglMakeCurrent(glObjects.eGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    if (glObjects.grContext == nullptr) {
        return onGrContextError(glObjects);
    }

    _glObjects = std::move(glObjects);

    return _glObjects.value();
}

Ref<ANativeWindowGraphicsContext> ANativeWindowGraphicsContext::create(const DisplayParams& displayParams,
                                                                       const Ref<GrGraphicsContextOptions>& options) {
    return Valdi::makeShared<ANativeWindowGraphicsContext>(displayParams, options);
}

Ref<DrawableSurface> ANativeWindowGraphicsContext::createSurfaceForNativeWindow(ANativeWindow* nativeWindow) {
    return Valdi::makeShared<ANativeWindowDrawableSurface>(Valdi::strongSmallRef(this), nativeWindow);
}

int64_t ANativeWindowGraphicsContext::getMaxRenderTargetSize() {
    auto glObjectsResult = getGLObjects();
    if (!glObjectsResult) {
        return -1;
    }
    return glObjectsResult.value().grContext->maxRenderTargetSize();
}

ANativeWindowGraphicsContext::GLObjects::GLObjects() : eGLDisplay(EGL_NO_DISPLAY), eGLContext(EGL_NO_CONTEXT) {}

} // namespace snap::drawing

#endif
