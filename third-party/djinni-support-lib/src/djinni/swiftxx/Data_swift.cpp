#include "Data_swift.hpp"
#include <cassert>

namespace djinni {

class DataRefSwift : public DataRef::Impl {
public:
    // create empty buffer from c++
    explicit DataRefSwift(size_t len) {
        allocate(len);
    }
    // create new data object and initialize with data. although this still
    // copies data, it does the allocation and initialization in one step.
    explicit DataRefSwift(const void* data, size_t len) {
        _mutableData = CFDataCreateMutable(kCFAllocatorDefault, len);
        assert(_mutableData != nullptr);
        CFDataAppendBytes(_mutableData, reinterpret_cast<const uint8_t*>(data), len);
        _data = _mutableData;
    }
    // take over a std::vector's buffer without copying it
    explicit DataRefSwift(std::vector<uint8_t>&& vec) {
        if (!vec.empty()) {
            takeOver(std::move(vec));
        } else {
            allocate(0);
        }
    }
    // take over a std::string's buffer without copying it
    explicit DataRefSwift(std::string&& str) {
        if (!str.empty()) {
            takeOver(std::move(str));
        } else {
            allocate(0);
        }
    }
    // wrap an immutable CFData object
    explicit DataRefSwift(CFDataRef data) {
        _data = data;
        _mutableData = nullptr;
        CFRetain(_data);
    }
    // wrap a mutable CFData object (CFMutableData)
    explicit DataRefSwift(CFMutableDataRef data) {
        _data = data;
        _mutableData = data;
        CFRetain(_data);
    }
    DataRefSwift(const DataRefSwift&) = delete;
    ~DataRefSwift() {
        CFRelease(_data);
    }

    const uint8_t* buf() const override {
        // must query from the CF object every time in case the buffer is
        // relocated.
        return CFDataGetBytePtr(_data);
    }
    size_t len() const override {
        // must query from the CF object every time in case the buffer is
        // resized.
        return CFDataGetLength(_data);
    }
    uint8_t* mutableBuf() override {
        return _mutableData ? CFDataGetMutableBytePtr(_mutableData) : nullptr;
    }

    PlatformObject platformObj() const {
        return _data;
    }

private:
    CFDataRef _data;
    CFMutableDataRef _mutableData;

    void allocate(size_t len) {
        _mutableData = CFDataCreateMutable(kCFAllocatorDefault, len);
        assert(_mutableData != nullptr);
        CFDataSetLength(_mutableData, len);
        _data = _mutableData;
    }

    template<typename T>
    void takeOver(T&& obj) {
        using DataObj = std::decay_t<T>;
        CFAllocatorContext context = {};
        // create a new instance and steal the input object's buffer
        auto* p = new DataObj(std::forward<T>(obj));
        auto buf = reinterpret_cast<const uint8_t*>(p->data());
        auto len = p->size();
        context.info = p;
        context.deallocate = [](void*, void* info) {
            // delete the wrapped object
            auto* p = reinterpret_cast<DataObj*>(info);
            delete p;
        };
        CFAllocatorRef deallocator = CFAllocatorCreate(kCFAllocatorDefault, &context);
        assert(deallocator != nullptr);
        // create the CFData without copying
        _data = CFDataCreateWithBytesNoCopy(nullptr, buf, len, deallocator);
        assert(_data != nullptr);
        CFRelease(deallocator); // CFDataCreateWithBytesNoCopy retains the object, so release here
        _mutableData = nullptr; // our CFData is immutable because it can't realloc and resize
    }
};

DataRef::DataRef(size_t len) {
    _impl = std::make_shared<DataRefSwift>(len);
}

DataRef::DataRef(const void* data, size_t len) {
    _impl = std::make_shared<DataRefSwift>(data, len);
}

DataRef::DataRef(std::vector<uint8_t>&& vec) {
    _impl = std::make_shared<DataRefSwift>(std::move(vec));
}
DataRef::DataRef(std::string&& str) {
    _impl = std::make_shared<DataRefSwift>(std::move(str));
}

DataRef::DataRef(CFMutableDataRef platformObj) {
    _impl = std::make_shared<DataRefSwift>(platformObj);
}

DataRef::DataRef(CFDataRef platformObj) {
    _impl = std::make_shared<DataRefSwift>(platformObj);
}

}

namespace djinni::swift {

DataView DataViewAdaptor::toCpp(const AnyValue& s) {
    auto range = std::get<RangeValue>(s);
    return DataView(reinterpret_cast<const uint8_t*>(range.bytes), range.size);
}

AnyValue DataViewAdaptor::fromCpp(DataView c) {
    return RangeValue{c.buf(), c.len()};
}

DataRef DataRefAdaptor::toCpp(const AnyValue& s) {
    auto range = std::get<RangeValue>(s);
    if (range.size > 0) {
        auto cfmutabledataref = const_cast<CFMutableDataRef>(reinterpret_cast<CFDataRef>(range.bytes));
        return DataRef(cfmutabledataref);
    } else {
        return DataRef(reinterpret_cast<CFDataRef>(range.bytes));
    }
}

AnyValue DataRefAdaptor::fromCpp(const DataRef& c) {
    auto impl = std::dynamic_pointer_cast<DataRefSwift>(c.impl());
    auto cfdata = impl->platformObj();
    CFRetain(cfdata);
    return RangeValue{reinterpret_cast<const uint8_t*>(cfdata), 0};
}

}
