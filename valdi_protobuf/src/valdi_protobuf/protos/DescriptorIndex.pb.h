// Generate by the Snap-modified protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: DescriptorIndex.proto
// Protobuf C++ Version: 5.27.0

#ifndef GOOGLE_PROTOBUF_INCLUDED_DescriptorIndex_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_DescriptorIndex_2eproto_2epb_2eh

#include <limits>
#include <string>
#include <type_traits>
#include <utility>

#include "google/protobuf/runtime_version.h"
#if PROTOBUF_VERSION != 5027000
#error "Protobuf C++ gencode is built with an incompatible version of"
#error "Protobuf C++ headers/runtime. See"
#error "https://protobuf.dev/support/cross-version-runtime-guarantee/#cpp"
#endif
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/arenastring.h"
#include "google/protobuf/generated_message_tctable_decl.h"
#include "google/protobuf/generated_message_util.h"
#include "google/protobuf/metadata_lite.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/repeated_field.h"  // IWYU pragma: export
#include "google/protobuf/extension_set.h"  // IWYU pragma: export
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_DescriptorIndex_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_DescriptorIndex_2eproto {
  static const ::uint32_t offsets[];
};
namespace DescriptorIndex {
class DescriptorIndex;
struct DescriptorIndexDefaultTypeInternal;
extern DescriptorIndexDefaultTypeInternal _DescriptorIndex_default_instance_;
class FileEntry;
struct FileEntryDefaultTypeInternal;
extern FileEntryDefaultTypeInternal _FileEntry_default_instance_;
class Package;
struct PackageDefaultTypeInternal;
extern PackageDefaultTypeInternal _Package_default_instance_;
class Symbol;
struct SymbolDefaultTypeInternal;
extern SymbolDefaultTypeInternal _Symbol_default_instance_;
}  // namespace DescriptorIndex
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google

namespace DescriptorIndex {

// ===================================================================


// -------------------------------------------------------------------

class Symbol final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:DescriptorIndex.Symbol) */ {
 public:
  inline Symbol() : Symbol(nullptr) {}
  ~Symbol() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR Symbol(
      ::google::protobuf::internal::ConstantInitialized);

  inline Symbol(const Symbol& from) : Symbol(nullptr, from) {}
  inline Symbol(Symbol&& from) noexcept
      : Symbol(nullptr, std::move(from)) {}
  inline Symbol& operator=(const Symbol& from) {
    CopyFrom(from);
    return *this;
  }
  inline Symbol& operator=(Symbol&& from) noexcept {
    if (this == &from) return *this;
    if (GetArena() == from.GetArena()
#ifdef PROTOBUF_FORCE_COPY_IN_MOVE
        && GetArena() != nullptr
#endif  // !PROTOBUF_FORCE_COPY_IN_MOVE
    ) {
      InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }

  inline const std::string& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString);
  }
  inline std::string* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<std::string>();
  }

  static const Symbol& default_instance() {
    return *internal_default_instance();
  }
  static inline const Symbol* internal_default_instance() {
    return reinterpret_cast<const Symbol*>(
        &_Symbol_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(Symbol& a, Symbol& b) { a.Swap(&b); }
  inline void Swap(Symbol* other) {
    if (other == this) return;
#ifdef PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetArena() != nullptr && GetArena() == other->GetArena()) {
#else   // PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetArena() == other->GetArena()) {
#endif  // !PROTOBUF_FORCE_COPY_IN_SWAP
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(Symbol* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  Symbol* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<Symbol>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const Symbol& from);
  void MergeFrom(const Symbol& from);
  bool IsInitialized() const {
    return true;
  }
  ABSL_ATTRIBUTE_REINITIALIZES void Clear() final;
  ::size_t ByteSizeLong() const final;
  ::uint8_t* _InternalSerialize(
      ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream) const final;
  int GetCachedSize() const { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::google::protobuf::Arena* arena);
  void SharedDtor();
  void InternalSwap(Symbol* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "DescriptorIndex.Symbol"; }

 protected:
  explicit Symbol(::google::protobuf::Arena* arena);
  Symbol(::google::protobuf::Arena* arena, const Symbol& from);
  Symbol(::google::protobuf::Arena* arena, Symbol&& from) noexcept
      : Symbol(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kFullNameFieldNumber = 1,
    kFileIndexFieldNumber = 2,
  };
  // string full_name = 1;
  void clear_full_name() ;
  const std::string& full_name() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_full_name(Arg_&& arg, Args_... args);
  std::string* mutable_full_name();
  PROTOBUF_NODISCARD std::string* release_full_name();
  void set_allocated_full_name(std::string* value);

  private:
  const std::string& _internal_full_name() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_full_name(
      const std::string& value);
  std::string* _internal_mutable_full_name();

  public:
  // uint32 file_index = 2;
  void clear_file_index() ;
  ::uint32_t file_index() const;
  void set_file_index(::uint32_t value);

  private:
  ::uint32_t _internal_file_index() const;
  void _internal_set_file_index(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:DescriptorIndex.Symbol)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      1, 2, 0,
      40, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_Symbol_default_instance_;

  friend class ::google::protobuf::MessageLite;
  friend class ::google::protobuf::Arena;
  template <typename T>
  friend class ::google::protobuf::Arena::InternalHelper;
  using InternalArenaConstructable_ = void;
  using DestructorSkippable_ = void;
  struct Impl_ {
    inline explicit constexpr Impl_(
        ::google::protobuf::internal::ConstantInitialized) noexcept;
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena);
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena, const Impl_& from,
                          const Symbol& from_msg);
    ::google::protobuf::internal::ArenaStringPtr full_name_;
    ::uint32_t file_index_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_DescriptorIndex_2eproto;
};
// -------------------------------------------------------------------

class Package final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:DescriptorIndex.Package) */ {
 public:
  inline Package() : Package(nullptr) {}
  ~Package() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR Package(
      ::google::protobuf::internal::ConstantInitialized);

  inline Package(const Package& from) : Package(nullptr, from) {}
  inline Package(Package&& from) noexcept
      : Package(nullptr, std::move(from)) {}
  inline Package& operator=(const Package& from) {
    CopyFrom(from);
    return *this;
  }
  inline Package& operator=(Package&& from) noexcept {
    if (this == &from) return *this;
    if (GetArena() == from.GetArena()
#ifdef PROTOBUF_FORCE_COPY_IN_MOVE
        && GetArena() != nullptr
#endif  // !PROTOBUF_FORCE_COPY_IN_MOVE
    ) {
      InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }

  inline const std::string& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString);
  }
  inline std::string* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<std::string>();
  }

  static const Package& default_instance() {
    return *internal_default_instance();
  }
  static inline const Package* internal_default_instance() {
    return reinterpret_cast<const Package*>(
        &_Package_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 2;
  friend void swap(Package& a, Package& b) { a.Swap(&b); }
  inline void Swap(Package* other) {
    if (other == this) return;
#ifdef PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetArena() != nullptr && GetArena() == other->GetArena()) {
#else   // PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetArena() == other->GetArena()) {
#endif  // !PROTOBUF_FORCE_COPY_IN_SWAP
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(Package* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  Package* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<Package>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const Package& from);
  void MergeFrom(const Package& from);
  bool IsInitialized() const {
    return true;
  }
  ABSL_ATTRIBUTE_REINITIALIZES void Clear() final;
  ::size_t ByteSizeLong() const final;
  ::uint8_t* _InternalSerialize(
      ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream) const final;
  int GetCachedSize() const { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::google::protobuf::Arena* arena);
  void SharedDtor();
  void InternalSwap(Package* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "DescriptorIndex.Package"; }

 protected:
  explicit Package(::google::protobuf::Arena* arena);
  Package(::google::protobuf::Arena* arena, const Package& from);
  Package(::google::protobuf::Arena* arena, Package&& from) noexcept
      : Package(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kSymbolIndexesFieldNumber = 2,
    kNestedPackageIndexesFieldNumber = 3,
    kFullNameFieldNumber = 1,
  };
  // repeated uint32 symbol_indexes = 2;
  int symbol_indexes_size() const;
  private:
  int _internal_symbol_indexes_size() const;

  public:
  void clear_symbol_indexes() ;
  ::uint32_t symbol_indexes(int index) const;
  void set_symbol_indexes(int index, ::uint32_t value);
  void add_symbol_indexes(::uint32_t value);
  const ::google::protobuf::RepeatedField<::uint32_t>& symbol_indexes() const;
  ::google::protobuf::RepeatedField<::uint32_t>* mutable_symbol_indexes();

  private:
  const ::google::protobuf::RepeatedField<::uint32_t>& _internal_symbol_indexes() const;
  ::google::protobuf::RepeatedField<::uint32_t>* _internal_mutable_symbol_indexes();

  public:
  // repeated uint32 nested_package_indexes = 3;
  int nested_package_indexes_size() const;
  private:
  int _internal_nested_package_indexes_size() const;

  public:
  void clear_nested_package_indexes() ;
  ::uint32_t nested_package_indexes(int index) const;
  void set_nested_package_indexes(int index, ::uint32_t value);
  void add_nested_package_indexes(::uint32_t value);
  const ::google::protobuf::RepeatedField<::uint32_t>& nested_package_indexes() const;
  ::google::protobuf::RepeatedField<::uint32_t>* mutable_nested_package_indexes();

  private:
  const ::google::protobuf::RepeatedField<::uint32_t>& _internal_nested_package_indexes() const;
  ::google::protobuf::RepeatedField<::uint32_t>* _internal_mutable_nested_package_indexes();

  public:
  // string full_name = 1;
  void clear_full_name() ;
  const std::string& full_name() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_full_name(Arg_&& arg, Args_... args);
  std::string* mutable_full_name();
  PROTOBUF_NODISCARD std::string* release_full_name();
  void set_allocated_full_name(std::string* value);

  private:
  const std::string& _internal_full_name() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_full_name(
      const std::string& value);
  std::string* _internal_mutable_full_name();

  public:
  // @@protoc_insertion_point(class_scope:DescriptorIndex.Package)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      2, 3, 0,
      41, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_Package_default_instance_;

  friend class ::google::protobuf::MessageLite;
  friend class ::google::protobuf::Arena;
  template <typename T>
  friend class ::google::protobuf::Arena::InternalHelper;
  using InternalArenaConstructable_ = void;
  using DestructorSkippable_ = void;
  struct Impl_ {
    inline explicit constexpr Impl_(
        ::google::protobuf::internal::ConstantInitialized) noexcept;
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena);
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena, const Impl_& from,
                          const Package& from_msg);
    ::google::protobuf::RepeatedField<::uint32_t> symbol_indexes_;
    mutable ::google::protobuf::internal::CachedSize _symbol_indexes_cached_byte_size_;
    ::google::protobuf::RepeatedField<::uint32_t> nested_package_indexes_;
    mutable ::google::protobuf::internal::CachedSize _nested_package_indexes_cached_byte_size_;
    ::google::protobuf::internal::ArenaStringPtr full_name_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_DescriptorIndex_2eproto;
};
// -------------------------------------------------------------------

class FileEntry final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:DescriptorIndex.FileEntry) */ {
 public:
  inline FileEntry() : FileEntry(nullptr) {}
  ~FileEntry() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR FileEntry(
      ::google::protobuf::internal::ConstantInitialized);

  inline FileEntry(const FileEntry& from) : FileEntry(nullptr, from) {}
  inline FileEntry(FileEntry&& from) noexcept
      : FileEntry(nullptr, std::move(from)) {}
  inline FileEntry& operator=(const FileEntry& from) {
    CopyFrom(from);
    return *this;
  }
  inline FileEntry& operator=(FileEntry&& from) noexcept {
    if (this == &from) return *this;
    if (GetArena() == from.GetArena()
#ifdef PROTOBUF_FORCE_COPY_IN_MOVE
        && GetArena() != nullptr
#endif  // !PROTOBUF_FORCE_COPY_IN_MOVE
    ) {
      InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }

  inline const std::string& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString);
  }
  inline std::string* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<std::string>();
  }

  static const FileEntry& default_instance() {
    return *internal_default_instance();
  }
  static inline const FileEntry* internal_default_instance() {
    return reinterpret_cast<const FileEntry*>(
        &_FileEntry_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(FileEntry& a, FileEntry& b) { a.Swap(&b); }
  inline void Swap(FileEntry* other) {
    if (other == this) return;
#ifdef PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetArena() != nullptr && GetArena() == other->GetArena()) {
#else   // PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetArena() == other->GetArena()) {
#endif  // !PROTOBUF_FORCE_COPY_IN_SWAP
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(FileEntry* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  FileEntry* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<FileEntry>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const FileEntry& from);
  void MergeFrom(const FileEntry& from);
  bool IsInitialized() const {
    return true;
  }
  ABSL_ATTRIBUTE_REINITIALIZES void Clear() final;
  ::size_t ByteSizeLong() const final;
  ::uint8_t* _InternalSerialize(
      ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream) const final;
  int GetCachedSize() const { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::google::protobuf::Arena* arena);
  void SharedDtor();
  void InternalSwap(FileEntry* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "DescriptorIndex.FileEntry"; }

 protected:
  explicit FileEntry(::google::protobuf::Arena* arena);
  FileEntry(::google::protobuf::Arena* arena, const FileEntry& from);
  FileEntry(::google::protobuf::Arena* arena, FileEntry&& from) noexcept
      : FileEntry(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kFileNameFieldNumber = 1,
    kDataOffsetFieldNumber = 2,
    kLengthFieldNumber = 3,
  };
  // string file_name = 1;
  void clear_file_name() ;
  const std::string& file_name() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_file_name(Arg_&& arg, Args_... args);
  std::string* mutable_file_name();
  PROTOBUF_NODISCARD std::string* release_file_name();
  void set_allocated_file_name(std::string* value);

  private:
  const std::string& _internal_file_name() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_file_name(
      const std::string& value);
  std::string* _internal_mutable_file_name();

  public:
  // uint64 data_offset = 2;
  void clear_data_offset() ;
  ::uint64_t data_offset() const;
  void set_data_offset(::uint64_t value);

  private:
  ::uint64_t _internal_data_offset() const;
  void _internal_set_data_offset(::uint64_t value);

  public:
  // uint32 length = 3;
  void clear_length() ;
  ::uint32_t length() const;
  void set_length(::uint32_t value);

  private:
  ::uint32_t _internal_length() const;
  void _internal_set_length(::uint32_t value);

  public:
  // @@protoc_insertion_point(class_scope:DescriptorIndex.FileEntry)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      2, 3, 0,
      43, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_FileEntry_default_instance_;

  friend class ::google::protobuf::MessageLite;
  friend class ::google::protobuf::Arena;
  template <typename T>
  friend class ::google::protobuf::Arena::InternalHelper;
  using InternalArenaConstructable_ = void;
  using DestructorSkippable_ = void;
  struct Impl_ {
    inline explicit constexpr Impl_(
        ::google::protobuf::internal::ConstantInitialized) noexcept;
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena);
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena, const Impl_& from,
                          const FileEntry& from_msg);
    ::google::protobuf::internal::ArenaStringPtr file_name_;
    ::uint64_t data_offset_;
    ::uint32_t length_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_DescriptorIndex_2eproto;
};
// -------------------------------------------------------------------

class DescriptorIndex final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:DescriptorIndex.DescriptorIndex) */ {
 public:
  inline DescriptorIndex() : DescriptorIndex(nullptr) {}
  ~DescriptorIndex() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR DescriptorIndex(
      ::google::protobuf::internal::ConstantInitialized);

  inline DescriptorIndex(const DescriptorIndex& from) : DescriptorIndex(nullptr, from) {}
  inline DescriptorIndex(DescriptorIndex&& from) noexcept
      : DescriptorIndex(nullptr, std::move(from)) {}
  inline DescriptorIndex& operator=(const DescriptorIndex& from) {
    CopyFrom(from);
    return *this;
  }
  inline DescriptorIndex& operator=(DescriptorIndex&& from) noexcept {
    if (this == &from) return *this;
    if (GetArena() == from.GetArena()
#ifdef PROTOBUF_FORCE_COPY_IN_MOVE
        && GetArena() != nullptr
#endif  // !PROTOBUF_FORCE_COPY_IN_MOVE
    ) {
      InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }

  inline const std::string& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString);
  }
  inline std::string* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<std::string>();
  }

  static const DescriptorIndex& default_instance() {
    return *internal_default_instance();
  }
  static inline const DescriptorIndex* internal_default_instance() {
    return reinterpret_cast<const DescriptorIndex*>(
        &_DescriptorIndex_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 3;
  friend void swap(DescriptorIndex& a, DescriptorIndex& b) { a.Swap(&b); }
  inline void Swap(DescriptorIndex* other) {
    if (other == this) return;
#ifdef PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetArena() != nullptr && GetArena() == other->GetArena()) {
#else   // PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetArena() == other->GetArena()) {
#endif  // !PROTOBUF_FORCE_COPY_IN_SWAP
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(DescriptorIndex* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  DescriptorIndex* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<DescriptorIndex>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const DescriptorIndex& from);
  void MergeFrom(const DescriptorIndex& from);
  bool IsInitialized() const {
    return true;
  }
  ABSL_ATTRIBUTE_REINITIALIZES void Clear() final;
  ::size_t ByteSizeLong() const final;
  ::uint8_t* _InternalSerialize(
      ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream) const final;
  int GetCachedSize() const { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::google::protobuf::Arena* arena);
  void SharedDtor();
  void InternalSwap(DescriptorIndex* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "DescriptorIndex.DescriptorIndex"; }

 protected:
  explicit DescriptorIndex(::google::protobuf::Arena* arena);
  DescriptorIndex(::google::protobuf::Arena* arena, const DescriptorIndex& from);
  DescriptorIndex(::google::protobuf::Arena* arena, DescriptorIndex&& from) noexcept
      : DescriptorIndex(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kFilesFieldNumber = 1,
    kSymbolsFieldNumber = 2,
    kPackagesFieldNumber = 3,
  };
  // repeated .DescriptorIndex.FileEntry files = 1;
  int files_size() const;
  private:
  int _internal_files_size() const;

  public:
  void clear_files() ;
  ::DescriptorIndex::FileEntry* mutable_files(int index);
  ::google::protobuf::RepeatedPtrField<::DescriptorIndex::FileEntry>* mutable_files();

  private:
  const ::google::protobuf::RepeatedPtrField<::DescriptorIndex::FileEntry>& _internal_files() const;
  ::google::protobuf::RepeatedPtrField<::DescriptorIndex::FileEntry>* _internal_mutable_files();
  public:
  const ::DescriptorIndex::FileEntry& files(int index) const;
  ::DescriptorIndex::FileEntry* add_files();
  const ::google::protobuf::RepeatedPtrField<::DescriptorIndex::FileEntry>& files() const;
  // repeated .DescriptorIndex.Symbol symbols = 2;
  int symbols_size() const;
  private:
  int _internal_symbols_size() const;

  public:
  void clear_symbols() ;
  ::DescriptorIndex::Symbol* mutable_symbols(int index);
  ::google::protobuf::RepeatedPtrField<::DescriptorIndex::Symbol>* mutable_symbols();

  private:
  const ::google::protobuf::RepeatedPtrField<::DescriptorIndex::Symbol>& _internal_symbols() const;
  ::google::protobuf::RepeatedPtrField<::DescriptorIndex::Symbol>* _internal_mutable_symbols();
  public:
  const ::DescriptorIndex::Symbol& symbols(int index) const;
  ::DescriptorIndex::Symbol* add_symbols();
  const ::google::protobuf::RepeatedPtrField<::DescriptorIndex::Symbol>& symbols() const;
  // repeated .DescriptorIndex.Package packages = 3;
  int packages_size() const;
  private:
  int _internal_packages_size() const;

  public:
  void clear_packages() ;
  ::DescriptorIndex::Package* mutable_packages(int index);
  ::google::protobuf::RepeatedPtrField<::DescriptorIndex::Package>* mutable_packages();

  private:
  const ::google::protobuf::RepeatedPtrField<::DescriptorIndex::Package>& _internal_packages() const;
  ::google::protobuf::RepeatedPtrField<::DescriptorIndex::Package>* _internal_mutable_packages();
  public:
  const ::DescriptorIndex::Package& packages(int index) const;
  ::DescriptorIndex::Package* add_packages();
  const ::google::protobuf::RepeatedPtrField<::DescriptorIndex::Package>& packages() const;
  // @@protoc_insertion_point(class_scope:DescriptorIndex.DescriptorIndex)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      2, 3, 3,
      0, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_DescriptorIndex_default_instance_;

  friend class ::google::protobuf::MessageLite;
  friend class ::google::protobuf::Arena;
  template <typename T>
  friend class ::google::protobuf::Arena::InternalHelper;
  using InternalArenaConstructable_ = void;
  using DestructorSkippable_ = void;
  struct Impl_ {
    inline explicit constexpr Impl_(
        ::google::protobuf::internal::ConstantInitialized) noexcept;
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena);
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena, const Impl_& from,
                          const DescriptorIndex& from_msg);
    ::google::protobuf::RepeatedPtrField< ::DescriptorIndex::FileEntry > files_;
    ::google::protobuf::RepeatedPtrField< ::DescriptorIndex::Symbol > symbols_;
    ::google::protobuf::RepeatedPtrField< ::DescriptorIndex::Package > packages_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_DescriptorIndex_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// FileEntry

// string file_name = 1;
inline void FileEntry::clear_file_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.file_name_.ClearToEmpty();
}
inline const std::string& FileEntry::file_name() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:DescriptorIndex.FileEntry.file_name)
  return _internal_file_name();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void FileEntry::set_file_name(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.file_name_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:DescriptorIndex.FileEntry.file_name)
}
inline std::string* FileEntry::mutable_file_name() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_file_name();
  // @@protoc_insertion_point(field_mutable:DescriptorIndex.FileEntry.file_name)
  return _s;
}
inline const std::string& FileEntry::_internal_file_name() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.file_name_.Get();
}
inline void FileEntry::_internal_set_file_name(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.file_name_.Set(value, GetArena());
}
inline std::string* FileEntry::_internal_mutable_file_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.file_name_.Mutable( GetArena());
}
inline std::string* FileEntry::release_file_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:DescriptorIndex.FileEntry.file_name)
  return _impl_.file_name_.Release();
}
inline void FileEntry::set_allocated_file_name(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.file_name_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.file_name_.IsDefault()) {
          _impl_.file_name_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:DescriptorIndex.FileEntry.file_name)
}

// uint64 data_offset = 2;
inline void FileEntry::clear_data_offset() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.data_offset_ = ::uint64_t{0u};
}
inline ::uint64_t FileEntry::data_offset() const {
  // @@protoc_insertion_point(field_get:DescriptorIndex.FileEntry.data_offset)
  return _internal_data_offset();
}
inline void FileEntry::set_data_offset(::uint64_t value) {
  _internal_set_data_offset(value);
  // @@protoc_insertion_point(field_set:DescriptorIndex.FileEntry.data_offset)
}
inline ::uint64_t FileEntry::_internal_data_offset() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.data_offset_;
}
inline void FileEntry::_internal_set_data_offset(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.data_offset_ = value;
}

// uint32 length = 3;
inline void FileEntry::clear_length() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.length_ = 0u;
}
inline ::uint32_t FileEntry::length() const {
  // @@protoc_insertion_point(field_get:DescriptorIndex.FileEntry.length)
  return _internal_length();
}
inline void FileEntry::set_length(::uint32_t value) {
  _internal_set_length(value);
  // @@protoc_insertion_point(field_set:DescriptorIndex.FileEntry.length)
}
inline ::uint32_t FileEntry::_internal_length() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.length_;
}
inline void FileEntry::_internal_set_length(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.length_ = value;
}

// -------------------------------------------------------------------

// Symbol

// string full_name = 1;
inline void Symbol::clear_full_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.full_name_.ClearToEmpty();
}
inline const std::string& Symbol::full_name() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:DescriptorIndex.Symbol.full_name)
  return _internal_full_name();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void Symbol::set_full_name(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.full_name_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:DescriptorIndex.Symbol.full_name)
}
inline std::string* Symbol::mutable_full_name() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_full_name();
  // @@protoc_insertion_point(field_mutable:DescriptorIndex.Symbol.full_name)
  return _s;
}
inline const std::string& Symbol::_internal_full_name() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.full_name_.Get();
}
inline void Symbol::_internal_set_full_name(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.full_name_.Set(value, GetArena());
}
inline std::string* Symbol::_internal_mutable_full_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.full_name_.Mutable( GetArena());
}
inline std::string* Symbol::release_full_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:DescriptorIndex.Symbol.full_name)
  return _impl_.full_name_.Release();
}
inline void Symbol::set_allocated_full_name(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.full_name_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.full_name_.IsDefault()) {
          _impl_.full_name_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:DescriptorIndex.Symbol.full_name)
}

// uint32 file_index = 2;
inline void Symbol::clear_file_index() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.file_index_ = 0u;
}
inline ::uint32_t Symbol::file_index() const {
  // @@protoc_insertion_point(field_get:DescriptorIndex.Symbol.file_index)
  return _internal_file_index();
}
inline void Symbol::set_file_index(::uint32_t value) {
  _internal_set_file_index(value);
  // @@protoc_insertion_point(field_set:DescriptorIndex.Symbol.file_index)
}
inline ::uint32_t Symbol::_internal_file_index() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.file_index_;
}
inline void Symbol::_internal_set_file_index(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.file_index_ = value;
}

// -------------------------------------------------------------------

// Package

// string full_name = 1;
inline void Package::clear_full_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.full_name_.ClearToEmpty();
}
inline const std::string& Package::full_name() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:DescriptorIndex.Package.full_name)
  return _internal_full_name();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void Package::set_full_name(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.full_name_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:DescriptorIndex.Package.full_name)
}
inline std::string* Package::mutable_full_name() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_full_name();
  // @@protoc_insertion_point(field_mutable:DescriptorIndex.Package.full_name)
  return _s;
}
inline const std::string& Package::_internal_full_name() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.full_name_.Get();
}
inline void Package::_internal_set_full_name(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.full_name_.Set(value, GetArena());
}
inline std::string* Package::_internal_mutable_full_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.full_name_.Mutable( GetArena());
}
inline std::string* Package::release_full_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:DescriptorIndex.Package.full_name)
  return _impl_.full_name_.Release();
}
inline void Package::set_allocated_full_name(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.full_name_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.full_name_.IsDefault()) {
          _impl_.full_name_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:DescriptorIndex.Package.full_name)
}

// repeated uint32 symbol_indexes = 2;
inline int Package::_internal_symbol_indexes_size() const {
  return _internal_symbol_indexes().size();
}
inline int Package::symbol_indexes_size() const {
  return _internal_symbol_indexes_size();
}
inline void Package::clear_symbol_indexes() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.symbol_indexes_.Clear();
}
inline ::uint32_t Package::symbol_indexes(int index) const {
  // @@protoc_insertion_point(field_get:DescriptorIndex.Package.symbol_indexes)
  return _internal_symbol_indexes().Get(index);
}
inline void Package::set_symbol_indexes(int index, ::uint32_t value) {
  _internal_mutable_symbol_indexes()->Set(index, value);
  // @@protoc_insertion_point(field_set:DescriptorIndex.Package.symbol_indexes)
}
inline void Package::add_symbol_indexes(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_symbol_indexes()->Add(value);
  // @@protoc_insertion_point(field_add:DescriptorIndex.Package.symbol_indexes)
}
inline const ::google::protobuf::RepeatedField<::uint32_t>& Package::symbol_indexes() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:DescriptorIndex.Package.symbol_indexes)
  return _internal_symbol_indexes();
}
inline ::google::protobuf::RepeatedField<::uint32_t>* Package::mutable_symbol_indexes()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:DescriptorIndex.Package.symbol_indexes)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_symbol_indexes();
}
inline const ::google::protobuf::RepeatedField<::uint32_t>&
Package::_internal_symbol_indexes() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.symbol_indexes_;
}
inline ::google::protobuf::RepeatedField<::uint32_t>* Package::_internal_mutable_symbol_indexes() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.symbol_indexes_;
}

// repeated uint32 nested_package_indexes = 3;
inline int Package::_internal_nested_package_indexes_size() const {
  return _internal_nested_package_indexes().size();
}
inline int Package::nested_package_indexes_size() const {
  return _internal_nested_package_indexes_size();
}
inline void Package::clear_nested_package_indexes() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.nested_package_indexes_.Clear();
}
inline ::uint32_t Package::nested_package_indexes(int index) const {
  // @@protoc_insertion_point(field_get:DescriptorIndex.Package.nested_package_indexes)
  return _internal_nested_package_indexes().Get(index);
}
inline void Package::set_nested_package_indexes(int index, ::uint32_t value) {
  _internal_mutable_nested_package_indexes()->Set(index, value);
  // @@protoc_insertion_point(field_set:DescriptorIndex.Package.nested_package_indexes)
}
inline void Package::add_nested_package_indexes(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_nested_package_indexes()->Add(value);
  // @@protoc_insertion_point(field_add:DescriptorIndex.Package.nested_package_indexes)
}
inline const ::google::protobuf::RepeatedField<::uint32_t>& Package::nested_package_indexes() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:DescriptorIndex.Package.nested_package_indexes)
  return _internal_nested_package_indexes();
}
inline ::google::protobuf::RepeatedField<::uint32_t>* Package::mutable_nested_package_indexes()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:DescriptorIndex.Package.nested_package_indexes)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_nested_package_indexes();
}
inline const ::google::protobuf::RepeatedField<::uint32_t>&
Package::_internal_nested_package_indexes() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.nested_package_indexes_;
}
inline ::google::protobuf::RepeatedField<::uint32_t>* Package::_internal_mutable_nested_package_indexes() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.nested_package_indexes_;
}

// -------------------------------------------------------------------

// DescriptorIndex

// repeated .DescriptorIndex.FileEntry files = 1;
inline int DescriptorIndex::_internal_files_size() const {
  return _internal_files().size();
}
inline int DescriptorIndex::files_size() const {
  return _internal_files_size();
}
inline void DescriptorIndex::clear_files() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.files_.Clear();
}
inline ::DescriptorIndex::FileEntry* DescriptorIndex::mutable_files(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:DescriptorIndex.DescriptorIndex.files)
  return _internal_mutable_files()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::DescriptorIndex::FileEntry>* DescriptorIndex::mutable_files()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:DescriptorIndex.DescriptorIndex.files)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_files();
}
inline const ::DescriptorIndex::FileEntry& DescriptorIndex::files(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:DescriptorIndex.DescriptorIndex.files)
  return _internal_files().Get(index);
}
inline ::DescriptorIndex::FileEntry* DescriptorIndex::add_files() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::DescriptorIndex::FileEntry* _add = _internal_mutable_files()->Add();
  // @@protoc_insertion_point(field_add:DescriptorIndex.DescriptorIndex.files)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::DescriptorIndex::FileEntry>& DescriptorIndex::files() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:DescriptorIndex.DescriptorIndex.files)
  return _internal_files();
}
inline const ::google::protobuf::RepeatedPtrField<::DescriptorIndex::FileEntry>&
DescriptorIndex::_internal_files() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.files_;
}
inline ::google::protobuf::RepeatedPtrField<::DescriptorIndex::FileEntry>*
DescriptorIndex::_internal_mutable_files() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.files_;
}

// repeated .DescriptorIndex.Symbol symbols = 2;
inline int DescriptorIndex::_internal_symbols_size() const {
  return _internal_symbols().size();
}
inline int DescriptorIndex::symbols_size() const {
  return _internal_symbols_size();
}
inline void DescriptorIndex::clear_symbols() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.symbols_.Clear();
}
inline ::DescriptorIndex::Symbol* DescriptorIndex::mutable_symbols(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:DescriptorIndex.DescriptorIndex.symbols)
  return _internal_mutable_symbols()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::DescriptorIndex::Symbol>* DescriptorIndex::mutable_symbols()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:DescriptorIndex.DescriptorIndex.symbols)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_symbols();
}
inline const ::DescriptorIndex::Symbol& DescriptorIndex::symbols(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:DescriptorIndex.DescriptorIndex.symbols)
  return _internal_symbols().Get(index);
}
inline ::DescriptorIndex::Symbol* DescriptorIndex::add_symbols() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::DescriptorIndex::Symbol* _add = _internal_mutable_symbols()->Add();
  // @@protoc_insertion_point(field_add:DescriptorIndex.DescriptorIndex.symbols)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::DescriptorIndex::Symbol>& DescriptorIndex::symbols() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:DescriptorIndex.DescriptorIndex.symbols)
  return _internal_symbols();
}
inline const ::google::protobuf::RepeatedPtrField<::DescriptorIndex::Symbol>&
DescriptorIndex::_internal_symbols() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.symbols_;
}
inline ::google::protobuf::RepeatedPtrField<::DescriptorIndex::Symbol>*
DescriptorIndex::_internal_mutable_symbols() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.symbols_;
}

// repeated .DescriptorIndex.Package packages = 3;
inline int DescriptorIndex::_internal_packages_size() const {
  return _internal_packages().size();
}
inline int DescriptorIndex::packages_size() const {
  return _internal_packages_size();
}
inline void DescriptorIndex::clear_packages() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.packages_.Clear();
}
inline ::DescriptorIndex::Package* DescriptorIndex::mutable_packages(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:DescriptorIndex.DescriptorIndex.packages)
  return _internal_mutable_packages()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::DescriptorIndex::Package>* DescriptorIndex::mutable_packages()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:DescriptorIndex.DescriptorIndex.packages)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_packages();
}
inline const ::DescriptorIndex::Package& DescriptorIndex::packages(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:DescriptorIndex.DescriptorIndex.packages)
  return _internal_packages().Get(index);
}
inline ::DescriptorIndex::Package* DescriptorIndex::add_packages() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::DescriptorIndex::Package* _add = _internal_mutable_packages()->Add();
  // @@protoc_insertion_point(field_add:DescriptorIndex.DescriptorIndex.packages)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::DescriptorIndex::Package>& DescriptorIndex::packages() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:DescriptorIndex.DescriptorIndex.packages)
  return _internal_packages();
}
inline const ::google::protobuf::RepeatedPtrField<::DescriptorIndex::Package>&
DescriptorIndex::_internal_packages() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.packages_;
}
inline ::google::protobuf::RepeatedPtrField<::DescriptorIndex::Package>*
DescriptorIndex::_internal_mutable_packages() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.packages_;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)
}  // namespace DescriptorIndex


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_DescriptorIndex_2eproto_2epb_2eh
