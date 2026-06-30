//
//  ViewNodeChildrenIndexer.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 12/6/19.
//

#pragma once

#include "valdi_core/cpp/Views/Frame.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/ObjectPool.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace Valdi {

class ViewNode;

constexpr size_t kMaxChildrenBeforeIndexing = 10;

struct ChildrenVisibilityResult {
    // Children which are visible;
    ReusableArray<ViewNode*> visibleChildren;
    // Children which became invisible
    ReusableArray<ViewNode*> invisibleChildren;

    ChildrenVisibilityResult();
};

class ViewNodeChildrenIndexer {
public:
    explicit ViewNodeChildrenIndexer(ViewNode* viewNode);

    void setHorizontal(bool horizontal);

    void setNeedsUpdate();
    bool needsUpdate() const;

    ChildrenVisibilityResult findChildrenVisibility(const Frame& viewport);

private:
    ViewNode* _viewNode;
    std::vector<SmallVector<ViewNode*, 2>> _cells;
    size_t _visibleLowerBound = 0;
    size_t _visibleUpperBound = 0;
    float _cellSize = 0;
    int _updateId = 0;
    bool _horizontal = false;
    bool _needUpdate = true;

    void rebuild();

    void updateVisibleBounds(const Frame& viewport);

    void insertNodeInCells(ViewNode* viewNode, float start, float end);

    void appendNodesIfNeeded(std::vector<ViewNode*>& output, size_t from, size_t to, int updateId);
};

} // namespace Valdi
