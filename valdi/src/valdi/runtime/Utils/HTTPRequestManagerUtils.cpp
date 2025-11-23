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
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>

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

namespace {
    // Decode URL-encoded characters to prevent encoding bypasses
    std::string urlDecode(const std::string_view& encoded) {
        std::string decoded;
        decoded.reserve(encoded.length());
        
        for (size_t i = 0; i < encoded.length(); ++i) {
            if (encoded[i] == '%' && i + 2 < encoded.length()) {
                char hex1 = encoded[i + 1];
                char hex2 = encoded[i + 2];
                
                if (std::isxdigit(hex1) && std::isxdigit(hex2)) {
                    std::stringstream ss;
                    ss << std::hex << hex1 << hex2;
                    int value;
                    ss >> value;
                    decoded += static_cast<char>(value);
                    i += 2;
                } else {
                    decoded += encoded[i];
                }
            } else {
                decoded += encoded[i];
            }
        }
        
        return decoded;
    }
    
    // Normalize IP addresses from decimal/hex/octal formats to standard IPv4
    std::string normalizeIP(const std::string_view& ip) {
        std::string ipStr(ip);
        
        bool isPureNumber = true;
        for (char c : ipStr) {
            if (!std::isdigit(c)) {
                isPureNumber = false;
                break;
            }
        }
        
        if (isPureNumber && !ipStr.empty()) {
            try {
                unsigned long decimal = std::stoul(ipStr);
                if (decimal <= 0xFFFFFFFF) {
                    uint8_t octets[4];
                    octets[0] = (decimal >> 24) & 0xFF;
                    octets[1] = (decimal >> 16) & 0xFF;
                    octets[2] = (decimal >> 8) & 0xFF;
                    octets[3] = decimal & 0xFF;
                    
                    std::stringstream ss;
                    ss << static_cast<int>(octets[0]) << "." 
                       << static_cast<int>(octets[1]) << "."
                       << static_cast<int>(octets[2]) << "."
                       << static_cast<int>(octets[3]);
                    return ss.str();
                }
            } catch (...) {
            }
        }
        
        if (ipStr.length() > 2 && ipStr.substr(0, 2) == "0x") {
            try {
                unsigned long hex = std::stoul(ipStr.substr(2), nullptr, 16);
                if (hex <= 0xFFFFFFFF) {
                    uint8_t octets[4];
                    octets[0] = (hex >> 24) & 0xFF;
                    octets[1] = (hex >> 16) & 0xFF;
                    octets[2] = (hex >> 8) & 0xFF;
                    octets[3] = hex & 0xFF;
                    
                    std::stringstream ss;
                    ss << static_cast<int>(octets[0]) << "." 
                       << static_cast<int>(octets[1]) << "."
                       << static_cast<int>(octets[2]) << "."
                       << static_cast<int>(octets[3]);
                    return ss.str();
                }
            } catch (...) {
            }
        }
        
        size_t dotCount = 0;
        for (char c : ipStr) {
            if (c == '.') dotCount++;
        }
        
        if (dotCount == 3) {
            std::string normalized;
            size_t start = 0;
            bool allValid = true;
            
            for (int i = 0; i < 4; i++) {
                size_t dotPos = (i < 3) ? ipStr.find('.', start) : std::string::npos;
                size_t end = (dotPos != std::string::npos) ? dotPos : ipStr.length();
                
                if (end <= start) {
                    allValid = false;
                    break;
                }
                
                std::string octetStr(ipStr.substr(start, end - start));
                int octet = -1;
                
                if (octetStr.length() > 1 && octetStr[0] == '0' && std::isdigit(octetStr[1])) {
                    try {
                        octet = std::stoi(octetStr, nullptr, 8);
                    } catch (...) {}
                } else if (octetStr.length() > 2 && octetStr.substr(0, 2) == "0x") {
                    try {
                        octet = std::stoi(octetStr.substr(2), nullptr, 16);
                    } catch (...) {}
                } else {
                    try {
                        octet = std::stoi(octetStr);
                    } catch (...) {}
                }
                
                if (octet < 0 || octet > 255) {
                    allValid = false;
                    break;
                }
                
                if (i > 0) normalized += ".";
                normalized += std::to_string(octet);
                
                start = end + 1;
            }
            
            if (allValid) {
                return normalized;
            }
        }
        
        return ipStr;
    }
    
    // Extract host from URL, handling userinfo and edge cases
    std::string extractHost(const std::string_view& url, size_t schemeEnd) {
        size_t pathStart = url.find_first_of("/?#", schemeEnd);
        std::string_view authority = (pathStart != std::string::npos) 
            ? url.substr(schemeEnd, pathStart - schemeEnd)
            : url.substr(schemeEnd);
        
        size_t atPos = authority.find('@');
        std::string_view hostPart = (atPos != std::string::npos) 
            ? authority.substr(atPos + 1)
            : authority;
        
        std::string host;
        if (!hostPart.empty() && hostPart[0] == '[') {
            size_t bracketEnd = hostPart.find(']');
            if (bracketEnd != std::string::npos) {
                host = std::string(hostPart.substr(0, bracketEnd + 1));
            } else {
                host = std::string(hostPart);
            }
        } else {
            size_t portStart = hostPart.find(':');
            host = (portStart != std::string::npos) 
                ? std::string(hostPart.substr(0, portStart))
                : std::string(hostPart);
        }
        
        return host;
    }
    
    // Helper function to parse IPv4 address and return octets
    // Returns true if valid IPv4, false otherwise
    bool parseIPv4(const std::string_view& host, uint8_t octets[4]) {
        if (host.empty() || host.length() > 15) {
            return false;
        }
        
        size_t start = 0;
        for (int i = 0; i < 4; i++) {
            size_t dotPos = (i < 3) ? host.find('.', start) : std::string_view::npos;
            size_t end = (dotPos != std::string_view::npos) ? dotPos : host.length();
            
            if (end == start || end - start > 3) {
                return false;
            }
            
            std::string octetStr(host.substr(start, end - start));
            try {
                int octet = std::stoi(octetStr);
                if (octet < 0 || octet > 255) {
                    return false;
                }
                octets[i] = static_cast<uint8_t>(octet);
            } catch (...) {
                return false;
            }
            
            start = end + 1;
        }
        
        return true;
    }
    
    // Check if IPv4 address is in a blocked range
    bool isIPv4Blocked(const std::string_view& host) {
        uint8_t octets[4];
        if (!parseIPv4(host, octets)) {
            return false; // Not a valid IPv4, will be checked as hostname
        }
        
        // Block 0.0.0.0/8 (0.0.0.0 to 0.255.255.255)
        if (octets[0] == 0) {
            return true;
        }
        
        // Block 127.0.0.0/8 (127.0.0.0 to 127.255.255.255)
        if (octets[0] == 127) {
            return true;
        }
        
        // Block 10.0.0.0/8 (10.0.0.0 to 10.255.255.255)
        if (octets[0] == 10) {
            return true;
        }
        
        // Block 172.16.0.0/12 (172.16.0.0 to 172.31.255.255)
        if (octets[0] == 172 && octets[1] >= 16 && octets[1] <= 31) {
            return true;
        }
        
        // Block 192.168.0.0/16 (192.168.0.0 to 192.168.255.255)
        if (octets[0] == 192 && octets[1] == 168) {
            return true;
        }
        
        // Block 169.254.0.0/16 (169.254.0.0 to 169.254.255.255)
        if (octets[0] == 169 && octets[1] == 254) {
            return true;
        }
        
        return false;
    }
    
    // Only allow IPv6 addresses in 2000::/3 range
    bool isIPv6Allowed(const std::string_view& host) {
        std::string_view ipv6 = host;
        if (ipv6.length() >= 2 && ipv6[0] == '[' && ipv6[ipv6.length() - 1] == ']') {
            ipv6 = ipv6.substr(1, ipv6.length() - 2);
        }
        
        bool hasColon = false;
        for (char c : ipv6) {
            if (c == ':') {
                hasColon = true;
            } else if (!std::isxdigit(c) && c != '.') {
                return false;
            }
        }
        
        if (!hasColon) {
            return false;
        }
        
        size_t ipv4MappedPos = ipv6.find("::ffff:");
        if (ipv4MappedPos != std::string_view::npos) {
            size_t ipv4Start = ipv4MappedPos + 7;
            if (ipv4Start < ipv6.length()) {
                std::string_view ipv4Part = ipv6.substr(ipv4Start);
                size_t ipv4End = ipv4Part.find_first_of(":/");
                if (ipv4End != std::string_view::npos) {
                    ipv4Part = ipv4Part.substr(0, ipv4End);
                }
                if (isIPv4Blocked(ipv4Part)) {
                    return false;
                }
            }
        }
        
        size_t firstHexStart = 0;
        
        if (ipv6.length() >= 2 && ipv6[0] == ':' && ipv6[1] == ':') {
            firstHexStart = 2;
            while (firstHexStart < ipv6.length() && ipv6[firstHexStart] == ':') {
                firstHexStart++;
            }
            
            if (firstHexStart >= ipv6.length()) {
                return false;
            }
        } else {
            while (firstHexStart < ipv6.length() && (ipv6[firstHexStart] == ':' || !std::isxdigit(ipv6[firstHexStart]))) {
                firstHexStart++;
            }
        }
        
        if (firstHexStart >= ipv6.length()) {
            return false;
        }
        
        size_t firstGroupEnd = firstHexStart;
        while (firstGroupEnd < ipv6.length() && std::isxdigit(ipv6[firstGroupEnd])) {
            firstGroupEnd++;
        }
        
        if (firstGroupEnd == firstHexStart) {
            return false;
        }
        
        char firstHex = std::tolower(ipv6[firstHexStart]);
        if (firstHex != '2' && firstHex != '3') {
            return false;
        }
        
        return true;
    }
}

bool HTTPRequestManagerUtils::isUrlAllowed(const StringBox& url) {
    if (url.isEmpty()) {
        return false;
    }
    
    std::string_view urlView = url.toStringView();
    
    std::string decodedUrl = urlDecode(urlView);
    
    std::string urlLower;
    urlLower.reserve(decodedUrl.size());
    std::transform(decodedUrl.begin(), decodedUrl.end(), std::back_inserter(urlLower),
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
    
    std::string hostStr = extractHost(urlLowerView, schemeEnd);
    
    std::string normalizedHost = normalizeIP(hostStr);
    
    std::string_view hostWithoutPort = normalizedHost;
    
    std::string hostLower = normalizedHost;
    std::transform(hostLower.begin(), hostLower.end(), hostLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    if (hostLower == "localhost" || hostLower.find("localhost") == 0) {
        return false;
    }
    
    std::string originalHostLower = hostStr;
    std::transform(originalHostLower.begin(), originalHostLower.end(), originalHostLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (originalHostLower == "localhost" || originalHostLower.find("localhost") == 0) {
        return false;
    }
    
    if (hostLower == "metadata.google.internal" || originalHostLower == "metadata.google.internal") {
        return false;
    }
    
    if (isIPv4Blocked(hostWithoutPort)) {
        return false;
    }
    
    if (normalizedHost != hostStr && isIPv4Blocked(hostStr)) {
        return false;
    }
    
    if (hostWithoutPort.find(':') != std::string_view::npos || 
        (!hostWithoutPort.empty() && hostWithoutPort[0] == '[')) {
        if (!isIPv6Allowed(hostWithoutPort)) {
            return false;
        }
    }
    
    return true;
}

} // namespace Valdi
