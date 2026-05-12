//
//  ViewNodeAttributesApplier.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 9/13/19.
//

#pragma once

#include "valdi/runtime/Attributes/AttributeOwner.hpp"
#include "valdi/runtime/Attributes/AttributesApplier.hpp"
#include "valdi/runtime/Utils/SafeReentrantContainer.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/FlatSet.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"

namespace Valdi {

class ViewNode;
class BoundAttributes;
class ILogger;
class CompositeAttribute;
class ViewNodeAttribute;
class AttributeHandler;
class ViewTransactionScope;

struct CompositeAttributeResult {
    bool failed;
    bool hasAValue;
    bool isIncomplete;
};

/**
 Attributes applier for a ViewNode
 */
class ViewNodeAttributesApplier : public AttributesApplier, public AttributeOwner {
public:
    explicit ViewNodeAttributesApplier(ViewNode* viewNode);
    ~ViewNodeAttributesApplier() override;

    bool setAttribute(ViewTransactionScope& viewTransactionScope,
                      AttributeId id,
                      const AttributeOwner* owner,
                      const Value& value,
                      const Ref<Animator>& animator) override;

    bool hasResolvedAttributeValue(AttributeId id) const override;
    Value getResolvedAttributeValue(AttributeId id) const override;

    bool removeAllAttributesForOwner(ViewTransactionScope& viewTransactionScope,
                                     const AttributeOwner* owner,
                                     const Ref<Animator>& animator) override;

    void reapplyAttribute(ViewTransactionScope& viewTransactionScope, AttributeId id) override;

    void flush(ViewTransactionScope& viewTransactionScope) override;
    bool needsFlush() const override;

    void updateAttributeWithoutUpdate(AttributeId id, const Value& value);

    void willRemoveView(ViewTransactionScope& viewTransactionScope);
    void didAddView(ViewTransactionScope& viewTransactionScope, const Ref<Animator>& animator);

    /**
     Returns a map with all the resolved attributes as key value
     */
    Ref<ValueMap> dumpResolvedAttributes() const;

    /**
     Returns a map containing all attributes and their sources
     */
    Ref<ValueMap> dumpAttributes() const;

    /**
     Copy all the attributes requiring a view and which can have an impact in the layout.
     */
    void copyViewLayoutAttributes(ViewNodeAttributesApplier& attributesApplier);

    Result<Ref<ValueMap>> copyProcessedViewLayoutAttributes();

    const Ref<BoundAttributes>& getBoundAttributes() const;
    void setBoundAttributes(Ref<BoundAttributes> boundAttributes);

    // Used for composite attributes
    int getAttributePriority(AttributeId id) const override;
    StringBox getAttributeSource(AttributeId id) const override;

    ILogger& getLogger() const;

    void onApplyAttributeFailed(AttributeId id, const Error& error);

    void destroy();

private:
    ViewNode* _viewNode;
    Ref<BoundAttributes> _boundAttributes;

    // Attributes set on this applier.
    SafeReentrantContainer<FlatMap<AttributeId, Ref<ViewNodeAttribute>>> _attributes;
    FlatMap<AttributeId, Ref<Animator>> _dirtyCompositeAttributes;

    bool _hasView = false;

    void updateCompositeAttribute(ViewTransactionScope& viewTransactionScope,
                                  AttributeId compositeId,
                                  const Ref<Animator>& animator);

    Ref<ViewNodeAttribute> emplaceAttribute(AttributeId id);
    void processAttributeChange(ViewTransactionScope& viewTransactionScope,
                                AttributeId id,
                                ViewNodeAttribute& attribute,
                                const Ref<Animator>& animator);

    CompositeAttributeResult populateCompositeAttribute(ValueArray& valueArray,
                                                        const CompositeAttribute& compositeAttribute);

    void updateAttributes(ViewTransactionScope& viewTransactionScope,
                          const Ref<Animator>& animator,
                          bool justAddedView);

    void updateAttribute(ViewTransactionScope& viewTransactionScope,
                         AttributeId id,
                         ViewNodeAttribute& attribute,
                         const Ref<Animator>& animator,
                         bool justAddedView);

    AttributeIds& getAttributeIds() const;
    StringBox getAttributeName(AttributeId id) const;

    void updateAttributeHandlers();

    const AttributeHandler* getAttributeHandler(AttributeId id) const;
};
} // namespace Valdi
