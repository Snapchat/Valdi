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
    
    std::string urlLower;
    urlLower.reserve(urlView.size());
    std::transform(urlView.begin(), urlView.end(), std::back_inserter(urlLower),
                   [](unsigned char c) { return std::tolower(c); });
    std::string_view urlLowerView(urlLower);
    
    if (urlLowerView.find("http://") != 0 && urlLowerView.find("https://") != 0) {
        return false;
    }
    
    size_t schemeEnd = urlLowerView.find("://");
    if (schemeEnd == std::string_view::npos) {
        return false;
    }
    schemeEnd += 3;
    
    size_t hostEnd = urlLowerView.find_first_of("/?#", schemeEnd);
    std::string_view hostPart = hostEnd == std::string_view::npos 
        ? urlLowerView.substr(schemeEnd)
        : urlLowerView.substr(schemeEnd, hostEnd - schemeEnd);
    
    size_t portStart = hostPart.find(':');
    if (portStart != std::string_view::npos) {
        hostPart = hostPart.substr(0, portStart);
    }
    
    if (hostPart == "localhost" ||
        hostPart == "127.0.0.1" ||
        hostPart == "::1" ||
        hostPart == "[::1]" ||
        hostPart.find("localhost") == 0) {
        return false;
    }
    
    if (hostPart == "metadata.google.internal" ||
        hostPart == "169.254.169.254") {
        return false;
    }
    
    if (hostPart.length() >= 3 && hostPart.substr(0, 3) == "10.") {
        if (hostPart.length() > 3 && std::isdigit(hostPart[3])) {
            return false;
        }
    }
    
    if (hostPart.length() >= 7 && hostPart.substr(0, 4) == "172.") {
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
