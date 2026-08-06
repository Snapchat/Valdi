//
//  TextInlineAttachment.hpp
//  valdi_core
//

#pragma once

#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Views/Frame.hpp"

#include <cstddef>
#include <cstdint>

namespace Valdi {

enum class InlineViewVerticalAlignment : int32_t {
    Center = 0,
    Top = 1,
    Bottom = 2,
    Baseline = 3,
};

/**
 * Platform-independent description of a Valdi child view embedded inside a text
 * attribute run.
 *
 * The text parser stores one of these for each inline-view part. Text layout
 * engines use the child index and resolved size to reserve text space, while
 * the actual child frame is applied later by the owning label/text view.
 */
class TextInlineAttachment : public SimpleRefCountable {
public:
    explicit TextInlineAttachment(size_t childIndex);
    ~TextInlineAttachment() override;

    size_t getChildIndex() const;

    InlineViewVerticalAlignment getVerticalAlignment() const;
    void setVerticalAlignment(InlineViewVerticalAlignment verticalAlignment);

    virtual Size getSize() const = 0;

private:
    size_t _childIndex;
    InlineViewVerticalAlignment _verticalAlignment = InlineViewVerticalAlignment::Center;
};

} // namespace Valdi
