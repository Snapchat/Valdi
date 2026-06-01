//
//  MmapBuffer.hpp
//  ValdiRuntime
//

#pragma once

#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/PathUtils.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

namespace Valdi {

/**
 * A buffer backed by a file-mapped memory region (mmap).
 *
 * Unlike heap-allocated ByteBuffer, the pages in an MmapBuffer are
 * file-backed. Once the OS flushes dirty pages to the backing file,
 * they become clean and can be transparently evicted under memory
 * pressure and reloaded on demand via page faults.
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
     * Create a writable mmap region backed by a new file at filePath.
     * The file is created/truncated to the given size and the disk blocks
     * are preallocated (via F_PREALLOCATE on Apple, posix_fallocate on Linux)
     * so subsequent writes through the mapping cannot SIGBUS on ENOSPC.
     * The mapping is MAP_SHARED | PROT_READ | PROT_WRITE.
     */
    [[nodiscard]] static Result<Ref<MmapBuffer>> createWritable(const Path& filePath, size_t size);

    /**
     * Same as createWritable(Path, size), but takes ownership of an already-open
     * fd (e.g., one returned by mkstemp). The fd is always closed by this call,
     * whether it succeeds or fails. diagPath is used only in error messages.
     */
    [[nodiscard]] static Result<Ref<MmapBuffer>> createWritable(int fd, size_t size, std::string_view diagPath);

    /**
     * Open a read-only mmap over an existing file.
     * The mapping is MAP_PRIVATE | PROT_READ.
     */
    [[nodiscard]] static Result<Ref<MmapBuffer>> openReadOnly(const Path& filePath);

    /**
     * Transition from writable to read-only after writing is complete.
     * Calls msync(MS_ASYNC) to encourage OS flush, then mprotect(PROT_READ).
     */
    [[nodiscard]] Result<Void> makeReadOnly();

    MmapBuffer(void* addr, size_t size);

private:
    void* _addr;
    size_t _size;
};

} // namespace Valdi
