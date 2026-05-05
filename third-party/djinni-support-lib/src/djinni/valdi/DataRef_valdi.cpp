/**
  * Copyright 2024 Snap, Inc.
  *
  * Licensed under the Apache License, Version 2.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  *    http://www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */

#include "../cpp/DataRef.hpp"
#include "DataRef_valdi.hpp"
#include <variant>

namespace djinni::valdi {

struct ValdiDataObject: Valdi::ValdiObject {
    VALDI_CLASS_HEADER(ValdiDataObject);
    std::variant<std::vector<uint8_t>, std::string, std::shared_ptr<DataRef::Impl>> _data;
};
VALDI_CLASS_IMPL(ValdiDataObject);

class DataRefValdi: public DataRef::Impl {
public:
    // create an empty buffer from c++
    explicit DataRefValdi(size_t len) {
        auto bytes = Valdi::makeShared<Valdi::Bytes>();
        bytes->assignVec(std::vector<uint8_t>(len));
        _array = Valdi::makeShared<Valdi::ValueTypedArray>(Valdi::kDefaultTypedArrayType, bytes);
    }
    // wrap an array object from JS
    explicit DataRefValdi(const Valdi::Ref<Valdi::ValueTypedArray>& array) {
        _array = array;
    }
    // take over a std::vector's buffer without copying it
    explicit DataRefValdi(std::vector<uint8_t>&& vec) {
        auto container = Valdi::makeShared<ValdiDataObject>();
        container->_data = std::move(vec);
        const auto& containedVec = std::get<std::vector<uint8_t>>(container->_data);
        auto bytes = containedVec.data();
        auto len = containedVec.size();
        _array = Valdi::makeShared<Valdi::ValueTypedArray>(Valdi::kDefaultTypedArrayType,
                                                       Valdi::BytesView(container, bytes, len));
    }
    // take over a std::string's buffer without copying it
    explicit DataRefValdi(std::string&& str) {
        auto container = Valdi::makeShared<ValdiDataObject>();
        container->_data = std::move(str);
        const std::string& containedStr = std::get<std::string>(container->_data);
        auto bytes = reinterpret_cast<const uint8_t*>(containedStr.data());
        auto len = containedStr.size();
        _array = Valdi::makeShared<Valdi::ValueTypedArray>(Valdi::kDefaultTypedArrayType,
                                                                 Valdi::BytesView(container, bytes, len));
    }

    DataRefValdi(const DataRefValdi&) = delete;

    const uint8_t* buf() const override {
        return _array->getBuffer().data();
    }
    size_t len() const override {
        return _array->getBuffer().size();
    }
    uint8_t* mutableBuf() override {
        return const_cast<uint8_t*>(_array->getBuffer().data());
    }

    Valdi::Ref<Valdi::ValueTypedArray> platformObj() const {
        return _array;
    }
    
private:
    Valdi::Ref<Valdi::ValueTypedArray> _array;
};

DataRef NativeDataRef::toCpp(const Valdi::Value& v) {
    auto arr = v.getTypedArrayRef();
    auto impl = std::make_shared<DataRefValdi>(arr);
    return DataRef(impl);
}

Valdi::Value NativeDataRef::fromCpp(const DataRef& c) {
    auto impl = std::dynamic_pointer_cast<DataRefValdi>(c.impl());
    if (impl) {
        auto arr = impl->platformObj();
        return Valdi::Value(arr);
    } else {
        auto container = Valdi::makeShared<ValdiDataObject>();
        auto bytes = c.buf();
        auto len = c.len();
        container->_data = c.impl();
        auto arr = Valdi::makeShared<Valdi::ValueTypedArray>(Valdi::kDefaultTypedArrayType,
                                                                   Valdi::BytesView(container, bytes, len));
        return Valdi::Value(arr);
    }
}

const Valdi::ValueSchema& NativeDataRef::schema() {
    static auto schema = Valdi::ValueSchema::valueTypedArray();
    return schema;
}


} // namespace djinni
