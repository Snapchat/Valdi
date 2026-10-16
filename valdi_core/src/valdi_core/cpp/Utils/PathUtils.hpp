//
//  PathUtils.hpp
//  ValdiRuntime
//
//  Created by Brandon Francis on 8/1/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi_core/cpp/Utils/StringBox.hpp"
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace Valdi {

class Path {
public:
    Path();
    explicit Path(const std::string_view& path);
    explicit Path(const StringBox& path);

    /**
     * Normalize the path, which simplifies the path by removing relative components and trailing slashes when
     * needed.
     */
    Path& normalize();

    /**
     * Appends another path to this base path (e.g. /path + to/file.txt -> /path/to/file.txt).
     */
    Path& append(const Path& other);

    /**
     * Appends a path as string to this base path (e.g. /path + to/file.txt -> /path/to/file.txt).
     */
    Path& append(const std::string_view& component);

    /**
     Returns a new path by appending this component to this base path
     */
    Path appending(const Path& other) const;

    /**
     Returns a new path by appending this component to this base path
     */
    Path appending(const std::string_view& component) const;

    /**
     Returns a new path that addresses this path relative to the given base path.
     */
    Path relativeTo(const Path& basePath) const;

    /**
     Returns the last path component, or empty string if the path is empty
     */
    std::string_view getLastComponent() const;

    /**
     Returns the first path component, or empty string if the path is empty
     */
    std::string getFirstComponent() const;

    /**
     Remove the last component from this path.
     Does nothing if the path is empty
     */
    Path& removeLastComponent();

    /**
     Returns a new path by removing the last component from this path.
     Does nothing if the path is empty
     */
    Path removingLastComponent() const;

    /**
     Remove the first component from this path.
     Does nothing if the path is empty
     */
    Path& removeFirstComponent();

    /**
     Returns a new path by removing the first component from this path.
     Does nothing if the path is empty
     */
    Path removingFirstComponent() const;

    /**
     Remove the file extension from the last path component.
     Does nothing if the path is empty or no extension was found.
     */
    Path& removeFileExtension();

    /**
     Add the file extension to the last path component.
     Does nothing if the path is empty.
     Does not check if the last path component already has an extension.
     Does not check if the last path component is a valid file name.
     Does not check if the newExtension already has a dot (it shouldn't).
     */
    Path& appendFileExtension(const std::string_view& newExtension);

    /**
     Returns the file extension represented by this Path, or empty string
     if the Path is empty or does not have a file extension
     */
    std::string_view getFileExtension() const;

    /**
     Returns a representation of this path as a string
     */
    std::string toString() const;

    /**
     Returns an interned representation of this path as a string
     */
    StringBox toStringBox() const;

    /**
     Returns whether this path starts with the given path, meaning
     all components of the given path are at the beginning of this path.
     The given path and this path should have been normalized() first.
     */
    bool startsWith(const Path& otherPath) const;

    /**
     * Returns whether or not a path is absolute, e.g. starts with a slash.
     */
    bool isAbsolute() const;

    bool empty() const;

    const std::vector<std::string>& getComponents() const;

    bool operator==(const Path& other) const;
    bool operator!=(const Path& other) const;

    static std::string_view removeFileExtensionFromComponent(const std::string_view& pathComponent);

private:
    std::vector<std::string> _components;
};

std::ostream& operator<<(std::ostream& os, const Path& path);

} // namespace Valdi

namespace std {

template<>
struct hash<Valdi::Path> {
    std::size_t operator()(const Valdi::Path& path) const noexcept;
};

} // namespace std
