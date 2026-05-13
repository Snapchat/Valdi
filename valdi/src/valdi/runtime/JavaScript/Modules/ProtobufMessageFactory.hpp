//
//  ProtobufMessageFactory.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 11/22/19.
//

#pragma once

#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"

#include <mutex>

#include "valdi_protobuf/FullyQualifiedName.hpp"

#include <google/protobuf/descriptor.h>

namespace Valdi {

namespace Protobuf {
class DescriptorDatabase;
}

class ProtobufMessageFactory : public ValdiObject {
public:
    ProtobufMessageFactory();
    ~ProtobufMessageFactory() override;

    struct NamespaceEntry {
        size_t id = 0;
        std::string_view name;
        bool isMessage = false;
    };

    /**
     * Load descriptors from a previously parsed set of .proto files
     * encoded as a FileDescriptorSet.
     */
    bool load(const BytesView& data, ExceptionTracker& exceptionTracker);

    /**
     * Parse a .proto file content and load its descriptors
     */
    bool parseAndLoad(const std::string& filename,
                      std::string_view protoFileContent,
                      ExceptionTracker& exceptionTracker);

    std::vector<std::string> getDescriptorNames() const;

    const google::protobuf::Descriptor* getDescriptorAtIndex(size_t index, ExceptionTracker& exceptionTracker);

    size_t getMessagePrototypeIndexForDescriptor(const google::protobuf::Descriptor* descriptor,
                                                 ExceptionTracker& exceptionTracker) const;

    std::vector<NamespaceEntry> getRootNamespaceEntries() const;
    std::vector<NamespaceEntry> getNamespaceEntriesForId(size_t id, ExceptionTracker& exceptionTracker) const;

    /**
     * Acquire the factory lock before calling a sequence of factory methods that must complete
     * atomically. The lock is recursive, so internal factory methods may also acquire it.
     */
    std::unique_lock<std::recursive_mutex> lock() const;

    VALDI_CLASS_HEADER(ProtobufMessageFactory)

private:
    std::unique_ptr<Protobuf::DescriptorDatabase> _descriptorDatabase;
    google::protobuf::DescriptorPool _pool;
    mutable std::recursive_mutex _mutex;
};

} // namespace Valdi
