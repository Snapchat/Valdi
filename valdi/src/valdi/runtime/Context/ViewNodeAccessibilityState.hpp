#pragma once

#include "valdi/runtime/Attributes/AttributeIds.hpp"
#include "valdi/runtime/Attributes/ViewNodeAttributesApplier.hpp"
#include "valdi/runtime/Context/ViewNodeAccessibility.hpp"
#include "valdi_core/cpp/Views/Frame.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

#include <optional>

namespace Valdi {

class ViewNodeAccessibilityState {
public:
    explicit ViewNodeAccessibilityState(const ViewNodeAttributesApplier& attributesApplier);
    ~ViewNodeAccessibilityState();

    void setAccessibilityCategory(AccessibilityCategory accessibilityCategory);
    AccessibilityCategory getAccessibilityCategory() const;

    void setAccessibilityNavigation(AccessibilityNavigation accessibilityNavigation);
    AccessibilityNavigation getAccessibilityNavigation() const;

    void setAccessibilityPriority(float accessibilityPriority);
    float getAccessibilityPriority() const;

    void setAccessibilityLabel(const StringBox& accessibilityLabel);
    StringBox getAccessibilityLabel() const;

    void setAccessibilityHint(const StringBox& accessibilityHint);
    StringBox getAccessibilityHint() const;

    void setAccessibilityValue(const StringBox& accessibilityValue);
    StringBox getAccessibilityValue() const;

    void setAccessibilityStateDisabled(bool accessibilityStateDisabled);
    bool getAccessibilityStateDisabled() const;

    void setAccessibilityStateSelected(bool accessibilityStateSelected);
    bool getAccessibilityStateSelected() const;

    void setAccessibilityStateLiveRegion(bool accessibilityStateLiveRegion);
    bool getAccessibilityStateLiveRegion() const;

    void setAccessibilityId(const StringBox& accessibilityId);
    StringBox getAccessibilityId() const;

    bool hasAccessibilityId() const;

private:
    const ViewNodeAttributesApplier& _attributesApplier;

    AccessibilityCategory _accessibilityCategory;
    AccessibilityNavigation _accessibilityNavigation;
    float _accessibilityPriority;

    StringBox _accessibilityLabel;
    StringBox _accessibilityHint;
    StringBox _accessibilityValue;
    StringBox _accessibilityId;

    bool _accessibilityStateDisabled;
    bool _accessibilityStateSelected;
    bool _accessibilityStateLiveRegion;

    bool hasAccessibilityLabel() const;
    bool hasAccessibilityHint() const;
    bool hasAccessibilityValue() const;
    bool hasAccessibility() const;

    bool hasAttribute(AttributeId id) const;
    Value getAttribute(AttributeId id) const;
};

} // namespace Valdi
