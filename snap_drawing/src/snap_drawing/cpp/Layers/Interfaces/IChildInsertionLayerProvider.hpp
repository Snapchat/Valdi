//
//  IChildInsertionLayerProvider.hpp
//  snap_drawing
//

#pragma once

namespace snap::drawing {

class Layer;

/**
 * Implemented by layers whose Valdi children should be inserted into a backing
 * content layer rather than the layer object exposed to the runtime.
 *
 * Text editing layers use this to route inline child layers into the actual
 * TextLayer that owns text layout, while wrappers such as scroll layers can
 * expose their content layer for normal child insertion.
 */
class IChildInsertionLayerProvider {
public:
    virtual ~IChildInsertionLayerProvider() = default;

    virtual Layer& getChildInsertionLayer() = 0;
};

} // namespace snap::drawing
