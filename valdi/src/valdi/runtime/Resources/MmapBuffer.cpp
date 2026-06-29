//
//  MmapBuffer.cpp
//  ValdiRuntime
//

#include "valdi/runtime/Resources/MmapBuffer.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fmt/format.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Valdi {

MmapBuffer::MmapBuffer(void* addr, size_t size) : _addr(addr), _size(size) {}

MmapBuffer::~MmapBuffer() {
    if (_addr != nullptr && _addr != MAP_FAILED) {
        munmap(_addr, _size);
    }
}

const Byte* MmapBuffer::data() const {
    return static_cast<const Byte*>(_addr);
}

size_t MmapBuffer::size() const {
    return _size;
}

BytesView MmapBuffer::toBytesView() {
    return BytesView(strongSmallRef(this), data(), _size);
}

Result<Ref<MmapBuffer>> MmapBuffer::createWritable(const Path& filePath, size_t size) {
    if (size == 0) {
        return Error("Cannot mmap a zero-length region");
    }

    auto pathStr = filePath.toString();
    int fd = open(pathStr.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return Error(STRING_FORMAT("Failed to create mmap backing file '{}': {}", pathStr, strerror(errno)));
    }

    return createWritable(fd, size, pathStr);
}

Result<Ref<MmapBuffer>> MmapBuffer::createWritable(int fd, size_t size, std::string_view diagPath) {
    if (size == 0) {
        close(fd);
        return Error("Cannot mmap a zero-length region");
    }

    if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
        auto err = errno;
        close(fd);
        return Error(STRING_FORMAT("Failed to ftruncate mmap backing file '{}': {}", diagPath, strerror(err)));
    }

#if defined(__APPLE__)
    fstore_t fst = {};
    fst.fst_flags = F_ALLOCATEALL;
    fst.fst_posmode = F_PEOFPOSMODE;
    fst.fst_length = static_cast<off_t>(size);
    if (fcntl(fd, F_PREALLOCATE, &fst) != 0) {
        auto err = errno;
        close(fd);
        return Error(STRING_FORMAT("Failed to preallocate disk space for '{}': {}", diagPath, strerror(err)));
    }
#elif defined(__linux__)
    // posix_fallocate returns the error number directly; it does not set errno.
    if (int err = posix_fallocate(fd, 0, static_cast<off_t>(size)); err != 0) {
        close(fd);
        return Error(STRING_FORMAT("Failed to preallocate disk space for '{}': {}", diagPath, strerror(err)));
    }
#endif

    void* addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    auto mmapErr = errno;
    close(fd);

    if (addr == MAP_FAILED) {
        return Error(STRING_FORMAT("Failed to mmap backing file '{}': {}", diagPath, strerror(mmapErr)));
    }

    return makeShared<MmapBuffer>(addr, size);
}

Result<Ref<MmapBuffer>> MmapBuffer::openReadOnly(const Path& filePath) {
    auto pathStr = filePath.toString();
    int fd = open(pathStr.c_str(), O_RDONLY);
    if (fd < 0) {
        return Error(STRING_FORMAT("Failed to open mmap backing file '{}': {}", pathStr, strerror(errno)));
    }

    struct stat st{};
    if (fstat(fd, &st) != 0) {
        auto err = errno;
        close(fd);
        return Error(STRING_FORMAT("Failed to stat mmap backing file '{}': {}", pathStr, strerror(err)));
    }

    auto size = static_cast<size_t>(st.st_size);
    if (size == 0) {
        close(fd);
        return Error(STRING_FORMAT("mmap backing file '{}' is empty", pathStr));
    }

    void* addr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    auto mmapErr = errno;
    close(fd);

    if (addr == MAP_FAILED) {
        return Error(STRING_FORMAT("Failed to mmap backing file '{}': {}", pathStr, strerror(mmapErr)));
    }

    return makeShared<MmapBuffer>(addr, size);
}

Result<Void> MmapBuffer::makeReadOnly() {
    if (mprotect(_addr, _size, PROT_READ) != 0) {
        return Error(STRING_FORMAT("mprotect failed: {}", strerror(errno)));
    }

    return Void();
}

} // namespace Valdi
