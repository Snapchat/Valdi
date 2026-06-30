//
//  TextLayout.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 7/14/20.
//

#include "snap_drawing/cpp/Text/TextLayout.hpp"
#include "snap_drawing/cpp/Utils/JSONUtils.hpp"

#include "valdi_core/cpp/Utils/ValueArray.hpp"

namespace snap::drawing {

TextLayoutEntrySegment::TextLayoutEntrySegment() = default;
TextLayoutEntrySegment::TextLayoutEntrySegment(const Rect& bounds, const Ref<Font>& font, const std::string& characters)
    : bounds(bounds), font(font), characters(characters) {}

TextLayoutEntry::TextLayoutEntry() = default;
TextLayoutEntry::TextLayoutEntry(const Rect& bounds,
                                 std::optional<Color> color,
                                 std::vector<TextLayoutEntrySegment> segments)
    : bounds(bounds), color(color), segments(std::move(segments)) {}
TextLayoutEntry::~TextLayoutEntry() = default;

TextLayout::TextLayout(Size maxSize,
                       std::vector<TextLayoutEntry>&& entries,
                       std::vector<TextLayoutVisualEntry>&& visualEntries,
                       std::vector<TextLayoutAttachment>&& attachments,
                       bool fitsInMaxSize)
    : _maxSize(maxSize),
      _fitsInMaxSize(fitsInMaxSize),
      _entries(std::move(entries)),
      _visualEntries(std::move(visualEntries)),
      _attachments(std::move(attachments)) {
    _bounds = Rect::makeEmpty();

    for (const auto& entry : _entries) {
        _bounds.join(entry.bounds);
    }
}

TextLayout::~TextLayout() = default;

const std::vector<TextLayoutEntry>& TextLayout::getEntries() const {
    return _entries;
}

const std::vector<TextLayoutVisualEntry>& TextLayout::getVisualEntries() const {
    return _visualEntries;
}

const std::vector<TextLayoutAttachment>& TextLayout::getAttachments() const {
    return _attachments;
}

const Rect& TextLayout::getBounds() const {
    return _bounds;
}

const Size& TextLayout::getMaxSize() const {
    return _maxSize;
}

bool TextLayout::fitsInMaxSize() const {
    return _fitsInMaxSize;
}

Valdi::Value TextLayout::toJSONValue() const {
    Valdi::Value out;
    out.setMapValue("maxSize", snap::drawing::toJSONValue(_maxSize));

    auto entries = Valdi::ValueArray::make(_entries.size());

    for (size_t i = 0; i < _entries.size(); i++) {
        const auto& entry = _entries[i];

        Valdi::Value entryJson;
        entryJson.setMapValue("bounds", snap::drawing::toJSONValue(entry.bounds));

        if (!entry.segments.empty()) {
            auto segments = Valdi::ValueArray::make(entry.segments.size());
            for (size_t j = 0; j < entry.segments.size(); j++) {
                const auto& segment = entry.segments[j];
                Valdi::Value segmentJson;

                segmentJson.setMapValue("bounds", snap::drawing::toJSONValue(segment.bounds));
                segmentJson.setMapValue("font", Valdi::Value(segment.font->getDescription()));
                segmentJson.setMapValue("characters", Valdi::Value(segment.characters));

                segments->emplace(j, std::move(segmentJson));
            }

            entryJson.setMapValue("segments", Valdi::Value(segments));
        }

        if (entry.color) {
            entryJson.setMapValue("color", Valdi::Value(entry.color.value().toString()));
        }

        entries->emplace(i, std::move(entryJson));
    }

    out.setMapValue("entries", Valdi::Value(entries));

    auto visualEntries = Valdi::ValueArray::make(_visualEntries.size());
    for (size_t i = 0; i < _visualEntries.size(); i++) {
        const auto& visualEntry = _visualEntries[i];

        Valdi::Value visualEntryJson;
        visualEntryJson.setMapValue("bounds", snap::drawing::toJSONValue(visualEntry.bounds));
        visualEntryJson.setMapValue("predraw", Valdi::Value(visualEntry.predraw));
        if (visualEntry.kind == TextLayoutVisualEntryKindBackground) {
            visualEntryJson.setMapValue("kind", Valdi::Value("background"));
        }
        if (!visualEntry.borderRadius.isEmpty()) {
            visualEntryJson.setMapValue("borderRadius", Valdi::Value(visualEntry.borderRadius.toString()));
        }
        if (visualEntry.style == TextLayoutDecorationStyleDashed) {
            visualEntryJson.setMapValue("style", Valdi::Value("dashed"));
        } else if (visualEntry.style == TextLayoutDecorationStyleDotted) {
            visualEntryJson.setMapValue("style", Valdi::Value("dotted"));
        }

        if (visualEntry.color) {
            visualEntryJson.setMapValue("color", Valdi::Value(visualEntry.color.value().toString()));
        }

        visualEntries->emplace(i, std::move(visualEntryJson));
    }

    out.setMapValue("visualEntries", Valdi::Value(visualEntries));

    return out;
}

Ref<Valdi::RefCountable> TextLayout::getAttachmentAtPoint(Point location, Scalar tolerance) const {
    Ref<Valdi::RefCountable> bestCandidate;
    Scalar shortestDistance = 0.0f;
    for (const auto& attachment : _attachments) {
        if (attachment.bounds.contains(location)) {
            return attachment.attachment;
        }

        auto closestPoint = attachment.bounds.closestPoint(location);
        auto distance = Point::distance(location, closestPoint);

        if (distance <= tolerance && (bestCandidate == nullptr || distance < shortestDistance)) {
            shortestDistance = distance;
            bestCandidate = attachment.attachment;
        }
    }

    return bestCandidate;
}

} // namespace snap::drawing
