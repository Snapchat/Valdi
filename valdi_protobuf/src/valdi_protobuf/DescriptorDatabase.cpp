// Copyright © 2024 Snap, Inc. All rights reserved.

#include "valdi_protobuf/DescriptorDatabase.hpp"
#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "valdi_protobuf/DescriptorDatabaseBuilder.hpp"
#include "valdi_protobuf/Message.hpp"

#include <google/protobuf/compiler/parser.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include <chrono>
#include <fstream>

namespace Valdi::Protobuf {

DescriptorDatabase::DescriptorDatabase() = default;

bool DescriptorDatabase::addFileDescriptorSet(const BytesView& data, ExceptionTracker& exceptionTracker) {
    // prebuilt index can only be loaded once. Supporting loading things in
    // multiple batches increase the complexity considerably but not much
    // benefit because all of Snap's use cases only load descriptor set once.
    assert(!_prebuiltIndexLoaded);

    if (data.size() > 12 /*header*/) {
        std::string_view signature(reinterpret_cast<const char*>(data.data()), 8);
        if (signature == "VALDIPRO") { // signature match
            uint32_t indexSize = *reinterpret_cast<const uint32_t*>(data.data() + 8);
            const uint8_t* pIndex = reinterpret_cast<const uint8_t*>(data.data() + 12);
            const uint8_t* pBody = pIndex + indexSize;
            size_t bodySize = data.size() - 12 /*header*/ - indexSize;

            // load from prebuilt index
            _retainedBuffers.emplace_back(data);
            if (!_index.ParseFromArray(pIndex, static_cast<int>(indexSize))) {
                return false;
            }
            // patch file pointers
            for (auto& f : *_index.mutable_files()) {
                if (f.data_offset() > bodySize) {
                    assert(false);
                    return false;
                }
                f.set_data_offset(reinterpret_cast<uint64_t>(pBody + f.data_offset()));
            }
            finaliseIndex();
            _prebuiltIndexLoaded = true;
            return true;
        }
    }

    // fallback: load with builder
    return addFileDescriptorSetWithBuilder(data, exceptionTracker);
}

bool DescriptorDatabase::parseAndAddFileDescriptorSet(const std::string& filename,
                                                      std::string_view protoFileContent,
                                                      ExceptionTracker& exceptionTracker) {
    // load from proto source is always handled by builder.  this is only used
    // for testing. prod code always loads binary files (with
    // addFileDescriptorSet).
    assert(!_prebuiltIndexLoaded);
    if (_builder == nullptr) {
        _builder = std::make_shared<DescriptorDatabaseBuilder>();
    }
    if (!_builder->parseAndAddFileDescriptorSet(filename, protoFileContent, exceptionTracker)) {
        return false;
    }
    if (!_builder->build(_retainedBuffers, _index)) {
        return false;
    }
    finaliseIndex();
    return true;
}

bool DescriptorDatabase::FindFileByName(const std::string& filename, google::protobuf::FileDescriptorProto* output) {
    const auto& it = _fileIndexByName.find(filename);
    if (it == _fileIndexByName.end()) {
        return false;
    }
    return copyProtoOfFile(it->second, output);
}

bool DescriptorDatabase::FindFileContainingSymbol(const std::string& symbolName,
                                                  google::protobuf::FileDescriptorProto* output) {
    const auto& it = _symbolIndexByName.find(symbolName);
    if (it == _symbolIndexByName.end()) {
        return false;
    }
    return copyProtoOfFile(_index.symbols(static_cast<int>(it->second)).file_index(), output);
}

bool DescriptorDatabase::FindFileContainingExtension(const std::string& /* containingType */,
                                                     int /* fieldNumber */,
                                                     google::protobuf::FileDescriptorProto* /*output*/) {
    return false;
}

bool DescriptorDatabase::FindAllExtensionNumbers(const std::string& /* extendeeType */,
                                                 std::vector<int>* /* output */) {
    return false;
}

bool DescriptorDatabase::FindAllFileNames(std::vector<std::string>* output) {
    for (const auto& it : _index.files()) {
        output->push_back(it.file_name());
    }
    return true;
}

std::vector<std::string> DescriptorDatabase::getAllSymbolNames() const {
    std::vector<std::string> output;
    output.reserve(_index.symbols_size());
    for (const auto& symbol : _index.symbols()) {
        output.push_back(symbol.full_name());
    }
    return output;
}

size_t DescriptorDatabase::getSymbolsSize() const {
    return _index.symbols_size();
}

const google::protobuf::Descriptor* DescriptorDatabase::getDescriptorOfSymbolAtIndex(size_t index) const {
    return _descriptors[index];
}

const std::string& DescriptorDatabase::getSymbolNameAtIndex(size_t index) const {
    return _index.symbols(static_cast<int>(index)).full_name();
}

void DescriptorDatabase::setDescriptorOfSymbolAtIndex(size_t index, const google::protobuf::Descriptor* descriptor) {
    _descriptors[index] = descriptor;
}

size_t DescriptorDatabase::getPackagesSize() const {
    return _index.packages_size();
}

const DescriptorIndex::Package& DescriptorDatabase::getPackageAtIndex(size_t index) const {
    return _index.packages(static_cast<int>(index));
}

const DescriptorIndex::Package& DescriptorDatabase::getRootPackage() const {
    return _index.packages(0);
}

std::optional<size_t> DescriptorDatabase::getSymbolIndexForName(std::string_view name) {
    const auto& it = _symbolIndexByName.find(name);
    if (it == _symbolIndexByName.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool DescriptorDatabase::copyProtoOfFile(size_t fileIndex, google::protobuf::FileDescriptorProto* output) {
    const auto& file = _index.files(static_cast<int>(fileIndex));
    return output->ParseFromArray(reinterpret_cast<const void*>(file.data_offset()), static_cast<int>(file.length()));
}

Value DescriptorDatabase::toDebugJSON() const {
    return packageToDebugJSON(getRootPackage());
}

Value DescriptorDatabase::packageToDebugJSON(const DescriptorIndex::Package& package) const {
    auto out = Value().setMapValue("name", Value(package.full_name()));

    if (package.symbol_indexes_size() != 0) {
        auto symbols = ValueArray::make(package.symbol_indexes_size());
        for (int i = 0; i < package.symbol_indexes_size(); i++) {
            auto symbolIndex = package.symbol_indexes(i);
            const auto& symbolName = getSymbolNameAtIndex(symbolIndex);
            symbols->emplace(i, Value(symbolName));
        }
        symbols->sort();

        out.setMapValue("symbols", Value(symbols));
    }

    if (package.nested_package_indexes_size() != 0) {
        auto packages = ValueArray::make(package.nested_package_indexes_size());
        for (int i = 0; i < package.nested_package_indexes_size(); i++) {
            auto packageIndex = package.nested_package_indexes(i);
            const auto& package = getPackageAtIndex(packageIndex);
            packages->emplace(i, packageToDebugJSON(package));
        }

        out.setMapValue("packages", Value(packages));
    }

    return out;
}

void DescriptorDatabase::finaliseIndex() {
    _fileIndexByName.clear();
    _fileIndexByName.reserve(_index.files_size());
    _symbolIndexByName.clear();
    _symbolIndexByName.reserve(_index.symbols_size());
    _packageIndexByName.clear();
    _packageIndexByName.reserve(_index.packages_size());
    for (int i = 0; i < _index.files_size(); ++i) {
        _fileIndexByName[_index.files(i).file_name()] = static_cast<size_t>(i);
    }
    for (int i = 0; i < _index.symbols_size(); ++i) {
        _symbolIndexByName[_index.symbols(i).full_name()] = static_cast<size_t>(i);
    }
    for (int i = 0; i < _index.packages_size(); ++i) {
        _packageIndexByName[_index.packages(i).full_name()] = static_cast<size_t>(i);
    }
    _descriptors = std::vector<const google::protobuf::Descriptor*>(_index.symbols_size(), nullptr);
}

bool DescriptorDatabase::addFileDescriptorSetWithBuilder(const BytesView& data, ExceptionTracker& exceptionTracker) {
    if (_builder == nullptr) {
        _builder = std::make_shared<DescriptorDatabaseBuilder>();
    }
    if (!_builder->addFileDescriptorSet(data, exceptionTracker)) {
        return false;
    }
    if (!_builder->build(_retainedBuffers, _index)) {
        return false;
    }
    finaliseIndex();
    return true;
}

} // namespace Valdi::Protobuf
