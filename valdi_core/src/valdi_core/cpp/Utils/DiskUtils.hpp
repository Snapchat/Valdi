//
//  DiskUtils.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 10/1/19.
//

#pragma once

#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/PathUtils.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include <fstream>
#include <optional>
#include <string_view>
#include <vector>

namespace Valdi {

class FileStat {
public:
    FileStat(bool exists, bool isDir, bool isFile, size_t size)
        : _exists(exists), _isDir(isDir), _isFile(isFile), _size(size) {}

    constexpr bool exists() const {
        return _exists;
    }

    constexpr bool isDir() const {
        return _isDir;
    }

    constexpr bool isFile() const {
        return _isFile;
    }

    constexpr size_t size() const {
        return _size;
    }

private:
    bool _exists;
    bool _isDir;
    bool _isFile;
    size_t _size;
};

class FileReader {
public:
    FileReader(const Path& path);
    ~FileReader();

    Result<FileStat> open();
    void close();

    Result<BytesView> read();
    Result<BytesView> read(size_t byteOffset, std::optional<size_t> size);

    bool isOpened() const;

private:
    std::string _path;
    std::ifstream _stream;
    size_t _fileSize = 0;
};

class DiskUtils {
public:
    static FileStat stat(const Path& path);

    static bool isFile(const Path& path);

    static bool isDirectory(const Path& path);

    static Result<BytesView> load(const Path& path);

    static Result<BytesView> load(const Path& path, size_t byteOffset, std::optional<size_t> size);

    static Result<BytesView> loadFromFd(int fd);

    static Result<Void> store(const Path& path, const BytesView& bytes);

    static Result<Void> store(const Path& path, std::string_view bytes);

    static Result<Void> copy(const Path& source, const Path& destination);

    static bool remove(const Path& path);

    static bool makeDirectory(const Path& path, bool createIntermediates);

    // Read a directory and returns all its files.
    static std::vector<Path> listDirectory(const Path& path);

    static Path temporaryFilePath();
    static Path currentWorkingDirectory();
    static Path absolutePathFromString(const std::string_view& pathString);
};

} // namespace Valdi
