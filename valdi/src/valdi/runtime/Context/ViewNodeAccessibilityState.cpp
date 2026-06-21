#include "valdi/runtime/Context/ViewNodeAccessibilityState.hpp"
#include "valdi/runtime/Attributes/TextAttributeValueParser.hpp"
#include "valdi_core/cpp/Views/Measure.hpp"
#include "valdi_core/cpp/Events/TouchEvents.hpp"

#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/Utils/Marshaller.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

namespace Valdi {

ViewNodeAccessibilityState::ViewNodeAccessibilityState(const ViewNodeAttributesApplier& attributesApplier)
    : _attributesApplier(attributesApplier),
      _accessibilityCategory(AccessibilityCategoryAuto),
      _accessibilityNavigation(AccessibilityNavigationAuto),
      _accessibilityPriority(0),
      _accessibilityStateDisabled(false),
      _accessibilityStateSelected(false),
      _accessibilityStateLiveRegion(false) {}

ViewNodeAccessibilityState::~ViewNodeAccessibilityState() = default;

void ViewNodeAccessibilityState::setAccessibilityCategory(AccessibilityCategory accessibilityCategory) {
    _accessibilityCategory = accessibilityCategory;
}

AccessibilityCategory ViewNodeAccessibilityState::getAccessibilityCategory() const {
    if (_accessibilityCategory == AccessibilityCategoryAuto) {
        // If it's an image or a visual content with a label, we should find a src
        bool hasAttributeSrc = hasAttribute(DefaultAttributeSrc);
        if (hasAttributeSrc && hasAccessibility()) {
            return AccessibilityCategoryImage;
        }
        // If it has a placeholder, it's most likely an input
        bool hasAttributePlaceholder = hasAttribute(DefaultAttributePlaceholder);
        if (hasAttributePlaceholder) {
            // If the input is disabled, we consider it a regular label
            auto attributeEnabled = getAttribute(DefaultAttributeEnabled);
            if (!attributeEnabled.isUndefined() && !attributeEnabled.toBool()) {
                return AccessibilityCategoryText;
            }
            // Otherwise, it's a regular text input, ready to edit
            return AccessibilityCategoryInput;
        }
        // If it has a value, but not a placeholder, probably is a text label
        bool hasAttributeValue = hasAttribute(DefaultAttributeValue);
        if (hasAttributeValue) {
            return AccessibilityCategoryText;
        }
        // Otherwise it's a view
        return AccessibilityCategoryView;
    }
    return _accessibilityCategory;
}

void ViewNodeAccessibilityState::setAccessibilityNavigation(AccessibilityNavigation accessibilityNavigation) {
    _accessibilityNavigation = accessibilityNavigation;
}

AccessibilityNavigation ViewNodeAccessibilityState::getAccessibilityNavigation() const {
    if (_accessibilityNavigation == AccessibilityNavigationAuto) {
        // If the element is transparent, we ignore
        auto attributeOpacity = getAttribute(DefaultAttributeOpacity);
        if (!attributeOpacity.isUndefined() && attributeOpacity.toDouble() <= 0) {
            return AccessibilityNavigationIgnored;
        }
        // If the element has a special element category, it's most likely a leaf
        if (getAccessibilityCategory() != AccessibilityCategoryView) {
            return AccessibilityNavigationLeaf;
        }
        // If the element has some accessibility, it most likely should be announced
        if (hasAccessibility()) {
            if (hasAccessibilityId()) {
                return AccessibilityNavigationGroup;
            } else {
                return AccessibilityNavigationCover;
            }
        }
        // Otherwise, the element can be safely navigated as an inert node
        return AccessibilityNavigationPassthrough;
    }
    return _accessibilityNavigation;
}

void ViewNodeAccessibilityState::setAccessibilityPriority(float accessibilityPriority) {
    _accessibilityPriority = accessibilityPriority;
}

float ViewNodeAccessibilityState::getAccessibilityPriority() const {
    return _accessibilityPriority;
}

void ViewNodeAccessibilityState::setAccessibilityLabel(const StringBox& accessibilityLabel) {
    _accessibilityLabel = accessibilityLabel;
}

StringBox ViewNodeAccessibilityState::getAccessibilityLabel() const {
    return _accessibilityLabel;
}

void ViewNodeAccessibilityState::setAccessibilityHint(const StringBox& accessibilityHint) {
    _accessibilityHint = accessibilityHint;
}

StringBox ViewNodeAccessibilityState::getAccessibilityHint() const {
    return _accessibilityHint;
}

void ViewNodeAccessibilityState::setAccessibilityId(const StringBox& accessibilityId) {
    _accessibilityId = accessibilityId;
}

StringBox ViewNodeAccessibilityState::getAccessibilityId() const {
    return _accessibilityId;
}

void ViewNodeAccessibilityState::setAccessibilityValue(const StringBox& accessibilityValue) {
    _accessibilityValue = accessibilityValue;
}

StringBox ViewNodeAccessibilityState::getAccessibilityValue() const {
    if (_accessibilityValue.isEmpty()) {
        auto attributeValue = getAttribute(DefaultAttributeValue);
        if (attributeValue.isString()) {
            return attributeValue.toStringBox();
        } else if (attributeValue.isArray()) {
            // TextAttributeValue are passed as an array where the first entry
            // is the text content
            return TextAttributeValueParser::toString(attributeValue);
        }
        auto attributePlaceholder = getAttribute(DefaultAttributePlaceholder);
        if (attributePlaceholder.isString()) {
            return attributePlaceholder.toStringBox();
        }
    }
    return _accessibilityValue;
}

void ViewNodeAccessibilityState::setAccessibilityStateDisabled(bool accessibilityStateDisabled) {
    _accessibilityStateDisabled = accessibilityStateDisabled;
}
bool ViewNodeAccessibilityState::getAccessibilityStateDisabled() const {
    return _accessibilityStateDisabled;
}

void ViewNodeAccessibilityState::setAccessibilityStateSelected(bool accessibilityStateSelected) {
    _accessibilityStateSelected = accessibilityStateSelected;
}
bool ViewNodeAccessibilityState::getAccessibilityStateSelected() const {
    return _accessibilityStateSelected;
}

void ViewNodeAccessibilityState::setAccessibilityStateLiveRegion(bool accessibilityStateLiveRegion) {
    _accessibilityStateLiveRegion = accessibilityStateLiveRegion;
}
bool ViewNodeAccessibilityState::getAccessibilityStateLiveRegion() const {
    return _accessibilityStateLiveRegion;
}

bool ViewNodeAccessibilityState::hasAccessibilityLabel() const {
    return !_accessibilityLabel.isEmpty();
}

bool ViewNodeAccessibilityState::hasAccessibilityHint() const {
    return !_accessibilityHint.isEmpty();
}

bool ViewNodeAccessibilityState::hasAccessibilityValue() const {
    return !_accessibilityValue.isEmpty();
}

bool ViewNodeAccessibilityState::hasAccessibilityId() const {
    return !_accessibilityId.isEmpty();
}

bool ViewNodeAccessibilityState::hasAccessibility() const {
    return hasAccessibilityLabel() || hasAccessibilityHint() || hasAccessibilityValue() || hasAccessibilityId();
}

bool ViewNodeAccessibilityState::hasAttribute(AttributeId id) const {
    return _attributesApplier.hasResolvedAttributeValue(id);
}

Value ViewNodeAccessibilityState::getAttribute(AttributeId id) const {
    return _attributesApplier.getResolvedAttributeValue(id);
}

} // namespace Valdi
