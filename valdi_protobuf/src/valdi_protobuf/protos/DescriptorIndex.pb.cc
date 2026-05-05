// Generate by the Snap-modified protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: DescriptorIndex.proto
// Protobuf C++ Version: 5.27.0

#include "DescriptorIndex.pb.h"

#include <algorithm>
#include <type_traits>
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/generated_message_tctable_impl.h"
#include "google/protobuf/extension_set.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"
PROTOBUF_PRAGMA_INIT_SEG
namespace _pb = ::google::protobuf;
namespace _pbi = ::google::protobuf::internal;
namespace _fl = ::google::protobuf::internal::field_layout;
namespace DescriptorIndex {

inline constexpr Symbol::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : full_name_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        file_index_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR Symbol::Symbol(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct SymbolDefaultTypeInternal {
  PROTOBUF_CONSTEXPR SymbolDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~SymbolDefaultTypeInternal() {}
  union {
    Symbol _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 SymbolDefaultTypeInternal _Symbol_default_instance_;

inline constexpr Package::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : symbol_indexes_{},
        _symbol_indexes_cached_byte_size_{0},
        nested_package_indexes_{},
        _nested_package_indexes_cached_byte_size_{0},
        full_name_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR Package::Package(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct PackageDefaultTypeInternal {
  PROTOBUF_CONSTEXPR PackageDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~PackageDefaultTypeInternal() {}
  union {
    Package _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 PackageDefaultTypeInternal _Package_default_instance_;

inline constexpr FileEntry::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : file_name_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        data_offset_{::uint64_t{0u}},
        length_{0u},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR FileEntry::FileEntry(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct FileEntryDefaultTypeInternal {
  PROTOBUF_CONSTEXPR FileEntryDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~FileEntryDefaultTypeInternal() {}
  union {
    FileEntry _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 FileEntryDefaultTypeInternal _FileEntry_default_instance_;

inline constexpr DescriptorIndex::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : files_{},
        symbols_{},
        packages_{},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR DescriptorIndex::DescriptorIndex(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct DescriptorIndexDefaultTypeInternal {
  PROTOBUF_CONSTEXPR DescriptorIndexDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~DescriptorIndexDefaultTypeInternal() {}
  union {
    DescriptorIndex _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 DescriptorIndexDefaultTypeInternal _DescriptorIndex_default_instance_;
}  // namespace DescriptorIndex
namespace DescriptorIndex {
// ===================================================================

class FileEntry::_Internal {
 public:
};

FileEntry::FileEntry(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:DescriptorIndex.FileEntry)
}
inline PROTOBUF_NDEBUG_INLINE FileEntry::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::DescriptorIndex::FileEntry& from_msg)
      : file_name_(arena, from.file_name_),
        _cached_size_{0} {}

FileEntry::FileEntry(
    ::google::protobuf::Arena* arena,
    const FileEntry& from)
    : ::google::protobuf::MessageLite(arena) {
  FileEntry* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  ::memcpy(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, data_offset_),
           reinterpret_cast<const char *>(&from._impl_) +
               offsetof(Impl_, data_offset_),
           offsetof(Impl_, length_) -
               offsetof(Impl_, data_offset_) +
               sizeof(Impl_::length_));

  // @@protoc_insertion_point(copy_constructor:DescriptorIndex.FileEntry)
}
inline PROTOBUF_NDEBUG_INLINE FileEntry::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : file_name_(arena),
        _cached_size_{0} {}

inline void FileEntry::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, data_offset_),
           0,
           offsetof(Impl_, length_) -
               offsetof(Impl_, data_offset_) +
               sizeof(Impl_::length_));
}
FileEntry::~FileEntry() {
  // @@protoc_insertion_point(destructor:DescriptorIndex.FileEntry)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void FileEntry::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.file_name_.Destroy();
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
FileEntry::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<26> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(FileEntry, _impl_._cached_size_),
              true,
          },
          "DescriptorIndex.FileEntry",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 3, 0, 43, 2> FileEntry::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    3, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967288,  // skipmap
    offsetof(decltype(_table_), field_entries),
    3,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    &_FileEntry_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::DescriptorIndex::FileEntry>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // string file_name = 1;
    {::_pbi::TcParser::FastUS1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(FileEntry, _impl_.file_name_)}},
    // uint64 data_offset = 2;
    {::_pbi::TcParser::FastV64S1,
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(FileEntry, _impl_.data_offset_)}},
    // uint32 length = 3;
    {::_pbi::TcParser::FastV32S1,
     {24, 63, 0, PROTOBUF_FIELD_OFFSET(FileEntry, _impl_.length_)}},
  }}, {{
    65535, 65535
  }}, {{
    // string file_name = 1;
    {PROTOBUF_FIELD_OFFSET(FileEntry, _impl_.file_name_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // uint64 data_offset = 2;
    {PROTOBUF_FIELD_OFFSET(FileEntry, _impl_.data_offset_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
    // uint32 length = 3;
    {PROTOBUF_FIELD_OFFSET(FileEntry, _impl_.length_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
    "\31\11\0\0\0\0\0\0"
    "DescriptorIndex.FileEntry"
    "file_name"
  }},
};

PROTOBUF_NOINLINE void FileEntry::Clear() {
// @@protoc_insertion_point(message_clear_start:DescriptorIndex.FileEntry)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.file_name_.ClearToEmpty();
  ::memset(&_impl_.data_offset_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.length_) -
      reinterpret_cast<char*>(&_impl_.data_offset_)) + sizeof(_impl_.length_));
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* FileEntry::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:DescriptorIndex.FileEntry)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // string file_name = 1;
  if (!this->_internal_file_name().empty()) {
    const std::string& _s = this->_internal_file_name();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "DescriptorIndex.FileEntry.file_name");
    target = stream->WriteStringMaybeAliased(1, _s, target);
  }

  // uint64 data_offset = 2;
  if (this->_internal_data_offset() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
        2, this->_internal_data_offset(), target);
  }

  // uint32 length = 3;
  if (this->_internal_length() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        3, this->_internal_length(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:DescriptorIndex.FileEntry)
  return target;
}

::size_t FileEntry::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:DescriptorIndex.FileEntry)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // string file_name = 1;
  if (!this->_internal_file_name().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_file_name());
  }

  // uint64 data_offset = 2;
  if (this->_internal_data_offset() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
        this->_internal_data_offset());
  }

  // uint32 length = 3;
  if (this->_internal_length() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_length());
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void FileEntry::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const FileEntry*>(
      &from));
}

void FileEntry::MergeFrom(const FileEntry& from) {
  FileEntry* const _this = this;
  // @@protoc_insertion_point(class_specific_merge_from_start:DescriptorIndex.FileEntry)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_file_name().empty()) {
    _this->_internal_set_file_name(from._internal_file_name());
  }
  if (from._internal_data_offset() != 0) {
    _this->_impl_.data_offset_ = from._impl_.data_offset_;
  }
  if (from._internal_length() != 0) {
    _this->_impl_.length_ = from._impl_.length_;
  }
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void FileEntry::CopyFrom(const FileEntry& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:DescriptorIndex.FileEntry)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void FileEntry::InternalSwap(FileEntry* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.file_name_, &other->_impl_.file_name_, arena);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(FileEntry, _impl_.length_)
      + sizeof(FileEntry::_impl_.length_)
      - PROTOBUF_FIELD_OFFSET(FileEntry, _impl_.data_offset_)>(
          reinterpret_cast<char*>(&_impl_.data_offset_),
          reinterpret_cast<char*>(&other->_impl_.data_offset_));
}

// ===================================================================

class Symbol::_Internal {
 public:
};

Symbol::Symbol(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:DescriptorIndex.Symbol)
}
inline PROTOBUF_NDEBUG_INLINE Symbol::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::DescriptorIndex::Symbol& from_msg)
      : full_name_(arena, from.full_name_),
        _cached_size_{0} {}

Symbol::Symbol(
    ::google::protobuf::Arena* arena,
    const Symbol& from)
    : ::google::protobuf::MessageLite(arena) {
  Symbol* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  _impl_.file_index_ = from._impl_.file_index_;

  // @@protoc_insertion_point(copy_constructor:DescriptorIndex.Symbol)
}
inline PROTOBUF_NDEBUG_INLINE Symbol::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : full_name_(arena),
        _cached_size_{0} {}

inline void Symbol::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.file_index_ = {};
}
Symbol::~Symbol() {
  // @@protoc_insertion_point(destructor:DescriptorIndex.Symbol)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void Symbol::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.full_name_.Destroy();
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
Symbol::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<23> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(Symbol, _impl_._cached_size_),
              true,
          },
          "DescriptorIndex.Symbol",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 0, 40, 2> Symbol::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    2, 8,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967292,  // skipmap
    offsetof(decltype(_table_), field_entries),
    2,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    &_Symbol_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::DescriptorIndex::Symbol>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // uint32 file_index = 2;
    {::_pbi::TcParser::FastV32S1,
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(Symbol, _impl_.file_index_)}},
    // string full_name = 1;
    {::_pbi::TcParser::FastUS1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(Symbol, _impl_.full_name_)}},
  }}, {{
    65535, 65535
  }}, {{
    // string full_name = 1;
    {PROTOBUF_FIELD_OFFSET(Symbol, _impl_.full_name_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // uint32 file_index = 2;
    {PROTOBUF_FIELD_OFFSET(Symbol, _impl_.file_index_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
  }},
  // no aux_entries
  {{
    "\26\11\0\0\0\0\0\0"
    "DescriptorIndex.Symbol"
    "full_name"
  }},
};

PROTOBUF_NOINLINE void Symbol::Clear() {
// @@protoc_insertion_point(message_clear_start:DescriptorIndex.Symbol)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.full_name_.ClearToEmpty();
  _impl_.file_index_ = 0u;
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* Symbol::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:DescriptorIndex.Symbol)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // string full_name = 1;
  if (!this->_internal_full_name().empty()) {
    const std::string& _s = this->_internal_full_name();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "DescriptorIndex.Symbol.full_name");
    target = stream->WriteStringMaybeAliased(1, _s, target);
  }

  // uint32 file_index = 2;
  if (this->_internal_file_index() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        2, this->_internal_file_index(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:DescriptorIndex.Symbol)
  return target;
}

::size_t Symbol::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:DescriptorIndex.Symbol)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // string full_name = 1;
  if (!this->_internal_full_name().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_full_name());
  }

  // uint32 file_index = 2;
  if (this->_internal_file_index() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_file_index());
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void Symbol::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const Symbol*>(
      &from));
}

void Symbol::MergeFrom(const Symbol& from) {
  Symbol* const _this = this;
  // @@protoc_insertion_point(class_specific_merge_from_start:DescriptorIndex.Symbol)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_full_name().empty()) {
    _this->_internal_set_full_name(from._internal_full_name());
  }
  if (from._internal_file_index() != 0) {
    _this->_impl_.file_index_ = from._impl_.file_index_;
  }
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void Symbol::CopyFrom(const Symbol& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:DescriptorIndex.Symbol)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void Symbol::InternalSwap(Symbol* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.full_name_, &other->_impl_.full_name_, arena);
        swap(_impl_.file_index_, other->_impl_.file_index_);
}

// ===================================================================

class Package::_Internal {
 public:
};

Package::Package(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:DescriptorIndex.Package)
}
inline PROTOBUF_NDEBUG_INLINE Package::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::DescriptorIndex::Package& from_msg)
      : symbol_indexes_{visibility, arena, from.symbol_indexes_},
        _symbol_indexes_cached_byte_size_{0},
        nested_package_indexes_{visibility, arena, from.nested_package_indexes_},
        _nested_package_indexes_cached_byte_size_{0},
        full_name_(arena, from.full_name_),
        _cached_size_{0} {}

Package::Package(
    ::google::protobuf::Arena* arena,
    const Package& from)
    : ::google::protobuf::MessageLite(arena) {
  Package* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:DescriptorIndex.Package)
}
inline PROTOBUF_NDEBUG_INLINE Package::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : symbol_indexes_{visibility, arena},
        _symbol_indexes_cached_byte_size_{0},
        nested_package_indexes_{visibility, arena},
        _nested_package_indexes_cached_byte_size_{0},
        full_name_(arena),
        _cached_size_{0} {}

inline void Package::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
Package::~Package() {
  // @@protoc_insertion_point(destructor:DescriptorIndex.Package)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void Package::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.full_name_.Destroy();
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
Package::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<24> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(Package, _impl_._cached_size_),
              true,
          },
          "DescriptorIndex.Package",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 3, 0, 41, 2> Package::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    3, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967288,  // skipmap
    offsetof(decltype(_table_), field_entries),
    3,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    &_Package_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::DescriptorIndex::Package>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // string full_name = 1;
    {::_pbi::TcParser::FastUS1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(Package, _impl_.full_name_)}},
    // repeated uint32 symbol_indexes = 2;
    {::_pbi::TcParser::FastV32P1,
     {18, 63, 0, PROTOBUF_FIELD_OFFSET(Package, _impl_.symbol_indexes_)}},
    // repeated uint32 nested_package_indexes = 3;
    {::_pbi::TcParser::FastV32P1,
     {26, 63, 0, PROTOBUF_FIELD_OFFSET(Package, _impl_.nested_package_indexes_)}},
  }}, {{
    65535, 65535
  }}, {{
    // string full_name = 1;
    {PROTOBUF_FIELD_OFFSET(Package, _impl_.full_name_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // repeated uint32 symbol_indexes = 2;
    {PROTOBUF_FIELD_OFFSET(Package, _impl_.symbol_indexes_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedUInt32)},
    // repeated uint32 nested_package_indexes = 3;
    {PROTOBUF_FIELD_OFFSET(Package, _impl_.nested_package_indexes_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedUInt32)},
  }},
  // no aux_entries
  {{
    "\27\11\0\0\0\0\0\0"
    "DescriptorIndex.Package"
    "full_name"
  }},
};

PROTOBUF_NOINLINE void Package::Clear() {
// @@protoc_insertion_point(message_clear_start:DescriptorIndex.Package)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.symbol_indexes_.Clear();
  _impl_.nested_package_indexes_.Clear();
  _impl_.full_name_.ClearToEmpty();
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* Package::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:DescriptorIndex.Package)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // string full_name = 1;
  if (!this->_internal_full_name().empty()) {
    const std::string& _s = this->_internal_full_name();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "DescriptorIndex.Package.full_name");
    target = stream->WriteStringMaybeAliased(1, _s, target);
  }

  // repeated uint32 symbol_indexes = 2;
  {
    int byte_size = _impl_._symbol_indexes_cached_byte_size_.Get();
    if (byte_size > 0) {
      target = stream->WriteUInt32Packed(
          2, _internal_symbol_indexes(), byte_size, target);
    }
  }

  // repeated uint32 nested_package_indexes = 3;
  {
    int byte_size = _impl_._nested_package_indexes_cached_byte_size_.Get();
    if (byte_size > 0) {
      target = stream->WriteUInt32Packed(
          3, _internal_nested_package_indexes(), byte_size, target);
    }
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:DescriptorIndex.Package)
  return target;
}

::size_t Package::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:DescriptorIndex.Package)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // repeated uint32 symbol_indexes = 2;
  {
    std::size_t data_size = ::_pbi::WireFormatLite::UInt32Size(
        this->_internal_symbol_indexes())
    ;
    _impl_._symbol_indexes_cached_byte_size_.Set(::_pbi::ToCachedSize(data_size));
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // repeated uint32 nested_package_indexes = 3;
  {
    std::size_t data_size = ::_pbi::WireFormatLite::UInt32Size(
        this->_internal_nested_package_indexes())
    ;
    _impl_._nested_package_indexes_cached_byte_size_.Set(::_pbi::ToCachedSize(data_size));
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // string full_name = 1;
  if (!this->_internal_full_name().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_full_name());
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void Package::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const Package*>(
      &from));
}

void Package::MergeFrom(const Package& from) {
  Package* const _this = this;
  // @@protoc_insertion_point(class_specific_merge_from_start:DescriptorIndex.Package)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_symbol_indexes()->MergeFrom(from._internal_symbol_indexes());
  _this->_internal_mutable_nested_package_indexes()->MergeFrom(from._internal_nested_package_indexes());
  if (!from._internal_full_name().empty()) {
    _this->_internal_set_full_name(from._internal_full_name());
  }
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void Package::CopyFrom(const Package& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:DescriptorIndex.Package)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void Package::InternalSwap(Package* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.symbol_indexes_.InternalSwap(&other->_impl_.symbol_indexes_);
  _impl_.nested_package_indexes_.InternalSwap(&other->_impl_.nested_package_indexes_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.full_name_, &other->_impl_.full_name_, arena);
}

// ===================================================================

class DescriptorIndex::_Internal {
 public:
};

DescriptorIndex::DescriptorIndex(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:DescriptorIndex.DescriptorIndex)
}
inline PROTOBUF_NDEBUG_INLINE DescriptorIndex::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::DescriptorIndex::DescriptorIndex& from_msg)
      : files_{visibility, arena, from.files_},
        symbols_{visibility, arena, from.symbols_},
        packages_{visibility, arena, from.packages_},
        _cached_size_{0} {}

DescriptorIndex::DescriptorIndex(
    ::google::protobuf::Arena* arena,
    const DescriptorIndex& from)
    : ::google::protobuf::MessageLite(arena) {
  DescriptorIndex* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:DescriptorIndex.DescriptorIndex)
}
inline PROTOBUF_NDEBUG_INLINE DescriptorIndex::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : files_{visibility, arena},
        symbols_{visibility, arena},
        packages_{visibility, arena},
        _cached_size_{0} {}

inline void DescriptorIndex::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
DescriptorIndex::~DescriptorIndex() {
  // @@protoc_insertion_point(destructor:DescriptorIndex.DescriptorIndex)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void DescriptorIndex::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
DescriptorIndex::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<32> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(DescriptorIndex, _impl_._cached_size_),
              true,
          },
          "DescriptorIndex.DescriptorIndex",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 3, 3, 0, 2> DescriptorIndex::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    3, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967288,  // skipmap
    offsetof(decltype(_table_), field_entries),
    3,  // num_field_entries
    3,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_DescriptorIndex_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::DescriptorIndex::DescriptorIndex>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // repeated .DescriptorIndex.FileEntry files = 1;
    {::_pbi::TcParser::FastMtR1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(DescriptorIndex, _impl_.files_)}},
    // repeated .DescriptorIndex.Symbol symbols = 2;
    {::_pbi::TcParser::FastMtR1,
     {18, 63, 1, PROTOBUF_FIELD_OFFSET(DescriptorIndex, _impl_.symbols_)}},
    // repeated .DescriptorIndex.Package packages = 3;
    {::_pbi::TcParser::FastMtR1,
     {26, 63, 2, PROTOBUF_FIELD_OFFSET(DescriptorIndex, _impl_.packages_)}},
  }}, {{
    65535, 65535
  }}, {{
    // repeated .DescriptorIndex.FileEntry files = 1;
    {PROTOBUF_FIELD_OFFSET(DescriptorIndex, _impl_.files_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
    // repeated .DescriptorIndex.Symbol symbols = 2;
    {PROTOBUF_FIELD_OFFSET(DescriptorIndex, _impl_.symbols_), 0, 1,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
    // repeated .DescriptorIndex.Package packages = 3;
    {PROTOBUF_FIELD_OFFSET(DescriptorIndex, _impl_.packages_), 0, 2,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::DescriptorIndex::FileEntry>()},
    {::_pbi::TcParser::GetTable<::DescriptorIndex::Symbol>()},
    {::_pbi::TcParser::GetTable<::DescriptorIndex::Package>()},
  }}, {{
  }},
};

PROTOBUF_NOINLINE void DescriptorIndex::Clear() {
// @@protoc_insertion_point(message_clear_start:DescriptorIndex.DescriptorIndex)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.files_.Clear();
  _impl_.symbols_.Clear();
  _impl_.packages_.Clear();
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* DescriptorIndex::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:DescriptorIndex.DescriptorIndex)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // repeated .DescriptorIndex.FileEntry files = 1;
  for (unsigned i = 0, n = static_cast<unsigned>(
                           this->_internal_files_size());
       i < n; i++) {
    const auto& repfield = this->_internal_files().Get(i);
    target =
        ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
            1, repfield, repfield.GetCachedSize(),
            target, stream);
  }

  // repeated .DescriptorIndex.Symbol symbols = 2;
  for (unsigned i = 0, n = static_cast<unsigned>(
                           this->_internal_symbols_size());
       i < n; i++) {
    const auto& repfield = this->_internal_symbols().Get(i);
    target =
        ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
            2, repfield, repfield.GetCachedSize(),
            target, stream);
  }

  // repeated .DescriptorIndex.Package packages = 3;
  for (unsigned i = 0, n = static_cast<unsigned>(
                           this->_internal_packages_size());
       i < n; i++) {
    const auto& repfield = this->_internal_packages().Get(i);
    target =
        ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
            3, repfield, repfield.GetCachedSize(),
            target, stream);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:DescriptorIndex.DescriptorIndex)
  return target;
}

::size_t DescriptorIndex::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:DescriptorIndex.DescriptorIndex)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // repeated .DescriptorIndex.FileEntry files = 1;
  total_size += 1UL * this->_internal_files_size();
  for (const auto& msg : this->_internal_files()) {
    total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  // repeated .DescriptorIndex.Symbol symbols = 2;
  total_size += 1UL * this->_internal_symbols_size();
  for (const auto& msg : this->_internal_symbols()) {
    total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  // repeated .DescriptorIndex.Package packages = 3;
  total_size += 1UL * this->_internal_packages_size();
  for (const auto& msg : this->_internal_packages()) {
    total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void DescriptorIndex::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const DescriptorIndex*>(
      &from));
}

void DescriptorIndex::MergeFrom(const DescriptorIndex& from) {
  DescriptorIndex* const _this = this;
  // @@protoc_insertion_point(class_specific_merge_from_start:DescriptorIndex.DescriptorIndex)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_files()->MergeFrom(
      from._internal_files());
  _this->_internal_mutable_symbols()->MergeFrom(
      from._internal_symbols());
  _this->_internal_mutable_packages()->MergeFrom(
      from._internal_packages());
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void DescriptorIndex::CopyFrom(const DescriptorIndex& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:DescriptorIndex.DescriptorIndex)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void DescriptorIndex::InternalSwap(DescriptorIndex* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.files_.InternalSwap(&other->_impl_.files_);
  _impl_.symbols_.InternalSwap(&other->_impl_.symbols_);
  _impl_.packages_.InternalSwap(&other->_impl_.packages_);
}

// @@protoc_insertion_point(namespace_scope)
}  // namespace DescriptorIndex
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
#include "google/protobuf/port_undef.inc"
