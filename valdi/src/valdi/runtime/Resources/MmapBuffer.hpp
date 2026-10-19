//
//  MmapBuffer.hpp
//  ValdiRuntime
//

#pragma once

#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

namespace Valdi {

/**
 * A read-only, file-backed memory mapping (mmap).
 *
 * Unlike heap-allocated ByteBuffer, the pages are clean and file-backed: the OS can evict them under memory
 * pressure and reloads them on demand via page faults.
 */
class MmapBuffer : public SimpleRefCountable {
public:
    ~MmapBuffer() override;

    MmapBuffer(const MmapBuffer&) = delete;
    MmapBuffer& operator=(const MmapBuffer&) = delete;

    const Byte* data() const;
    size_t size() const;

    BytesView toBytesView();

    /**
     * Maps `size` bytes of an already-open file read-only (MAP_PRIVATE | PROT_READ). The fd is not
     * closed and may be closed by the caller as soon as this returns.
     */
    [[nodiscard]] static Result<Ref<MmapBuffer>> mapReadOnly(int fd, size_t size, std::string_view diagPath);

    MmapBuffer(void* addr, size_t size);

private:
    void* _addr;
    size_t _size;
};

} // namespace Valdi
