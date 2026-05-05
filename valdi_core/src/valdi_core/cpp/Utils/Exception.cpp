//
//  Exception.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/25/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi_core/cpp/Utils/Exception.hpp"

namespace Valdi {

Exception::Exception(std::string_view message) : _error(message) {}

Exception::Exception(StringBox message) : _error(std::move(message)) {}

Exception::Exception(const char* message) : _error(message) {}

Exception::Exception(Error error) : _error(std::move(error)) {}

const char* Exception::what() const noexcept {
    return _error.getMessage().getCStr();
}

const StringBox& Exception::getMessage() const {
    return _error.getMessage();
}

const Error& Exception::getError() const {
    return _error;
}

} // namespace Valdi
