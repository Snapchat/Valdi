//
//  Yoga.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/13/18.
//

#include "valdi/runtime/Attributes/Yoga/Yoga.hpp"
#include "valdi/runtime/Views/Frame.hpp"
#include <yoga/Yoga.h>
#include <yoga/node/Node.h>

namespace Valdi {

std::shared_ptr<YGConfig> Yoga::createConfig(float pointScale) {
    auto* config = YGConfigNew();
    YGConfigSetExperimentalFeatureEnabled(config, YGExperimentalFeatureWebFlexBasis, true);
    YGConfigSetPointScaleFactor(config, pointScale);

    return std::shared_ptr<YGConfig>(config, YGConfigFree);
}

YGNode* Yoga::createNode(YGConfig* config) {
    return YGNodeNewWithConfig(config);
}

void Yoga::destroyNode(YGNode* node) {
    YGNodeFree(node);
}

void Yoga::attachViewNode(YGNode* node, ViewNode* viewNode) {
    facebook::yoga::resolveRef(node)->setContext(viewNode);
}

ViewNode* Yoga::getAttachedViewNode(YGNode* node) {
    return reinterpret_cast<ViewNode*>(facebook::yoga::resolveRef(node)->getContext());
}

void Yoga::detachViewNode(YGNode* node) {
    facebook::yoga::resolveRef(node)->setContext(nullptr);
}

void Yoga::markNodeDirty(YGNode* node) {
    if (node != nullptr) {
        facebook::yoga::resolveRef(node)->markDirtyAndPropagate();
    }
}

std::string Yoga::nodeToString(YGNode* /*node*/) {
    std::string str;

    // Re-enable if needed, this is currently under an #ifdef DEBUG in Yoga.
    // auto options = static_cast<YGPrintOptions>(YGPrintOptionsLayout | YGPrintOptionsStyle | YGPrintOptionsChildren);
    // facebook::yoga::YGNodeToString(str, node, options, 0);

    return str;
}

} // namespace Valdi
