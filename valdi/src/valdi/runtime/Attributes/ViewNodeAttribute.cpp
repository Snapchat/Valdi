//
//  ViewNodeAttribute.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 9/13/19.
//

#include "valdi/runtime/Attributes/ViewNodeAttribute.hpp"
#include "valdi/runtime/Attributes/AttributeHandler.hpp"
#include "valdi/runtime/Attributes/AttributeOwner.hpp"
#include "valdi/runtime/Attributes/AttributesApplier.hpp"

#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"

#include <limits>

namespace Valdi {

ViewNodeAttribute::ViewNodeAttribute(const AttributeHandler* handler)
    : _handler(handler),
      _handlerNeedsView(handler->requiresView()),
      _handlerCanAffectLayout(handler->shouldInvalidateLayoutOnChange()),
      _handlerIsCompositePart(handler->isCompositePart()) {}

ViewNodeAttribute::~ViewNodeAttribute() {
    if (_hasSingleAttribute) {
        getSingleAttributeValue().~AttributeValue();
    } else if (_hasAttributeCollection) {
        delete &getAttributeValueCollection();
    }
}

Value ViewNodeAttribute::getValue(const AttributeOwner* owner) const {
    if (_hasSingleAttribute) {
        const auto& attributeValue = getSingleAttributeValue();
        if (attributeValue.owner == owner) {
            return attributeValue.value;
        } else {
            return Value::undefined();
        }
    } else if (_hasAttributeCollection) {
        for (const auto& attributeValue : getAttributeValueCollection().values) {
            if (attributeValue.owner == owner) {
                return attributeValue.value;
            }
        }
        return Value::undefined();
    } else {
        return Value::undefined();
    }
}

bool ViewNodeAttribute::setValue(const AttributeOwner* owner, const Value& value) {
    if (_hasSingleAttribute) {
        auto& attributeValue = getSingleAttributeValue();

        if (attributeValue.owner == owner) {
            // We have a single attribute matching the given owner

            if (attributeValue.value == value) {
                // Value is the same, nothing has changed
                return false;
            } else {
                // Value has changed
                attributeValue.value = value;
                attributeValue.preprocessedValue = Result<PreprocessedValue>();

                _appliedValueDirty = true;
                return true;
            }
        } else {
            // Our single attribute's owner doesn't match the given owner
            // We need to turn our single attribute into attribute collection
            // to fit the new owner

            auto attributeValueCopy = attributeValue;
            attributeValue.~AttributeValue();

            _hasSingleAttribute = false;

            *reinterpret_cast<AttributeValueCollection**>(&_valuesUnion) = new AttributeValueCollection();
            _hasAttributeCollection = true;

            auto& attributeCollection = getAttributeValueCollection();
            auto& inserted =
                attributeCollection.values.emplace_back(attributeValueCopy.owner, Value(attributeValueCopy.value));
            inserted.preprocessedValue = std::move(attributeValueCopy.preprocessedValue);

            // Try again now that we have an attribute collection
            return setValue(owner, value);
        }
    } else if (_hasAttributeCollection) {
        auto& attributeCollection = getAttributeValueCollection();

        size_t i = 0;
        for (auto& attributeValue : attributeCollection.values) {
            if (attributeValue.owner == owner) {
                // Found existing value for owner

                if (attributeValue.value == value) {
                    // Value is the same, nothing has changed
                    return false;
                } else {
                    attributeValue.value = value;
                    attributeValue.preprocessedValue = Result<PreprocessedValue>();

                    if (attributeCollection.resolvedValueIndex == i) {
                        // Value has changed and is currently being used
                        _appliedValueDirty = true;
                        return true;
                    } else {
                        // The value for the owner changed, but the current used value
                        // is in a different owner.
                        return false;
                    }
                }
            }
            i++;
        }

        // Didn't find an existing value, appending the value to the collection
        attributeCollection.values.emplace_back(owner, Value(value));

        if (attributeCollection.updateResolvedValueIndex(getAttributeId())) {
            // Resolved value index changed, we need to reapply the attribute
            _appliedValueDirty = true;
            return true;
        }

        // We added the new value but the current used value
        // is in a different owner.
        return false;
    } else {
        unsafeSetAsTrivial(owner, Value(value));
        return true;
    }
}

bool ViewNodeAttribute::removeValue(const AttributeOwner* owner) {
    if (_hasSingleAttribute) {
        auto& attributeValue = getSingleAttributeValue();
        if (attributeValue.owner == owner) {
            // Deconstruct our single attribute, which will make our ViewNodeAttribute empty
            _hasSingleAttribute = false;
            attributeValue.~AttributeValue();
            return true;
        } else {
            return false;
        }
    } else if (_hasAttributeCollection) {
        auto& attributeCollection = getAttributeValueCollection();
        size_t i = 0;

        for (auto& attributeValue : attributeCollection.values) {
            if (attributeValue.owner == owner) {
                // Owner found, erase it from the collection

                attributeCollection.values.erase(attributeCollection.values.begin() + i);

                if (i == attributeCollection.resolvedValueIndex) {
                    // If our owner was the resolved value, update the
                    attributeCollection.updateResolvedValueIndex(getAttributeId());
                    _appliedValueDirty = true;

                    return true;
                } else if (i < attributeCollection.resolvedValueIndex) {
                    attributeCollection.resolvedValueIndex--;
                }

                return false;
            }

            i++;
        }

        // Owner was not found in the collection

        return false;
    } else {
        return false;
    }
}

void ViewNodeAttribute::replaceValue(const Value& value) {
    // TODO(simon): In replaceValue() we are overidding values
    // in every existing attribute. This is so that attributes
    // updated from platform side can be reflected here. We should
    // remove this concept at some point and instead have contentOffset
    // and others not exposed as an attribute.

    if (_hasSingleAttribute) {
        auto& attributeValue = getSingleAttributeValue();
        attributeValue.value = value;
        attributeValue.preprocessedValue = PreprocessedValue(value);
    } else if (_hasAttributeCollection) {
        for (auto& attributeValue : getAttributeValueCollection().values) {
            attributeValue.value = value;
            attributeValue.preprocessedValue = PreprocessedValue(value);
        }
    }

    _appliedValueDirty = false;
    _hasAppliedValue = true;
}

Value ViewNodeAttribute::getResolvedValue() const {
    if (_hasSingleAttribute) {
        return getSingleAttributeValue().value;
    } else if (_hasAttributeCollection) {
        const auto& attributeCollection = getAttributeValueCollection();
        if (attributeCollection.values.empty()) {
            return Value::undefined();
        }

        SC_ASSERT(attributeCollection.resolvedValueIndex < attributeCollection.values.size());
        return attributeCollection.values[attributeCollection.resolvedValueIndex].value;
    } else {
        return Value::undefined();
    }
}

AttributeValue* ViewNodeAttribute::getActiveAttributeValue() {
    if (_hasSingleAttribute) {
        return &getSingleAttributeValue();
    } else if (_hasAttributeCollection) {
        auto& attributeCollection = getAttributeValueCollection();
        if (attributeCollection.values.empty()) {
            return nullptr;
        }

        SC_ASSERT(attributeCollection.resolvedValueIndex < attributeCollection.values.size());
        return &attributeCollection.values[attributeCollection.resolvedValueIndex];
    } else {
        return nullptr;
    }
}

Result<Value> ViewNodeAttribute::getResolvedPreprocessedValue() {
    auto* attributeValue = getActiveAttributeValue();
    if (attributeValue == nullptr) {
        return Value::undefined();
    }

    return attributeValue->preprocessValue(_handler);
}

Result<Value> ViewNodeAttribute::getResolvedProcessedValue(ViewNode& viewNode) {
    auto result = getResolvedPreprocessedValue();

    if (result) {
        const auto& value = result.value();
        if (!value.isUndefined()) {
            result = _handler->postprocess(viewNode, value);
        }
    }

    return result;
}

int ViewNodeAttribute::getResolvedValuePriority() const {
    if (_hasSingleAttribute) {
        return getSingleAttributeValue().owner->getAttributePriority(getAttributeId());
    } else if (_hasAttributeCollection) {
        return getAttributeValueCollection().resolveValueIndexAndPriority(getAttributeId()).priority;
    } else {
        return std::numeric_limits<int>::max();
    }
}

Value ViewNodeAttribute::dump() const {
    Ref<ValueArray> values;

    size_t activeIndex = 0;

    if (_hasSingleAttribute) {
        const auto& attributeValue = getSingleAttributeValue();

        values = ValueArray::make(1);
        values->emplace(0, attributeValue.dump(getAttributeId()));
    } else if (_hasAttributeCollection) {
        const auto& attributeCollection = getAttributeValueCollection();

        values = ValueArray::make(attributeCollection.values.size());
        size_t i = 0;

        for (const auto& value : attributeCollection.values) {
            values->emplace(i++, value.dump(getAttributeId()));
        }

        activeIndex = attributeCollection.resolvedValueIndex;
    } else {
        values = ValueArray::make(0);
    }

    auto out = makeShared<ValueMap>();
    out->reserve(2);

    static auto kValuesKey = STRING_LITERAL("values");
    static auto kActiveIndexKey = STRING_LITERAL("activeValueIndex");

    (*out)[kValuesKey] = Value(values);
    (*out)[kActiveIndexKey] = Value(static_cast<int32_t>(activeIndex));

    return Value(out);
}

bool ViewNodeAttribute::empty() const {
    if (_hasSingleAttribute) {
        return false;
    } else if (_hasAttributeCollection) {
        return getAttributeValueCollection().values.empty();
    } else {
        return true;
    }
}

bool ViewNodeAttribute::requiresView() const {
    return _handlerNeedsView;
}

bool ViewNodeAttribute::canAffectLayout() const {
    return _handlerCanAffectLayout;
}

bool ViewNodeAttribute::isCompositePart() const {
    return _handlerIsCompositePart;
}

const CompositeAttribute* ViewNodeAttribute::getCompositeAttribute() const {
    return _handler->getCompositeAttribute().get();
}

AttributeId ViewNodeAttribute::getAttributeId() const {
    return _handler->getId();
}

const StringBox& ViewNodeAttribute::getAttributeName() const {
    return _handler->getName();
}

Result<Void> ViewNodeAttribute::update(ViewTransactionScope& viewTransactionScope,
                                       ViewNode* viewNode,
                                       bool hasView,
                                       bool justAddedView,
                                       const Ref<Animator>& animator) {
    auto needValue = !empty() && (!_handlerNeedsView || hasView);

    if (needValue) {
        if (!_hasAppliedValue || _appliedValueDirty) {
            _hasAppliedValue = true;
            _appliedValueDirty = false;

            auto pendingAnimatedValue = std::move(_pendingAnimatedValue);

            auto result = getResolvedPreprocessedValue();
            if (!result) {
                return result.moveError();
            }

            // The intention here is to only apply the attribute values that are supposed to describe the initial
            // pre-animation state of the view, but got captured in the view's "appearing in the viewport" animation
            bool shouldForceApplyWithoutAnimation = pendingAnimatedValue == nullptr && justAddedView;
            if (shouldForceApplyWithoutAnimation) {
                return _handler->applyAttribute(viewTransactionScope, *viewNode, result.value(), nullptr);
            }
            auto flushResult =
                flushPendingAnimatedValue(viewTransactionScope, viewNode, pendingAnimatedValue, animator != nullptr);

            auto applyResult = _handler->applyAttribute(viewTransactionScope, *viewNode, result.value(), animator);

            if (!applyResult) {
                return applyResult;
            }

            if (!flushResult) {
                return flushResult;
            }
        }

        return Void();
    } else {
        if (_hasAppliedValue) {
            _hasAppliedValue = false;
            _appliedValueDirty = false;

            auto pendingAnimatedValue = std::move(_pendingAnimatedValue);
            auto flushResult =
                flushPendingAnimatedValue(viewTransactionScope, viewNode, pendingAnimatedValue, animator != nullptr);

            _handler->resetAttribute(viewTransactionScope, *viewNode, animator);

            return flushResult;
        }

        return Void();
    }
}

void ViewNodeAttribute::willAnimate() {
    if (!_handlerNeedsView || empty() || _hasAppliedValue) {
        return;
    }

    _pendingAnimatedValue = std::make_unique<Result<Value>>(getResolvedPreprocessedValue());
}

Result<Void> ViewNodeAttribute::flushPendingAnimatedValue(ViewTransactionScope& viewTransactionScope,
                                                          ViewNode* viewNode,
                                                          const std::unique_ptr<Result<Value>>& pendingAnimatedValue,
                                                          bool willAnimate) {
    if (!willAnimate || pendingAnimatedValue == nullptr) {
        return Void();
    }

    if (!pendingAnimatedValue->success()) {
        return pendingAnimatedValue->moveError();
    }

    return _handler->applyAttribute(viewTransactionScope, *viewNode, pendingAnimatedValue->value(), nullptr);
}

void ViewNodeAttribute::markDirty() {
    _appliedValueDirty = true;

    if (_hasSingleAttribute) {
        getSingleAttributeValue().preprocessedValue = Result<PreprocessedValue>();
    } else if (_hasAttributeCollection) {
        for (auto& it : getAttributeValueCollection().values) {
            it.preprocessedValue = Result<PreprocessedValue>();
        }
    }
}

Ref<ViewNodeAttribute> ViewNodeAttribute::copy() {
    auto copy = makeShared<ViewNodeAttribute>(_handler);

    if (!empty()) {
        auto result = getResolvedPreprocessedValue();
        if (result) {
            copy->unsafeSetAsTrivial(nullptr, Value(result.value()));
            copy->getSingleAttributeValue().preprocessedValue = PreprocessedValue(result.moveValue());
        }
        copy->_appliedValueDirty = true;
    }

    return copy;
}

void ViewNodeAttribute::setHandler(const AttributeHandler* handler) {
    _handler = handler;
    _handlerNeedsView = handler->requiresView();
    _handlerCanAffectLayout = handler->shouldInvalidateLayoutOnChange();
    _handlerIsCompositePart = handler->isCompositePart();
}

} // namespace Valdi
