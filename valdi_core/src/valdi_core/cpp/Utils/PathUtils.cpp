//
//  PathUtils.cpp
//  ValdiRuntime
//
//  Created by Brandon Francis on 8/1/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi_core/cpp/Utils/PathUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include <boost/functional/hash.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Valdi {

Path::Path() = default;

Path::Path(const std::string_view& path) {
    append(path);
}

Path::Path(const StringBox& path) {
    append(path.toStringView());
}

bool Path::isAbsolute() const {
    if (_components.empty()) {
        return false;
    }
    return _components[0] == "/";
}

Path& Path::normalize() {
    auto it = _components.begin();
    while (it != _components.end()) {
        auto& pathComponent = *it;

        // Ignore all single dots
        if (pathComponent == ".") {
            it = _components.erase(it);
            continue;
        }

        if (pathComponent == "..") {
            if (it != _components.begin()) {
                // Process last path component
                --it;

                if (*it == "..") {
                    // If our last had .., we are already outside our root, we keep appending ..
                    ++it;
                    ++it;
                } else if (*it == "/") {
                    // We reached our root, ignore the ..
                    ++it;
                    it = _components.erase(it);
                } else {
                    // We had something, we can pop the directory, effectively moving up the dir tree
                    it = _components.erase(it);
                    it = _components.erase(it);
                }
            } else {
                // Keep the .. we went outside our root
                ++it;
            }
            continue;
        }

        ++it;
    }
    return *this;
}

Path& Path::append(const std::string_view& component) {
    size_t length = component.length();
    size_t componentStart = 0;
    bool componentStarted = false;

    for (size_t i = 0; i < length; i++) {
        auto c = component[i];

        if (c == '/') {
            // Keep leading / as root directory
            if (!componentStarted && _components.empty()) {
                _components.emplace_back("/");
            } else if (componentStarted) {
                componentStarted = false;
                _components.emplace_back(component.data() + componentStart, i - componentStart);
            }
        } else {
            if (!componentStarted) {
                componentStarted = true;
                componentStart = i;
            }
        }
    }

    if (componentStarted) {
        _components.emplace_back(component.data() + componentStart, length - componentStart);
    }
    return *this;
}

Path& Path::append(const Path& other) {
    _components.reserve(_components.size() + other._components.size());
    _components.insert(_components.end(), other._components.begin(), other._components.end());
    return *this;
}

Path Path::appending(const Path& other) const {
    auto copy = *this;
    copy.append(other);
    return copy;
}

Path Path::appending(const std::string_view& component) const {
    auto copy = *this;
    copy.append(component);
    return copy;
}

Path Path::relativeTo(const Path& basePath) const {
    auto target = *this;
    target.normalize();
    auto base = basePath;
    base.normalize();

    size_t commonComponentCount = 0;
    while (commonComponentCount < target._components.size() && commonComponentCount < base._components.size() &&
           target._components[commonComponentCount] == base._components[commonComponentCount]) {
        commonComponentCount++;
    }

    Path result;
    for (size_t i = commonComponentCount; i < base._components.size(); i++) {
        result.append("..");
    }
    for (size_t i = commonComponentCount; i < target._components.size(); i++) {
        result.append(target._components[i]);
    }
    return result;
}

std::string Path::toString() const {
    std::string out;
    auto hadComponent = false;

    for (const auto& pathComponent : _components) {
        if (hadComponent) {
            out += '/';
        }
        out.append(pathComponent);

        if (!hadComponent && pathComponent != "/") {
            hadComponent = true;
        }
    }

    return out;
}

StringBox Path::toStringBox() const {
    return StringCache::getGlobal().makeString(toString());
}

std::string Path::getFirstComponent() const {
    if (_components.empty()) {
        return std::string();
    }
    return _components[0];
}

std::string_view Path::getLastComponent() const {
    if (_components.empty()) {
        return std::string_view();
    }
    return _components[_components.size() - 1];
}

Path& Path::removeLastComponent() {
    if (!_components.empty()) {
        _components.pop_back();
    }

    return *this;
}

Path Path::removingLastComponent() const {
    auto copy = *this;
    copy.removeLastComponent();
    return copy;
}

Path& Path::removeFirstComponent() {
    if (!_components.empty()) {
        _components.erase(_components.begin());
    }
    return *this;
}

Path Path::removingFirstComponent() const {
    auto copy = *this;
    copy.removeFirstComponent();
    return copy;
}

std::string_view Path::removeFileExtensionFromComponent(const std::string_view& pathComponent) {
    auto fileExtensionIndex = pathComponent.find_last_of('.');
    if (fileExtensionIndex == std::string_view::npos) {
        return pathComponent;
    }

    return pathComponent.substr(0, fileExtensionIndex);
}

Path& Path::removeFileExtension() {
    if (!empty()) {
        auto lastIndex = _components.size() - 1;
        _components[lastIndex] = Path::removeFileExtensionFromComponent(_components[lastIndex]);
    }

    return *this;
}

Path& Path::appendFileExtension(const std::string_view& newExtension) {
    if (_components.empty()) {
        return *this;
    }
    auto lastIndex = _components.size() - 1;
    auto& lastComponent = _components[lastIndex];
    lastComponent += '.';
    lastComponent += newExtension;

    return *this;
}

std::string_view Path::getFileExtension() const {
    auto lastComponent = getLastComponent();
    auto fileExtensionIndex = lastComponent.find_last_of('.');
    if (fileExtensionIndex == std::string_view::npos) {
        return std::string_view();
    }

    return lastComponent.substr(fileExtensionIndex + 1);
}

bool Path::empty() const {
    return _components.empty();
}

bool Path::startsWith(const Path& otherPath) const {
    auto otherSize = otherPath._components.size();
    if (otherSize > _components.size()) {
        return false;
    }

    for (size_t i = 0; i < otherSize; i++) {
        if (_components[i] != otherPath._components[i]) {
            return false;
        }
    }

    return true;
}

const std::vector<std::string>& Path::getComponents() const {
    return _components;
}

bool Path::operator==(const Path& other) const {
    return _components == other._components;
}

bool Path::operator!=(const Path& other) const {
    return !(*this == other);
}

std::ostream& operator<<(std::ostream& os, const Path& path) {
    return os << path.toString();
}

} // namespace Valdi

namespace std {

std::size_t hash<Valdi::Path>::operator()(const Valdi::Path& path) const noexcept {
    size_t hash = 0;

    auto hadComponent = false;

    for (const auto& pathComponent : path.getComponents()) {
        if (hadComponent) {
            boost::hash_combine(hash, "/");
        }
        boost::hash_combine(hash, pathComponent);

        if (!hadComponent && pathComponent != "/") {
            hadComponent = true;
        }
    }

    return hash;
}

} // namespace std
