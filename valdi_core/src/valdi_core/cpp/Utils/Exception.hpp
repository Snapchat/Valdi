//
//  Exception.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/25/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi_core/cpp/Utils/Error.hpp"

namespace Valdi {

class Exception : public std::exception {
public:
    explicit Exception(std::string_view message);
    explicit Exception(StringBox message);
    explicit Exception(const char* message);
    explicit Exception(Error error);

    const char* what() const noexcept override;

    const StringBox& getMessage() const;

    const Error& getError() const;

private:
    Error _error;
};

} // namespace Valdi
