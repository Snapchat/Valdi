//
//  Bytes.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 3/7/19.
//

#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "utils/debugging/Assert.hpp"

namespace Valdi {

Bytes::~Bytes() = default;

void Bytes::assignVec(std::vector<Byte>&& vec) {
    std::vector<Byte>* asVec = this;
    *asVec = std::move(vec);
}

void Bytes::assignData(const Byte* data, size_t size) {
    assign(data, data + size);
}

BytesView::BytesView(Ref<RefCountable> source, const Byte* data, size_t size)
    : _source(std::move(source)), _data(data), _size(size) {}

BytesView::BytesView(const Ref<Bytes>& bytes)
    : BytesView(Ref<RefCountable>(bytes.get()),
                bytes != nullptr ? bytes->data() : nullptr,
                bytes != nullptr ? bytes->size() : 0) {}

BytesView::BytesView() = default;

const Byte* BytesView::begin() const {
    return _data;
}

const Byte* BytesView::end() const {
    return _data + _size;
}

const Byte* BytesView::data() const {
    return _data;
}

size_t BytesView::size() const {
    return _size;
}

bool BytesView::empty() const {
    return _size == 0;
}

size_t BytesView::hash() const {
    return std::hash<std::string_view>()(asStringView());
}

BytesView BytesView::subrange(size_t start, size_t length) const {
    auto* newBegin = _data + start;
    auto* newEnd = newBegin + length;

    SC_ASSERT(newEnd <= end());

    return BytesView(_source, newBegin, length);
}

const Ref<RefCountable>& BytesView::getSource() const {
    return _source;
}

std::string_view BytesView::asStringView() const {
    return std::string_view(reinterpret_cast<const char*>(_data), _size);
}

bool BytesView::isStrictlyIdenticalTo(const BytesView& other) const {
    return _source.get() == other._source.get() && _data == other._data && _size == other._size;
}

bool BytesView::operator==(const BytesView& other) const {
    if (_size != other.size()) {
        return false;
    }

    return asStringView() == other.asStringView();
}

bool BytesView::operator!=(const BytesView& other) const {
    return !(*this == other);
}

} // namespace Valdi
