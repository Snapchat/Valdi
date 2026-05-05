// Copyright © 2025 Snap, Inc. All rights reserved.

#include "valdi_protobuf/DescriptorDatabaseBuilder.hpp"

#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "valdi_protobuf/FullyQualifiedName.hpp"
#include "valdi_protobuf/Message.hpp"
#include "valdi_protobuf/protos/DescriptorIndex.pb.h"

#include <google/protobuf/descriptor.pb.h>

#include <google/protobuf/compiler/parser.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

namespace Valdi::Protobuf {

// Field numbers must be in sync with descriptor.proto
struct FileDescriptorSetProtoFieldNumbers {
    static constexpr FieldNumber kFile = 1;
};

struct FileDescriptorProtoFieldNumbers {
    static constexpr FieldNumber kName = 1;
    static constexpr FieldNumber kPackage = 2;
    static constexpr FieldNumber kMessageType = 4;
    static constexpr FieldNumber kEnumType = 5;
};

struct DescriptorProtoFieldNumbers {
    static constexpr FieldNumber kName = 1;
    static constexpr FieldNumber kNestedType = 3;
    static constexpr FieldNumber kEnumType = 4;
};

struct EnumDescriptorProtoFieldNumbers {
    static constexpr FieldNumber kName = 1;
    // We rely on this assumption
    static_assert(EnumDescriptorProtoFieldNumbers::kName == DescriptorProtoFieldNumbers::kName);
};

class MessagePool;

class BorrowedMessage;

class MessagePool {
public:
    MessagePool() = default;

    BorrowedMessage dequeueMessage();

    Message& getMessage(size_t index) const {
        return *_messages[index];
    }

private:
    SmallVector<Ref<Message>, 4> _messages;
    SmallVector<size_t, 4> _freeIndexes;

    friend BorrowedMessage;
};

class BorrowedMessage {
public:
    BorrowedMessage(MessagePool& pool, size_t index) : _pool(pool), _index(index) {}
    ~BorrowedMessage() {
        get()->clearAllFields();
        _pool._freeIndexes.emplace_back(_index);
    }

    constexpr Message* operator->() const {
        return &_pool.getMessage(_index);
    }

    constexpr Message& operator*() const {
        return _pool.getMessage(_index);
    }

    constexpr Message* get() const {
        return &_pool.getMessage(_index);
    }

private:
    MessagePool& _pool;
    size_t _index;
};

BorrowedMessage MessagePool::dequeueMessage() {
    if (_freeIndexes.empty()) {
        auto index = _messages.size();
        _messages.emplace_back(makeShared<Message>());
        return BorrowedMessage(*this, index);
    } else {
        auto index = _freeIndexes.back();
        _freeIndexes.pop_back();
        return BorrowedMessage(*this, index);
    }
}

class ProtobufErrorCollector : public google::protobuf::DescriptorPool::ErrorCollector,
                               public google::protobuf::io::ErrorCollector {
public:
    ~ProtobufErrorCollector() override = default;

    void RecordError(absl::string_view filename,
                     [[maybe_unused]] absl::string_view elementName,
                     [[maybe_unused]] const google::protobuf::Message* descriptor,
                     [[maybe_unused]] ErrorLocation location,
                     absl::string_view message) final {
        _messages.emplace_back(ProtobufErrorCollector::makeMessage("error", filename, message));
    }

    void RecordWarning(absl::string_view filename,
                       [[maybe_unused]] absl::string_view elementName,
                       [[maybe_unused]] const google::protobuf::Message* descriptor,
                       [[maybe_unused]] ErrorLocation location,
                       absl::string_view message) final {
        _messages.emplace_back(ProtobufErrorCollector::makeMessage("warning", filename, message));
    }

    void RecordError(int line, google::protobuf::io::ColumnNumber column, absl::string_view message) final {
        _messages.emplace_back(STRING_FORMAT("At {}:{}: {}", line, column, message));
    }

    void RecordWarning([[maybe_unused]] int line,
                       [[maybe_unused]] google::protobuf::io::ColumnNumber column,
                       [[maybe_unused]] absl::string_view message) final {}

    bool failed() const {
        return !_messages.empty();
    }

    StringBox getErrorMessage() const {
        return StringBox::join(_messages, "\n");
    }

private:
    std::vector<StringBox> _messages;

    static StringBox makeMessage(std::string_view type, std::string_view filename, std::string_view message) {
        return STRING_FORMAT("{} in {}: {}", type, filename, message);
    }
};

DescriptorDatabaseBuilder::DescriptorDatabaseBuilder() {
    // First package represent the root package
    auto& package = _packages.emplace_back();
    package.fullName = FullyQualifiedName("<root>");
}

bool DescriptorDatabaseBuilder::addFileDescriptorSet(const BytesView& data, ExceptionTracker& exceptionTracker) {
    Message message(nullptr, nullptr);
    if (!message.decode(data.data(), data.size(), exceptionTracker)) {
        return false;
    }

    _retainedBuffers.emplace_back(data);

    MessagePool pool;
    for (const auto& file : message.getFieldIterator(FileDescriptorSetProtoFieldNumbers::kFile)) {
        auto raw = file.getRaw();

        if (!addFileDescriptorInner(pool, raw.data, raw.length, exceptionTracker)) {
            return false;
        }
    }
    return true;
}

bool DescriptorDatabaseBuilder::addFileDescriptor(const BytesView& data, ExceptionTracker& exceptionTracker) {
    _retainedBuffers.emplace_back(data);
    MessagePool pool;
    return addFileDescriptorInner(pool, data.data(), data.size(), exceptionTracker);
}

bool DescriptorDatabaseBuilder::parseAndAddFileDescriptorSet(const std::string& filename,
                                                             std::string_view protoFileContent,
                                                             ExceptionTracker& exceptionTracker) {
    google::protobuf::io::ArrayInputStream inputStream(protoFileContent.data(),
                                                       static_cast<int>(protoFileContent.size()));
    ProtobufErrorCollector errorCollector;
    google::protobuf::io::Tokenizer tokenizer(&inputStream, &errorCollector);
    google::protobuf::compiler::Parser parser;
    parser.RecordErrorsTo(&errorCollector);

    google::protobuf::FileDescriptorProto proto;
    if (!parser.Parse(&tokenizer, &proto)) {
        exceptionTracker.onError(fmt::format("Failed to parse protobuf file: {}", errorCollector.getErrorMessage()));
        return false;
    }
    proto.set_name(filename);

    auto buffer = makeShared<ByteBuffer>();
    buffer->resize(proto.ByteSizeLong());

    proto.SerializeToArray(buffer->data(), static_cast<int>(buffer->size()));

    return addFileDescriptor(buffer->toBytesView(), exceptionTracker);
}

bool DescriptorDatabaseBuilder::addFileDescriptorInner(MessagePool& messagePool,
                                                       const Byte* data,
                                                       size_t length,
                                                       ExceptionTracker& exceptionTracker) {
    auto fileDescriptor = messagePool.dequeueMessage();
    if (!fileDescriptor->decode(data, length, exceptionTracker)) {
        return false;
    }

    auto key =
        StringCache::getGlobal().makeString(fileDescriptor->getFieldAsString(FileDescriptorProtoFieldNumbers::kName));
    if (_fileIndexByName.contains(key)) {
        exceptionTracker.onError(Error(STRING_FORMAT("File '{}' already exist", key)));
        return false;
    }

    auto fileIndex = _files.size();
    auto& file = _files.emplace_back();
    file.fileName = key;
    file.data = data;
    file.length = length;

    _fileIndexByName[key] = fileIndex;

    FullyQualifiedName packageName(fileDescriptor->getFieldAsString(FileDescriptorProtoFieldNumbers::kPackage));
    size_t packageIndex = 0;
    if (!packageName.empty()) {
        // We have a package, resolve it
        packageIndex = getOrCreatePackageIndex(packageName);
    }

    for (const auto& messageTypeField :
         fileDescriptor->getFieldIterator(FileDescriptorProtoFieldNumbers::kMessageType)) {
        if (!addDescriptor(fileIndex, packageIndex, packageName, messagePool, messageTypeField, exceptionTracker)) {
            return false;
        }
    }
    for (const auto& enumType : fileDescriptor->getFieldIterator(FileDescriptorProtoFieldNumbers::kEnumType)) {
        auto enumName = packageName;
        auto enumMessage = messagePool.dequeueMessage();
        if (!addSymbolFromField(fileIndex, packageIndex, enumName, *enumMessage, enumType, exceptionTracker)) {
            return false;
        }
    }

    return true;
}

bool DescriptorDatabaseBuilder::addDescriptor(size_t fileIndex,
                                              size_t packageIndex,
                                              const FullyQualifiedName& packageName,
                                              MessagePool& messagePool,
                                              const Protobuf::Field& field,
                                              ExceptionTracker& exceptionTracker) {
    auto fullName = packageName;

    auto outMessage = messagePool.dequeueMessage();

    if (!addSymbolFromField(fileIndex, packageIndex, fullName, *outMessage, field, exceptionTracker)) {
        return false;
    }

    auto nestedTypesIterator = outMessage->getFieldIterator(DescriptorProtoFieldNumbers::kNestedType);
    auto nestedEnumsIterator = outMessage->getFieldIterator(DescriptorProtoFieldNumbers::kEnumType);

    if (!nestedTypesIterator.empty() || !nestedEnumsIterator.empty()) {
        auto childPackageIndex = getOrCreatePackageIndex(fullName);

        for (const auto& nestedType : nestedTypesIterator) {
            if (!addDescriptor(fileIndex, childPackageIndex, fullName, messagePool, nestedType, exceptionTracker)) {
                return false;
            }
        }

        for (const auto& nestedEnum : nestedEnumsIterator) {
            auto nestedEnumName = fullName;
            auto nestedMessage = messagePool.dequeueMessage();
            if (!addSymbolFromField(
                    fileIndex, childPackageIndex, nestedEnumName, *nestedMessage, nestedEnum, exceptionTracker)) {
                return false;
            }
        }
    }

    return true;
}

bool DescriptorDatabaseBuilder::addSymbolFromField(size_t fileIndex,
                                                   size_t packageIndex,
                                                   FullyQualifiedName& outputName,
                                                   Message& outMessage,
                                                   const Protobuf::Field& field,
                                                   ExceptionTracker& exceptionTracker) {
    auto raw = field.getRaw();
    if (!outMessage.decode(raw.data, raw.length, exceptionTracker)) {
        return false;
    }

    outputName.append(outMessage.getFieldAsString(DescriptorProtoFieldNumbers::kName));

    return addSymbol(fileIndex, packageIndex, outputName, exceptionTracker);
}

bool DescriptorDatabaseBuilder::addSymbol(size_t fileIndex,
                                          size_t packageIndex,
                                          const FullyQualifiedName& name,
                                          ExceptionTracker& exceptionTracker) {
    if (_symbolIndexByName.find(name) != _symbolIndexByName.end()) {
        exceptionTracker.onError(Error(STRING_FORMAT("Symbol '{}' already exist", name)));
        return false;
    }

    auto symbolIndex = _symbols.size();
    auto& symbol = _symbols.emplace_back();
    symbol.fullName = name;
    symbol.fileIndex = fileIndex;

    _symbolIndexByName[name] = symbolIndex;

    // Append our new message type inside the package
    _packages[packageIndex].symbolIndexes.emplace_back(symbolIndex);

    return true;
}

size_t DescriptorDatabaseBuilder::getOrCreatePackageIndex(const FullyQualifiedName& name) {
    const auto& it = _packageIndexByName.find(name);
    if (it != _packageIndexByName.end()) {
        return it->second;
    }

    // Use 0 as default which will point to the root package
    size_t ancestorPackageIndex = 0;

    // New package. We first need to register the ancestor package if there is one
    if (name.canRemoveLastComponent()) {
        ancestorPackageIndex = getOrCreatePackageIndex(name.removingLastComponent());
    }

    // Now register our package

    auto packageIndex = _packages.size();
    auto& package = _packages.emplace_back();
    package.fullName = name;

    _packageIndexByName[name] = packageIndex;

    _packages[ancestorPackageIndex].nestedPackageIndexes.emplace_back(packageIndex);

    return packageIndex;
}

bool DescriptorDatabaseBuilder::build(std::vector<BytesView>& retainedBuffers,
                                      DescriptorIndex::DescriptorIndex& descriptorIndex) {
    DescriptorIndex::DescriptorIndex idx;
    for (const auto& f : _files) {
        auto* ff = idx.add_files();
        ff->set_file_name(f.fileName.slowToString());
        ff->set_data_offset(reinterpret_cast<uintptr_t>(f.data));
        ff->set_length(static_cast<uint32_t>(f.length));
    }
    for (const auto& s : _symbols) {
        auto* ss = idx.add_symbols();
        ss->set_full_name(s.fullName.toString());
        ss->set_file_index(static_cast<uint32_t>(s.fileIndex));
    }
    for (const auto& p : _packages) {
        auto* pp = idx.add_packages();
        pp->set_full_name(p.fullName.toString());
        for (const auto si : p.symbolIndexes) {
            pp->add_symbol_indexes(static_cast<uint32_t>(si));
        }
        for (const auto pi : p.nestedPackageIndexes) {
            pp->add_nested_package_indexes(static_cast<uint32_t>(pi));
        }
    }
    retainedBuffers = _retainedBuffers;
    descriptorIndex = std::move(idx);
    return true;
}

} // namespace Valdi::Protobuf
