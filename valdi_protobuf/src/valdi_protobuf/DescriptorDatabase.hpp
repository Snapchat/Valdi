// Copyright © 2024 Snap, Inc. All rights reserved.

#pragma once

#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_protobuf/protos/DescriptorIndex.pb.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor_database.h>
#include <vector>

namespace Valdi {
class ExceptionTracker;
}

namespace Valdi::Protobuf {

class DescriptorDatabaseBuilder;

/**
 * DescriptorDatabase is a memory efficient implementation of google::protobuf::DescriptorDatabase
 * that is similar to Protobuf's builtin EncodedDescriptorDatabase but is faster to ingest
 * and exposes public APIs to query about the ingested types which won't induce parsing the
 * files.
 */
class DescriptorDatabase : public google::protobuf::DescriptorDatabase {
public:
    DescriptorDatabase();

    bool addFileDescriptorSet(const BytesView& data, ExceptionTracker& exceptionTracker);

    bool parseAndAddFileDescriptorSet(const std::string& filename,
                                      std::string_view protoFileContent,
                                      ExceptionTracker& exceptionTracker);

    bool FindFileByName(const std::string& filename, google::protobuf::FileDescriptorProto* output) final;

    bool FindFileContainingSymbol(const std::string& symbolName, google::protobuf::FileDescriptorProto* output) final;

    bool FindFileContainingExtension(const std::string& containingType,
                                     int fieldNumber,
                                     google::protobuf::FileDescriptorProto* output) final;

    bool FindAllExtensionNumbers(const std::string& extendeeType, std::vector<int>* output) final;

    bool FindAllFileNames(std::vector<std::string>* output) final;

    std::vector<std::string> getAllSymbolNames() const;

    size_t getSymbolsSize() const;

    const google::protobuf::Descriptor* getDescriptorOfSymbolAtIndex(size_t index) const;

    const std::string& getSymbolNameAtIndex(size_t index) const;

    void setDescriptorOfSymbolAtIndex(size_t index, const google::protobuf::Descriptor* descriptor);

    size_t getPackagesSize() const;

    const DescriptorIndex::Package& getPackageAtIndex(size_t index) const;

    const DescriptorIndex::Package& getRootPackage() const;

    std::optional<size_t> getSymbolIndexForName(std::string_view name);

    Value toDebugJSON() const;

private:
    std::vector<BytesView> _retainedBuffers;
    DescriptorIndex::DescriptorIndex _index;
    std::vector<const google::protobuf::Descriptor*> _descriptors;
    FlatMap<std::string, size_t> _fileIndexByName;
    FlatMap<std::string, size_t> _symbolIndexByName;
    FlatMap<std::string, size_t> _packageIndexByName;
    bool _prebuiltIndexLoaded = false;
    std::shared_ptr<DescriptorDatabaseBuilder> _builder;

    void finaliseIndex();
    bool copyProtoOfFile(size_t fileIndex, google::protobuf::FileDescriptorProto* output);
    Value packageToDebugJSON(const DescriptorIndex::Package& package) const;
    bool addIndexedFileDescriptorSet(const BytesView& data, ExceptionTracker& exceptionTracker);
    bool addFileDescriptorSetWithBuilder(const BytesView& data, ExceptionTracker& exceptionTracker);
};

} // namespace Valdi::Protobuf
