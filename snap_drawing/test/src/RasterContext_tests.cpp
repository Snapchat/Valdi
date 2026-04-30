#include "gtest/gtest.h"
#include <cstdint>
#include <gtest/gtest.h>

#include "TestBitmap.hpp"
#include "snap_drawing/cpp/Drawing/DisplayList/DisplayList.hpp"
#include "snap_drawing/cpp/Drawing/GraphicsContext/BitmapGraphicsContext.hpp"
#include "snap_drawing/cpp/Drawing/Raster/RasterContext.hpp"
#include "snap_drawing/cpp/Layers/ExternalLayer.hpp"
#include "snap_drawing/cpp/Layers/Interfaces/ILayerRoot.hpp"
#include "snap_drawing/cpp/Layers/Layer.hpp"
#include "snap_drawing/cpp/Utils/Bitmap.hpp"
#include "valdi_core/cpp/Interfaces/IBitmapFactory.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"

using namespace Valdi;

namespace snap::drawing {

class TestBitmapFactory : public IBitmapFactory {
public:
    size_t createdBitmapCount = 0;

    TestBitmapFactory() = default;
    ~TestBitmapFactory() override = default;

    Result<Ref<IBitmap>> createBitmap(int width, int height) override {
        createdBitmapCount++;

        Ref<IBitmap> bitmap = makeShared<TestBitmap>(width, height);
        return bitmap;
    }
};

class TestLayerRoot : public ILayerRoot {
public:
    TestLayerRoot() = default;
    ~TestLayerRoot() override = default;

    EventId enqueueEvent(EventCallback&& eventCallback, Duration after) final {
        return EventId();
    }

    bool cancelEvent(EventId eventId) final {
        return false;
    }

    bool shouldRasterizeExternalSurface() const final {
        return false;
    }

    void onInitialize() final {}
    void setChildNeedsDisplay() final {}
    void requestLayout(ILayer* layer) final {}
    void requestFocus(ILayer* layer) final {}

    LayerId allocateLayerId() override {
        return ++_layerIdSequence;
    }

private:
    uint64_t _layerIdSequence = 0;
};

class RasterContextTestExternalSurface : public ExternalSurface {
public:
    size_t rasterCount = 0;
    RasterContextTestExternalSurface(const Ref<Valdi::IBitmapFactory>& bitmapFactory, Color color)
        : _bitmapFactory(bitmapFactory), _color(color) {}
    ~RasterContextTestExternalSurface() override = default;

    Ref<Valdi::IBitmapFactory> getRasterBitmapFactory() const override {
        return _bitmapFactory;
    }

    Valdi::Result<Valdi::Void> rasterInto(const Ref<Valdi::IBitmap>& bitmap,
                                          const Rect& frame,
                                          const Matrix& transform,
                                          float rasterScaleX,
                                          float rasterScaleY) override {
        rasterCount++;
        BitmapGraphicsContext bitmapContext;
        auto surface = bitmapContext.createBitmapSurface(bitmap);
        auto canvas = surface->prepareCanvas();
        if (!canvas) {
            return canvas.moveError();
        }

        auto* skCanvas = canvas.value().getSkiaCanvas();

        skCanvas->concat(transform.getSkValue());
        skCanvas->scale(rasterScaleX, rasterScaleY);
        SkPaint paint;
        paint.setColor(_color.getSkValue());
        skCanvas->drawRect(frame.getSkValue(), paint);

        surface->flush();

        return Valdi::Void();
    }

private:
    Ref<Valdi::IBitmapFactory> _bitmapFactory;
    Color _color;
};

class RasterContextTests : public ::testing::Test {
protected:
    void SetUp() override {
        _resources = makeShared<Resources>(nullptr, 1.0f, ConsoleLogger::getLogger());
        _rasterContext =
            makeShared<RasterContext>(_resources->getLogger(), ExternalSurfaceRasterizationMethod::ACCURATE, false);
        _layerRoot = makeShared<TestLayerRoot>();
        _contentLayer = makeLayer<Layer>(_resources);
        _contentLayer->onParentChanged(_layerRoot);
        _bitmapFactory = makeShared<TestBitmapFactory>();
    }

    Result<Ref<TestBitmap>> raster() {
        auto outputBitmap = makeShared<TestBitmap>(4, 4);
        auto result = rasterInto(outputBitmap);
        if (!result) {
            return result.moveError();
        }

        return outputBitmap;
    }

    Result<RasterContext::RasterResult> rasterInto(const Ref<Valdi::IBitmap>& outputBitmap,
                                                   bool shouldClearBitmapBeforeDrawing = true) {
        auto displayList = Valdi::makeShared<DisplayList>(_contentLayer->getFrame().size(), TimePoint(0.0));
        DrawMetrics metrics;
        _contentLayer->draw(*displayList, metrics);

        return _rasterContext->raster(displayList, outputBitmap, shouldClearBitmapBeforeDrawing);
    }

    Result<RasterContext::RasterResult> rasterDelta(const Ref<TestBitmap>& inputBitmap) {
        auto displayList = Valdi::makeShared<DisplayList>(_contentLayer->getFrame().size(), TimePoint(0.0));
        DrawMetrics metrics;
        _contentLayer->draw(*displayList, metrics);

        return _rasterContext->rasterDelta(displayList, inputBitmap);
    }

    Ref<Resources> _resources;
    Ref<RasterContext> _rasterContext;
    Ref<TestLayerRoot> _layerRoot;
    Ref<Layer> _contentLayer;
    Ref<TestBitmapFactory> _bitmapFactory;
};

TEST_F(RasterContextTests, canRasterSimpleScene) {
    _contentLayer->setBackgroundColor(Color::red());

    auto centerLayer = makeLayer<Layer>(_resources);
    centerLayer->setBackgroundColor(Color::blue());
    centerLayer->setFrame(Rect::makeXYWH(1, 1, 2, 2));
    _contentLayer->addChild(centerLayer);
    _contentLayer->setFrame(Rect::makeXYWH(0, 0, 4, 4));

    auto result = raster();
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(*result.value(),
              std::initializer_list<Color>({
                  // clang-format off
                Color::red(), Color::red(), Color::red(), Color::red(),
                Color::red(), Color::blue(), Color::blue(), Color::red(),
                Color::red(), Color::blue(), Color::blue(), Color::red(),
                Color::red(), Color::red(), Color::red(), Color::red(),
                  // clang-format on
              }));

    ASSERT_EQ(0, _bitmapFactory->createdBitmapCount);
}

TEST_F(RasterContextTests, scalesSceneToBitmap) {
    _contentLayer->setBackgroundColor(Color::red());

    auto centerLayer = makeLayer<Layer>(_resources);
    centerLayer->setBackgroundColor(Color::blue());
    centerLayer->setFrame(Rect::makeXYWH(0, 0, 1, 1));
    _contentLayer->addChild(centerLayer);
    _contentLayer->setFrame(Rect::makeXYWH(0, 0, 2, 2));

    auto result = raster();
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(*result.value(),
              std::initializer_list<Color>({
                  // clang-format off
                Color::blue(), Color::blue(), Color::red(), Color::red(),
                Color::blue(), Color::blue(), Color::red(), Color::red(),
                Color::red(), Color::red(), Color::red(), Color::red(),
                Color::red(), Color::red(), Color::red(), Color::red(),
                  // clang-format on
              }));

    ASSERT_EQ(0, _bitmapFactory->createdBitmapCount);
}

TEST_F(RasterContextTests, canRasterDelta) {
    _contentLayer->setBackgroundColor(Color::red());

    auto centerLayer = makeLayer<Layer>(_resources);
    centerLayer->setBackgroundColor(Color::blue());
    centerLayer->setFrame(Rect::makeXYWH(1, 1, 2, 2));
    _contentLayer->addChild(centerLayer);
    _contentLayer->setFrame(Rect::makeXYWH(0, 0, 4, 4));

    auto outputBitmap = makeShared<TestBitmap>(4, 4);

    auto result = rasterDelta(outputBitmap);
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(*outputBitmap,
              std::initializer_list<Color>({
                  // clang-format off
                  Color::red(), Color::red(), Color::red(), Color::red(),
                  Color::red(), Color::blue(), Color::blue(), Color::red(),
                  Color::red(), Color::blue(), Color::blue(), Color::red(),
                  Color::red(), Color::red(), Color::red(), Color::red(),
                  // clang-format on
              }));

    // With 1px AA margin, 4x4 rect becomes 6x6 = 36 pixels
    ASSERT_EQ(36, result.value().renderedPixelsCount);

    outputBitmap->setPixels(std::initializer_list<Color>({
        // clang-format off
            Color::black(), Color::black(), Color::black(), Color::black(),
            Color::black(), Color::black(), Color::black(), Color::black(),
            Color::black(), Color::black(), Color::black(), Color::black(),
            Color::black(), Color::black(), Color::black(), Color::black(),
        // clang-format on
    }));

    result = rasterDelta(outputBitmap);
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(*outputBitmap,
              std::initializer_list<Color>({
                  // clang-format off
                  Color::black(), Color::black(), Color::black(), Color::black(),
                  Color::black(), Color::black(), Color::black(), Color::black(),
                  Color::black(), Color::black(), Color::black(), Color::black(),
                  Color::black(), Color::black(), Color::black(), Color::black(),
                  // clang-format on
              }));

    ASSERT_EQ(0, result.value().renderedPixelsCount);
    centerLayer->setBackgroundColor(Color::green());

    result = rasterDelta(outputBitmap);
    ASSERT_TRUE(result) << result.description();

    // With 1px AA margin, the damage rect extends beyond the changed 2x2 area,
    // causing the red background to be re-rendered in the margin area
    ASSERT_EQ(*outputBitmap,
              std::initializer_list<Color>({
                  // clang-format off
                            Color::red(), Color::red(), Color::red(), Color::red(),
                            Color::red(), Color::green(), Color::green(), Color::red(),
                            Color::red(), Color::green(), Color::green(), Color::red(),
                            Color::red(), Color::red(), Color::red(), Color::red(),
                  // clang-format on
              }));

    // With 1px AA margin, 2x2 rect becomes 4x4 = 16 pixels
    ASSERT_EQ(16, result.value().renderedPixelsCount);
}

TEST_F(RasterContextTests, canRasterDeltaWithScale) {
    _contentLayer->setBackgroundColor(Color::red());

    auto centerLayer = makeLayer<Layer>(_resources);
    centerLayer->setBackgroundColor(Color::blue());
    centerLayer->setFrame(Rect::makeXYWH(0, 0, 1, 1));
    _contentLayer->addChild(centerLayer);
    _contentLayer->setFrame(Rect::makeXYWH(0, 0, 2, 2));

    auto outputBitmap = makeShared<TestBitmap>(4, 4);

    auto result = rasterDelta(outputBitmap);
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(*outputBitmap,
              std::initializer_list<Color>({
                  // clang-format off
                  Color::blue(), Color::blue(), Color::red(), Color::red(),
                  Color::blue(), Color::blue(), Color::red(), Color::red(),
                  Color::red(), Color::red(), Color::red(), Color::red(),
                  Color::red(), Color::red(), Color::red(), Color::red(),
                  // clang-format on
              }));

    outputBitmap->setPixels(std::initializer_list<Color>({
        // clang-format off
            Color::black(), Color::black(), Color::black(), Color::black(),
            Color::black(), Color::black(), Color::black(), Color::black(),
            Color::black(), Color::black(), Color::black(), Color::black(),
            Color::black(), Color::black(), Color::black(), Color::black(),
        // clang-format on
    }));

    centerLayer->setBackgroundColor(Color::green());

    result = rasterDelta(outputBitmap);
    ASSERT_TRUE(result) << result.description();

    // With 1px AA margin, the 2x2 green layer's damage extends to adjacent red pixels
    ASSERT_EQ(*outputBitmap,
              std::initializer_list<Color>({
                  // clang-format off
                            Color::green(), Color::green(), Color::red(), Color::black(),
                            Color::green(), Color::green(), Color::red(), Color::black(),
                            Color::red(), Color::red(), Color::red(), Color::black(),
                            Color::black(), Color::black(), Color::black(), Color::black(),
                  // clang-format on
              }));
}

TEST_F(RasterContextTests, canRasterDeltaWitMovedSurfaces) {
    _contentLayer->setBackgroundColor(Color::red());

    auto centerLayer = makeLayer<Layer>(_resources);
    centerLayer->setBackgroundColor(Color::blue());
    centerLayer->setFrame(Rect::makeXYWH(1, 1, 2, 2));
    _contentLayer->addChild(centerLayer);
    _contentLayer->setFrame(Rect::makeXYWH(0, 0, 4, 4));

    auto outputBitmap = makeShared<TestBitmap>(4, 4);

    auto result = rasterDelta(outputBitmap);
    ASSERT_TRUE(result) << result.description();

    outputBitmap->setPixels(std::initializer_list<Color>({
        // clang-format off
            Color::black(), Color::black(), Color::black(), Color::black(),
            Color::black(), Color::black(), Color::black(), Color::black(),
            Color::black(), Color::black(), Color::black(), Color::black(),
            Color::black(), Color::black(), Color::black(), Color::black(),
        // clang-format on
    }));

    centerLayer->setFrame(Rect::makeXYWH(0, 1, 2, 2));

    result = rasterDelta(outputBitmap);
    ASSERT_TRUE(result) << result.description();

    // With 1px AA margin, moving the layer causes both old and new positions to
    // re-render with their margins, exposing the red background
    ASSERT_EQ(*outputBitmap,
              std::initializer_list<Color>({
                  // clang-format off
                            Color::red(), Color::red(), Color::red(), Color::red(),
                            Color::blue(), Color::blue(), Color::red(), Color::red(),
                            Color::blue(), Color::blue(), Color::red(), Color::red(),
                            Color::red(), Color::red(), Color::red(), Color::red(),
                  // clang-format on
              }));
}

TEST_F(RasterContextTests, canRasterSceneWithExternalSurface) {
    _contentLayer->setBackgroundColor(Color::red());

    auto centerLayer = makeLayer<ExternalLayer>(_resources);
    auto externalSurface = makeShared<RasterContextTestExternalSurface>(_bitmapFactory, Color::green());
    centerLayer->setExternalSurface(externalSurface);

    _contentLayer->addChild(centerLayer);

    _contentLayer->setFrame(Rect::makeXYWH(0, 0, 4, 4));
    centerLayer->setFrame(Rect::makeXYWH(1, 1, 2, 2));

    auto result = raster();
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(*result.value(),
              std::initializer_list<Color>({
                  // clang-format off
                Color::red(), Color::red(), Color::red(), Color::red(),
                Color::red(), Color::green(), Color::green(), Color::red(),
                Color::red(), Color::green(), Color::green(), Color::red(),
                Color::red(), Color::red(), Color::red(), Color::red(),
                  // clang-format on
              }));

    ASSERT_EQ(1, _bitmapFactory->createdBitmapCount);
    ASSERT_EQ(1, externalSurface->rasterCount);
}

TEST_F(RasterContextTests, appliesClipToExternalSurface) {
    _contentLayer->setBackgroundColor(Color::red());

    auto clipLayer = makeLayer<Layer>(_resources);

    auto centerLayer = makeLayer<ExternalLayer>(_resources);
    auto externalSurface = makeShared<RasterContextTestExternalSurface>(_bitmapFactory, Color::green());
    centerLayer->setExternalSurface(externalSurface);

    _contentLayer->addChild(clipLayer);
    clipLayer->addChild(centerLayer);

    _contentLayer->setFrame(Rect::makeXYWH(0, 0, 4, 4));
    clipLayer->setFrame(Rect::makeXYWH(1, 1, 1, 2));
    centerLayer->setFrame(Rect::makeXYWH(0, 0, 2, 2));

    auto result = raster();
    ASSERT_TRUE(result) << result.description();

    // Try first without clipping

    ASSERT_EQ(*result.value(),
              std::initializer_list<Color>({
                  // clang-format off
                Color::red(), Color::red(), Color::red(), Color::red(),
                Color::red(), Color::green(), Color::green(), Color::red(),
                Color::red(), Color::green(), Color::green(), Color::red(),
                Color::red(), Color::red(), Color::red(), Color::red(),
                  // clang-format on
              }));

    ASSERT_EQ(1, _bitmapFactory->createdBitmapCount);
    ASSERT_EQ(1, externalSurface->rasterCount);

    // Now raster with clipping. Only the left side of the external surface should be visible.

    clipLayer->setClipsToBounds(true);
    result = raster();
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(*result.value(),
              std::initializer_list<Color>({
                  // clang-format off
                Color::red(), Color::red(), Color::red(), Color::red(),
                Color::red(), Color::green(), Color::red(), Color::red(),
                Color::red(), Color::green(), Color::red(), Color::red(),
                Color::red(), Color::red(), Color::red(), Color::red(),
                  // clang-format on
              }));

    ASSERT_EQ(1, _bitmapFactory->createdBitmapCount);
    ASSERT_EQ(1, externalSurface->rasterCount);
}

TEST_F(RasterContextTests, redrawsExternalSurfaceOnlyWhenNeeded) {
    _contentLayer->setBackgroundColor(Color::red());

    auto centerLayer = makeLayer<ExternalLayer>(_resources);
    auto externalSurface = makeShared<RasterContextTestExternalSurface>(_bitmapFactory, Color::green());
    centerLayer->setExternalSurface(externalSurface);

    _contentLayer->addChild(centerLayer);

    _contentLayer->setFrame(Rect::makeXYWH(0, 0, 4, 4));
    centerLayer->setFrame(Rect::makeXYWH(1, 1, 2, 2));

    auto result = raster();
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(*result.value(),
              std::initializer_list<Color>({
                  // clang-format off
                Color::red(), Color::red(), Color::red(), Color::red(),
                Color::red(), Color::green(), Color::green(), Color::red(),
                Color::red(), Color::green(), Color::green(), Color::red(),
                Color::red(), Color::red(), Color::red(), Color::red(),
                  // clang-format on
              }));

    ASSERT_EQ(1, _bitmapFactory->createdBitmapCount);
    ASSERT_EQ(1, externalSurface->rasterCount);

    // Now raster again without changing the external surface.

    result = raster();
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(*result.value(),
              std::initializer_list<Color>({
                  // clang-format off
                Color::red(), Color::red(), Color::red(), Color::red(),
                Color::red(), Color::green(), Color::green(), Color::red(),
                Color::red(), Color::green(), Color::green(), Color::red(),
                Color::red(), Color::red(), Color::red(), Color::red(),
                  // clang-format on
              }));

    ASSERT_EQ(1, _bitmapFactory->createdBitmapCount);
    ASSERT_EQ(1, externalSurface->rasterCount);

    // Change position of the external surface.
    centerLayer->setFrame(Rect::makeXYWH(0, 1, 2, 2));

    result = raster();
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(*result.value(),
              std::initializer_list<Color>({
                  // clang-format off
                Color::red(), Color::red(), Color::red(), Color::red(),
                Color::green(), Color::green(), Color::red(), Color::red(),
                Color::green(), Color::green(), Color::red(), Color::red(),
                Color::red(), Color::red(), Color::red(), Color::red(),
                  // clang-format on
              }));

    ASSERT_EQ(2, _bitmapFactory->createdBitmapCount);
    ASSERT_EQ(2, externalSurface->rasterCount);

    // Change transform. It should reuse a bitmap from the cache, but raster should occur again.

    centerLayer->setScaleX(0.5f);
    centerLayer->setTranslationX(-1.0f);

    result = raster();
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(*result.value(),
              std::initializer_list<Color>({
                  // clang-format off
                Color::red(), Color::red(), Color::red(), Color::red(),
                Color::green(), Color::red(), Color::red(), Color::red(),
                Color::green(), Color::red(), Color::red(), Color::red(),
                Color::red(), Color::red(), Color::red(), Color::red(),
                  // clang-format on
              }));

    ASSERT_EQ(2, _bitmapFactory->createdBitmapCount);
    ASSERT_EQ(3, externalSurface->rasterCount);
}

TEST_F(RasterContextTests, canRasterWithInternalDeltaMode) {
    _rasterContext =
        makeShared<RasterContext>(_resources->getLogger(), ExternalSurfaceRasterizationMethod::ACCURATE, true);

    auto outputBitmap = makeShared<TestBitmap>(4, 4);

    _contentLayer->setBackgroundColor(Color::red());
    auto innerLayer = makeLayer<Layer>(_resources);
    innerLayer->setBackgroundColor(Color::blue());
    innerLayer->setFrame(Rect::makeXYWH(0, 0, 1, 1));
    _contentLayer->addChild(innerLayer);
    _contentLayer->setFrame(Rect::makeXYWH(0, 0, 2, 2));

    auto result = rasterInto(outputBitmap);
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(*outputBitmap,
              std::initializer_list<Color>({
                  // clang-format off
                Color::blue(), Color::blue(), Color::red(), Color::red(),
                Color::blue(), Color::blue(), Color::red(), Color::red(),
                Color::red(), Color::red(), Color::red(), Color::red(),
                Color::red(), Color::red(), Color::red(), Color::red(),
                  // clang-format on
              }));

    // Initially it should rasterize the entire bitmap (4x4 + margin = 6x6 = 36 pixels).
    ASSERT_EQ(36, result.value().renderedPixelsCount);

    // Raster again with no changes, it should not draw anything.

    result = rasterInto(outputBitmap);
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(*outputBitmap,
              std::initializer_list<Color>({
                  // clang-format off
          Color::blue(), Color::blue(), Color::red(), Color::red(),
          Color::blue(), Color::blue(), Color::red(), Color::red(),
          Color::red(), Color::red(), Color::red(), Color::red(),
          Color::red(), Color::red(), Color::red(), Color::red(),
                  // clang-format on
              }));

    ASSERT_EQ(0, result.value().renderedPixelsCount);

    innerLayer->setFrame(Rect::makeXYWH(1, 0, 1, 1));

    result = rasterInto(outputBitmap);
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(*outputBitmap,
              std::initializer_list<Color>({
                  // clang-format off
                    Color::red(), Color::red(), Color::blue(), Color::blue(),
                    Color::red(), Color::red(), Color::blue(), Color::blue(),
                    Color::red(), Color::red(), Color::red(), Color::red(),
                    Color::red(), Color::red(), Color::red(), Color::red(),
                  // clang-format on
              }));

    // With 1px AA margin, old (2x2) + new (2x2) positions overlap to ~24 pixels
    ASSERT_EQ(24, result.value().renderedPixelsCount);

    innerLayer->setBackgroundColor(Color::green());

    result = rasterInto(outputBitmap);
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(*outputBitmap,
              std::initializer_list<Color>({
                  // clang-format off
                    Color::red(), Color::red(), Color::green(), Color::green(),
                    Color::red(), Color::red(), Color::green(), Color::green(),
                    Color::red(), Color::red(), Color::red(), Color::red(),
                    Color::red(), Color::red(), Color::red(), Color::red(),
                  // clang-format on
              }));

    // With 1px AA margin, 2x2 rect becomes 4x4 = 16 pixels
    ASSERT_EQ(16, result.value().renderedPixelsCount);
}

TEST_F(RasterContextTests, canRasterWithInternalDeltaModeAndNoClear) {
    _rasterContext =
        makeShared<RasterContext>(_resources->getLogger(), ExternalSurfaceRasterizationMethod::ACCURATE, true);

    auto outputBitmap = makeShared<TestBitmap>(4, 4);

    outputBitmap->setPixels(std::initializer_list<Color>({
        // clang-format off
        Color::green(), Color::green(), Color::green(), Color::green(),
        Color::green(), Color::green(), Color::green(), Color::green(),
        Color::green(), Color::green(), Color::green(), Color::green(),
        Color::green(), Color::green(), Color::green(), Color::green(),
        // clang-format on
    }));

    _contentLayer->setBackgroundColor(Color::transparent());
    auto innerLayer = makeLayer<Layer>(_resources);
    innerLayer->setBackgroundColor(Color::blue());
    innerLayer->setFrame(Rect::makeXYWH(1, 1, 2, 2));
    _contentLayer->addChild(innerLayer);
    _contentLayer->setFrame(Rect::makeXYWH(0, 0, 4, 4));

    auto result = rasterInto(outputBitmap, false);
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(*outputBitmap,
              std::initializer_list<Color>({
                  // clang-format off
                  Color::green(), Color::green(), Color::green(), Color::green(),
                  Color::green(), Color::blue(), Color::blue(), Color::green(),
                  Color::green(), Color::blue(), Color::blue(), Color::green(),
                  Color::green(), Color::green(), Color::green(), Color::green(),
                  // clang-format on
              }));

    // With 1px AA margin, 4x4 rect becomes 6x6 = 36 pixels
    ASSERT_EQ(36, result.value().renderedPixelsCount);

    outputBitmap->setPixels(std::initializer_list<Color>({
        // clang-format off
        Color::green(), Color::green(), Color::green(), Color::green(),
        Color::green(), Color::green(), Color::green(), Color::green(),
        Color::green(), Color::green(), Color::green(), Color::green(),
        Color::green(), Color::green(), Color::green(), Color::green(),
        // clang-format on
    }));

    innerLayer->setFrame(Rect::makeXYWH(2, 2, 2, 2));

    result = rasterInto(outputBitmap, false);
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(*outputBitmap,
              std::initializer_list<Color>({
                  // clang-format off
                  Color::green(), Color::green(), Color::green(), Color::green(),
                  Color::green(), Color::green(), Color::green(), Color::green(),
                  Color::green(), Color::green(), Color::blue(), Color::blue(),
                  Color::green(), Color::green(), Color::blue(), Color::blue(),
                  // clang-format on
              }));

    // With 1px AA margin, old (2x2) and new (2x2) positions with overlap = 25 pixels
    ASSERT_EQ(25, result.value().renderedPixelsCount);
}

} // namespace snap::drawing