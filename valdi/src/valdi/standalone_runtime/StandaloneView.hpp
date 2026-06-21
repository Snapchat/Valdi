//
//  StandaloneView.hpp
//  valdi-standalone-pc
//
//  Created by Simon Corsin on 10/14/19.
//

#pragma once

#include "valdi_core/cpp/Views/Frame.hpp"
#include "valdi/runtime/Views/View.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

#include <sstream>

namespace Valdi {

using AttributesHistory = FlatMap<StringBox, std::vector<Value>>;

class StandaloneView : public View {
public:
    explicit StandaloneView(const StringBox& className);
    StandaloneView(const StringBox& className, bool keepAttributesHistory, bool allowPooling);
    ~StandaloneView() override;

    Ref<StandaloneView> getChild(size_t index) const;

    Ref<StandaloneView> getParent() const;

    void removeFromParent();
    void addChild(const Ref<StandaloneView>& child);
    void addChild(const Ref<StandaloneView>& child, size_t index);
    const std::vector<Ref<StandaloneView>>& getChildren() const;

    void setAttribute(const StringBox& name, const Value& value);
    const Value& getAttribute(const StringBox& name) const;

    void setFrame(const Frame& frame);
    const Frame& getFrame() const;

    const Shared<AttributesHistory>& getAttributesHistory() const;

    Value serialize() const;

    std::string toXML() const;

    VALDI_CLASS_HEADER(StandaloneView)

    bool operator==(const StandaloneView& other) const;
    bool operator!=(const StandaloneView& other) const;

    StringBox getViewClassName() const;

    snap::valdi_core::Platform getPlatform() const override;

    int getInvalidateLayoutCount() const;
    int getLayoutDidBecomeDirtyCount() const;
    void incrementLayoutDidBecomeDirty();

    bool isLayoutInvalidated() const;
    void setLayoutInvalidated(bool layoutInvalidated);
    void invalidateLayout();

    bool isRightToLeft() const;
    void setIsRightToLeft(bool isRightToLeft);

    bool allowsPooling() const;

    const Valdi::Ref<Valdi::LoadedAsset>& getLoadedAsset() const;
    void setLoadedAsset(const Valdi::Ref<Valdi::LoadedAsset>& loadedAsset);

    static Ref<StandaloneView> unwrap(const Ref<View>& view);

private:
    StringBox _className;
    Weak<StandaloneView> _parent;
    std::vector<Ref<StandaloneView>> _children;
    Shared<AttributesHistory> _attributesHistory;
    Ref<ValueMap> _attributes;
    Valdi::Ref<Valdi::LoadedAsset> _loadedAsset;
    Frame _frame;
    int _invalidateLayoutCount = 0;
    int _layoutDidBecomeDirtyCount = 0;
    bool _allowPooling;
    bool _layoutInvalidated = true;
    bool _isRightToLeft = false;

    void outputXML(std::ostringstream& stream, int indent) const;
};

} // namespace Valdi
