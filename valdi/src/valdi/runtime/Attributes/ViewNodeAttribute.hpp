//
//  ViewNodeAttribute.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 9/13/19.
//

#pragma once

#include "valdi/runtime/Attributes/AttributeIds.hpp"
#include "valdi/runtime/Attributes/AttributeValue.hpp"
#include "valdi/runtime/Attributes/PreprocessorCache.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

namespace Valdi {

class ViewNode;
class ILogger;
class Animator;
class AttributeHandler;
class AttributeOwner;
class CompositeAttribute;
class ViewTransactionScope;

class ViewNodeAttribute : public SimpleRefCountable {
public:
    explicit ViewNodeAttribute(const AttributeHandler* handler);
    ~ViewNodeAttribute() override;

    /**
     Update the attribute to the given ViewNode.
     This function will be a no-op if no updates need to be done.
     */
    [[nodiscard]] Result<Void> update(ViewTransactionScope& viewTransactionScope,
                                      ViewNode* viewNode,
                                      bool hasView,
                                      bool justAddedView,
                                      const Ref<Animator>& animator);

    void markDirty();

    /**
     Prepare this attribute for an animation.
     */
    void willAnimate();

    /**
     Copy this attribute with its resolved value
     */
    Ref<ViewNodeAttribute> copy();

    /**
     Returns the value for the given owner.
     Returns undefined if the value is not set.
     */
    Value getValue(const AttributeOwner* owner) const;

    /**
     Set the value for the given owner.
     Returns whether the resolved value has changed.
     This function should be used ONLY IF isTrivial() returns false
     */
    bool setValue(const AttributeOwner* owner, const Value& value);

    /**
     Remove the value for the given owner.
     Returns whether the resolved value has changed
     */
    bool removeValue(const AttributeOwner* owner);

    /**
     Replace the value in all owners by the given value.
     */
    void replaceValue(const Value& value);

    /**
     Whether this attribute has no attached values.
     */
    bool empty() const;

    /**
     Returns whether this represents a composite attribute part.
     */
    bool isCompositePart() const;

    /**
     Whether this attribute can affect the layout calculation.
     */
    bool canAffectLayout() const;

    /**
     Whether this attribute needs a view to be applied
     */
    bool requiresView() const;

    const CompositeAttribute* getCompositeAttribute() const;

    /**
     Returns the resolved value for this attribute.
     */
    [[nodiscard]] Value getResolvedValue() const;

    [[nodiscard]] Result<Value> getResolvedPreprocessedValue();

    [[nodiscard]] Result<Value> getResolvedProcessedValue(ViewNode& viewNode);

    /**
     Returns a dump containing all held values and their sources
     */
    Value dump() const;

    /**
     Get the priority for the resolved value
     */
    int getResolvedValuePriority() const;

    AttributeId getAttributeId() const;
    const StringBox& getAttributeName() const;

    /**
     Replace the attribute handler that this ViewNodeAttribute holds
     */
    void setHandler(const AttributeHandler* handler);

private:
    const AttributeHandler* _handler;
    // Set whenever animating an attribute for a ViewNode which doesn't have a view
    std::unique_ptr<Result<Value>> _pendingAnimatedValue;

    // Our dynamic storage, which can be either a single value or a collection of values
    std::aligned_union<0, AttributeValue, AttributeValueCollection*>::type _valuesUnion;

    // Whether the handler needs a view for apply to work
    bool _handlerNeedsView = false;
    bool _handlerCanAffectLayout = false;
    bool _handlerIsCompositePart = false;

    bool _appliedValueDirty = false;
    bool _hasAppliedValue = false;
    bool _hasSingleAttribute = false;
    bool _hasAttributeCollection = false;

    Result<Void> flushPendingAnimatedValue(ViewTransactionScope& viewTransactionScope,
                                           ViewNode* viewNode,
                                           const std::unique_ptr<Result<Value>>& pendingAnimatedValue,
                                           bool willAnimate);

    inline void unsafeSetAsTrivial(const AttributeOwner* owner, Value&& value) {
        new (&_valuesUnion) AttributeValue(owner, std::move(value));
        _hasSingleAttribute = true;
        _appliedValueDirty = true;
    }

    inline const AttributeValue& getSingleAttributeValue() const {
        return *reinterpret_cast<const AttributeValue*>(&_valuesUnion);
    }

    inline AttributeValue& getSingleAttributeValue() {
        return *reinterpret_cast<AttributeValue*>(&_valuesUnion);
    }

    inline const AttributeValueCollection& getAttributeValueCollection() const {
        return **reinterpret_cast<const AttributeValueCollection* const*>(&_valuesUnion);
    }

    inline AttributeValueCollection& getAttributeValueCollection() {
        return **reinterpret_cast<AttributeValueCollection**>(&_valuesUnion);
    }

    AttributeValue* getActiveAttributeValue();
};
} // namespace Valdi
