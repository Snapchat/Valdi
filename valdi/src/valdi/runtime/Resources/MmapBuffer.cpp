//
//  MmapBuffer.cpp
//  ValdiRuntime
//

#include "valdi/runtime/Resources/MmapBuffer.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include <cerrno>
#include <cstring>
#include <fmt/format.h>
#include <sys/mman.h>

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

Result<Ref<MmapBuffer>> MmapBuffer::mapReadOnly(int fd, size_t size, std::string_view diagPath) {
    if (size == 0) {
        return Error(STRING_FORMAT("mmap backing file '{}' is empty", diagPath));
    }

    void* addr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        return Error(STRING_FORMAT("Failed to mmap backing file '{}': {}", diagPath, strerror(errno)));
    }

    return makeShared<MmapBuffer>(addr, size);
}

} // namespace Valdi
