//
//  ViewNodeAttributesApplier.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 9/13/19.
//

#include "valdi/runtime/Attributes/ViewNodeAttributesApplier.hpp"
#include "valdi/runtime/Attributes/AttributeHandler.hpp"
#include "valdi/runtime/Attributes/AttributesManager.hpp"
#include "valdi/runtime/Attributes/BoundAttributes.hpp"
#include "valdi/runtime/Attributes/ViewNodeAttribute.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"

#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"

namespace Valdi {

ViewNodeAttributesApplier::ViewNodeAttributesApplier(ViewNode* viewNode) : _viewNode(viewNode) {}

ViewNodeAttributesApplier::~ViewNodeAttributesApplier() = default;

bool ViewNodeAttributesApplier::setAttribute(ViewTransactionScope& viewTransactionScope,
                                             AttributeId id,
                                             const AttributeOwner* owner,
                                             const Value& value,
                                             const Ref<Animator>& animator) {
    if (_viewNode == nullptr) {
        return false;
    }
    if (_viewNode->getViewFactory() == nullptr) {
        return false;
    }

    if (value.isNullOrUndefined()) {
        auto it = _attributes.find(id);
        if (it == _attributes.end()) {
            return false;
        }

        if (animator != nullptr) {
            it->second->willAnimate();
        }

        auto changed = it->second->removeValue(owner);

        if (changed) {
            auto attribute = it->second;

            if (attribute->empty()) {
                _attributes.erase(it);
            }

            processAttributeChange(viewTransactionScope, id, *attribute, animator);
        }

        return changed;
    } else {
        auto attribute = emplaceAttribute(id);
        if (attribute == nullptr) {
            return false;
        }

        if (animator != nullptr) {
            attribute->willAnimate();
        }

        auto changed = attribute->setValue(owner, value);

        if (changed) {
            processAttributeChange(viewTransactionScope, id, *attribute, animator);
        }

        return changed;
    }
}

bool ViewNodeAttributesApplier::hasResolvedAttributeValue(AttributeId id) const {
    const auto& it = _attributes.find(id);
    return it != _attributes.end();
}

Value ViewNodeAttributesApplier::getResolvedAttributeValue(AttributeId id) const {
    const auto& it = _attributes.find(id);
    if (it == _attributes.end()) {
        return Value::undefined();
    }
    return it->second->getResolvedValue();
}

void ViewNodeAttributesApplier::reapplyAttribute(ViewTransactionScope& viewTransactionScope, AttributeId id) {
    const auto& it = _attributes.find(id);
    if (it == _attributes.end()) {
        return;
    }
    auto attribute = it->second;

    attribute->markDirty();

    if (attribute->isCompositePart()) {
        _dirtyCompositeAttributes[attribute->getCompositeAttribute()->getAttributeId()] = nullptr;
    } else {
        updateAttribute(viewTransactionScope, id, *attribute, nullptr, /* justAddedView */ false);
    }
}

bool ViewNodeAttributesApplier::removeAllAttributesForOwner(ViewTransactionScope& viewTransactionScope,
                                                            const AttributeOwner* owner,
                                                            const Ref<Animator>& animator) {
    // This shims tries to efficiently remove the owner from our attributes map.
    // In the optimal case, we do a single iteration where we take care of removing
    // attributes and updating our iterator. In some cases, updating an attribute
    // could trigger a call back inside our attributes map, making our iterator invalid.
    // In those cases we restart our iteration in the map until we finished processing
    // all values.

    auto hadAChange = false;

    auto it = _attributes.begin();
    while (it != _attributes.end()) {
        auto changed = it->second->removeValue(owner);

        if (changed) {
            hadAChange = true;
            auto name = it->first;
            auto attribute = it->second;

            if (attribute->empty()) {
                it = _attributes.erase(it);

                processAttributeChange(viewTransactionScope, name, *attribute, animator);
            } else {
                ++it;

                processAttributeChange(viewTransactionScope, name, *attribute, animator);
            }
        } else {
            ++it;
        }
    }

    return hadAChange;
}

void ViewNodeAttributesApplier::updateAttributeWithoutUpdate(AttributeId id, const Value& value) {
    auto attribute = emplaceAttribute(id);
    if (attribute == nullptr) {
        return;
    }

    attribute->setValue(AttributeOwner::getPlaceholderAttributeOwner(), value);
    attribute->replaceValue(value);

    if (attribute->isCompositePart()) {
        const auto* compositeAttribute = attribute->getCompositeAttribute();
        if (_dirtyCompositeAttributes.find(compositeAttribute->getAttributeId()) == _dirtyCompositeAttributes.end()) {
            // If this attribute belongs to a composite attribute and that the composite attribute is not already
            // marked as dirtied, we directly update the composite value so that it is up to date with our change.

            auto existingCompositeValue = getResolvedAttributeValue(compositeAttribute->getAttributeId());

            auto compositeAttributeValue = existingCompositeValue.getArrayRef();
            if (compositeAttributeValue != nullptr &&
                compositeAttributeValue->size() == compositeAttribute->getParts().size()) {
                // We update the composite attribute value inline, which will make the value up to date
                populateCompositeAttribute(*compositeAttributeValue, *compositeAttribute);
            }
        }
    }
}

void ViewNodeAttributesApplier::processAttributeChange(ViewTransactionScope& viewTransactionScope,
                                                       AttributeId id,
                                                       ViewNodeAttribute& attribute,
                                                       const Ref<Animator>& animator) {
    if (attribute.canAffectLayout()) {
        _viewNode->invalidateMeasuredSize();
    }

    if (id == DefaultAttributeTranslationX) {
        _viewNode->setTranslationX(attribute.getResolvedValue().toFloat());
    } else if (id == DefaultAttributeTranslationY) {
        _viewNode->setTranslationY(attribute.getResolvedValue().toFloat());
    } else if (id == DefaultAttributeAccessibilityId) {
        auto value = attribute.getResolvedValue();
        if (value.isNullOrUndefined()) {
            _viewNode->setAccessibilityId(StringBox());
        } else {
            _viewNode->setAccessibilityId(value.toStringBox());
        }
    }

    if (attribute.isCompositePart()) {
        _dirtyCompositeAttributes[attribute.getCompositeAttribute()->getAttributeId()] = animator;
    } else {
        updateAttribute(viewTransactionScope, id, attribute, animator, /* justAddedView */ false);
    }
}

void ViewNodeAttributesApplier::willRemoveView(ViewTransactionScope& viewTransactionScope) {
    SC_ASSERT(_hasView);

    _hasView = false;
    updateAttributes(viewTransactionScope, nullptr, false);
}

void ViewNodeAttributesApplier::didAddView(ViewTransactionScope& viewTransactionScope, const Ref<Animator>& animator) {
    SC_ASSERT(!_hasView);

    _hasView = true;
    updateAttributes(viewTransactionScope, animator, true);
}

void ViewNodeAttributesApplier::updateAttributes(ViewTransactionScope& viewTransactionScope,
                                                 const Ref<Animator>& animator,
                                                 bool justAddedView) {
    for (const auto& it : _attributes) {
        if (it.second->isCompositePart()) {
            continue;
        }

        auto key = it.first;
        auto attribute = it.second;
        updateAttribute(viewTransactionScope, key, *attribute, animator, justAddedView);
    }
}

void ViewNodeAttributesApplier::updateAttribute(ViewTransactionScope& viewTransactionScope,
                                                AttributeId id,
                                                ViewNodeAttribute& attribute,
                                                const Ref<Animator>& animator,
                                                bool justAddedView) {
    Result<Void> result;
    if (animator != nullptr && _viewNode->isAnimationsEnabled()) {
        result = attribute.update(viewTransactionScope, _viewNode, _hasView, justAddedView, animator);
    } else {
        result = attribute.update(viewTransactionScope, _viewNode, _hasView, justAddedView, nullptr);
    }

    if (!result) {
        onApplyAttributeFailed(id, result.error());
    }
}

void ViewNodeAttributesApplier::onApplyAttributeFailed(AttributeId id, const Error& error) {
    VALDI_ERROR(_viewNode->getLogger(),
                "{}, Could not apply attribute '{}' in class {}: {}",
                _viewNode->getLoggerFormatPrefix(),
                getAttributeName(id),
                _boundAttributes->getClassName(),
                error);
}

void ViewNodeAttributesApplier::flush(ViewTransactionScope& viewTransactionScope) {
    if (_viewNode == nullptr) {
        return;
    }

    while (!_dirtyCompositeAttributes.empty()) {
        auto dirtyCompositeAttribute = *_dirtyCompositeAttributes.begin();
        _dirtyCompositeAttributes.erase(_dirtyCompositeAttributes.begin());

        updateCompositeAttribute(viewTransactionScope, dirtyCompositeAttribute.first, dirtyCompositeAttribute.second);
    }
}

bool ViewNodeAttributesApplier::needsFlush() const {
    return !_dirtyCompositeAttributes.empty();
}

void ViewNodeAttributesApplier::updateCompositeAttribute(ViewTransactionScope& viewTransactionScope,
                                                         AttributeId compositeId,
                                                         const Ref<Animator>& animator) {
    auto attribute = emplaceAttribute(compositeId);

    SC_ASSERT_NOTNULL(attribute);

    const auto& compositeAttribute = attribute->getCompositeAttribute();
    SC_ASSERT(compositeAttribute != nullptr,
              "Composite attributes should have a compositeAttribute object associated with them");

    auto value = ValueArray::make(compositeAttribute->getParts().size());
    auto result = populateCompositeAttribute(*value, *compositeAttribute);

    if (result.hasAValue) {
        if (result.isIncomplete) {
            VALDI_ERROR(getLogger(),
                        "{} Could not apply composite attribute '{}' in class {}: Attribute is incomplete",
                        _viewNode->getLoggerFormatPrefix(),
                        getAttributeName(compositeId),
                        _boundAttributes->getClassName());
            return;
        }

        setAttribute(viewTransactionScope, compositeId, this, Value(value), animator);
    } else {
        removeAttribute(viewTransactionScope, compositeId, this, animator);
    }
}

CompositeAttributeResult ViewNodeAttributesApplier::populateCompositeAttribute(
    ValueArray& valueArray, const CompositeAttribute& compositeAttribute) {
    CompositeAttributeResult result = {false, false, false};

    size_t size = compositeAttribute.getParts().size();
    SC_ASSERT(valueArray.size() == size);

    for (size_t index = 0; index < size; index++) {
        const auto& part = compositeAttribute.getParts()[index];

        auto attributeValue = _attributes.find(part.attributeId);
        Value valueToInsert;

        if (attributeValue == _attributes.end() || attributeValue->second->empty()) {
            if (!part.optional) {
                result.isIncomplete = true;
            }

            // valueToInsert will be null
        } else {
            result.hasAValue = true;

            auto attributeResult = attributeValue->second->getResolvedProcessedValue(*_viewNode);

            if (attributeResult) {
                valueToInsert = attributeResult.moveValue();
            } else {
                result.failed = true;
                VALDI_ERROR(
                    getLogger(),
                    "{} Could not populate composite attribute {} with part '{}' with value '{}' in class {}: {}",
                    _viewNode->getLoggerFormatPrefix(),
                    getAttributeName(compositeAttribute.getAttributeId()),
                    getAttributeName(part.attributeId),
                    attributeValue->second->getResolvedValue().toString(),
                    _boundAttributes->getClassName(),
                    attributeResult.error());
            }
        }

        valueArray[index] = std::move(valueToInsert);
    }

    return result;
}

const AttributeHandler* ViewNodeAttributesApplier::getAttributeHandler(AttributeId id) const {
    const auto* attributeHandler = _boundAttributes->getAttributeHandlerForId(id);
    if (VALDI_UNLIKELY(attributeHandler == nullptr)) {
        VALDI_ERROR(getLogger(),
                    "{} Could not find attribute '{}' in class {}",
                    _viewNode->getLoggerFormatPrefix(),
                    getAttributeName(id),
                    _boundAttributes->getClassName());
    }

    return attributeHandler;
}

Ref<ViewNodeAttribute> ViewNodeAttributesApplier::emplaceAttribute(AttributeId id) {
    auto it = _attributes.find(id);
    if (it != _attributes.end()) {
        return it->second;
    }

    SC_ASSERT_NOTNULL(_boundAttributes);
    SC_ASSERT_NOTNULL(_viewNode);

    const auto* attributeHandler = getAttributeHandler(id);
    if (VALDI_UNLIKELY(attributeHandler == nullptr)) {
        return nullptr;
    }

    auto attribute = makeShared<ViewNodeAttribute>(attributeHandler);

    _attributes.mutate([&](auto& container) { container[id] = attribute; });

    return attribute;
}

void ViewNodeAttributesApplier::copyViewLayoutAttributes(ViewNodeAttributesApplier& attributesApplier) {
    for (const auto& it : _attributes) {
        if (it.second->canAffectLayout() && it.second->requiresView()) {
            auto name = it.first;
            auto attribute = it.second;

            attributesApplier._attributes.mutate([&](auto& container) { container[name] = attribute->copy(); });
        }
    }
}

Result<Ref<ValueMap>> ViewNodeAttributesApplier::copyProcessedViewLayoutAttributes() {
    auto out = makeShared<ValueMap>();

    for (const auto& it : _attributes) {
        if (it.second->canAffectLayout() && it.second->requiresView() && !it.second->isCompositePart()) {
            auto result = it.second->getResolvedProcessedValue(*_viewNode);
            if (!result) {
                VALDI_ERROR(_viewNode->getLogger(),
                            "{}, Could not preprocess attribute '{}' in class {}: {}",
                            _viewNode->getLoggerFormatPrefix(),
                            it.second->getAttributeName(),
                            _boundAttributes->getClassName(),
                            result.error());

                return result.moveError();
            }

            (*out)[it.second->getAttributeName()] = result.value();
        }
    }

    return out;
}

Ref<ValueMap> ViewNodeAttributesApplier::dumpResolvedAttributes() const {
    auto out = makeShared<ValueMap>();
    for (const auto& it : _attributes) {
        if (it.second->getCompositeAttribute() == nullptr || it.second->isCompositePart()) {
            (*out)[getAttributeName(it.first)] = it.second->getResolvedValue();
        }
    }

    return out;
}

Ref<ValueMap> ViewNodeAttributesApplier::dumpAttributes() const {
    auto out = makeShared<ValueMap>();
    for (const auto& it : _attributes) {
        (*out)[getAttributeName(it.first)] = it.second->dump();
    }

    return out;
}

ILogger& ViewNodeAttributesApplier::getLogger() const {
    return _viewNode->getLogger();
}

void ViewNodeAttributesApplier::setBoundAttributes(Ref<BoundAttributes> boundAttributes) {
    auto hadAttributes = _boundAttributes != nullptr;
    _boundAttributes = std::move(boundAttributes);

    if (_boundAttributes == nullptr) {
        // Clear state so flush() / emplaceAttribute() are never called with null _boundAttributes.
        _dirtyCompositeAttributes.clear();
        _attributes.clear();
    } else if (VALDI_UNLIKELY(hadAttributes)) {
        updateAttributeHandlers();
    }
}

const Ref<BoundAttributes>& ViewNodeAttributesApplier::getBoundAttributes() const {
    return _boundAttributes;
}

void ViewNodeAttributesApplier::updateAttributeHandlers() {
    std::vector<AttributeId> attributeIdsToRemove;

    for (const auto& it : _attributes) {
        const auto* handler = getAttributeHandler(it.first);
        if (VALDI_LIKELY(handler != nullptr)) {
            it.second->setHandler(handler);

            if (handler->requiresView()) {
                // View attributes need to be re-applied
                it.second->markDirty();
            }
        } else {
            attributeIdsToRemove.emplace_back(it.first);
        }
    }

    for (const auto& attributeId : attributeIdsToRemove) {
        _attributes.mutate([&](auto& container) {
            auto it = container.find(attributeId);
            if (it != container.end()) {
                container.erase(it);
            }
        });
    }
}

void ViewNodeAttributesApplier::destroy() {
    _viewNode = nullptr;
    _dirtyCompositeAttributes.clear();
    _attributes.clear();
}

int ViewNodeAttributesApplier::getAttributePriority(AttributeId id) const {
    auto bestPriority = INT_MAX;

    // We set our composite attribute priority as the lowest priority we can
    // find among all composite parts.

    const auto* handler = _boundAttributes->getAttributeHandlerForId(id);
    for (const auto& part : handler->getCompositeAttribute()->getParts()) {
        const auto& it = _attributes.find(part.attributeId);
        if (it == _attributes.end()) {
            continue;
        }
        bestPriority = std::min(bestPriority, it->second->getResolvedValuePriority());
    }

    return bestPriority;
}

StringBox ViewNodeAttributesApplier::getAttributeSource(AttributeId /*id*/) const {
    static auto kSource = STRING_LITERAL("composite");

    return kSource;
}

AttributeIds& ViewNodeAttributesApplier::getAttributeIds() const {
    return _viewNode->getAttributeIds();
}

StringBox ViewNodeAttributesApplier::getAttributeName(AttributeId id) const {
    return getAttributeIds().getNameForId(id);
}

} // namespace Valdi
