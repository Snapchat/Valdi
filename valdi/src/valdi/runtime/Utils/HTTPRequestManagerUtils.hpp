//
//  HTTPRequestManagerUtils.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/30/19.
//

#pragma once

#include "valdi_core/HTTPRequestManagerCompletion.hpp"
#include "valdi_core/HTTPResponse.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

namespace Valdi {

class HTTPRequestManagerUtils {
public:
    static std::shared_ptr<snap::valdi_core::HTTPRequestManagerCompletion> makeRequestCompletion(
        Function<void(Result<snap::valdi_core::HTTPResponse>)> function);
    
    /**
     * Validates that a URL is safe to request, blocking SSRF attack vectors.
     * Returns false if the URL should be blocked (localhost, private IPs, cloud metadata, etc.)
     */
    static bool isUrlAllowed(const StringBox& url);
};

} // namespace Valdi
