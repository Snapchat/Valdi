//
//  DisplayList.hpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 9/15/20.
//

#pragma once

#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/ObjectPool.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"

#include "snap_drawing/cpp/Utils/Aliases.hpp"
#include "snap_drawing/cpp/Utils/BorderRadius.hpp"
#include "snap_drawing/cpp/Utils/Matrix.hpp"
#include "snap_drawing/cpp/Utils/TimePoint.hpp"

#include "snap_drawing/cpp/Drawing/DisplayList/DisplayListOperations.hpp"
#include "snap_drawing/cpp/Drawing/LayerContent.hpp"

#include "valdi_core/cpp/Utils/Value.hpp"

#include "include/core/SkPicture.h"

#include <optional>
#include <vector>

namespace snap::drawing {

class DrawableSurfaceCanvas;
class IMask;

extern size_t kDisplayListAllPlaneIndexes;

using PooledByteBuffer = Valdi::ObjectPoolEntry<Valdi::ByteBuffer, void (*)(Valdi::ByteBuffer&)>;

struct DisplayListPlane {
    PooledByteBuffer operations;

    explicit DisplayListPlane(PooledByteBuffer&& operations);
};

class DisplayList : public Valdi::SimpleRefCountable {
public:
    DisplayList(Size size, TimePoint frameTime);
    ~DisplayList() override;

    Size getSize() const;
    TimePoint getFrameTime() const;

    // Sub-rect of the content (in this display list's own coordinate space) that the destination
    // surface actually covers. Set this when the surface is smaller than the full content -- e.g. a
    // layer zoomed far larger than the visible viewport -- so the drawable is sized to the visible
    // region and content is translated into it, instead of allocating a surface for the full (huge)
    // content size.
    //
    // Three states, kept distinct on purpose:
    //   * unset (default)     -> getViewport() returns the full content; existing callers unchanged.
    //   * set to a non-empty  -> rasterize only that sub-rect (translated to the drawable origin).
    //   * set to an empty rect -> nothing is visible (e.g. fully scrolled off screen); skip drawing.
    // The empty case must NOT be confused with unset, which is why this is optional rather than a
    // sentinel empty rect.
    void setViewport(const Rect& viewport);
    Rect getViewport() const;

    void pushContext(const Matrix& matrix, Scalar opacity, uint64_t layerId, bool hasUpdates);
    void popContext();

    void appendLayerContent(const LayerContent& layerContent, Scalar opacity);
    void appendPicture(SkPicture* picture, Scalar opacity);
    void appendClipRound(const BorderRadius& borderRadius, Scalar width, Scalar height);
    void appendClipRect(Scalar width, Scalar height);

    void appendPrepareMask(IMask* mask);
    void appendApplyMask(IMask* mask);

    size_t getPlanesCount() const;
    bool hasExternalSurfaces() const;

    size_t getBytesUsed(size_t planeIndex) const;

    void appendPlane();
    void removePlane(size_t planeIndex);

    void setCurrentPlane(size_t planeIndex);

    void removeEmptyPlanes();
    void removeAllPlanes();

    Valdi::Value toDebugJSON() const;

    void draw(
        DrawableSurfaceCanvas& canvas, size_t planeIndex, Scalar scaleX, Scalar scaleY, bool shouldClearCanvas) const;

    void draw(DrawableSurfaceCanvas& canvas, size_t planeIndex, bool shouldClearCanvas = true) const;

    template<typename Visitor>
    void visitOperations(size_t planeIndex, Visitor& visitor) const {
        if (planeIndex == kDisplayListAllPlaneIndexes) {
            auto surfacesCount = getPlanesCount();
            for (size_t i = 0; i < surfacesCount; i++) {
                doVisitOperations(i, visitor);
            }
        } else {
            doVisitOperations(planeIndex, visitor);
        }
    }

private:
    Valdi::SmallVector<DisplayListPlane, 1> _planes;
    DisplayListPlane* _currentPlane = nullptr;
    Size _size;
    std::optional<Rect> _viewport;
    TimePoint _frameTime;
    bool _hasExternalSurfaces = false;
    bool _hasMask = false;

    std::pair<Valdi::Byte*, Valdi::Byte*> getBeginEndPtrs(size_t planeIndex) const;

    template<typename T>
    T* appendOperation() {
        auto* operation =
            reinterpret_cast<Operations::Operation*>(_currentPlane->operations->appendWritable(sizeof(T)));
        operation->type = T::kId;

        return reinterpret_cast<T*>(operation);
    }

    template<typename Visitor>
    struct BytesVisitor {
        Visitor& visitor;

        inline explicit BytesVisitor(Visitor& visitor) : visitor(visitor) {}

        template<typename Operation>
        inline const Valdi::Byte* visit(const Operation& operation) {
            visitor.visit(operation);
            return reinterpret_cast<const Valdi::Byte*>(&operation) + sizeof(Operation);
        }
    };

    template<typename Visitor>
    void doVisitOperations(size_t planeIndex, Visitor& visitor) const {
        auto ptrs = getBeginEndPtrs(planeIndex);
        BytesVisitor<Visitor> bytesVisitor(visitor);

        const auto* current = ptrs.first;
        const auto* end = ptrs.second;

        while (current != end) {
            current = snap::drawing::Operations::visitOperation(
                *reinterpret_cast<const Operations::Operation*>(current), bytesVisitor);
        }
    }
};

} // namespace snap::drawing
