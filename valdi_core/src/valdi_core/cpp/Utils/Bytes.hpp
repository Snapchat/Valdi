//
//  Bytes.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 3/7/19.
//

#pragma once

#include "valdi_core/cpp/Utils/Byte.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include <memory>
#include <string>
#include <vector>

namespace Valdi {

struct Bytes : public SimpleRefCountable, public std::vector<Byte> {
    ~Bytes() override;

    void assignVec(std::vector<Byte>&& vec);
    void assignData(const Byte* data, size_t size);
};

/**
 * A copyable, memory safe View over an arbitrary Bytes source.
 **/
class BytesView {
public:
    BytesView(Ref<RefCountable> source, const Byte* data, size_t size);
    explicit BytesView(const Ref<Bytes>& bytes);
    BytesView();

    const Byte* begin() const;
    const Byte* end() const;

    const Byte* data() const;

    size_t size() const;
    size_t hash() const;

    bool empty() const;

    BytesView subrange(size_t start, size_t length) const;

    /**
    Check whether the data and source of this BytesView points to the same data and source as the given BytesView,
    and the size is the same.
     */
    bool isStrictlyIdenticalTo(const BytesView& other) const;

    const Ref<RefCountable>& getSource() const;

    // Compares whether the data that this BytesView holds is equals to the data the
    // given BytesView holds. This doesn't compare the source.
    bool operator==(const BytesView& other) const;
    bool operator!=(const BytesView& other) const;

    /**
     Get an std::string_view representation of this BytesView.
     The returned instance is unsafe as it does not retain the source.
     */
    std::string_view asStringView() const;

private:
    // Use to retain the data
    Ref<RefCountable> _source;
    const Byte* _data = nullptr;
    size_t _size = 0;
};

} // namespace Valdi
