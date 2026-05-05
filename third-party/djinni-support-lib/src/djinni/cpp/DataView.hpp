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

#include <cstddef>
#include <cstdint>

namespace djinni {

class DataView {
public:
    DataView(const uint8_t* p, size_t len) : _buf(const_cast<uint8_t*>(p)), _len(len) {}

    DataView(DataView&&) = default;

    DataView(const DataView&) = default;

    DataView& operator=(DataView&&) = default;
    
    DataView& operator=(const DataView&) = default;

    uint8_t* buf() const {
        return _buf;
    }
    size_t len() const {
        return _len;
    }

private:
    uint8_t* _buf;
    size_t _len;
};

} // namespace djinni
