//
//  HTTPRequestManagerUtils.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/30/19.
//

#include "valdi/runtime/Utils/HTTPRequestManagerUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include <string_view>
#include <algorithm>
#include <cctype>
#include <string>

namespace Valdi {

class HTTPRequestManagerCompletionWithFunction : public snap::valdi_core::HTTPRequestManagerCompletion {
public:
    explicit HTTPRequestManagerCompletionWithFunction(Function<void(Result<snap::valdi_core::HTTPResponse>)> function)
        : _function(std::move(function)) {}

    void onComplete(const snap::valdi_core::HTTPResponse& response) override {
        _function(response);
    }

    void onFail(const std::string& error) override {
        _function(Error(StringCache::getGlobal().makeString(error)));
    }

private:
    Function<void(Result<snap::valdi_core::HTTPResponse>)> _function;
};

std::shared_ptr<snap::valdi_core::HTTPRequestManagerCompletion> HTTPRequestManagerUtils::makeRequestCompletion(
    Function<void(Result<snap::valdi_core::HTTPResponse>)> function) { // NOLINT(performance-unnecessary-value-param)
    return Valdi::makeShared<HTTPRequestManagerCompletionWithFunction>(std::move(function));
}

bool HTTPRequestManagerUtils::isUrlAllowed(const StringBox& url) {
    if (url.isEmpty()) {
        return false;
    }
    
    std::string_view urlView = url.toStringView();
    
    // Convert to lowercase for case-insensitive comparison
    std::string urlLower;
    urlLower.reserve(urlView.size());
    std::transform(urlView.begin(), urlView.end(), std::back_inserter(urlLower),
                   [](unsigned char c) { return std::tolower(c); });
    std::string_view urlLowerView(urlLower);
    
    // Only allow http:// and https:// schemes
    if (urlLowerView.find("http://") != 0 && urlLowerView.find("https://") != 0) {
        return false;
    }
    
    // Extract host part (between :// and / or end of string)
    size_t schemeEnd = urlLowerView.find("://");
    if (schemeEnd == std::string_view::npos) {
        return false;
    }
    schemeEnd += 3; // Skip "://"
    
    size_t hostEnd = urlLowerView.find_first_of("/?#", schemeEnd);
    std::string_view hostPart = hostEnd == std::string_view::npos 
        ? urlLowerView.substr(schemeEnd)
        : urlLowerView.substr(schemeEnd, hostEnd - schemeEnd);
    
    // Remove port if present
    size_t portStart = hostPart.find(':');
    if (portStart != std::string_view::npos) {
        hostPart = hostPart.substr(0, portStart);
    }
    
    // Block localhost and loopback addresses
    if (hostPart == "localhost" ||
        hostPart == "127.0.0.1" ||
        hostPart == "::1" ||
        hostPart == "[::1]" ||
        hostPart.find("localhost") == 0) {
        return false;
    }
    
    // Block cloud metadata service hostnames
    if (hostPart == "metadata.google.internal" ||
        hostPart == "169.254.169.254") {
        return false;
    }
    
    // Block private IP ranges - check if host starts with private IP prefixes
    if (hostPart.length() >= 3 && hostPart.substr(0, 3) == "10.") {
        if (hostPart.length() > 3 && std::isdigit(hostPart[3])) {
            return false;
        }
    }
    
    // 172.16.0.0/12 (172.16.0.0 to 172.31.255.255)
    if (hostPart.length() >= 7 && hostPart.substr(0, 4) == "172.") {
        // Check if second octet is 16-31
        if (hostPart.length() > 4 && std::isdigit(hostPart[4])) {
            size_t secondDot = hostPart.find('.', 4);
            if (secondDot != std::string_view::npos && secondDot > 4) {
                std::string secondOctetStr(hostPart.substr(4, secondDot - 4));
                try {
                    int secondOctet = std::stoi(secondOctetStr);
                    if (secondOctet >= 16 && secondOctet <= 31) {
                        return false;
                    }
                } catch (...) {
                }
            }
        }
    }
    
    if (hostPart.length() >= 8 && hostPart.substr(0, 8) == "192.168.") {
        return false;
    }
    
    return true;
}

} // namespace Valdi
