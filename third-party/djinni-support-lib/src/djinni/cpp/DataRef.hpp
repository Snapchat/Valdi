/**
  * Copyright 2021 Snap, Inc.
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

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <cstring>

#if !defined(DATAREF_JNI) && !defined(DATAREF_OBJC) && !defined(DATAREF_WASM)
  #if defined(__ANDROID__)
    #define DATAREF_JNI 1
  #elif defined(__APPLE__)
    #define DATAREF_OBJC 1
  #elif defined(__EMSCRIPTEN__)
    #define DATAREF_WASM 1
  #endif
#endif

#if DATAREF_JNI
  using PlatformObject = void*;
#elif DATAREF_OBJC
  #include <CoreFoundation/CFData.h>
  using PlatformObject = CFDataRef;
#elif DATAREF_WASM
  #include <emscripten/val.h>
  using PlatformObject = emscripten::val;
#else
  using PlatformObject = const void*;
#endif

namespace djinni {

class DataRef {
public:
    class Impl {
    public:
        virtual ~Impl() = default;
        
        virtual const uint8_t* buf() const = 0;
        virtual size_t len() const = 0;
        virtual uint8_t* mutableBuf() = 0;
    };

    DataRef() = default;
    DataRef(const DataRef&) = default;
    DataRef(DataRef&&) = default;
    
    explicit DataRef(const std::shared_ptr<Impl>& impl): _impl(impl) {}

    // initialize with empty buffer
    explicit DataRef(size_t len);
    // initialize with data
    DataRef(const void* data, size_t len);
    // initialize with copying vector
    explicit DataRef(const std::vector<uint8_t>& vec) : DataRef(vec.data(), vec.size()) {}
    // initialize with vector and try not to copy it
    explicit DataRef(std::vector<uint8_t>&& vec);
    // initialize with copying string
    explicit DataRef(const std::string& str) : DataRef(str.data(), str.size()) {}
    // initialize with string and try not to copy it
    explicit DataRef(std::string&& str);

#if DATAREF_JNI
    explicit DataRef(void* platformObj);
#elif DATAREF_OBJC
    explicit DataRef(CFDataRef platformObj);
    explicit DataRef(CFMutableDataRef platformObj);
#elif DATAREF_WASM
    explicit DataRef(PlatformObject platformObj);
#endif

    DataRef& operator=(const DataRef&) = default;
    DataRef& operator=(DataRef&&) = default;
    
    const uint8_t* buf() const {
        return _impl ? _impl->buf() : nullptr;
    }
    size_t len() const {
        return _impl ? _impl->len() : 0;
    }
    uint8_t* mutableBuf() const {
        return _impl ? _impl->mutableBuf() : nullptr;
    }

    std::shared_ptr<Impl> impl() const { return _impl; } 

private:
    std::shared_ptr<Impl> _impl;
};

} // namespace djinni
