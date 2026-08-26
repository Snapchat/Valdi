//
//  DiskUtils.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 10/1/19.
//

#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"

#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include <array>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

namespace Valdi {

static FileStat fileStatFromCStat(bool exists, const struct stat& stat) {
    auto isDir = S_ISDIR(stat.st_mode);
    auto isFile = S_ISREG(stat.st_mode);
    return FileStat(exists, isDir, isFile, static_cast<size_t>(stat.st_size));
}

static FileStat statFromPath(const char* path) {
    struct stat statStruct;
    std::memset(&statStruct, 0, sizeof(statStruct));
    return fileStatFromCStat(stat(path, &statStruct) >= 0, statStruct);
}

static FileStat statFromFd(int fd) {
    struct stat statStruct;
    std::memset(&statStruct, 0, sizeof(statStruct));
    return fileStatFromCStat(fstat(fd, &statStruct) >= 0, statStruct);
}

FileReader::FileReader(const Path& path) : _path(path.toString()) {}
FileReader::~FileReader() {
    close();
}

Result<FileStat> FileReader::open() {
    auto stat = statFromPath(_path.c_str());

    if (!stat.isFile()) {
        return Error(STRING_FORMAT("No file at {}", _path));
    }

    _stream.open(_path, std::ios::binary);
    if (!_stream.is_open()) {
        return Error(STRING_FORMAT("Unable to open file at {}", _path));
    }

    _fileSize = stat.size();
    return stat;
}

void FileReader::close() {
    if (_stream.is_open()) {
        _stream.close();
    }
    _fileSize = 0;
}

Result<BytesView> FileReader::read() {
    return read(0, std::nullopt);
}

static Error outOfBOundsLoad(const std::string& pathStr, size_t byteOffset, size_t toReadSize, size_t fileSize) {
    return Error(STRING_FORMAT("Out of bounds read {} (asked byte offest {} with size {}, file size is {})",
                               pathStr,
                               byteOffset,
                               toReadSize,
                               fileSize));
}

Result<BytesView> FileReader::read(size_t byteOffset, std::optional<size_t> size) {
    size_t toReadSize = size.has_value() ? size.value() : _fileSize;
    if (byteOffset > 0 && !size.has_value()) {
        if (byteOffset > _fileSize) {
            return outOfBOundsLoad(_path, byteOffset, _fileSize, _fileSize);
        }
        toReadSize = _fileSize - byteOffset;
    }

    if (byteOffset + toReadSize > _fileSize) {
        return outOfBOundsLoad(_path, byteOffset, toReadSize, _fileSize);
    }

    _stream.seekg(byteOffset, std::ios::beg);

    auto bytes = makeShared<ByteBuffer>();
    bytes->resize(static_cast<size_t>(toReadSize));

    _stream.read(reinterpret_cast<char*>(bytes->data()), bytes->size());
    if (!_stream.good()) {
        return Error(STRING_FORMAT("Unable to read file at {}", _path));
    }

    return bytes->toBytesView();
}

bool FileReader::isOpened() const {
    return _stream.is_open();
}

FileStat DiskUtils::stat(const Path& path) {
    return statFromPath(path.toString().c_str());
}

bool DiskUtils::isFile(const Path& path) {
    return stat(path).isFile();
}

bool DiskUtils::isDirectory(const Path& path) {
    return stat(path).isDir();
}

Result<BytesView> DiskUtils::load(const Path& path) {
    return load(path, 0, std::nullopt);
}

Result<BytesView> DiskUtils::load(const Path& path, size_t byteOffset, std::optional<size_t> size) {
    FileReader reader(path);

    auto open = reader.open();
    if (!open) {
        return open.moveError();
    }

    auto read = reader.read(byteOffset, size);
    reader.close();

    return read;
}

static Error onLoadFromFdError(int fd, const char* error) {
    return Valdi::Error(STRING_FORMAT("Failed to load from fd '{}': {}", fd, error));
}

Result<BytesView> DiskUtils::loadFromFd(int fd) {
    auto stat = statFromFd(fd);
    if (!stat.exists()) {
        return onLoadFromFdError(fd, "File does not exist");
    }

    auto fileSize = stat.size();

    auto bytes = makeShared<ByteBuffer>();
    bytes->resize(fileSize);

    size_t totalRead = 0;
    while (totalRead < fileSize) {
        auto result = read(fd, (*bytes)[totalRead], fileSize - totalRead);
        if (result < 0) {
            return onLoadFromFdError(fd, strerror(errno));
        }

        if (result == 0) {
            return onLoadFromFdError(fd, "Unexpectedly reached end of file");
        }

        totalRead += result;
    }

    return bytes->toBytesView();
}

Result<Void> DiskUtils::store(const Path& path, const BytesView& bytes) {
    return store(path, bytes.asStringView());
}

Result<Void> DiskUtils::store(const Path& path, std::string_view bytes) {
    auto pathStr = path.toString();
    std::ofstream s;
    s.open(pathStr, std::ios::trunc | std::ios::binary);
    if (!s.is_open()) {
        return Error(STRING_FORMAT("Unable to open file for writing at {}", pathStr));
    }

    s.write(bytes.data(), bytes.size());
    auto success = s.good();
    s.close();

    if (!success) {
        return Error(STRING_FORMAT("Unable to write file at {}", pathStr));
    }

    return Void();
}

Result<Void> DiskUtils::copy(const Path& source, const Path& destination) {
    auto sourcePath = source.toString();
    std::ifstream input(sourcePath, std::ios::binary);
    if (!input.is_open()) {
        return Error(fmt::format("Unable to open file for reading at {}", sourcePath));
    }

    auto destinationPath = destination.toString();
    std::ofstream output(destinationPath, std::ios::trunc | std::ios::binary);
    if (!output.is_open()) {
        return Error(fmt::format("Unable to open file for writing at {}", destinationPath));
    }

    std::array<char, 64 * 1024> buffer;
    while (input.good()) {
        input.read(buffer.data(), buffer.size());
        auto bytesRead = input.gcount();
        if (bytesRead > 0) {
            output.write(buffer.data(), bytesRead);
            if (!output.good()) {
                return Error(fmt::format("Unable to write file at {}", destinationPath));
            }
        }
    }
    if (!input.eof()) {
        return Error(fmt::format("Unable to read file at {}", sourcePath));
    }

    output.close();
    if (output.fail()) {
        return Error(fmt::format("Unable to write file at {}", destinationPath));
    }
    return Void();
}

bool DiskUtils::makeDirectory(const Path& path, bool createIntermediates) {
    if (createIntermediates && path.getComponents().size() > 1) {
        auto parentPath = path.removingLastComponent();
        if (!isDirectory(parentPath) && !makeDirectory(path.removingLastComponent(), true)) {
            return false;
        }
    }

    auto pathStr = path.toString();
    return mkdir(pathStr.c_str(), S_IRWXU | S_IRWXG | S_IRWXO) >= 0;
}

bool DiskUtils::remove(const Path& path) {
    auto pathStr = path.toString();
    auto stat = statFromPath(pathStr.c_str());

    if (stat.isDir()) {
        auto content = DiskUtils::listDirectory(path);
        for (const auto& file : content) {
            if (!remove(file)) {
                return false;
            }
        }
    }

    return std::remove(pathStr.c_str()) >= 0;
}

std::vector<Path> DiskUtils::listDirectory(const Path& path) {
    auto pathStr = path.toString();
    std::vector<Path> out;

    auto* dir = opendir(pathStr.c_str());
    if (dir != nullptr) {
        struct dirent* dp = nullptr;
        while ((dp = readdir(dir)) != nullptr) {
            std::string_view filepath(dp->d_name);

            if (filepath != ".." && filepath != ".") {
                out.emplace_back(path.appending(filepath));
            }
        }

        closedir(dir);
    }

    return out;
}

Path DiskUtils::temporaryFilePath() {
    char tempFileName[L_tmpnam];

    (void)std::tmpnam(tempFileName);
    return Path(std::string_view(tempFileName, strlen(tempFileName)));
}

Path DiskUtils::currentWorkingDirectory() {
    char cwdBuffer[PATH_MAX];
    (void)::getcwd(cwdBuffer, PATH_MAX);

    return Path(cwdBuffer);
}

Path DiskUtils::absolutePathFromString(const std::string_view& pathString) {
    Path path(pathString);
    if (!path.isAbsolute()) {
        path = currentWorkingDirectory().appending(path);
    }

    path.normalize();
    return path;
}

} // namespace Valdi
