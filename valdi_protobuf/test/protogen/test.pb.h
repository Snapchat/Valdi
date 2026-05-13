// Generate by the Snap-modified protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: test.proto
// Protobuf C++ Version: 5.27.0

#ifndef GOOGLE_PROTOBUF_INCLUDED_test_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_test_2eproto_2epb_2eh

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
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/message.h"
#include "google/protobuf/repeated_field.h"  // IWYU pragma: export
#include "google/protobuf/extension_set.h"  // IWYU pragma: export
#include "google/protobuf/map.h"  // IWYU pragma: export
#include "google/protobuf/map_entry.h"
#include "google/protobuf/map_field_inl.h"
#include "google/protobuf/generated_enum_reflection.h"
#include "google/protobuf/unknown_field_set.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_test_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_test_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_test_2eproto;
namespace test {
class MapMessage;
struct MapMessageDefaultTypeInternal;
extern MapMessageDefaultTypeInternal _MapMessage_default_instance_;
class MapMessage_IntToStringEntry_DoNotUse;
struct MapMessage_IntToStringEntry_DoNotUseDefaultTypeInternal;
extern MapMessage_IntToStringEntry_DoNotUseDefaultTypeInternal _MapMessage_IntToStringEntry_DoNotUse_default_instance_;
class MapMessage_LongToStringEntry_DoNotUse;
struct MapMessage_LongToStringEntry_DoNotUseDefaultTypeInternal;
extern MapMessage_LongToStringEntry_DoNotUseDefaultTypeInternal _MapMessage_LongToStringEntry_DoNotUse_default_instance_;
class MapMessage_StringToDoubleEntry_DoNotUse;
struct MapMessage_StringToDoubleEntry_DoNotUseDefaultTypeInternal;
extern MapMessage_StringToDoubleEntry_DoNotUseDefaultTypeInternal _MapMessage_StringToDoubleEntry_DoNotUse_default_instance_;
class MapMessage_StringToMessageEntry_DoNotUse;
struct MapMessage_StringToMessageEntry_DoNotUseDefaultTypeInternal;
extern MapMessage_StringToMessageEntry_DoNotUseDefaultTypeInternal _MapMessage_StringToMessageEntry_DoNotUse_default_instance_;
class MapMessage_StringToNumberEntry_DoNotUse;
struct MapMessage_StringToNumberEntry_DoNotUseDefaultTypeInternal;
extern MapMessage_StringToNumberEntry_DoNotUseDefaultTypeInternal _MapMessage_StringToNumberEntry_DoNotUse_default_instance_;
class MapMessage_StringToSignedLongEntry_DoNotUse;
struct MapMessage_StringToSignedLongEntry_DoNotUseDefaultTypeInternal;
extern MapMessage_StringToSignedLongEntry_DoNotUseDefaultTypeInternal _MapMessage_StringToSignedLongEntry_DoNotUse_default_instance_;
class MapMessage_StringToStringEntry_DoNotUse;
struct MapMessage_StringToStringEntry_DoNotUseDefaultTypeInternal;
extern MapMessage_StringToStringEntry_DoNotUseDefaultTypeInternal _MapMessage_StringToStringEntry_DoNotUse_default_instance_;
class MapMessage_StringToUnsignedLongEntry_DoNotUse;
struct MapMessage_StringToUnsignedLongEntry_DoNotUseDefaultTypeInternal;
extern MapMessage_StringToUnsignedLongEntry_DoNotUseDefaultTypeInternal _MapMessage_StringToUnsignedLongEntry_DoNotUse_default_instance_;
class Message;
struct MessageDefaultTypeInternal;
extern MessageDefaultTypeInternal _Message_default_instance_;
class NewEnumMessage;
struct NewEnumMessageDefaultTypeInternal;
extern NewEnumMessageDefaultTypeInternal _NewEnumMessage_default_instance_;
class NewEnumMessage_MappedValueEntry_DoNotUse;
struct NewEnumMessage_MappedValueEntry_DoNotUseDefaultTypeInternal;
extern NewEnumMessage_MappedValueEntry_DoNotUseDefaultTypeInternal _NewEnumMessage_MappedValueEntry_DoNotUse_default_instance_;
class NewMessage;
struct NewMessageDefaultTypeInternal;
extern NewMessageDefaultTypeInternal _NewMessage_default_instance_;
class OldEnumMessage;
struct OldEnumMessageDefaultTypeInternal;
extern OldEnumMessageDefaultTypeInternal _OldEnumMessage_default_instance_;
class OldEnumMessage_MappedValueEntry_DoNotUse;
struct OldEnumMessage_MappedValueEntry_DoNotUseDefaultTypeInternal;
extern OldEnumMessage_MappedValueEntry_DoNotUseDefaultTypeInternal _OldEnumMessage_MappedValueEntry_DoNotUse_default_instance_;
class OldMessage;
struct OldMessageDefaultTypeInternal;
extern OldMessageDefaultTypeInternal _OldMessage_default_instance_;
class OneOfMessage;
struct OneOfMessageDefaultTypeInternal;
extern OneOfMessageDefaultTypeInternal _OneOfMessage_default_instance_;
class OtherMessage;
struct OtherMessageDefaultTypeInternal;
extern OtherMessageDefaultTypeInternal _OtherMessage_default_instance_;
class ParentMessage;
struct ParentMessageDefaultTypeInternal;
extern ParentMessageDefaultTypeInternal _ParentMessage_default_instance_;
class ParentMessage_ChildMessage;
struct ParentMessage_ChildMessageDefaultTypeInternal;
extern ParentMessage_ChildMessageDefaultTypeInternal _ParentMessage_ChildMessage_default_instance_;
class RepeatedMessage;
struct RepeatedMessageDefaultTypeInternal;
extern RepeatedMessageDefaultTypeInternal _RepeatedMessage_default_instance_;
}  // namespace test
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google

namespace test {
enum ParentMessage_ChildEnum : int {
  ParentMessage_ChildEnum_VALUE_0 = 0,
  ParentMessage_ChildEnum_VALUE_1 = 1,
  ParentMessage_ChildEnum_ParentMessage_ChildEnum_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  ParentMessage_ChildEnum_ParentMessage_ChildEnum_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

bool ParentMessage_ChildEnum_IsValid(int value);
extern const uint32_t ParentMessage_ChildEnum_internal_data_[];
constexpr ParentMessage_ChildEnum ParentMessage_ChildEnum_ChildEnum_MIN = static_cast<ParentMessage_ChildEnum>(0);
constexpr ParentMessage_ChildEnum ParentMessage_ChildEnum_ChildEnum_MAX = static_cast<ParentMessage_ChildEnum>(1);
constexpr int ParentMessage_ChildEnum_ChildEnum_ARRAYSIZE = 1 + 1;
const ::google::protobuf::EnumDescriptor*
ParentMessage_ChildEnum_descriptor();
template <typename T>
const std::string& ParentMessage_ChildEnum_Name(T value) {
  static_assert(std::is_same<T, ParentMessage_ChildEnum>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to ChildEnum_Name().");
  return ParentMessage_ChildEnum_Name(static_cast<ParentMessage_ChildEnum>(value));
}
template <>
inline const std::string& ParentMessage_ChildEnum_Name(ParentMessage_ChildEnum value) {
  return ::google::protobuf::internal::NameOfDenseEnum<ParentMessage_ChildEnum_descriptor,
                                                 0, 1>(
      static_cast<int>(value));
}
inline bool ParentMessage_ChildEnum_Parse(absl::string_view name, ParentMessage_ChildEnum* value) {
  return ::google::protobuf::internal::ParseNamedEnum<ParentMessage_ChildEnum>(
      ParentMessage_ChildEnum_descriptor(), name, value);
}
enum OldEnumMessage_OldEnum : int {
  OldEnumMessage_OldEnum_VALUE_0 = 0,
  OldEnumMessage_OldEnum_VALUE_1 = 1,
  OldEnumMessage_OldEnum_OldEnumMessage_OldEnum_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  OldEnumMessage_OldEnum_OldEnumMessage_OldEnum_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

bool OldEnumMessage_OldEnum_IsValid(int value);
extern const uint32_t OldEnumMessage_OldEnum_internal_data_[];
constexpr OldEnumMessage_OldEnum OldEnumMessage_OldEnum_OldEnum_MIN = static_cast<OldEnumMessage_OldEnum>(0);
constexpr OldEnumMessage_OldEnum OldEnumMessage_OldEnum_OldEnum_MAX = static_cast<OldEnumMessage_OldEnum>(1);
constexpr int OldEnumMessage_OldEnum_OldEnum_ARRAYSIZE = 1 + 1;
const ::google::protobuf::EnumDescriptor*
OldEnumMessage_OldEnum_descriptor();
template <typename T>
const std::string& OldEnumMessage_OldEnum_Name(T value) {
  static_assert(std::is_same<T, OldEnumMessage_OldEnum>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to OldEnum_Name().");
  return OldEnumMessage_OldEnum_Name(static_cast<OldEnumMessage_OldEnum>(value));
}
template <>
inline const std::string& OldEnumMessage_OldEnum_Name(OldEnumMessage_OldEnum value) {
  return ::google::protobuf::internal::NameOfDenseEnum<OldEnumMessage_OldEnum_descriptor,
                                                 0, 1>(
      static_cast<int>(value));
}
inline bool OldEnumMessage_OldEnum_Parse(absl::string_view name, OldEnumMessage_OldEnum* value) {
  return ::google::protobuf::internal::ParseNamedEnum<OldEnumMessage_OldEnum>(
      OldEnumMessage_OldEnum_descriptor(), name, value);
}
enum NewEnumMessage_NewEnum : int {
  NewEnumMessage_NewEnum_VALUE_0 = 0,
  NewEnumMessage_NewEnum_VALUE_1 = 1,
  NewEnumMessage_NewEnum_VALUE_2 = 200,
  NewEnumMessage_NewEnum_VALUE_3 = 300,
  NewEnumMessage_NewEnum_NewEnumMessage_NewEnum_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  NewEnumMessage_NewEnum_NewEnumMessage_NewEnum_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

bool NewEnumMessage_NewEnum_IsValid(int value);
extern const uint32_t NewEnumMessage_NewEnum_internal_data_[];
constexpr NewEnumMessage_NewEnum NewEnumMessage_NewEnum_NewEnum_MIN = static_cast<NewEnumMessage_NewEnum>(0);
constexpr NewEnumMessage_NewEnum NewEnumMessage_NewEnum_NewEnum_MAX = static_cast<NewEnumMessage_NewEnum>(300);
constexpr int NewEnumMessage_NewEnum_NewEnum_ARRAYSIZE = 300 + 1;
const ::google::protobuf::EnumDescriptor*
NewEnumMessage_NewEnum_descriptor();
template <typename T>
const std::string& NewEnumMessage_NewEnum_Name(T value) {
  static_assert(std::is_same<T, NewEnumMessage_NewEnum>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to NewEnum_Name().");
  return ::google::protobuf::internal::NameOfEnum(NewEnumMessage_NewEnum_descriptor(), value);
}
inline bool NewEnumMessage_NewEnum_Parse(absl::string_view name, NewEnumMessage_NewEnum* value) {
  return ::google::protobuf::internal::ParseNamedEnum<NewEnumMessage_NewEnum>(
      NewEnumMessage_NewEnum_descriptor(), name, value);
}
enum Enum : int {
  VALUE_0 = 0,
  VALUE_1 = 1,
  Enum_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  Enum_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

bool Enum_IsValid(int value);
extern const uint32_t Enum_internal_data_[];
constexpr Enum Enum_MIN = static_cast<Enum>(0);
constexpr Enum Enum_MAX = static_cast<Enum>(1);
constexpr int Enum_ARRAYSIZE = 1 + 1;
const ::google::protobuf::EnumDescriptor*
Enum_descriptor();
template <typename T>
const std::string& Enum_Name(T value) {
  static_assert(std::is_same<T, Enum>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to Enum_Name().");
  return Enum_Name(static_cast<Enum>(value));
}
template <>
inline const std::string& Enum_Name(Enum value) {
  return ::google::protobuf::internal::NameOfDenseEnum<Enum_descriptor,
                                                 0, 1>(
      static_cast<int>(value));
}
inline bool Enum_Parse(absl::string_view name, Enum* value) {
  return ::google::protobuf::internal::ParseNamedEnum<Enum>(
      Enum_descriptor(), name, value);
}

// ===================================================================


// -------------------------------------------------------------------

class ParentMessage_ChildMessage final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:test.ParentMessage.ChildMessage) */ {
 public:
  inline ParentMessage_ChildMessage() : ParentMessage_ChildMessage(nullptr) {}
  ~ParentMessage_ChildMessage() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR ParentMessage_ChildMessage(
      ::google::protobuf::internal::ConstantInitialized);

  inline ParentMessage_ChildMessage(const ParentMessage_ChildMessage& from) : ParentMessage_ChildMessage(nullptr, from) {}
  inline ParentMessage_ChildMessage(ParentMessage_ChildMessage&& from) noexcept
      : ParentMessage_ChildMessage(nullptr, std::move(from)) {}
  inline ParentMessage_ChildMessage& operator=(const ParentMessage_ChildMessage& from) {
    CopyFrom(from);
    return *this;
  }
  inline ParentMessage_ChildMessage& operator=(ParentMessage_ChildMessage&& from) noexcept {
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

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const ParentMessage_ChildMessage& default_instance() {
    return *internal_default_instance();
  }
  static inline const ParentMessage_ChildMessage* internal_default_instance() {
    return reinterpret_cast<const ParentMessage_ChildMessage*>(
        &_ParentMessage_ChildMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 3;
  friend void swap(ParentMessage_ChildMessage& a, ParentMessage_ChildMessage& b) { a.Swap(&b); }
  inline void Swap(ParentMessage_ChildMessage* other) {
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
  void UnsafeArenaSwap(ParentMessage_ChildMessage* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  ParentMessage_ChildMessage* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<ParentMessage_ChildMessage>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const ParentMessage_ChildMessage& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const ParentMessage_ChildMessage& from) { ParentMessage_ChildMessage::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
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
  void InternalSwap(ParentMessage_ChildMessage* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "test.ParentMessage.ChildMessage"; }

 protected:
  explicit ParentMessage_ChildMessage(::google::protobuf::Arena* arena);
  ParentMessage_ChildMessage(::google::protobuf::Arena* arena, const ParentMessage_ChildMessage& from);
  ParentMessage_ChildMessage(::google::protobuf::Arena* arena, ParentMessage_ChildMessage&& from) noexcept
      : ParentMessage_ChildMessage(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::Message::ClassData* GetClassData() const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kValueFieldNumber = 1,
  };
  // string value = 1;
  void clear_value() ;
  const std::string& value() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_value(Arg_&& arg, Args_... args);
  std::string* mutable_value();
  PROTOBUF_NODISCARD std::string* release_value();
  void set_allocated_value(std::string* value);

  private:
  const std::string& _internal_value() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_value(
      const std::string& value);
  std::string* _internal_mutable_value();

  public:
  // @@protoc_insertion_point(class_scope:test.ParentMessage.ChildMessage)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      0, 1, 0,
      45, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_ParentMessage_ChildMessage_default_instance_;

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
                          const ParentMessage_ChildMessage& from_msg);
    ::google::protobuf::internal::ArenaStringPtr value_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class OtherMessage final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:test.OtherMessage) */ {
 public:
  inline OtherMessage() : OtherMessage(nullptr) {}
  ~OtherMessage() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR OtherMessage(
      ::google::protobuf::internal::ConstantInitialized);

  inline OtherMessage(const OtherMessage& from) : OtherMessage(nullptr, from) {}
  inline OtherMessage(OtherMessage&& from) noexcept
      : OtherMessage(nullptr, std::move(from)) {}
  inline OtherMessage& operator=(const OtherMessage& from) {
    CopyFrom(from);
    return *this;
  }
  inline OtherMessage& operator=(OtherMessage&& from) noexcept {
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

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const OtherMessage& default_instance() {
    return *internal_default_instance();
  }
  static inline const OtherMessage* internal_default_instance() {
    return reinterpret_cast<const OtherMessage*>(
        &_OtherMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(OtherMessage& a, OtherMessage& b) { a.Swap(&b); }
  inline void Swap(OtherMessage* other) {
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
  void UnsafeArenaSwap(OtherMessage* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  OtherMessage* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<OtherMessage>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const OtherMessage& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const OtherMessage& from) { OtherMessage::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
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
  void InternalSwap(OtherMessage* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "test.OtherMessage"; }

 protected:
  explicit OtherMessage(::google::protobuf::Arena* arena);
  OtherMessage(::google::protobuf::Arena* arena, const OtherMessage& from);
  OtherMessage(::google::protobuf::Arena* arena, OtherMessage&& from) noexcept
      : OtherMessage(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::Message::ClassData* GetClassData() const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kValueFieldNumber = 1,
  };
  // string value = 1;
  void clear_value() ;
  const std::string& value() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_value(Arg_&& arg, Args_... args);
  std::string* mutable_value();
  PROTOBUF_NODISCARD std::string* release_value();
  void set_allocated_value(std::string* value);

  private:
  const std::string& _internal_value() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_value(
      const std::string& value);
  std::string* _internal_mutable_value();

  public:
  // @@protoc_insertion_point(class_scope:test.OtherMessage)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      0, 1, 0,
      31, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_OtherMessage_default_instance_;

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
                          const OtherMessage& from_msg);
    ::google::protobuf::internal::ArenaStringPtr value_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class OldMessage final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:test.OldMessage) */ {
 public:
  inline OldMessage() : OldMessage(nullptr) {}
  ~OldMessage() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR OldMessage(
      ::google::protobuf::internal::ConstantInitialized);

  inline OldMessage(const OldMessage& from) : OldMessage(nullptr, from) {}
  inline OldMessage(OldMessage&& from) noexcept
      : OldMessage(nullptr, std::move(from)) {}
  inline OldMessage& operator=(const OldMessage& from) {
    CopyFrom(from);
    return *this;
  }
  inline OldMessage& operator=(OldMessage&& from) noexcept {
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

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const OldMessage& default_instance() {
    return *internal_default_instance();
  }
  static inline const OldMessage* internal_default_instance() {
    return reinterpret_cast<const OldMessage*>(
        &_OldMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 6;
  friend void swap(OldMessage& a, OldMessage& b) { a.Swap(&b); }
  inline void Swap(OldMessage* other) {
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
  void UnsafeArenaSwap(OldMessage* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  OldMessage* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<OldMessage>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const OldMessage& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const OldMessage& from) { OldMessage::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
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
  void InternalSwap(OldMessage* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "test.OldMessage"; }

 protected:
  explicit OldMessage(::google::protobuf::Arena* arena);
  OldMessage(::google::protobuf::Arena* arena, const OldMessage& from);
  OldMessage(::google::protobuf::Arena* arena, OldMessage&& from) noexcept
      : OldMessage(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::Message::ClassData* GetClassData() const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kOldValueFieldNumber = 1,
  };
  // string old_value = 1;
  void clear_old_value() ;
  const std::string& old_value() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_old_value(Arg_&& arg, Args_... args);
  std::string* mutable_old_value();
  PROTOBUF_NODISCARD std::string* release_old_value();
  void set_allocated_old_value(std::string* value);

  private:
  const std::string& _internal_old_value() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_old_value(
      const std::string& value);
  std::string* _internal_mutable_old_value();

  public:
  // @@protoc_insertion_point(class_scope:test.OldMessage)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      0, 1, 0,
      33, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_OldMessage_default_instance_;

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
                          const OldMessage& from_msg);
    ::google::protobuf::internal::ArenaStringPtr old_value_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class OldEnumMessage_MappedValueEntry_DoNotUse final
    : public ::google::protobuf::internal::MapEntry<
          OldEnumMessage_MappedValueEntry_DoNotUse, std::string, ::test::OldEnumMessage_OldEnum,
          ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
          ::google::protobuf::internal::WireFormatLite::TYPE_ENUM> {
 public:
  using SuperType = ::google::protobuf::internal::MapEntry<
      OldEnumMessage_MappedValueEntry_DoNotUse, std::string, ::test::OldEnumMessage_OldEnum,
      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
      ::google::protobuf::internal::WireFormatLite::TYPE_ENUM>;
  OldEnumMessage_MappedValueEntry_DoNotUse();
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR OldEnumMessage_MappedValueEntry_DoNotUse(
      ::google::protobuf::internal::ConstantInitialized);
  explicit OldEnumMessage_MappedValueEntry_DoNotUse(::google::protobuf::Arena* arena);
  static const OldEnumMessage_MappedValueEntry_DoNotUse* internal_default_instance() {
    return reinterpret_cast<const OldEnumMessage_MappedValueEntry_DoNotUse*>(
        &_OldEnumMessage_MappedValueEntry_DoNotUse_default_instance_);
  }
const ::google::protobuf::Message::ClassData* GetClassData() const final;
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class NewMessage final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:test.NewMessage) */ {
 public:
  inline NewMessage() : NewMessage(nullptr) {}
  ~NewMessage() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR NewMessage(
      ::google::protobuf::internal::ConstantInitialized);

  inline NewMessage(const NewMessage& from) : NewMessage(nullptr, from) {}
  inline NewMessage(NewMessage&& from) noexcept
      : NewMessage(nullptr, std::move(from)) {}
  inline NewMessage& operator=(const NewMessage& from) {
    CopyFrom(from);
    return *this;
  }
  inline NewMessage& operator=(NewMessage&& from) noexcept {
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

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const NewMessage& default_instance() {
    return *internal_default_instance();
  }
  static inline const NewMessage* internal_default_instance() {
    return reinterpret_cast<const NewMessage*>(
        &_NewMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 7;
  friend void swap(NewMessage& a, NewMessage& b) { a.Swap(&b); }
  inline void Swap(NewMessage* other) {
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
  void UnsafeArenaSwap(NewMessage* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  NewMessage* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<NewMessage>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const NewMessage& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const NewMessage& from) { NewMessage::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
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
  void InternalSwap(NewMessage* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "test.NewMessage"; }

 protected:
  explicit NewMessage(::google::protobuf::Arena* arena);
  NewMessage(::google::protobuf::Arena* arena, const NewMessage& from);
  NewMessage(::google::protobuf::Arena* arena, NewMessage&& from) noexcept
      : NewMessage(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::Message::ClassData* GetClassData() const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kOldValueFieldNumber = 1,
    kNewValueFieldNumber = 2,
  };
  // string old_value = 1;
  void clear_old_value() ;
  const std::string& old_value() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_old_value(Arg_&& arg, Args_... args);
  std::string* mutable_old_value();
  PROTOBUF_NODISCARD std::string* release_old_value();
  void set_allocated_old_value(std::string* value);

  private:
  const std::string& _internal_old_value() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_old_value(
      const std::string& value);
  std::string* _internal_mutable_old_value();

  public:
  // string new_value = 2;
  void clear_new_value() ;
  const std::string& new_value() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_new_value(Arg_&& arg, Args_... args);
  std::string* mutable_new_value();
  PROTOBUF_NODISCARD std::string* release_new_value();
  void set_allocated_new_value(std::string* value);

  private:
  const std::string& _internal_new_value() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_new_value(
      const std::string& value);
  std::string* _internal_mutable_new_value();

  public:
  // @@protoc_insertion_point(class_scope:test.NewMessage)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      1, 2, 0,
      42, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_NewMessage_default_instance_;

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
                          const NewMessage& from_msg);
    ::google::protobuf::internal::ArenaStringPtr old_value_;
    ::google::protobuf::internal::ArenaStringPtr new_value_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class NewEnumMessage_MappedValueEntry_DoNotUse final
    : public ::google::protobuf::internal::MapEntry<
          NewEnumMessage_MappedValueEntry_DoNotUse, std::string, ::test::NewEnumMessage_NewEnum,
          ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
          ::google::protobuf::internal::WireFormatLite::TYPE_ENUM> {
 public:
  using SuperType = ::google::protobuf::internal::MapEntry<
      NewEnumMessage_MappedValueEntry_DoNotUse, std::string, ::test::NewEnumMessage_NewEnum,
      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
      ::google::protobuf::internal::WireFormatLite::TYPE_ENUM>;
  NewEnumMessage_MappedValueEntry_DoNotUse();
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR NewEnumMessage_MappedValueEntry_DoNotUse(
      ::google::protobuf::internal::ConstantInitialized);
  explicit NewEnumMessage_MappedValueEntry_DoNotUse(::google::protobuf::Arena* arena);
  static const NewEnumMessage_MappedValueEntry_DoNotUse* internal_default_instance() {
    return reinterpret_cast<const NewEnumMessage_MappedValueEntry_DoNotUse*>(
        &_NewEnumMessage_MappedValueEntry_DoNotUse_default_instance_);
  }
const ::google::protobuf::Message::ClassData* GetClassData() const final;
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class MapMessage_StringToUnsignedLongEntry_DoNotUse final
    : public ::google::protobuf::internal::MapEntry<
          MapMessage_StringToUnsignedLongEntry_DoNotUse, std::string, ::uint64_t,
          ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
          ::google::protobuf::internal::WireFormatLite::TYPE_UINT64> {
 public:
  using SuperType = ::google::protobuf::internal::MapEntry<
      MapMessage_StringToUnsignedLongEntry_DoNotUse, std::string, ::uint64_t,
      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
      ::google::protobuf::internal::WireFormatLite::TYPE_UINT64>;
  MapMessage_StringToUnsignedLongEntry_DoNotUse();
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR MapMessage_StringToUnsignedLongEntry_DoNotUse(
      ::google::protobuf::internal::ConstantInitialized);
  explicit MapMessage_StringToUnsignedLongEntry_DoNotUse(::google::protobuf::Arena* arena);
  static const MapMessage_StringToUnsignedLongEntry_DoNotUse* internal_default_instance() {
    return reinterpret_cast<const MapMessage_StringToUnsignedLongEntry_DoNotUse*>(
        &_MapMessage_StringToUnsignedLongEntry_DoNotUse_default_instance_);
  }
const ::google::protobuf::Message::ClassData* GetClassData() const final;
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class MapMessage_StringToStringEntry_DoNotUse final
    : public ::google::protobuf::internal::MapEntry<
          MapMessage_StringToStringEntry_DoNotUse, std::string, std::string,
          ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
          ::google::protobuf::internal::WireFormatLite::TYPE_STRING> {
 public:
  using SuperType = ::google::protobuf::internal::MapEntry<
      MapMessage_StringToStringEntry_DoNotUse, std::string, std::string,
      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
      ::google::protobuf::internal::WireFormatLite::TYPE_STRING>;
  MapMessage_StringToStringEntry_DoNotUse();
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR MapMessage_StringToStringEntry_DoNotUse(
      ::google::protobuf::internal::ConstantInitialized);
  explicit MapMessage_StringToStringEntry_DoNotUse(::google::protobuf::Arena* arena);
  static const MapMessage_StringToStringEntry_DoNotUse* internal_default_instance() {
    return reinterpret_cast<const MapMessage_StringToStringEntry_DoNotUse*>(
        &_MapMessage_StringToStringEntry_DoNotUse_default_instance_);
  }
const ::google::protobuf::Message::ClassData* GetClassData() const final;
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class MapMessage_StringToSignedLongEntry_DoNotUse final
    : public ::google::protobuf::internal::MapEntry<
          MapMessage_StringToSignedLongEntry_DoNotUse, std::string, ::int64_t,
          ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
          ::google::protobuf::internal::WireFormatLite::TYPE_INT64> {
 public:
  using SuperType = ::google::protobuf::internal::MapEntry<
      MapMessage_StringToSignedLongEntry_DoNotUse, std::string, ::int64_t,
      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
      ::google::protobuf::internal::WireFormatLite::TYPE_INT64>;
  MapMessage_StringToSignedLongEntry_DoNotUse();
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR MapMessage_StringToSignedLongEntry_DoNotUse(
      ::google::protobuf::internal::ConstantInitialized);
  explicit MapMessage_StringToSignedLongEntry_DoNotUse(::google::protobuf::Arena* arena);
  static const MapMessage_StringToSignedLongEntry_DoNotUse* internal_default_instance() {
    return reinterpret_cast<const MapMessage_StringToSignedLongEntry_DoNotUse*>(
        &_MapMessage_StringToSignedLongEntry_DoNotUse_default_instance_);
  }
const ::google::protobuf::Message::ClassData* GetClassData() const final;
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class MapMessage_StringToNumberEntry_DoNotUse final
    : public ::google::protobuf::internal::MapEntry<
          MapMessage_StringToNumberEntry_DoNotUse, std::string, ::int32_t,
          ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
          ::google::protobuf::internal::WireFormatLite::TYPE_INT32> {
 public:
  using SuperType = ::google::protobuf::internal::MapEntry<
      MapMessage_StringToNumberEntry_DoNotUse, std::string, ::int32_t,
      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
      ::google::protobuf::internal::WireFormatLite::TYPE_INT32>;
  MapMessage_StringToNumberEntry_DoNotUse();
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR MapMessage_StringToNumberEntry_DoNotUse(
      ::google::protobuf::internal::ConstantInitialized);
  explicit MapMessage_StringToNumberEntry_DoNotUse(::google::protobuf::Arena* arena);
  static const MapMessage_StringToNumberEntry_DoNotUse* internal_default_instance() {
    return reinterpret_cast<const MapMessage_StringToNumberEntry_DoNotUse*>(
        &_MapMessage_StringToNumberEntry_DoNotUse_default_instance_);
  }
const ::google::protobuf::Message::ClassData* GetClassData() const final;
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class MapMessage_StringToDoubleEntry_DoNotUse final
    : public ::google::protobuf::internal::MapEntry<
          MapMessage_StringToDoubleEntry_DoNotUse, std::string, double,
          ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
          ::google::protobuf::internal::WireFormatLite::TYPE_DOUBLE> {
 public:
  using SuperType = ::google::protobuf::internal::MapEntry<
      MapMessage_StringToDoubleEntry_DoNotUse, std::string, double,
      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
      ::google::protobuf::internal::WireFormatLite::TYPE_DOUBLE>;
  MapMessage_StringToDoubleEntry_DoNotUse();
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR MapMessage_StringToDoubleEntry_DoNotUse(
      ::google::protobuf::internal::ConstantInitialized);
  explicit MapMessage_StringToDoubleEntry_DoNotUse(::google::protobuf::Arena* arena);
  static const MapMessage_StringToDoubleEntry_DoNotUse* internal_default_instance() {
    return reinterpret_cast<const MapMessage_StringToDoubleEntry_DoNotUse*>(
        &_MapMessage_StringToDoubleEntry_DoNotUse_default_instance_);
  }
const ::google::protobuf::Message::ClassData* GetClassData() const final;
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class MapMessage_LongToStringEntry_DoNotUse final
    : public ::google::protobuf::internal::MapEntry<
          MapMessage_LongToStringEntry_DoNotUse, ::int64_t, std::string,
          ::google::protobuf::internal::WireFormatLite::TYPE_INT64,
          ::google::protobuf::internal::WireFormatLite::TYPE_STRING> {
 public:
  using SuperType = ::google::protobuf::internal::MapEntry<
      MapMessage_LongToStringEntry_DoNotUse, ::int64_t, std::string,
      ::google::protobuf::internal::WireFormatLite::TYPE_INT64,
      ::google::protobuf::internal::WireFormatLite::TYPE_STRING>;
  MapMessage_LongToStringEntry_DoNotUse();
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR MapMessage_LongToStringEntry_DoNotUse(
      ::google::protobuf::internal::ConstantInitialized);
  explicit MapMessage_LongToStringEntry_DoNotUse(::google::protobuf::Arena* arena);
  static const MapMessage_LongToStringEntry_DoNotUse* internal_default_instance() {
    return reinterpret_cast<const MapMessage_LongToStringEntry_DoNotUse*>(
        &_MapMessage_LongToStringEntry_DoNotUse_default_instance_);
  }
const ::google::protobuf::Message::ClassData* GetClassData() const final;
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class MapMessage_IntToStringEntry_DoNotUse final
    : public ::google::protobuf::internal::MapEntry<
          MapMessage_IntToStringEntry_DoNotUse, ::int32_t, std::string,
          ::google::protobuf::internal::WireFormatLite::TYPE_INT32,
          ::google::protobuf::internal::WireFormatLite::TYPE_STRING> {
 public:
  using SuperType = ::google::protobuf::internal::MapEntry<
      MapMessage_IntToStringEntry_DoNotUse, ::int32_t, std::string,
      ::google::protobuf::internal::WireFormatLite::TYPE_INT32,
      ::google::protobuf::internal::WireFormatLite::TYPE_STRING>;
  MapMessage_IntToStringEntry_DoNotUse();
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR MapMessage_IntToStringEntry_DoNotUse(
      ::google::protobuf::internal::ConstantInitialized);
  explicit MapMessage_IntToStringEntry_DoNotUse(::google::protobuf::Arena* arena);
  static const MapMessage_IntToStringEntry_DoNotUse* internal_default_instance() {
    return reinterpret_cast<const MapMessage_IntToStringEntry_DoNotUse*>(
        &_MapMessage_IntToStringEntry_DoNotUse_default_instance_);
  }
const ::google::protobuf::Message::ClassData* GetClassData() const final;
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class RepeatedMessage final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:test.RepeatedMessage) */ {
 public:
  inline RepeatedMessage() : RepeatedMessage(nullptr) {}
  ~RepeatedMessage() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR RepeatedMessage(
      ::google::protobuf::internal::ConstantInitialized);

  inline RepeatedMessage(const RepeatedMessage& from) : RepeatedMessage(nullptr, from) {}
  inline RepeatedMessage(RepeatedMessage&& from) noexcept
      : RepeatedMessage(nullptr, std::move(from)) {}
  inline RepeatedMessage& operator=(const RepeatedMessage& from) {
    CopyFrom(from);
    return *this;
  }
  inline RepeatedMessage& operator=(RepeatedMessage&& from) noexcept {
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

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const RepeatedMessage& default_instance() {
    return *internal_default_instance();
  }
  static inline const RepeatedMessage* internal_default_instance() {
    return reinterpret_cast<const RepeatedMessage*>(
        &_RepeatedMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 2;
  friend void swap(RepeatedMessage& a, RepeatedMessage& b) { a.Swap(&b); }
  inline void Swap(RepeatedMessage* other) {
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
  void UnsafeArenaSwap(RepeatedMessage* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  RepeatedMessage* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<RepeatedMessage>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const RepeatedMessage& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const RepeatedMessage& from) { RepeatedMessage::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
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
  void InternalSwap(RepeatedMessage* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "test.RepeatedMessage"; }

 protected:
  explicit RepeatedMessage(::google::protobuf::Arena* arena);
  RepeatedMessage(::google::protobuf::Arena* arena, const RepeatedMessage& from);
  RepeatedMessage(::google::protobuf::Arena* arena, RepeatedMessage&& from) noexcept
      : RepeatedMessage(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::Message::ClassData* GetClassData() const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kInt32FieldNumber = 1,
    kInt64FieldNumber = 2,
    kUint32FieldNumber = 3,
    kUint64FieldNumber = 4,
    kSint32FieldNumber = 5,
    kSint64FieldNumber = 6,
    kFixed32FieldNumber = 7,
    kFixed64FieldNumber = 8,
    kSfixed32FieldNumber = 9,
    kSfixed64FieldNumber = 10,
    kFloatFieldNumber = 11,
    kDoubleFieldNumber = 12,
    kBoolFieldNumber = 13,
    kStringFieldNumber = 14,
    kBytesFieldNumber = 15,
    kEnumFieldNumber = 16,
    kSelfMessageFieldNumber = 17,
    kOtherMessageFieldNumber = 18,
  };
  // repeated int32 int32 = 1;
  int int32_size() const;
  private:
  int _internal_int32_size() const;

  public:
  void clear_int32() ;
  ::int32_t int32(int index) const;
  void set_int32(int index, ::int32_t value);
  void add_int32(::int32_t value);
  const ::google::protobuf::RepeatedField<::int32_t>& int32() const;
  ::google::protobuf::RepeatedField<::int32_t>* mutable_int32();

  private:
  const ::google::protobuf::RepeatedField<::int32_t>& _internal_int32() const;
  ::google::protobuf::RepeatedField<::int32_t>* _internal_mutable_int32();

  public:
  // repeated int64 int64 = 2;
  int int64_size() const;
  private:
  int _internal_int64_size() const;

  public:
  void clear_int64() ;
  ::int64_t int64(int index) const;
  void set_int64(int index, ::int64_t value);
  void add_int64(::int64_t value);
  const ::google::protobuf::RepeatedField<::int64_t>& int64() const;
  ::google::protobuf::RepeatedField<::int64_t>* mutable_int64();

  private:
  const ::google::protobuf::RepeatedField<::int64_t>& _internal_int64() const;
  ::google::protobuf::RepeatedField<::int64_t>* _internal_mutable_int64();

  public:
  // repeated uint32 uint32 = 3;
  int uint32_size() const;
  private:
  int _internal_uint32_size() const;

  public:
  void clear_uint32() ;
  ::uint32_t uint32(int index) const;
  void set_uint32(int index, ::uint32_t value);
  void add_uint32(::uint32_t value);
  const ::google::protobuf::RepeatedField<::uint32_t>& uint32() const;
  ::google::protobuf::RepeatedField<::uint32_t>* mutable_uint32();

  private:
  const ::google::protobuf::RepeatedField<::uint32_t>& _internal_uint32() const;
  ::google::protobuf::RepeatedField<::uint32_t>* _internal_mutable_uint32();

  public:
  // repeated uint64 uint64 = 4;
  int uint64_size() const;
  private:
  int _internal_uint64_size() const;

  public:
  void clear_uint64() ;
  ::uint64_t uint64(int index) const;
  void set_uint64(int index, ::uint64_t value);
  void add_uint64(::uint64_t value);
  const ::google::protobuf::RepeatedField<::uint64_t>& uint64() const;
  ::google::protobuf::RepeatedField<::uint64_t>* mutable_uint64();

  private:
  const ::google::protobuf::RepeatedField<::uint64_t>& _internal_uint64() const;
  ::google::protobuf::RepeatedField<::uint64_t>* _internal_mutable_uint64();

  public:
  // repeated sint32 sint32 = 5;
  int sint32_size() const;
  private:
  int _internal_sint32_size() const;

  public:
  void clear_sint32() ;
  ::int32_t sint32(int index) const;
  void set_sint32(int index, ::int32_t value);
  void add_sint32(::int32_t value);
  const ::google::protobuf::RepeatedField<::int32_t>& sint32() const;
  ::google::protobuf::RepeatedField<::int32_t>* mutable_sint32();

  private:
  const ::google::protobuf::RepeatedField<::int32_t>& _internal_sint32() const;
  ::google::protobuf::RepeatedField<::int32_t>* _internal_mutable_sint32();

  public:
  // repeated sint64 sint64 = 6;
  int sint64_size() const;
  private:
  int _internal_sint64_size() const;

  public:
  void clear_sint64() ;
  ::int64_t sint64(int index) const;
  void set_sint64(int index, ::int64_t value);
  void add_sint64(::int64_t value);
  const ::google::protobuf::RepeatedField<::int64_t>& sint64() const;
  ::google::protobuf::RepeatedField<::int64_t>* mutable_sint64();

  private:
  const ::google::protobuf::RepeatedField<::int64_t>& _internal_sint64() const;
  ::google::protobuf::RepeatedField<::int64_t>* _internal_mutable_sint64();

  public:
  // repeated fixed32 fixed32 = 7;
  int fixed32_size() const;
  private:
  int _internal_fixed32_size() const;

  public:
  void clear_fixed32() ;
  ::uint32_t fixed32(int index) const;
  void set_fixed32(int index, ::uint32_t value);
  void add_fixed32(::uint32_t value);
  const ::google::protobuf::RepeatedField<::uint32_t>& fixed32() const;
  ::google::protobuf::RepeatedField<::uint32_t>* mutable_fixed32();

  private:
  const ::google::protobuf::RepeatedField<::uint32_t>& _internal_fixed32() const;
  ::google::protobuf::RepeatedField<::uint32_t>* _internal_mutable_fixed32();

  public:
  // repeated fixed64 fixed64 = 8;
  int fixed64_size() const;
  private:
  int _internal_fixed64_size() const;

  public:
  void clear_fixed64() ;
  ::uint64_t fixed64(int index) const;
  void set_fixed64(int index, ::uint64_t value);
  void add_fixed64(::uint64_t value);
  const ::google::protobuf::RepeatedField<::uint64_t>& fixed64() const;
  ::google::protobuf::RepeatedField<::uint64_t>* mutable_fixed64();

  private:
  const ::google::protobuf::RepeatedField<::uint64_t>& _internal_fixed64() const;
  ::google::protobuf::RepeatedField<::uint64_t>* _internal_mutable_fixed64();

  public:
  // repeated sfixed32 sfixed32 = 9;
  int sfixed32_size() const;
  private:
  int _internal_sfixed32_size() const;

  public:
  void clear_sfixed32() ;
  ::int32_t sfixed32(int index) const;
  void set_sfixed32(int index, ::int32_t value);
  void add_sfixed32(::int32_t value);
  const ::google::protobuf::RepeatedField<::int32_t>& sfixed32() const;
  ::google::protobuf::RepeatedField<::int32_t>* mutable_sfixed32();

  private:
  const ::google::protobuf::RepeatedField<::int32_t>& _internal_sfixed32() const;
  ::google::protobuf::RepeatedField<::int32_t>* _internal_mutable_sfixed32();

  public:
  // repeated sfixed64 sfixed64 = 10;
  int sfixed64_size() const;
  private:
  int _internal_sfixed64_size() const;

  public:
  void clear_sfixed64() ;
  ::int64_t sfixed64(int index) const;
  void set_sfixed64(int index, ::int64_t value);
  void add_sfixed64(::int64_t value);
  const ::google::protobuf::RepeatedField<::int64_t>& sfixed64() const;
  ::google::protobuf::RepeatedField<::int64_t>* mutable_sfixed64();

  private:
  const ::google::protobuf::RepeatedField<::int64_t>& _internal_sfixed64() const;
  ::google::protobuf::RepeatedField<::int64_t>* _internal_mutable_sfixed64();

  public:
  // repeated float float = 11;
  int float__size() const;
  private:
  int _internal_float__size() const;

  public:
  void clear_float_() ;
  float float_(int index) const;
  void set_float_(int index, float value);
  void add_float_(float value);
  const ::google::protobuf::RepeatedField<float>& float_() const;
  ::google::protobuf::RepeatedField<float>* mutable_float_();

  private:
  const ::google::protobuf::RepeatedField<float>& _internal_float_() const;
  ::google::protobuf::RepeatedField<float>* _internal_mutable_float_();

  public:
  // repeated double double = 12;
  int double__size() const;
  private:
  int _internal_double__size() const;

  public:
  void clear_double_() ;
  double double_(int index) const;
  void set_double_(int index, double value);
  void add_double_(double value);
  const ::google::protobuf::RepeatedField<double>& double_() const;
  ::google::protobuf::RepeatedField<double>* mutable_double_();

  private:
  const ::google::protobuf::RepeatedField<double>& _internal_double_() const;
  ::google::protobuf::RepeatedField<double>* _internal_mutable_double_();

  public:
  // repeated bool bool = 13;
  int bool__size() const;
  private:
  int _internal_bool__size() const;

  public:
  void clear_bool_() ;
  bool bool_(int index) const;
  void set_bool_(int index, bool value);
  void add_bool_(bool value);
  const ::google::protobuf::RepeatedField<bool>& bool_() const;
  ::google::protobuf::RepeatedField<bool>* mutable_bool_();

  private:
  const ::google::protobuf::RepeatedField<bool>& _internal_bool_() const;
  ::google::protobuf::RepeatedField<bool>* _internal_mutable_bool_();

  public:
  // repeated string string = 14;
  int string_size() const;
  private:
  int _internal_string_size() const;

  public:
  void clear_string() ;
  const std::string& string(int index) const;
  std::string* mutable_string(int index);
  void set_string(int index, const std::string& value);
  void set_string(int index, std::string&& value);
  void set_string(int index, const char* value);
  void set_string(int index, const char* value, std::size_t size);
  void set_string(int index, absl::string_view value);
  std::string* add_string();
  void add_string(const std::string& value);
  void add_string(std::string&& value);
  void add_string(const char* value);
  void add_string(const char* value, std::size_t size);
  void add_string(absl::string_view value);
  const ::google::protobuf::RepeatedPtrField<std::string>& string() const;
  ::google::protobuf::RepeatedPtrField<std::string>* mutable_string();

  private:
  const ::google::protobuf::RepeatedPtrField<std::string>& _internal_string() const;
  ::google::protobuf::RepeatedPtrField<std::string>* _internal_mutable_string();

  public:
  // repeated bytes bytes = 15;
  int bytes_size() const;
  private:
  int _internal_bytes_size() const;

  public:
  void clear_bytes() ;
  const std::string& bytes(int index) const;
  std::string* mutable_bytes(int index);
  void set_bytes(int index, const std::string& value);
  void set_bytes(int index, std::string&& value);
  void set_bytes(int index, const char* value);
  void set_bytes(int index, const void* value, std::size_t size);
  void set_bytes(int index, absl::string_view value);
  std::string* add_bytes();
  void add_bytes(const std::string& value);
  void add_bytes(std::string&& value);
  void add_bytes(const char* value);
  void add_bytes(const void* value, std::size_t size);
  void add_bytes(absl::string_view value);
  const ::google::protobuf::RepeatedPtrField<std::string>& bytes() const;
  ::google::protobuf::RepeatedPtrField<std::string>* mutable_bytes();

  private:
  const ::google::protobuf::RepeatedPtrField<std::string>& _internal_bytes() const;
  ::google::protobuf::RepeatedPtrField<std::string>* _internal_mutable_bytes();

  public:
  // repeated .test.Enum enum = 16;
  int enum__size() const;
  private:
  int _internal_enum__size() const;

  public:
  void clear_enum_() ;
  public:
  ::test::Enum enum_(int index) const;
  void set_enum_(int index, ::test::Enum value);
  void add_enum_(::test::Enum value);
  const ::google::protobuf::RepeatedField<int>& enum_() const;
  ::google::protobuf::RepeatedField<int>* mutable_enum_();

  private:
  const ::google::protobuf::RepeatedField<int>& _internal_enum_() const;
  ::google::protobuf::RepeatedField<int>* _internal_mutable_enum_();

  public:
  // repeated .test.RepeatedMessage self_message = 17;
  int self_message_size() const;
  private:
  int _internal_self_message_size() const;

  public:
  void clear_self_message() ;
  ::test::RepeatedMessage* mutable_self_message(int index);
  ::google::protobuf::RepeatedPtrField<::test::RepeatedMessage>* mutable_self_message();

  private:
  const ::google::protobuf::RepeatedPtrField<::test::RepeatedMessage>& _internal_self_message() const;
  ::google::protobuf::RepeatedPtrField<::test::RepeatedMessage>* _internal_mutable_self_message();
  public:
  const ::test::RepeatedMessage& self_message(int index) const;
  ::test::RepeatedMessage* add_self_message();
  const ::google::protobuf::RepeatedPtrField<::test::RepeatedMessage>& self_message() const;
  // repeated .test.OtherMessage other_message = 18;
  int other_message_size() const;
  private:
  int _internal_other_message_size() const;

  public:
  void clear_other_message() ;
  ::test::OtherMessage* mutable_other_message(int index);
  ::google::protobuf::RepeatedPtrField<::test::OtherMessage>* mutable_other_message();

  private:
  const ::google::protobuf::RepeatedPtrField<::test::OtherMessage>& _internal_other_message() const;
  ::google::protobuf::RepeatedPtrField<::test::OtherMessage>* _internal_mutable_other_message();
  public:
  const ::test::OtherMessage& other_message(int index) const;
  ::test::OtherMessage* add_other_message();
  const ::google::protobuf::RepeatedPtrField<::test::OtherMessage>& other_message() const;
  // @@protoc_insertion_point(class_scope:test.RepeatedMessage)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      5, 18, 2,
      51, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_RepeatedMessage_default_instance_;

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
                          const RepeatedMessage& from_msg);
    ::google::protobuf::RepeatedField<::int32_t> int32_;
    mutable ::google::protobuf::internal::CachedSize _int32_cached_byte_size_;
    ::google::protobuf::RepeatedField<::int64_t> int64_;
    mutable ::google::protobuf::internal::CachedSize _int64_cached_byte_size_;
    ::google::protobuf::RepeatedField<::uint32_t> uint32_;
    mutable ::google::protobuf::internal::CachedSize _uint32_cached_byte_size_;
    ::google::protobuf::RepeatedField<::uint64_t> uint64_;
    mutable ::google::protobuf::internal::CachedSize _uint64_cached_byte_size_;
    ::google::protobuf::RepeatedField<::int32_t> sint32_;
    mutable ::google::protobuf::internal::CachedSize _sint32_cached_byte_size_;
    ::google::protobuf::RepeatedField<::int64_t> sint64_;
    mutable ::google::protobuf::internal::CachedSize _sint64_cached_byte_size_;
    ::google::protobuf::RepeatedField<::uint32_t> fixed32_;
    ::google::protobuf::RepeatedField<::uint64_t> fixed64_;
    ::google::protobuf::RepeatedField<::int32_t> sfixed32_;
    ::google::protobuf::RepeatedField<::int64_t> sfixed64_;
    ::google::protobuf::RepeatedField<float> float__;
    ::google::protobuf::RepeatedField<double> double__;
    ::google::protobuf::RepeatedField<bool> bool__;
    ::google::protobuf::RepeatedPtrField<std::string> string_;
    ::google::protobuf::RepeatedPtrField<std::string> bytes_;
    ::google::protobuf::RepeatedField<int> enum__;
    mutable ::google::protobuf::internal::CachedSize _enum__cached_byte_size_;
    ::google::protobuf::RepeatedPtrField< ::test::RepeatedMessage > self_message_;
    ::google::protobuf::RepeatedPtrField< ::test::OtherMessage > other_message_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class ParentMessage final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:test.ParentMessage) */ {
 public:
  inline ParentMessage() : ParentMessage(nullptr) {}
  ~ParentMessage() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR ParentMessage(
      ::google::protobuf::internal::ConstantInitialized);

  inline ParentMessage(const ParentMessage& from) : ParentMessage(nullptr, from) {}
  inline ParentMessage(ParentMessage&& from) noexcept
      : ParentMessage(nullptr, std::move(from)) {}
  inline ParentMessage& operator=(const ParentMessage& from) {
    CopyFrom(from);
    return *this;
  }
  inline ParentMessage& operator=(ParentMessage&& from) noexcept {
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

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const ParentMessage& default_instance() {
    return *internal_default_instance();
  }
  static inline const ParentMessage* internal_default_instance() {
    return reinterpret_cast<const ParentMessage*>(
        &_ParentMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 4;
  friend void swap(ParentMessage& a, ParentMessage& b) { a.Swap(&b); }
  inline void Swap(ParentMessage* other) {
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
  void UnsafeArenaSwap(ParentMessage* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  ParentMessage* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<ParentMessage>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const ParentMessage& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const ParentMessage& from) { ParentMessage::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
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
  void InternalSwap(ParentMessage* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "test.ParentMessage"; }

 protected:
  explicit ParentMessage(::google::protobuf::Arena* arena);
  ParentMessage(::google::protobuf::Arena* arena, const ParentMessage& from);
  ParentMessage(::google::protobuf::Arena* arena, ParentMessage&& from) noexcept
      : ParentMessage(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::Message::ClassData* GetClassData() const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------
  using ChildMessage = ParentMessage_ChildMessage;
  using ChildEnum = ParentMessage_ChildEnum;
  static constexpr ChildEnum VALUE_0 = ParentMessage_ChildEnum_VALUE_0;
  static constexpr ChildEnum VALUE_1 = ParentMessage_ChildEnum_VALUE_1;
  static inline bool ChildEnum_IsValid(int value) {
    return ParentMessage_ChildEnum_IsValid(value);
  }
  static constexpr ChildEnum ChildEnum_MIN = ParentMessage_ChildEnum_ChildEnum_MIN;
  static constexpr ChildEnum ChildEnum_MAX = ParentMessage_ChildEnum_ChildEnum_MAX;
  static constexpr int ChildEnum_ARRAYSIZE = ParentMessage_ChildEnum_ChildEnum_ARRAYSIZE;
  static inline const ::google::protobuf::EnumDescriptor* ChildEnum_descriptor() {
    return ParentMessage_ChildEnum_descriptor();
  }
  template <typename T>
  static inline const std::string& ChildEnum_Name(T value) {
    return ParentMessage_ChildEnum_Name(value);
  }
  static inline bool ChildEnum_Parse(absl::string_view name, ChildEnum* value) {
    return ParentMessage_ChildEnum_Parse(name, value);
  }

  // accessors -------------------------------------------------------
  enum : int {
    kChildMessageFieldNumber = 1,
    kChildEnumFieldNumber = 2,
  };
  // .test.ParentMessage.ChildMessage child_message = 1;
  bool has_child_message() const;
  void clear_child_message() ;
  const ::test::ParentMessage_ChildMessage& child_message() const;
  PROTOBUF_NODISCARD ::test::ParentMessage_ChildMessage* release_child_message();
  ::test::ParentMessage_ChildMessage* mutable_child_message();
  void set_allocated_child_message(::test::ParentMessage_ChildMessage* value);
  void unsafe_arena_set_allocated_child_message(::test::ParentMessage_ChildMessage* value);
  ::test::ParentMessage_ChildMessage* unsafe_arena_release_child_message();

  private:
  const ::test::ParentMessage_ChildMessage& _internal_child_message() const;
  ::test::ParentMessage_ChildMessage* _internal_mutable_child_message();

  public:
  // .test.ParentMessage.ChildEnum child_enum = 2;
  void clear_child_enum() ;
  ::test::ParentMessage_ChildEnum child_enum() const;
  void set_child_enum(::test::ParentMessage_ChildEnum value);

  private:
  ::test::ParentMessage_ChildEnum _internal_child_enum() const;
  void _internal_set_child_enum(::test::ParentMessage_ChildEnum value);

  public:
  // @@protoc_insertion_point(class_scope:test.ParentMessage)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      1, 2, 1,
      0, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_ParentMessage_default_instance_;

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
                          const ParentMessage& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    ::test::ParentMessage_ChildMessage* child_message_;
    int child_enum_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class OneOfMessage final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:test.OneOfMessage) */ {
 public:
  inline OneOfMessage() : OneOfMessage(nullptr) {}
  ~OneOfMessage() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR OneOfMessage(
      ::google::protobuf::internal::ConstantInitialized);

  inline OneOfMessage(const OneOfMessage& from) : OneOfMessage(nullptr, from) {}
  inline OneOfMessage(OneOfMessage&& from) noexcept
      : OneOfMessage(nullptr, std::move(from)) {}
  inline OneOfMessage& operator=(const OneOfMessage& from) {
    CopyFrom(from);
    return *this;
  }
  inline OneOfMessage& operator=(OneOfMessage&& from) noexcept {
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

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const OneOfMessage& default_instance() {
    return *internal_default_instance();
  }
  enum StringsCase {
    kString0 = 1,
    kString1 = 2,
    STRINGS_NOT_SET = 0,
  };
  enum MessagesCase {
    kMessage0 = 3,
    kMessage1 = 4,
    MESSAGES_NOT_SET = 0,
  };
  enum BytesCase {
    kBytes0 = 5,
    kBytes1 = 6,
    BYTES_NOT_SET = 0,
  };
  static inline const OneOfMessage* internal_default_instance() {
    return reinterpret_cast<const OneOfMessage*>(
        &_OneOfMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 5;
  friend void swap(OneOfMessage& a, OneOfMessage& b) { a.Swap(&b); }
  inline void Swap(OneOfMessage* other) {
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
  void UnsafeArenaSwap(OneOfMessage* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  OneOfMessage* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<OneOfMessage>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const OneOfMessage& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const OneOfMessage& from) { OneOfMessage::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
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
  void InternalSwap(OneOfMessage* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "test.OneOfMessage"; }

 protected:
  explicit OneOfMessage(::google::protobuf::Arena* arena);
  OneOfMessage(::google::protobuf::Arena* arena, const OneOfMessage& from);
  OneOfMessage(::google::protobuf::Arena* arena, OneOfMessage&& from) noexcept
      : OneOfMessage(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::Message::ClassData* GetClassData() const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kString0FieldNumber = 1,
    kString1FieldNumber = 2,
    kMessage0FieldNumber = 3,
    kMessage1FieldNumber = 4,
    kBytes0FieldNumber = 5,
    kBytes1FieldNumber = 6,
  };
  // string string_0 = 1;
  bool has_string_0() const;
  void clear_string_0() ;
  const std::string& string_0() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_string_0(Arg_&& arg, Args_... args);
  std::string* mutable_string_0();
  PROTOBUF_NODISCARD std::string* release_string_0();
  void set_allocated_string_0(std::string* value);

  private:
  const std::string& _internal_string_0() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_string_0(
      const std::string& value);
  std::string* _internal_mutable_string_0();

  public:
  // string string_1 = 2;
  bool has_string_1() const;
  void clear_string_1() ;
  const std::string& string_1() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_string_1(Arg_&& arg, Args_... args);
  std::string* mutable_string_1();
  PROTOBUF_NODISCARD std::string* release_string_1();
  void set_allocated_string_1(std::string* value);

  private:
  const std::string& _internal_string_1() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_string_1(
      const std::string& value);
  std::string* _internal_mutable_string_1();

  public:
  // .test.OtherMessage message_0 = 3;
  bool has_message_0() const;
  private:
  bool _internal_has_message_0() const;

  public:
  void clear_message_0() ;
  const ::test::OtherMessage& message_0() const;
  PROTOBUF_NODISCARD ::test::OtherMessage* release_message_0();
  ::test::OtherMessage* mutable_message_0();
  void set_allocated_message_0(::test::OtherMessage* value);
  void unsafe_arena_set_allocated_message_0(::test::OtherMessage* value);
  ::test::OtherMessage* unsafe_arena_release_message_0();

  private:
  const ::test::OtherMessage& _internal_message_0() const;
  ::test::OtherMessage* _internal_mutable_message_0();

  public:
  // .test.OtherMessage message_1 = 4;
  bool has_message_1() const;
  private:
  bool _internal_has_message_1() const;

  public:
  void clear_message_1() ;
  const ::test::OtherMessage& message_1() const;
  PROTOBUF_NODISCARD ::test::OtherMessage* release_message_1();
  ::test::OtherMessage* mutable_message_1();
  void set_allocated_message_1(::test::OtherMessage* value);
  void unsafe_arena_set_allocated_message_1(::test::OtherMessage* value);
  ::test::OtherMessage* unsafe_arena_release_message_1();

  private:
  const ::test::OtherMessage& _internal_message_1() const;
  ::test::OtherMessage* _internal_mutable_message_1();

  public:
  // bytes bytes_0 = 5;
  bool has_bytes_0() const;
  void clear_bytes_0() ;
  const std::string& bytes_0() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_bytes_0(Arg_&& arg, Args_... args);
  std::string* mutable_bytes_0();
  PROTOBUF_NODISCARD std::string* release_bytes_0();
  void set_allocated_bytes_0(std::string* value);

  private:
  const std::string& _internal_bytes_0() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_bytes_0(
      const std::string& value);
  std::string* _internal_mutable_bytes_0();

  public:
  // bytes bytes_1 = 6;
  bool has_bytes_1() const;
  void clear_bytes_1() ;
  const std::string& bytes_1() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_bytes_1(Arg_&& arg, Args_... args);
  std::string* mutable_bytes_1();
  PROTOBUF_NODISCARD std::string* release_bytes_1();
  void set_allocated_bytes_1(std::string* value);

  private:
  const std::string& _internal_bytes_1() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_bytes_1(
      const std::string& value);
  std::string* _internal_mutable_bytes_1();

  public:
  void clear_strings();
  StringsCase strings_case() const;
  void clear_messages();
  MessagesCase messages_case() const;
  void clear_bytes();
  BytesCase bytes_case() const;
  // @@protoc_insertion_point(class_scope:test.OneOfMessage)
 private:
  class _Internal;
  void set_has_string_0();
  void set_has_string_1();
  void set_has_message_0();
  void set_has_message_1();
  void set_has_bytes_0();
  void set_has_bytes_1();
  inline bool has_strings() const;
  inline void clear_has_strings();
  inline bool has_messages() const;
  inline void clear_has_messages();
  inline bool has_bytes() const;
  inline void clear_has_bytes();
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      0, 6, 2,
      42, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_OneOfMessage_default_instance_;

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
                          const OneOfMessage& from_msg);
    union StringsUnion {
      constexpr StringsUnion() : _constinit_{} {}
      ::google::protobuf::internal::ConstantInitialized _constinit_;
      ::google::protobuf::internal::ArenaStringPtr string_0_;
      ::google::protobuf::internal::ArenaStringPtr string_1_;
    } strings_;
    union MessagesUnion {
      constexpr MessagesUnion() : _constinit_{} {}
      ::google::protobuf::internal::ConstantInitialized _constinit_;
      ::test::OtherMessage* message_0_;
      ::test::OtherMessage* message_1_;
    } messages_;
    union BytesUnion {
      constexpr BytesUnion() : _constinit_{} {}
      ::google::protobuf::internal::ConstantInitialized _constinit_;
      ::google::protobuf::internal::ArenaStringPtr bytes_0_;
      ::google::protobuf::internal::ArenaStringPtr bytes_1_;
    } bytes_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    ::uint32_t _oneof_case_[3];
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class OldEnumMessage final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:test.OldEnumMessage) */ {
 public:
  inline OldEnumMessage() : OldEnumMessage(nullptr) {}
  ~OldEnumMessage() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR OldEnumMessage(
      ::google::protobuf::internal::ConstantInitialized);

  inline OldEnumMessage(const OldEnumMessage& from) : OldEnumMessage(nullptr, from) {}
  inline OldEnumMessage(OldEnumMessage&& from) noexcept
      : OldEnumMessage(nullptr, std::move(from)) {}
  inline OldEnumMessage& operator=(const OldEnumMessage& from) {
    CopyFrom(from);
    return *this;
  }
  inline OldEnumMessage& operator=(OldEnumMessage&& from) noexcept {
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

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const OldEnumMessage& default_instance() {
    return *internal_default_instance();
  }
  static inline const OldEnumMessage* internal_default_instance() {
    return reinterpret_cast<const OldEnumMessage*>(
        &_OldEnumMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 9;
  friend void swap(OldEnumMessage& a, OldEnumMessage& b) { a.Swap(&b); }
  inline void Swap(OldEnumMessage* other) {
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
  void UnsafeArenaSwap(OldEnumMessage* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  OldEnumMessage* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<OldEnumMessage>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const OldEnumMessage& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const OldEnumMessage& from) { OldEnumMessage::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
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
  void InternalSwap(OldEnumMessage* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "test.OldEnumMessage"; }

 protected:
  explicit OldEnumMessage(::google::protobuf::Arena* arena);
  OldEnumMessage(::google::protobuf::Arena* arena, const OldEnumMessage& from);
  OldEnumMessage(::google::protobuf::Arena* arena, OldEnumMessage&& from) noexcept
      : OldEnumMessage(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::Message::ClassData* GetClassData() const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------
  using OldEnum = OldEnumMessage_OldEnum;
  static constexpr OldEnum VALUE_0 = OldEnumMessage_OldEnum_VALUE_0;
  static constexpr OldEnum VALUE_1 = OldEnumMessage_OldEnum_VALUE_1;
  static inline bool OldEnum_IsValid(int value) {
    return OldEnumMessage_OldEnum_IsValid(value);
  }
  static constexpr OldEnum OldEnum_MIN = OldEnumMessage_OldEnum_OldEnum_MIN;
  static constexpr OldEnum OldEnum_MAX = OldEnumMessage_OldEnum_OldEnum_MAX;
  static constexpr int OldEnum_ARRAYSIZE = OldEnumMessage_OldEnum_OldEnum_ARRAYSIZE;
  static inline const ::google::protobuf::EnumDescriptor* OldEnum_descriptor() {
    return OldEnumMessage_OldEnum_descriptor();
  }
  template <typename T>
  static inline const std::string& OldEnum_Name(T value) {
    return OldEnumMessage_OldEnum_Name(value);
  }
  static inline bool OldEnum_Parse(absl::string_view name, OldEnum* value) {
    return OldEnumMessage_OldEnum_Parse(name, value);
  }

  // accessors -------------------------------------------------------
  enum : int {
    kRepeatedValueFieldNumber = 2,
    kMappedValueFieldNumber = 3,
    kValueFieldNumber = 1,
  };
  // repeated .test.OldEnumMessage.OldEnum repeated_value = 2;
  int repeated_value_size() const;
  private:
  int _internal_repeated_value_size() const;

  public:
  void clear_repeated_value() ;
  public:
  ::test::OldEnumMessage_OldEnum repeated_value(int index) const;
  void set_repeated_value(int index, ::test::OldEnumMessage_OldEnum value);
  void add_repeated_value(::test::OldEnumMessage_OldEnum value);
  const ::google::protobuf::RepeatedField<int>& repeated_value() const;
  ::google::protobuf::RepeatedField<int>* mutable_repeated_value();

  private:
  const ::google::protobuf::RepeatedField<int>& _internal_repeated_value() const;
  ::google::protobuf::RepeatedField<int>* _internal_mutable_repeated_value();

  public:
  // map<string, .test.OldEnumMessage.OldEnum> mapped_value = 3;
  int mapped_value_size() const;
  private:
  int _internal_mapped_value_size() const;

  public:
  void clear_mapped_value() ;
  const ::google::protobuf::Map<std::string, ::test::OldEnumMessage_OldEnum>& mapped_value() const;
  ::google::protobuf::Map<std::string, ::test::OldEnumMessage_OldEnum>* mutable_mapped_value();

  private:
  const ::google::protobuf::Map<std::string, ::test::OldEnumMessage_OldEnum>& _internal_mapped_value() const;
  ::google::protobuf::Map<std::string, ::test::OldEnumMessage_OldEnum>* _internal_mutable_mapped_value();

  public:
  // .test.OldEnumMessage.OldEnum value = 1;
  void clear_value() ;
  ::test::OldEnumMessage_OldEnum value() const;
  void set_value(::test::OldEnumMessage_OldEnum value);

  private:
  ::test::OldEnumMessage_OldEnum _internal_value() const;
  void _internal_set_value(::test::OldEnumMessage_OldEnum value);

  public:
  // @@protoc_insertion_point(class_scope:test.OldEnumMessage)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      1, 3, 1,
      40, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_OldEnumMessage_default_instance_;

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
                          const OldEnumMessage& from_msg);
    ::google::protobuf::RepeatedField<int> repeated_value_;
    mutable ::google::protobuf::internal::CachedSize _repeated_value_cached_byte_size_;
    ::google::protobuf::internal::MapField<OldEnumMessage_MappedValueEntry_DoNotUse, std::string, ::test::OldEnumMessage_OldEnum,
                      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
                      ::google::protobuf::internal::WireFormatLite::TYPE_ENUM>
        mapped_value_;
    int value_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class NewEnumMessage final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:test.NewEnumMessage) */ {
 public:
  inline NewEnumMessage() : NewEnumMessage(nullptr) {}
  ~NewEnumMessage() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR NewEnumMessage(
      ::google::protobuf::internal::ConstantInitialized);

  inline NewEnumMessage(const NewEnumMessage& from) : NewEnumMessage(nullptr, from) {}
  inline NewEnumMessage(NewEnumMessage&& from) noexcept
      : NewEnumMessage(nullptr, std::move(from)) {}
  inline NewEnumMessage& operator=(const NewEnumMessage& from) {
    CopyFrom(from);
    return *this;
  }
  inline NewEnumMessage& operator=(NewEnumMessage&& from) noexcept {
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

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const NewEnumMessage& default_instance() {
    return *internal_default_instance();
  }
  static inline const NewEnumMessage* internal_default_instance() {
    return reinterpret_cast<const NewEnumMessage*>(
        &_NewEnumMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 11;
  friend void swap(NewEnumMessage& a, NewEnumMessage& b) { a.Swap(&b); }
  inline void Swap(NewEnumMessage* other) {
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
  void UnsafeArenaSwap(NewEnumMessage* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  NewEnumMessage* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<NewEnumMessage>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const NewEnumMessage& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const NewEnumMessage& from) { NewEnumMessage::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
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
  void InternalSwap(NewEnumMessage* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "test.NewEnumMessage"; }

 protected:
  explicit NewEnumMessage(::google::protobuf::Arena* arena);
  NewEnumMessage(::google::protobuf::Arena* arena, const NewEnumMessage& from);
  NewEnumMessage(::google::protobuf::Arena* arena, NewEnumMessage&& from) noexcept
      : NewEnumMessage(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::Message::ClassData* GetClassData() const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------
  using NewEnum = NewEnumMessage_NewEnum;
  static constexpr NewEnum VALUE_0 = NewEnumMessage_NewEnum_VALUE_0;
  static constexpr NewEnum VALUE_1 = NewEnumMessage_NewEnum_VALUE_1;
  static constexpr NewEnum VALUE_2 = NewEnumMessage_NewEnum_VALUE_2;
  static constexpr NewEnum VALUE_3 = NewEnumMessage_NewEnum_VALUE_3;
  static inline bool NewEnum_IsValid(int value) {
    return NewEnumMessage_NewEnum_IsValid(value);
  }
  static constexpr NewEnum NewEnum_MIN = NewEnumMessage_NewEnum_NewEnum_MIN;
  static constexpr NewEnum NewEnum_MAX = NewEnumMessage_NewEnum_NewEnum_MAX;
  static constexpr int NewEnum_ARRAYSIZE = NewEnumMessage_NewEnum_NewEnum_ARRAYSIZE;
  static inline const ::google::protobuf::EnumDescriptor* NewEnum_descriptor() {
    return NewEnumMessage_NewEnum_descriptor();
  }
  template <typename T>
  static inline const std::string& NewEnum_Name(T value) {
    return NewEnumMessage_NewEnum_Name(value);
  }
  static inline bool NewEnum_Parse(absl::string_view name, NewEnum* value) {
    return NewEnumMessage_NewEnum_Parse(name, value);
  }

  // accessors -------------------------------------------------------
  enum : int {
    kRepeatedValueFieldNumber = 2,
    kMappedValueFieldNumber = 3,
    kValueFieldNumber = 1,
  };
  // repeated .test.NewEnumMessage.NewEnum repeated_value = 2;
  int repeated_value_size() const;
  private:
  int _internal_repeated_value_size() const;

  public:
  void clear_repeated_value() ;
  public:
  ::test::NewEnumMessage_NewEnum repeated_value(int index) const;
  void set_repeated_value(int index, ::test::NewEnumMessage_NewEnum value);
  void add_repeated_value(::test::NewEnumMessage_NewEnum value);
  const ::google::protobuf::RepeatedField<int>& repeated_value() const;
  ::google::protobuf::RepeatedField<int>* mutable_repeated_value();

  private:
  const ::google::protobuf::RepeatedField<int>& _internal_repeated_value() const;
  ::google::protobuf::RepeatedField<int>* _internal_mutable_repeated_value();

  public:
  // map<string, .test.NewEnumMessage.NewEnum> mapped_value = 3;
  int mapped_value_size() const;
  private:
  int _internal_mapped_value_size() const;

  public:
  void clear_mapped_value() ;
  const ::google::protobuf::Map<std::string, ::test::NewEnumMessage_NewEnum>& mapped_value() const;
  ::google::protobuf::Map<std::string, ::test::NewEnumMessage_NewEnum>* mutable_mapped_value();

  private:
  const ::google::protobuf::Map<std::string, ::test::NewEnumMessage_NewEnum>& _internal_mapped_value() const;
  ::google::protobuf::Map<std::string, ::test::NewEnumMessage_NewEnum>* _internal_mutable_mapped_value();

  public:
  // .test.NewEnumMessage.NewEnum value = 1;
  void clear_value() ;
  ::test::NewEnumMessage_NewEnum value() const;
  void set_value(::test::NewEnumMessage_NewEnum value);

  private:
  ::test::NewEnumMessage_NewEnum _internal_value() const;
  void _internal_set_value(::test::NewEnumMessage_NewEnum value);

  public:
  // @@protoc_insertion_point(class_scope:test.NewEnumMessage)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      1, 3, 1,
      40, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_NewEnumMessage_default_instance_;

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
                          const NewEnumMessage& from_msg);
    ::google::protobuf::RepeatedField<int> repeated_value_;
    mutable ::google::protobuf::internal::CachedSize _repeated_value_cached_byte_size_;
    ::google::protobuf::internal::MapField<NewEnumMessage_MappedValueEntry_DoNotUse, std::string, ::test::NewEnumMessage_NewEnum,
                      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
                      ::google::protobuf::internal::WireFormatLite::TYPE_ENUM>
        mapped_value_;
    int value_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class Message final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:test.Message) */ {
 public:
  inline Message() : Message(nullptr) {}
  ~Message() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR Message(
      ::google::protobuf::internal::ConstantInitialized);

  inline Message(const Message& from) : Message(nullptr, from) {}
  inline Message(Message&& from) noexcept
      : Message(nullptr, std::move(from)) {}
  inline Message& operator=(const Message& from) {
    CopyFrom(from);
    return *this;
  }
  inline Message& operator=(Message&& from) noexcept {
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

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const Message& default_instance() {
    return *internal_default_instance();
  }
  static inline const Message* internal_default_instance() {
    return reinterpret_cast<const Message*>(
        &_Message_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(Message& a, Message& b) { a.Swap(&b); }
  inline void Swap(Message* other) {
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
  void UnsafeArenaSwap(Message* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  Message* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<Message>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const Message& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const Message& from) { Message::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
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
  void InternalSwap(Message* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "test.Message"; }

 protected:
  explicit Message(::google::protobuf::Arena* arena);
  Message(::google::protobuf::Arena* arena, const Message& from);
  Message(::google::protobuf::Arena* arena, Message&& from) noexcept
      : Message(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::Message::ClassData* GetClassData() const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kStringFieldNumber = 14,
    kBytesFieldNumber = 15,
    kSelfMessageFieldNumber = 17,
    kOtherMessageFieldNumber = 18,
    kInt64FieldNumber = 2,
    kInt32FieldNumber = 1,
    kUint32FieldNumber = 3,
    kUint64FieldNumber = 4,
    kSint64FieldNumber = 6,
    kSint32FieldNumber = 5,
    kFixed32FieldNumber = 7,
    kFixed64FieldNumber = 8,
    kSfixed64FieldNumber = 10,
    kSfixed32FieldNumber = 9,
    kFloatFieldNumber = 11,
    kDoubleFieldNumber = 12,
    kBoolFieldNumber = 13,
    kEnumFieldNumber = 16,
  };
  // string string = 14;
  void clear_string() ;
  const std::string& string() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_string(Arg_&& arg, Args_... args);
  std::string* mutable_string();
  PROTOBUF_NODISCARD std::string* release_string();
  void set_allocated_string(std::string* value);

  private:
  const std::string& _internal_string() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_string(
      const std::string& value);
  std::string* _internal_mutable_string();

  public:
  // bytes bytes = 15;
  void clear_bytes() ;
  const std::string& bytes() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_bytes(Arg_&& arg, Args_... args);
  std::string* mutable_bytes();
  PROTOBUF_NODISCARD std::string* release_bytes();
  void set_allocated_bytes(std::string* value);

  private:
  const std::string& _internal_bytes() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_bytes(
      const std::string& value);
  std::string* _internal_mutable_bytes();

  public:
  // .test.Message self_message = 17;
  bool has_self_message() const;
  void clear_self_message() ;
  const ::test::Message& self_message() const;
  PROTOBUF_NODISCARD ::test::Message* release_self_message();
  ::test::Message* mutable_self_message();
  void set_allocated_self_message(::test::Message* value);
  void unsafe_arena_set_allocated_self_message(::test::Message* value);
  ::test::Message* unsafe_arena_release_self_message();

  private:
  const ::test::Message& _internal_self_message() const;
  ::test::Message* _internal_mutable_self_message();

  public:
  // .test.OtherMessage other_message = 18;
  bool has_other_message() const;
  void clear_other_message() ;
  const ::test::OtherMessage& other_message() const;
  PROTOBUF_NODISCARD ::test::OtherMessage* release_other_message();
  ::test::OtherMessage* mutable_other_message();
  void set_allocated_other_message(::test::OtherMessage* value);
  void unsafe_arena_set_allocated_other_message(::test::OtherMessage* value);
  ::test::OtherMessage* unsafe_arena_release_other_message();

  private:
  const ::test::OtherMessage& _internal_other_message() const;
  ::test::OtherMessage* _internal_mutable_other_message();

  public:
  // int64 int64 = 2;
  void clear_int64() ;
  ::int64_t int64() const;
  void set_int64(::int64_t value);

  private:
  ::int64_t _internal_int64() const;
  void _internal_set_int64(::int64_t value);

  public:
  // int32 int32 = 1;
  void clear_int32() ;
  ::int32_t int32() const;
  void set_int32(::int32_t value);

  private:
  ::int32_t _internal_int32() const;
  void _internal_set_int32(::int32_t value);

  public:
  // uint32 uint32 = 3;
  void clear_uint32() ;
  ::uint32_t uint32() const;
  void set_uint32(::uint32_t value);

  private:
  ::uint32_t _internal_uint32() const;
  void _internal_set_uint32(::uint32_t value);

  public:
  // uint64 uint64 = 4;
  void clear_uint64() ;
  ::uint64_t uint64() const;
  void set_uint64(::uint64_t value);

  private:
  ::uint64_t _internal_uint64() const;
  void _internal_set_uint64(::uint64_t value);

  public:
  // sint64 sint64 = 6;
  void clear_sint64() ;
  ::int64_t sint64() const;
  void set_sint64(::int64_t value);

  private:
  ::int64_t _internal_sint64() const;
  void _internal_set_sint64(::int64_t value);

  public:
  // sint32 sint32 = 5;
  void clear_sint32() ;
  ::int32_t sint32() const;
  void set_sint32(::int32_t value);

  private:
  ::int32_t _internal_sint32() const;
  void _internal_set_sint32(::int32_t value);

  public:
  // fixed32 fixed32 = 7;
  void clear_fixed32() ;
  ::uint32_t fixed32() const;
  void set_fixed32(::uint32_t value);

  private:
  ::uint32_t _internal_fixed32() const;
  void _internal_set_fixed32(::uint32_t value);

  public:
  // fixed64 fixed64 = 8;
  void clear_fixed64() ;
  ::uint64_t fixed64() const;
  void set_fixed64(::uint64_t value);

  private:
  ::uint64_t _internal_fixed64() const;
  void _internal_set_fixed64(::uint64_t value);

  public:
  // sfixed64 sfixed64 = 10;
  void clear_sfixed64() ;
  ::int64_t sfixed64() const;
  void set_sfixed64(::int64_t value);

  private:
  ::int64_t _internal_sfixed64() const;
  void _internal_set_sfixed64(::int64_t value);

  public:
  // sfixed32 sfixed32 = 9;
  void clear_sfixed32() ;
  ::int32_t sfixed32() const;
  void set_sfixed32(::int32_t value);

  private:
  ::int32_t _internal_sfixed32() const;
  void _internal_set_sfixed32(::int32_t value);

  public:
  // float float = 11;
  void clear_float_() ;
  float float_() const;
  void set_float_(float value);

  private:
  float _internal_float_() const;
  void _internal_set_float_(float value);

  public:
  // double double = 12;
  void clear_double_() ;
  double double_() const;
  void set_double_(double value);

  private:
  double _internal_double_() const;
  void _internal_set_double_(double value);

  public:
  // bool bool = 13;
  void clear_bool_() ;
  bool bool_() const;
  void set_bool_(bool value);

  private:
  bool _internal_bool_() const;
  void _internal_set_bool_(bool value);

  public:
  // .test.Enum enum = 16;
  void clear_enum_() ;
  ::test::Enum enum_() const;
  void set_enum_(::test::Enum value);

  private:
  ::test::Enum _internal_enum_() const;
  void _internal_set_enum_(::test::Enum value);

  public:
  // @@protoc_insertion_point(class_scope:test.Message)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      5, 18, 2,
      43, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_Message_default_instance_;

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
                          const Message& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    ::google::protobuf::internal::ArenaStringPtr string_;
    ::google::protobuf::internal::ArenaStringPtr bytes_;
    ::test::Message* self_message_;
    ::test::OtherMessage* other_message_;
    ::int64_t int64_;
    ::int32_t int32_;
    ::uint32_t uint32_;
    ::uint64_t uint64_;
    ::int64_t sint64_;
    ::int32_t sint32_;
    ::uint32_t fixed32_;
    ::uint64_t fixed64_;
    ::int64_t sfixed64_;
    ::int32_t sfixed32_;
    float float__;
    double double__;
    bool bool__;
    int enum__;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class MapMessage_StringToMessageEntry_DoNotUse final
    : public ::google::protobuf::internal::MapEntry<
          MapMessage_StringToMessageEntry_DoNotUse, std::string, ::test::OtherMessage,
          ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
          ::google::protobuf::internal::WireFormatLite::TYPE_MESSAGE> {
 public:
  using SuperType = ::google::protobuf::internal::MapEntry<
      MapMessage_StringToMessageEntry_DoNotUse, std::string, ::test::OtherMessage,
      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
      ::google::protobuf::internal::WireFormatLite::TYPE_MESSAGE>;
  MapMessage_StringToMessageEntry_DoNotUse();
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR MapMessage_StringToMessageEntry_DoNotUse(
      ::google::protobuf::internal::ConstantInitialized);
  explicit MapMessage_StringToMessageEntry_DoNotUse(::google::protobuf::Arena* arena);
  static const MapMessage_StringToMessageEntry_DoNotUse* internal_default_instance() {
    return reinterpret_cast<const MapMessage_StringToMessageEntry_DoNotUse*>(
        &_MapMessage_StringToMessageEntry_DoNotUse_default_instance_);
  }
const ::google::protobuf::Message::ClassData* GetClassData() const final;
  friend struct ::TableStruct_test_2eproto;
};
// -------------------------------------------------------------------

class MapMessage final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:test.MapMessage) */ {
 public:
  inline MapMessage() : MapMessage(nullptr) {}
  ~MapMessage() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR MapMessage(
      ::google::protobuf::internal::ConstantInitialized);

  inline MapMessage(const MapMessage& from) : MapMessage(nullptr, from) {}
  inline MapMessage(MapMessage&& from) noexcept
      : MapMessage(nullptr, std::move(from)) {}
  inline MapMessage& operator=(const MapMessage& from) {
    CopyFrom(from);
    return *this;
  }
  inline MapMessage& operator=(MapMessage&& from) noexcept {
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

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const MapMessage& default_instance() {
    return *internal_default_instance();
  }
  static inline const MapMessage* internal_default_instance() {
    return reinterpret_cast<const MapMessage*>(
        &_MapMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 20;
  friend void swap(MapMessage& a, MapMessage& b) { a.Swap(&b); }
  inline void Swap(MapMessage* other) {
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
  void UnsafeArenaSwap(MapMessage* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  MapMessage* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::Message::DefaultConstruct<MapMessage>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const MapMessage& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const MapMessage& from) { MapMessage::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
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
  void InternalSwap(MapMessage* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "test.MapMessage"; }

 protected:
  explicit MapMessage(::google::protobuf::Arena* arena);
  MapMessage(::google::protobuf::Arena* arena, const MapMessage& from);
  MapMessage(::google::protobuf::Arena* arena, MapMessage&& from) noexcept
      : MapMessage(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::Message::ClassData* GetClassData() const final;

 public:
  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kStringToStringFieldNumber = 1,
    kStringToNumberFieldNumber = 2,
    kStringToSignedLongFieldNumber = 3,
    kStringToUnsignedLongFieldNumber = 4,
    kStringToDoubleFieldNumber = 5,
    kStringToMessageFieldNumber = 6,
    kIntToStringFieldNumber = 7,
    kLongToStringFieldNumber = 8,
  };
  // map<string, string> stringToString = 1;
  int stringtostring_size() const;
  private:
  int _internal_stringtostring_size() const;

  public:
  void clear_stringtostring() ;
  const ::google::protobuf::Map<std::string, std::string>& stringtostring() const;
  ::google::protobuf::Map<std::string, std::string>* mutable_stringtostring();

  private:
  const ::google::protobuf::Map<std::string, std::string>& _internal_stringtostring() const;
  ::google::protobuf::Map<std::string, std::string>* _internal_mutable_stringtostring();

  public:
  // map<string, int32> stringToNumber = 2;
  int stringtonumber_size() const;
  private:
  int _internal_stringtonumber_size() const;

  public:
  void clear_stringtonumber() ;
  const ::google::protobuf::Map<std::string, ::int32_t>& stringtonumber() const;
  ::google::protobuf::Map<std::string, ::int32_t>* mutable_stringtonumber();

  private:
  const ::google::protobuf::Map<std::string, ::int32_t>& _internal_stringtonumber() const;
  ::google::protobuf::Map<std::string, ::int32_t>* _internal_mutable_stringtonumber();

  public:
  // map<string, int64> stringToSignedLong = 3;
  int stringtosignedlong_size() const;
  private:
  int _internal_stringtosignedlong_size() const;

  public:
  void clear_stringtosignedlong() ;
  const ::google::protobuf::Map<std::string, ::int64_t>& stringtosignedlong() const;
  ::google::protobuf::Map<std::string, ::int64_t>* mutable_stringtosignedlong();

  private:
  const ::google::protobuf::Map<std::string, ::int64_t>& _internal_stringtosignedlong() const;
  ::google::protobuf::Map<std::string, ::int64_t>* _internal_mutable_stringtosignedlong();

  public:
  // map<string, uint64> stringToUnsignedLong = 4;
  int stringtounsignedlong_size() const;
  private:
  int _internal_stringtounsignedlong_size() const;

  public:
  void clear_stringtounsignedlong() ;
  const ::google::protobuf::Map<std::string, ::uint64_t>& stringtounsignedlong() const;
  ::google::protobuf::Map<std::string, ::uint64_t>* mutable_stringtounsignedlong();

  private:
  const ::google::protobuf::Map<std::string, ::uint64_t>& _internal_stringtounsignedlong() const;
  ::google::protobuf::Map<std::string, ::uint64_t>* _internal_mutable_stringtounsignedlong();

  public:
  // map<string, double> stringToDouble = 5;
  int stringtodouble_size() const;
  private:
  int _internal_stringtodouble_size() const;

  public:
  void clear_stringtodouble() ;
  const ::google::protobuf::Map<std::string, double>& stringtodouble() const;
  ::google::protobuf::Map<std::string, double>* mutable_stringtodouble();

  private:
  const ::google::protobuf::Map<std::string, double>& _internal_stringtodouble() const;
  ::google::protobuf::Map<std::string, double>* _internal_mutable_stringtodouble();

  public:
  // map<string, .test.OtherMessage> stringToMessage = 6;
  int stringtomessage_size() const;
  private:
  int _internal_stringtomessage_size() const;

  public:
  void clear_stringtomessage() ;
  const ::google::protobuf::Map<std::string, ::test::OtherMessage>& stringtomessage() const;
  ::google::protobuf::Map<std::string, ::test::OtherMessage>* mutable_stringtomessage();

  private:
  const ::google::protobuf::Map<std::string, ::test::OtherMessage>& _internal_stringtomessage() const;
  ::google::protobuf::Map<std::string, ::test::OtherMessage>* _internal_mutable_stringtomessage();

  public:
  // map<int32, string> intToString = 7;
  int inttostring_size() const;
  private:
  int _internal_inttostring_size() const;

  public:
  void clear_inttostring() ;
  const ::google::protobuf::Map<::int32_t, std::string>& inttostring() const;
  ::google::protobuf::Map<::int32_t, std::string>* mutable_inttostring();

  private:
  const ::google::protobuf::Map<::int32_t, std::string>& _internal_inttostring() const;
  ::google::protobuf::Map<::int32_t, std::string>* _internal_mutable_inttostring();

  public:
  // map<int64, string> longToString = 8;
  int longtostring_size() const;
  private:
  int _internal_longtostring_size() const;

  public:
  void clear_longtostring() ;
  const ::google::protobuf::Map<::int64_t, std::string>& longtostring() const;
  ::google::protobuf::Map<::int64_t, std::string>* mutable_longtostring();

  private:
  const ::google::protobuf::Map<::int64_t, std::string>& _internal_longtostring() const;
  ::google::protobuf::Map<::int64_t, std::string>* _internal_mutable_longtostring();

  public:
  // @@protoc_insertion_point(class_scope:test.MapMessage)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      0, 8, 9,
      150, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_MapMessage_default_instance_;

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
                          const MapMessage& from_msg);
    ::google::protobuf::internal::MapField<MapMessage_StringToStringEntry_DoNotUse, std::string, std::string,
                      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
                      ::google::protobuf::internal::WireFormatLite::TYPE_STRING>
        stringtostring_;
    ::google::protobuf::internal::MapField<MapMessage_StringToNumberEntry_DoNotUse, std::string, ::int32_t,
                      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
                      ::google::protobuf::internal::WireFormatLite::TYPE_INT32>
        stringtonumber_;
    ::google::protobuf::internal::MapField<MapMessage_StringToSignedLongEntry_DoNotUse, std::string, ::int64_t,
                      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
                      ::google::protobuf::internal::WireFormatLite::TYPE_INT64>
        stringtosignedlong_;
    ::google::protobuf::internal::MapField<MapMessage_StringToUnsignedLongEntry_DoNotUse, std::string, ::uint64_t,
                      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
                      ::google::protobuf::internal::WireFormatLite::TYPE_UINT64>
        stringtounsignedlong_;
    ::google::protobuf::internal::MapField<MapMessage_StringToDoubleEntry_DoNotUse, std::string, double,
                      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
                      ::google::protobuf::internal::WireFormatLite::TYPE_DOUBLE>
        stringtodouble_;
    ::google::protobuf::internal::MapField<MapMessage_StringToMessageEntry_DoNotUse, std::string, ::test::OtherMessage,
                      ::google::protobuf::internal::WireFormatLite::TYPE_STRING,
                      ::google::protobuf::internal::WireFormatLite::TYPE_MESSAGE>
        stringtomessage_;
    ::google::protobuf::internal::MapField<MapMessage_IntToStringEntry_DoNotUse, ::int32_t, std::string,
                      ::google::protobuf::internal::WireFormatLite::TYPE_INT32,
                      ::google::protobuf::internal::WireFormatLite::TYPE_STRING>
        inttostring_;
    ::google::protobuf::internal::MapField<MapMessage_LongToStringEntry_DoNotUse, ::int64_t, std::string,
                      ::google::protobuf::internal::WireFormatLite::TYPE_INT64,
                      ::google::protobuf::internal::WireFormatLite::TYPE_STRING>
        longtostring_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_test_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// OtherMessage

// string value = 1;
inline void OtherMessage::clear_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.value_.ClearToEmpty();
}
inline const std::string& OtherMessage::value() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.OtherMessage.value)
  return _internal_value();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void OtherMessage::set_value(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.value_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:test.OtherMessage.value)
}
inline std::string* OtherMessage::mutable_value() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_value();
  // @@protoc_insertion_point(field_mutable:test.OtherMessage.value)
  return _s;
}
inline const std::string& OtherMessage::_internal_value() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.value_.Get();
}
inline void OtherMessage::_internal_set_value(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.value_.Set(value, GetArena());
}
inline std::string* OtherMessage::_internal_mutable_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.value_.Mutable( GetArena());
}
inline std::string* OtherMessage::release_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:test.OtherMessage.value)
  return _impl_.value_.Release();
}
inline void OtherMessage::set_allocated_value(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.value_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.value_.IsDefault()) {
          _impl_.value_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:test.OtherMessage.value)
}

// -------------------------------------------------------------------

// Message

// int32 int32 = 1;
inline void Message::clear_int32() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.int32_ = 0;
}
inline ::int32_t Message::int32() const {
  // @@protoc_insertion_point(field_get:test.Message.int32)
  return _internal_int32();
}
inline void Message::set_int32(::int32_t value) {
  _internal_set_int32(value);
  // @@protoc_insertion_point(field_set:test.Message.int32)
}
inline ::int32_t Message::_internal_int32() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.int32_;
}
inline void Message::_internal_set_int32(::int32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.int32_ = value;
}

// int64 int64 = 2;
inline void Message::clear_int64() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.int64_ = ::int64_t{0};
}
inline ::int64_t Message::int64() const {
  // @@protoc_insertion_point(field_get:test.Message.int64)
  return _internal_int64();
}
inline void Message::set_int64(::int64_t value) {
  _internal_set_int64(value);
  // @@protoc_insertion_point(field_set:test.Message.int64)
}
inline ::int64_t Message::_internal_int64() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.int64_;
}
inline void Message::_internal_set_int64(::int64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.int64_ = value;
}

// uint32 uint32 = 3;
inline void Message::clear_uint32() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.uint32_ = 0u;
}
inline ::uint32_t Message::uint32() const {
  // @@protoc_insertion_point(field_get:test.Message.uint32)
  return _internal_uint32();
}
inline void Message::set_uint32(::uint32_t value) {
  _internal_set_uint32(value);
  // @@protoc_insertion_point(field_set:test.Message.uint32)
}
inline ::uint32_t Message::_internal_uint32() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.uint32_;
}
inline void Message::_internal_set_uint32(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.uint32_ = value;
}

// uint64 uint64 = 4;
inline void Message::clear_uint64() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.uint64_ = ::uint64_t{0u};
}
inline ::uint64_t Message::uint64() const {
  // @@protoc_insertion_point(field_get:test.Message.uint64)
  return _internal_uint64();
}
inline void Message::set_uint64(::uint64_t value) {
  _internal_set_uint64(value);
  // @@protoc_insertion_point(field_set:test.Message.uint64)
}
inline ::uint64_t Message::_internal_uint64() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.uint64_;
}
inline void Message::_internal_set_uint64(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.uint64_ = value;
}

// sint32 sint32 = 5;
inline void Message::clear_sint32() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.sint32_ = 0;
}
inline ::int32_t Message::sint32() const {
  // @@protoc_insertion_point(field_get:test.Message.sint32)
  return _internal_sint32();
}
inline void Message::set_sint32(::int32_t value) {
  _internal_set_sint32(value);
  // @@protoc_insertion_point(field_set:test.Message.sint32)
}
inline ::int32_t Message::_internal_sint32() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.sint32_;
}
inline void Message::_internal_set_sint32(::int32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.sint32_ = value;
}

// sint64 sint64 = 6;
inline void Message::clear_sint64() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.sint64_ = ::int64_t{0};
}
inline ::int64_t Message::sint64() const {
  // @@protoc_insertion_point(field_get:test.Message.sint64)
  return _internal_sint64();
}
inline void Message::set_sint64(::int64_t value) {
  _internal_set_sint64(value);
  // @@protoc_insertion_point(field_set:test.Message.sint64)
}
inline ::int64_t Message::_internal_sint64() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.sint64_;
}
inline void Message::_internal_set_sint64(::int64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.sint64_ = value;
}

// fixed32 fixed32 = 7;
inline void Message::clear_fixed32() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.fixed32_ = 0u;
}
inline ::uint32_t Message::fixed32() const {
  // @@protoc_insertion_point(field_get:test.Message.fixed32)
  return _internal_fixed32();
}
inline void Message::set_fixed32(::uint32_t value) {
  _internal_set_fixed32(value);
  // @@protoc_insertion_point(field_set:test.Message.fixed32)
}
inline ::uint32_t Message::_internal_fixed32() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.fixed32_;
}
inline void Message::_internal_set_fixed32(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.fixed32_ = value;
}

// fixed64 fixed64 = 8;
inline void Message::clear_fixed64() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.fixed64_ = ::uint64_t{0u};
}
inline ::uint64_t Message::fixed64() const {
  // @@protoc_insertion_point(field_get:test.Message.fixed64)
  return _internal_fixed64();
}
inline void Message::set_fixed64(::uint64_t value) {
  _internal_set_fixed64(value);
  // @@protoc_insertion_point(field_set:test.Message.fixed64)
}
inline ::uint64_t Message::_internal_fixed64() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.fixed64_;
}
inline void Message::_internal_set_fixed64(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.fixed64_ = value;
}

// sfixed32 sfixed32 = 9;
inline void Message::clear_sfixed32() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.sfixed32_ = 0;
}
inline ::int32_t Message::sfixed32() const {
  // @@protoc_insertion_point(field_get:test.Message.sfixed32)
  return _internal_sfixed32();
}
inline void Message::set_sfixed32(::int32_t value) {
  _internal_set_sfixed32(value);
  // @@protoc_insertion_point(field_set:test.Message.sfixed32)
}
inline ::int32_t Message::_internal_sfixed32() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.sfixed32_;
}
inline void Message::_internal_set_sfixed32(::int32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.sfixed32_ = value;
}

// sfixed64 sfixed64 = 10;
inline void Message::clear_sfixed64() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.sfixed64_ = ::int64_t{0};
}
inline ::int64_t Message::sfixed64() const {
  // @@protoc_insertion_point(field_get:test.Message.sfixed64)
  return _internal_sfixed64();
}
inline void Message::set_sfixed64(::int64_t value) {
  _internal_set_sfixed64(value);
  // @@protoc_insertion_point(field_set:test.Message.sfixed64)
}
inline ::int64_t Message::_internal_sfixed64() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.sfixed64_;
}
inline void Message::_internal_set_sfixed64(::int64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.sfixed64_ = value;
}

// float float = 11;
inline void Message::clear_float_() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.float__ = 0;
}
inline float Message::float_() const {
  // @@protoc_insertion_point(field_get:test.Message.float)
  return _internal_float_();
}
inline void Message::set_float_(float value) {
  _internal_set_float_(value);
  // @@protoc_insertion_point(field_set:test.Message.float)
}
inline float Message::_internal_float_() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.float__;
}
inline void Message::_internal_set_float_(float value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.float__ = value;
}

// double double = 12;
inline void Message::clear_double_() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.double__ = 0;
}
inline double Message::double_() const {
  // @@protoc_insertion_point(field_get:test.Message.double)
  return _internal_double_();
}
inline void Message::set_double_(double value) {
  _internal_set_double_(value);
  // @@protoc_insertion_point(field_set:test.Message.double)
}
inline double Message::_internal_double_() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.double__;
}
inline void Message::_internal_set_double_(double value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.double__ = value;
}

// bool bool = 13;
inline void Message::clear_bool_() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.bool__ = false;
}
inline bool Message::bool_() const {
  // @@protoc_insertion_point(field_get:test.Message.bool)
  return _internal_bool_();
}
inline void Message::set_bool_(bool value) {
  _internal_set_bool_(value);
  // @@protoc_insertion_point(field_set:test.Message.bool)
}
inline bool Message::_internal_bool_() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.bool__;
}
inline void Message::_internal_set_bool_(bool value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.bool__ = value;
}

// string string = 14;
inline void Message::clear_string() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.string_.ClearToEmpty();
}
inline const std::string& Message::string() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.Message.string)
  return _internal_string();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void Message::set_string(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.string_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:test.Message.string)
}
inline std::string* Message::mutable_string() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_string();
  // @@protoc_insertion_point(field_mutable:test.Message.string)
  return _s;
}
inline const std::string& Message::_internal_string() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.string_.Get();
}
inline void Message::_internal_set_string(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.string_.Set(value, GetArena());
}
inline std::string* Message::_internal_mutable_string() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.string_.Mutable( GetArena());
}
inline std::string* Message::release_string() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:test.Message.string)
  return _impl_.string_.Release();
}
inline void Message::set_allocated_string(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.string_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.string_.IsDefault()) {
          _impl_.string_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:test.Message.string)
}

// bytes bytes = 15;
inline void Message::clear_bytes() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.bytes_.ClearToEmpty();
}
inline const std::string& Message::bytes() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.Message.bytes)
  return _internal_bytes();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void Message::set_bytes(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.bytes_.SetBytes(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:test.Message.bytes)
}
inline std::string* Message::mutable_bytes() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_bytes();
  // @@protoc_insertion_point(field_mutable:test.Message.bytes)
  return _s;
}
inline const std::string& Message::_internal_bytes() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.bytes_.Get();
}
inline void Message::_internal_set_bytes(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.bytes_.Set(value, GetArena());
}
inline std::string* Message::_internal_mutable_bytes() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.bytes_.Mutable( GetArena());
}
inline std::string* Message::release_bytes() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:test.Message.bytes)
  return _impl_.bytes_.Release();
}
inline void Message::set_allocated_bytes(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.bytes_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.bytes_.IsDefault()) {
          _impl_.bytes_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:test.Message.bytes)
}

// .test.Enum enum = 16;
inline void Message::clear_enum_() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.enum__ = 0;
}
inline ::test::Enum Message::enum_() const {
  // @@protoc_insertion_point(field_get:test.Message.enum)
  return _internal_enum_();
}
inline void Message::set_enum_(::test::Enum value) {
  _internal_set_enum_(value);
  // @@protoc_insertion_point(field_set:test.Message.enum)
}
inline ::test::Enum Message::_internal_enum_() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return static_cast<::test::Enum>(_impl_.enum__);
}
inline void Message::_internal_set_enum_(::test::Enum value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.enum__ = value;
}

// .test.Message self_message = 17;
inline bool Message::has_self_message() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.self_message_ != nullptr);
  return value;
}
inline void Message::clear_self_message() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.self_message_ != nullptr) _impl_.self_message_->Clear();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline const ::test::Message& Message::_internal_self_message() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::test::Message* p = _impl_.self_message_;
  return p != nullptr ? *p : reinterpret_cast<const ::test::Message&>(::test::_Message_default_instance_);
}
inline const ::test::Message& Message::self_message() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.Message.self_message)
  return _internal_self_message();
}
inline void Message::unsafe_arena_set_allocated_self_message(::test::Message* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.self_message_);
  }
  _impl_.self_message_ = reinterpret_cast<::test::Message*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:test.Message.self_message)
}
inline ::test::Message* Message::release_self_message() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::test::Message* released = _impl_.self_message_;
  _impl_.self_message_ = nullptr;
#ifdef PROTOBUF_FORCE_COPY_IN_RELEASE
  auto* old = reinterpret_cast<::google::protobuf::MessageLite*>(released);
  released = ::google::protobuf::internal::DuplicateIfNonNull(released);
  if (GetArena() == nullptr) {
    delete old;
  }
#else   // PROTOBUF_FORCE_COPY_IN_RELEASE
  if (GetArena() != nullptr) {
    released = ::google::protobuf::internal::DuplicateIfNonNull(released);
  }
#endif  // !PROTOBUF_FORCE_COPY_IN_RELEASE
  return released;
}
inline ::test::Message* Message::unsafe_arena_release_self_message() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:test.Message.self_message)

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::test::Message* temp = _impl_.self_message_;
  _impl_.self_message_ = nullptr;
  return temp;
}
inline ::test::Message* Message::_internal_mutable_self_message() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.self_message_ == nullptr) {
    auto* p = ::google::protobuf::Message::DefaultConstruct<::test::Message>(GetArena());
    _impl_.self_message_ = reinterpret_cast<::test::Message*>(p);
  }
  return _impl_.self_message_;
}
inline ::test::Message* Message::mutable_self_message() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000001u;
  ::test::Message* _msg = _internal_mutable_self_message();
  // @@protoc_insertion_point(field_mutable:test.Message.self_message)
  return _msg;
}
inline void Message::set_allocated_self_message(::test::Message* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete (_impl_.self_message_);
  }

  if (value != nullptr) {
    ::google::protobuf::Arena* submessage_arena = (value)->GetArena();
    if (message_arena != submessage_arena) {
      value = ::google::protobuf::internal::GetOwnedMessage(message_arena, value, submessage_arena);
    }
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }

  _impl_.self_message_ = reinterpret_cast<::test::Message*>(value);
  // @@protoc_insertion_point(field_set_allocated:test.Message.self_message)
}

// .test.OtherMessage other_message = 18;
inline bool Message::has_other_message() const {
  bool value = (_impl_._has_bits_[0] & 0x00000002u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.other_message_ != nullptr);
  return value;
}
inline void Message::clear_other_message() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.other_message_ != nullptr) _impl_.other_message_->Clear();
  _impl_._has_bits_[0] &= ~0x00000002u;
}
inline const ::test::OtherMessage& Message::_internal_other_message() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::test::OtherMessage* p = _impl_.other_message_;
  return p != nullptr ? *p : reinterpret_cast<const ::test::OtherMessage&>(::test::_OtherMessage_default_instance_);
}
inline const ::test::OtherMessage& Message::other_message() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.Message.other_message)
  return _internal_other_message();
}
inline void Message::unsafe_arena_set_allocated_other_message(::test::OtherMessage* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.other_message_);
  }
  _impl_.other_message_ = reinterpret_cast<::test::OtherMessage*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000002u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000002u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:test.Message.other_message)
}
inline ::test::OtherMessage* Message::release_other_message() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000002u;
  ::test::OtherMessage* released = _impl_.other_message_;
  _impl_.other_message_ = nullptr;
#ifdef PROTOBUF_FORCE_COPY_IN_RELEASE
  auto* old = reinterpret_cast<::google::protobuf::MessageLite*>(released);
  released = ::google::protobuf::internal::DuplicateIfNonNull(released);
  if (GetArena() == nullptr) {
    delete old;
  }
#else   // PROTOBUF_FORCE_COPY_IN_RELEASE
  if (GetArena() != nullptr) {
    released = ::google::protobuf::internal::DuplicateIfNonNull(released);
  }
#endif  // !PROTOBUF_FORCE_COPY_IN_RELEASE
  return released;
}
inline ::test::OtherMessage* Message::unsafe_arena_release_other_message() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:test.Message.other_message)

  _impl_._has_bits_[0] &= ~0x00000002u;
  ::test::OtherMessage* temp = _impl_.other_message_;
  _impl_.other_message_ = nullptr;
  return temp;
}
inline ::test::OtherMessage* Message::_internal_mutable_other_message() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.other_message_ == nullptr) {
    auto* p = ::google::protobuf::Message::DefaultConstruct<::test::OtherMessage>(GetArena());
    _impl_.other_message_ = reinterpret_cast<::test::OtherMessage*>(p);
  }
  return _impl_.other_message_;
}
inline ::test::OtherMessage* Message::mutable_other_message() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000002u;
  ::test::OtherMessage* _msg = _internal_mutable_other_message();
  // @@protoc_insertion_point(field_mutable:test.Message.other_message)
  return _msg;
}
inline void Message::set_allocated_other_message(::test::OtherMessage* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete (_impl_.other_message_);
  }

  if (value != nullptr) {
    ::google::protobuf::Arena* submessage_arena = (value)->GetArena();
    if (message_arena != submessage_arena) {
      value = ::google::protobuf::internal::GetOwnedMessage(message_arena, value, submessage_arena);
    }
    _impl_._has_bits_[0] |= 0x00000002u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000002u;
  }

  _impl_.other_message_ = reinterpret_cast<::test::OtherMessage*>(value);
  // @@protoc_insertion_point(field_set_allocated:test.Message.other_message)
}

// -------------------------------------------------------------------

// RepeatedMessage

// repeated int32 int32 = 1;
inline int RepeatedMessage::_internal_int32_size() const {
  return _internal_int32().size();
}
inline int RepeatedMessage::int32_size() const {
  return _internal_int32_size();
}
inline void RepeatedMessage::clear_int32() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.int32_.Clear();
}
inline ::int32_t RepeatedMessage::int32(int index) const {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.int32)
  return _internal_int32().Get(index);
}
inline void RepeatedMessage::set_int32(int index, ::int32_t value) {
  _internal_mutable_int32()->Set(index, value);
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.int32)
}
inline void RepeatedMessage::add_int32(::int32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_int32()->Add(value);
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.int32)
}
inline const ::google::protobuf::RepeatedField<::int32_t>& RepeatedMessage::int32() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.int32)
  return _internal_int32();
}
inline ::google::protobuf::RepeatedField<::int32_t>* RepeatedMessage::mutable_int32()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.int32)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_int32();
}
inline const ::google::protobuf::RepeatedField<::int32_t>&
RepeatedMessage::_internal_int32() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.int32_;
}
inline ::google::protobuf::RepeatedField<::int32_t>* RepeatedMessage::_internal_mutable_int32() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.int32_;
}

// repeated int64 int64 = 2;
inline int RepeatedMessage::_internal_int64_size() const {
  return _internal_int64().size();
}
inline int RepeatedMessage::int64_size() const {
  return _internal_int64_size();
}
inline void RepeatedMessage::clear_int64() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.int64_.Clear();
}
inline ::int64_t RepeatedMessage::int64(int index) const {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.int64)
  return _internal_int64().Get(index);
}
inline void RepeatedMessage::set_int64(int index, ::int64_t value) {
  _internal_mutable_int64()->Set(index, value);
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.int64)
}
inline void RepeatedMessage::add_int64(::int64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_int64()->Add(value);
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.int64)
}
inline const ::google::protobuf::RepeatedField<::int64_t>& RepeatedMessage::int64() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.int64)
  return _internal_int64();
}
inline ::google::protobuf::RepeatedField<::int64_t>* RepeatedMessage::mutable_int64()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.int64)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_int64();
}
inline const ::google::protobuf::RepeatedField<::int64_t>&
RepeatedMessage::_internal_int64() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.int64_;
}
inline ::google::protobuf::RepeatedField<::int64_t>* RepeatedMessage::_internal_mutable_int64() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.int64_;
}

// repeated uint32 uint32 = 3;
inline int RepeatedMessage::_internal_uint32_size() const {
  return _internal_uint32().size();
}
inline int RepeatedMessage::uint32_size() const {
  return _internal_uint32_size();
}
inline void RepeatedMessage::clear_uint32() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.uint32_.Clear();
}
inline ::uint32_t RepeatedMessage::uint32(int index) const {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.uint32)
  return _internal_uint32().Get(index);
}
inline void RepeatedMessage::set_uint32(int index, ::uint32_t value) {
  _internal_mutable_uint32()->Set(index, value);
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.uint32)
}
inline void RepeatedMessage::add_uint32(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_uint32()->Add(value);
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.uint32)
}
inline const ::google::protobuf::RepeatedField<::uint32_t>& RepeatedMessage::uint32() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.uint32)
  return _internal_uint32();
}
inline ::google::protobuf::RepeatedField<::uint32_t>* RepeatedMessage::mutable_uint32()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.uint32)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_uint32();
}
inline const ::google::protobuf::RepeatedField<::uint32_t>&
RepeatedMessage::_internal_uint32() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.uint32_;
}
inline ::google::protobuf::RepeatedField<::uint32_t>* RepeatedMessage::_internal_mutable_uint32() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.uint32_;
}

// repeated uint64 uint64 = 4;
inline int RepeatedMessage::_internal_uint64_size() const {
  return _internal_uint64().size();
}
inline int RepeatedMessage::uint64_size() const {
  return _internal_uint64_size();
}
inline void RepeatedMessage::clear_uint64() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.uint64_.Clear();
}
inline ::uint64_t RepeatedMessage::uint64(int index) const {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.uint64)
  return _internal_uint64().Get(index);
}
inline void RepeatedMessage::set_uint64(int index, ::uint64_t value) {
  _internal_mutable_uint64()->Set(index, value);
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.uint64)
}
inline void RepeatedMessage::add_uint64(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_uint64()->Add(value);
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.uint64)
}
inline const ::google::protobuf::RepeatedField<::uint64_t>& RepeatedMessage::uint64() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.uint64)
  return _internal_uint64();
}
inline ::google::protobuf::RepeatedField<::uint64_t>* RepeatedMessage::mutable_uint64()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.uint64)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_uint64();
}
inline const ::google::protobuf::RepeatedField<::uint64_t>&
RepeatedMessage::_internal_uint64() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.uint64_;
}
inline ::google::protobuf::RepeatedField<::uint64_t>* RepeatedMessage::_internal_mutable_uint64() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.uint64_;
}

// repeated sint32 sint32 = 5;
inline int RepeatedMessage::_internal_sint32_size() const {
  return _internal_sint32().size();
}
inline int RepeatedMessage::sint32_size() const {
  return _internal_sint32_size();
}
inline void RepeatedMessage::clear_sint32() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.sint32_.Clear();
}
inline ::int32_t RepeatedMessage::sint32(int index) const {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.sint32)
  return _internal_sint32().Get(index);
}
inline void RepeatedMessage::set_sint32(int index, ::int32_t value) {
  _internal_mutable_sint32()->Set(index, value);
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.sint32)
}
inline void RepeatedMessage::add_sint32(::int32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_sint32()->Add(value);
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.sint32)
}
inline const ::google::protobuf::RepeatedField<::int32_t>& RepeatedMessage::sint32() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.sint32)
  return _internal_sint32();
}
inline ::google::protobuf::RepeatedField<::int32_t>* RepeatedMessage::mutable_sint32()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.sint32)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_sint32();
}
inline const ::google::protobuf::RepeatedField<::int32_t>&
RepeatedMessage::_internal_sint32() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.sint32_;
}
inline ::google::protobuf::RepeatedField<::int32_t>* RepeatedMessage::_internal_mutable_sint32() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.sint32_;
}

// repeated sint64 sint64 = 6;
inline int RepeatedMessage::_internal_sint64_size() const {
  return _internal_sint64().size();
}
inline int RepeatedMessage::sint64_size() const {
  return _internal_sint64_size();
}
inline void RepeatedMessage::clear_sint64() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.sint64_.Clear();
}
inline ::int64_t RepeatedMessage::sint64(int index) const {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.sint64)
  return _internal_sint64().Get(index);
}
inline void RepeatedMessage::set_sint64(int index, ::int64_t value) {
  _internal_mutable_sint64()->Set(index, value);
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.sint64)
}
inline void RepeatedMessage::add_sint64(::int64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_sint64()->Add(value);
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.sint64)
}
inline const ::google::protobuf::RepeatedField<::int64_t>& RepeatedMessage::sint64() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.sint64)
  return _internal_sint64();
}
inline ::google::protobuf::RepeatedField<::int64_t>* RepeatedMessage::mutable_sint64()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.sint64)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_sint64();
}
inline const ::google::protobuf::RepeatedField<::int64_t>&
RepeatedMessage::_internal_sint64() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.sint64_;
}
inline ::google::protobuf::RepeatedField<::int64_t>* RepeatedMessage::_internal_mutable_sint64() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.sint64_;
}

// repeated fixed32 fixed32 = 7;
inline int RepeatedMessage::_internal_fixed32_size() const {
  return _internal_fixed32().size();
}
inline int RepeatedMessage::fixed32_size() const {
  return _internal_fixed32_size();
}
inline void RepeatedMessage::clear_fixed32() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.fixed32_.Clear();
}
inline ::uint32_t RepeatedMessage::fixed32(int index) const {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.fixed32)
  return _internal_fixed32().Get(index);
}
inline void RepeatedMessage::set_fixed32(int index, ::uint32_t value) {
  _internal_mutable_fixed32()->Set(index, value);
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.fixed32)
}
inline void RepeatedMessage::add_fixed32(::uint32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_fixed32()->Add(value);
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.fixed32)
}
inline const ::google::protobuf::RepeatedField<::uint32_t>& RepeatedMessage::fixed32() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.fixed32)
  return _internal_fixed32();
}
inline ::google::protobuf::RepeatedField<::uint32_t>* RepeatedMessage::mutable_fixed32()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.fixed32)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_fixed32();
}
inline const ::google::protobuf::RepeatedField<::uint32_t>&
RepeatedMessage::_internal_fixed32() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.fixed32_;
}
inline ::google::protobuf::RepeatedField<::uint32_t>* RepeatedMessage::_internal_mutable_fixed32() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.fixed32_;
}

// repeated fixed64 fixed64 = 8;
inline int RepeatedMessage::_internal_fixed64_size() const {
  return _internal_fixed64().size();
}
inline int RepeatedMessage::fixed64_size() const {
  return _internal_fixed64_size();
}
inline void RepeatedMessage::clear_fixed64() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.fixed64_.Clear();
}
inline ::uint64_t RepeatedMessage::fixed64(int index) const {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.fixed64)
  return _internal_fixed64().Get(index);
}
inline void RepeatedMessage::set_fixed64(int index, ::uint64_t value) {
  _internal_mutable_fixed64()->Set(index, value);
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.fixed64)
}
inline void RepeatedMessage::add_fixed64(::uint64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_fixed64()->Add(value);
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.fixed64)
}
inline const ::google::protobuf::RepeatedField<::uint64_t>& RepeatedMessage::fixed64() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.fixed64)
  return _internal_fixed64();
}
inline ::google::protobuf::RepeatedField<::uint64_t>* RepeatedMessage::mutable_fixed64()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.fixed64)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_fixed64();
}
inline const ::google::protobuf::RepeatedField<::uint64_t>&
RepeatedMessage::_internal_fixed64() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.fixed64_;
}
inline ::google::protobuf::RepeatedField<::uint64_t>* RepeatedMessage::_internal_mutable_fixed64() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.fixed64_;
}

// repeated sfixed32 sfixed32 = 9;
inline int RepeatedMessage::_internal_sfixed32_size() const {
  return _internal_sfixed32().size();
}
inline int RepeatedMessage::sfixed32_size() const {
  return _internal_sfixed32_size();
}
inline void RepeatedMessage::clear_sfixed32() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.sfixed32_.Clear();
}
inline ::int32_t RepeatedMessage::sfixed32(int index) const {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.sfixed32)
  return _internal_sfixed32().Get(index);
}
inline void RepeatedMessage::set_sfixed32(int index, ::int32_t value) {
  _internal_mutable_sfixed32()->Set(index, value);
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.sfixed32)
}
inline void RepeatedMessage::add_sfixed32(::int32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_sfixed32()->Add(value);
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.sfixed32)
}
inline const ::google::protobuf::RepeatedField<::int32_t>& RepeatedMessage::sfixed32() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.sfixed32)
  return _internal_sfixed32();
}
inline ::google::protobuf::RepeatedField<::int32_t>* RepeatedMessage::mutable_sfixed32()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.sfixed32)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_sfixed32();
}
inline const ::google::protobuf::RepeatedField<::int32_t>&
RepeatedMessage::_internal_sfixed32() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.sfixed32_;
}
inline ::google::protobuf::RepeatedField<::int32_t>* RepeatedMessage::_internal_mutable_sfixed32() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.sfixed32_;
}

// repeated sfixed64 sfixed64 = 10;
inline int RepeatedMessage::_internal_sfixed64_size() const {
  return _internal_sfixed64().size();
}
inline int RepeatedMessage::sfixed64_size() const {
  return _internal_sfixed64_size();
}
inline void RepeatedMessage::clear_sfixed64() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.sfixed64_.Clear();
}
inline ::int64_t RepeatedMessage::sfixed64(int index) const {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.sfixed64)
  return _internal_sfixed64().Get(index);
}
inline void RepeatedMessage::set_sfixed64(int index, ::int64_t value) {
  _internal_mutable_sfixed64()->Set(index, value);
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.sfixed64)
}
inline void RepeatedMessage::add_sfixed64(::int64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_sfixed64()->Add(value);
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.sfixed64)
}
inline const ::google::protobuf::RepeatedField<::int64_t>& RepeatedMessage::sfixed64() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.sfixed64)
  return _internal_sfixed64();
}
inline ::google::protobuf::RepeatedField<::int64_t>* RepeatedMessage::mutable_sfixed64()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.sfixed64)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_sfixed64();
}
inline const ::google::protobuf::RepeatedField<::int64_t>&
RepeatedMessage::_internal_sfixed64() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.sfixed64_;
}
inline ::google::protobuf::RepeatedField<::int64_t>* RepeatedMessage::_internal_mutable_sfixed64() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.sfixed64_;
}

// repeated float float = 11;
inline int RepeatedMessage::_internal_float__size() const {
  return _internal_float_().size();
}
inline int RepeatedMessage::float__size() const {
  return _internal_float__size();
}
inline void RepeatedMessage::clear_float_() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.float__.Clear();
}
inline float RepeatedMessage::float_(int index) const {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.float)
  return _internal_float_().Get(index);
}
inline void RepeatedMessage::set_float_(int index, float value) {
  _internal_mutable_float_()->Set(index, value);
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.float)
}
inline void RepeatedMessage::add_float_(float value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_float_()->Add(value);
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.float)
}
inline const ::google::protobuf::RepeatedField<float>& RepeatedMessage::float_() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.float)
  return _internal_float_();
}
inline ::google::protobuf::RepeatedField<float>* RepeatedMessage::mutable_float_()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.float)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_float_();
}
inline const ::google::protobuf::RepeatedField<float>&
RepeatedMessage::_internal_float_() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.float__;
}
inline ::google::protobuf::RepeatedField<float>* RepeatedMessage::_internal_mutable_float_() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.float__;
}

// repeated double double = 12;
inline int RepeatedMessage::_internal_double__size() const {
  return _internal_double_().size();
}
inline int RepeatedMessage::double__size() const {
  return _internal_double__size();
}
inline void RepeatedMessage::clear_double_() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.double__.Clear();
}
inline double RepeatedMessage::double_(int index) const {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.double)
  return _internal_double_().Get(index);
}
inline void RepeatedMessage::set_double_(int index, double value) {
  _internal_mutable_double_()->Set(index, value);
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.double)
}
inline void RepeatedMessage::add_double_(double value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_double_()->Add(value);
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.double)
}
inline const ::google::protobuf::RepeatedField<double>& RepeatedMessage::double_() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.double)
  return _internal_double_();
}
inline ::google::protobuf::RepeatedField<double>* RepeatedMessage::mutable_double_()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.double)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_double_();
}
inline const ::google::protobuf::RepeatedField<double>&
RepeatedMessage::_internal_double_() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.double__;
}
inline ::google::protobuf::RepeatedField<double>* RepeatedMessage::_internal_mutable_double_() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.double__;
}

// repeated bool bool = 13;
inline int RepeatedMessage::_internal_bool__size() const {
  return _internal_bool_().size();
}
inline int RepeatedMessage::bool__size() const {
  return _internal_bool__size();
}
inline void RepeatedMessage::clear_bool_() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.bool__.Clear();
}
inline bool RepeatedMessage::bool_(int index) const {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.bool)
  return _internal_bool_().Get(index);
}
inline void RepeatedMessage::set_bool_(int index, bool value) {
  _internal_mutable_bool_()->Set(index, value);
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.bool)
}
inline void RepeatedMessage::add_bool_(bool value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_bool_()->Add(value);
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.bool)
}
inline const ::google::protobuf::RepeatedField<bool>& RepeatedMessage::bool_() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.bool)
  return _internal_bool_();
}
inline ::google::protobuf::RepeatedField<bool>* RepeatedMessage::mutable_bool_()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.bool)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_bool_();
}
inline const ::google::protobuf::RepeatedField<bool>&
RepeatedMessage::_internal_bool_() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.bool__;
}
inline ::google::protobuf::RepeatedField<bool>* RepeatedMessage::_internal_mutable_bool_() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.bool__;
}

// repeated string string = 14;
inline int RepeatedMessage::_internal_string_size() const {
  return _internal_string().size();
}
inline int RepeatedMessage::string_size() const {
  return _internal_string_size();
}
inline void RepeatedMessage::clear_string() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.string_.Clear();
}
inline std::string* RepeatedMessage::add_string()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  std::string* _s = _internal_mutable_string()->Add();
  // @@protoc_insertion_point(field_add_mutable:test.RepeatedMessage.string)
  return _s;
}
inline const std::string& RepeatedMessage::string(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.string)
  return _internal_string().Get(index);
}
inline std::string* RepeatedMessage::mutable_string(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:test.RepeatedMessage.string)
  return _internal_mutable_string()->Mutable(index);
}
inline void RepeatedMessage::set_string(int index, const std::string& value) {
  _internal_mutable_string()->Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.string)
}
inline void RepeatedMessage::set_string(int index, std::string&& value) {
  _internal_mutable_string()->Mutable(index)->assign(std::move(value));
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.string)
}
inline void RepeatedMessage::set_string(int index, const char* value) {
  ABSL_DCHECK(value != nullptr);
  _internal_mutable_string()->Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set_char:test.RepeatedMessage.string)
}
inline void RepeatedMessage::set_string(int index, const char* value,
                              std::size_t size) {
  _internal_mutable_string()->Mutable(index)->assign(
      reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_set_pointer:test.RepeatedMessage.string)
}
inline void RepeatedMessage::set_string(int index, absl::string_view value) {
  _internal_mutable_string()->Mutable(index)->assign(
      value.data(), value.size());
  // @@protoc_insertion_point(field_set_string_piece:test.RepeatedMessage.string)
}
inline void RepeatedMessage::add_string(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_string()->Add()->assign(value);
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.string)
}
inline void RepeatedMessage::add_string(std::string&& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_string()->Add(std::move(value));
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.string)
}
inline void RepeatedMessage::add_string(const char* value) {
  ABSL_DCHECK(value != nullptr);
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_string()->Add()->assign(value);
  // @@protoc_insertion_point(field_add_char:test.RepeatedMessage.string)
}
inline void RepeatedMessage::add_string(const char* value, std::size_t size) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_string()->Add()->assign(
      reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_add_pointer:test.RepeatedMessage.string)
}
inline void RepeatedMessage::add_string(absl::string_view value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_string()->Add()->assign(value.data(),
                                                     value.size());
  // @@protoc_insertion_point(field_add_string_piece:test.RepeatedMessage.string)
}
inline const ::google::protobuf::RepeatedPtrField<std::string>&
RepeatedMessage::string() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.string)
  return _internal_string();
}
inline ::google::protobuf::RepeatedPtrField<std::string>*
RepeatedMessage::mutable_string() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.string)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_string();
}
inline const ::google::protobuf::RepeatedPtrField<std::string>&
RepeatedMessage::_internal_string() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.string_;
}
inline ::google::protobuf::RepeatedPtrField<std::string>*
RepeatedMessage::_internal_mutable_string() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.string_;
}

// repeated bytes bytes = 15;
inline int RepeatedMessage::_internal_bytes_size() const {
  return _internal_bytes().size();
}
inline int RepeatedMessage::bytes_size() const {
  return _internal_bytes_size();
}
inline void RepeatedMessage::clear_bytes() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.bytes_.Clear();
}
inline std::string* RepeatedMessage::add_bytes()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  std::string* _s = _internal_mutable_bytes()->Add();
  // @@protoc_insertion_point(field_add_mutable:test.RepeatedMessage.bytes)
  return _s;
}
inline const std::string& RepeatedMessage::bytes(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.bytes)
  return _internal_bytes().Get(index);
}
inline std::string* RepeatedMessage::mutable_bytes(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:test.RepeatedMessage.bytes)
  return _internal_mutable_bytes()->Mutable(index);
}
inline void RepeatedMessage::set_bytes(int index, const std::string& value) {
  _internal_mutable_bytes()->Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.bytes)
}
inline void RepeatedMessage::set_bytes(int index, std::string&& value) {
  _internal_mutable_bytes()->Mutable(index)->assign(std::move(value));
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.bytes)
}
inline void RepeatedMessage::set_bytes(int index, const char* value) {
  ABSL_DCHECK(value != nullptr);
  _internal_mutable_bytes()->Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set_char:test.RepeatedMessage.bytes)
}
inline void RepeatedMessage::set_bytes(int index, const void* value,
                              std::size_t size) {
  _internal_mutable_bytes()->Mutable(index)->assign(
      reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_set_pointer:test.RepeatedMessage.bytes)
}
inline void RepeatedMessage::set_bytes(int index, absl::string_view value) {
  _internal_mutable_bytes()->Mutable(index)->assign(
      value.data(), value.size());
  // @@protoc_insertion_point(field_set_string_piece:test.RepeatedMessage.bytes)
}
inline void RepeatedMessage::add_bytes(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_bytes()->Add()->assign(value);
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.bytes)
}
inline void RepeatedMessage::add_bytes(std::string&& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_bytes()->Add(std::move(value));
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.bytes)
}
inline void RepeatedMessage::add_bytes(const char* value) {
  ABSL_DCHECK(value != nullptr);
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_bytes()->Add()->assign(value);
  // @@protoc_insertion_point(field_add_char:test.RepeatedMessage.bytes)
}
inline void RepeatedMessage::add_bytes(const void* value, std::size_t size) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_bytes()->Add()->assign(
      reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_add_pointer:test.RepeatedMessage.bytes)
}
inline void RepeatedMessage::add_bytes(absl::string_view value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_bytes()->Add()->assign(value.data(),
                                                     value.size());
  // @@protoc_insertion_point(field_add_string_piece:test.RepeatedMessage.bytes)
}
inline const ::google::protobuf::RepeatedPtrField<std::string>&
RepeatedMessage::bytes() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.bytes)
  return _internal_bytes();
}
inline ::google::protobuf::RepeatedPtrField<std::string>*
RepeatedMessage::mutable_bytes() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.bytes)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_bytes();
}
inline const ::google::protobuf::RepeatedPtrField<std::string>&
RepeatedMessage::_internal_bytes() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.bytes_;
}
inline ::google::protobuf::RepeatedPtrField<std::string>*
RepeatedMessage::_internal_mutable_bytes() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.bytes_;
}

// repeated .test.Enum enum = 16;
inline int RepeatedMessage::_internal_enum__size() const {
  return _internal_enum_().size();
}
inline int RepeatedMessage::enum__size() const {
  return _internal_enum__size();
}
inline void RepeatedMessage::clear_enum_() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.enum__.Clear();
}
inline ::test::Enum RepeatedMessage::enum_(int index) const {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.enum)
  return static_cast<::test::Enum>(_internal_enum_().Get(index));
}
inline void RepeatedMessage::set_enum_(int index, ::test::Enum value) {
  _internal_mutable_enum_()->Set(index, value);
  // @@protoc_insertion_point(field_set:test.RepeatedMessage.enum)
}
inline void RepeatedMessage::add_enum_(::test::Enum value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_enum_()->Add(value);
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.enum)
}
inline const ::google::protobuf::RepeatedField<int>& RepeatedMessage::enum_() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.enum)
  return _internal_enum_();
}
inline ::google::protobuf::RepeatedField<int>* RepeatedMessage::mutable_enum_()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.enum)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_enum_();
}
inline const ::google::protobuf::RepeatedField<int>& RepeatedMessage::_internal_enum_()
    const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.enum__;
}
inline ::google::protobuf::RepeatedField<int>* RepeatedMessage::_internal_mutable_enum_() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.enum__;
}

// repeated .test.RepeatedMessage self_message = 17;
inline int RepeatedMessage::_internal_self_message_size() const {
  return _internal_self_message().size();
}
inline int RepeatedMessage::self_message_size() const {
  return _internal_self_message_size();
}
inline void RepeatedMessage::clear_self_message() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.self_message_.Clear();
}
inline ::test::RepeatedMessage* RepeatedMessage::mutable_self_message(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:test.RepeatedMessage.self_message)
  return _internal_mutable_self_message()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::test::RepeatedMessage>* RepeatedMessage::mutable_self_message()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.self_message)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_self_message();
}
inline const ::test::RepeatedMessage& RepeatedMessage::self_message(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.self_message)
  return _internal_self_message().Get(index);
}
inline ::test::RepeatedMessage* RepeatedMessage::add_self_message() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::test::RepeatedMessage* _add = _internal_mutable_self_message()->Add();
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.self_message)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::test::RepeatedMessage>& RepeatedMessage::self_message() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.self_message)
  return _internal_self_message();
}
inline const ::google::protobuf::RepeatedPtrField<::test::RepeatedMessage>&
RepeatedMessage::_internal_self_message() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.self_message_;
}
inline ::google::protobuf::RepeatedPtrField<::test::RepeatedMessage>*
RepeatedMessage::_internal_mutable_self_message() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.self_message_;
}

// repeated .test.OtherMessage other_message = 18;
inline int RepeatedMessage::_internal_other_message_size() const {
  return _internal_other_message().size();
}
inline int RepeatedMessage::other_message_size() const {
  return _internal_other_message_size();
}
inline void RepeatedMessage::clear_other_message() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.other_message_.Clear();
}
inline ::test::OtherMessage* RepeatedMessage::mutable_other_message(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:test.RepeatedMessage.other_message)
  return _internal_mutable_other_message()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::test::OtherMessage>* RepeatedMessage::mutable_other_message()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.RepeatedMessage.other_message)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_other_message();
}
inline const ::test::OtherMessage& RepeatedMessage::other_message(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.RepeatedMessage.other_message)
  return _internal_other_message().Get(index);
}
inline ::test::OtherMessage* RepeatedMessage::add_other_message() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::test::OtherMessage* _add = _internal_mutable_other_message()->Add();
  // @@protoc_insertion_point(field_add:test.RepeatedMessage.other_message)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::test::OtherMessage>& RepeatedMessage::other_message() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.RepeatedMessage.other_message)
  return _internal_other_message();
}
inline const ::google::protobuf::RepeatedPtrField<::test::OtherMessage>&
RepeatedMessage::_internal_other_message() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.other_message_;
}
inline ::google::protobuf::RepeatedPtrField<::test::OtherMessage>*
RepeatedMessage::_internal_mutable_other_message() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.other_message_;
}

// -------------------------------------------------------------------

// ParentMessage_ChildMessage

// string value = 1;
inline void ParentMessage_ChildMessage::clear_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.value_.ClearToEmpty();
}
inline const std::string& ParentMessage_ChildMessage::value() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.ParentMessage.ChildMessage.value)
  return _internal_value();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void ParentMessage_ChildMessage::set_value(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.value_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:test.ParentMessage.ChildMessage.value)
}
inline std::string* ParentMessage_ChildMessage::mutable_value() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_value();
  // @@protoc_insertion_point(field_mutable:test.ParentMessage.ChildMessage.value)
  return _s;
}
inline const std::string& ParentMessage_ChildMessage::_internal_value() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.value_.Get();
}
inline void ParentMessage_ChildMessage::_internal_set_value(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.value_.Set(value, GetArena());
}
inline std::string* ParentMessage_ChildMessage::_internal_mutable_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.value_.Mutable( GetArena());
}
inline std::string* ParentMessage_ChildMessage::release_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:test.ParentMessage.ChildMessage.value)
  return _impl_.value_.Release();
}
inline void ParentMessage_ChildMessage::set_allocated_value(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.value_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.value_.IsDefault()) {
          _impl_.value_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:test.ParentMessage.ChildMessage.value)
}

// -------------------------------------------------------------------

// ParentMessage

// .test.ParentMessage.ChildMessage child_message = 1;
inline bool ParentMessage::has_child_message() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.child_message_ != nullptr);
  return value;
}
inline void ParentMessage::clear_child_message() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.child_message_ != nullptr) _impl_.child_message_->Clear();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline const ::test::ParentMessage_ChildMessage& ParentMessage::_internal_child_message() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::test::ParentMessage_ChildMessage* p = _impl_.child_message_;
  return p != nullptr ? *p : reinterpret_cast<const ::test::ParentMessage_ChildMessage&>(::test::_ParentMessage_ChildMessage_default_instance_);
}
inline const ::test::ParentMessage_ChildMessage& ParentMessage::child_message() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.ParentMessage.child_message)
  return _internal_child_message();
}
inline void ParentMessage::unsafe_arena_set_allocated_child_message(::test::ParentMessage_ChildMessage* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.child_message_);
  }
  _impl_.child_message_ = reinterpret_cast<::test::ParentMessage_ChildMessage*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:test.ParentMessage.child_message)
}
inline ::test::ParentMessage_ChildMessage* ParentMessage::release_child_message() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::test::ParentMessage_ChildMessage* released = _impl_.child_message_;
  _impl_.child_message_ = nullptr;
#ifdef PROTOBUF_FORCE_COPY_IN_RELEASE
  auto* old = reinterpret_cast<::google::protobuf::MessageLite*>(released);
  released = ::google::protobuf::internal::DuplicateIfNonNull(released);
  if (GetArena() == nullptr) {
    delete old;
  }
#else   // PROTOBUF_FORCE_COPY_IN_RELEASE
  if (GetArena() != nullptr) {
    released = ::google::protobuf::internal::DuplicateIfNonNull(released);
  }
#endif  // !PROTOBUF_FORCE_COPY_IN_RELEASE
  return released;
}
inline ::test::ParentMessage_ChildMessage* ParentMessage::unsafe_arena_release_child_message() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:test.ParentMessage.child_message)

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::test::ParentMessage_ChildMessage* temp = _impl_.child_message_;
  _impl_.child_message_ = nullptr;
  return temp;
}
inline ::test::ParentMessage_ChildMessage* ParentMessage::_internal_mutable_child_message() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.child_message_ == nullptr) {
    auto* p = ::google::protobuf::Message::DefaultConstruct<::test::ParentMessage_ChildMessage>(GetArena());
    _impl_.child_message_ = reinterpret_cast<::test::ParentMessage_ChildMessage*>(p);
  }
  return _impl_.child_message_;
}
inline ::test::ParentMessage_ChildMessage* ParentMessage::mutable_child_message() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000001u;
  ::test::ParentMessage_ChildMessage* _msg = _internal_mutable_child_message();
  // @@protoc_insertion_point(field_mutable:test.ParentMessage.child_message)
  return _msg;
}
inline void ParentMessage::set_allocated_child_message(::test::ParentMessage_ChildMessage* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete (_impl_.child_message_);
  }

  if (value != nullptr) {
    ::google::protobuf::Arena* submessage_arena = (value)->GetArena();
    if (message_arena != submessage_arena) {
      value = ::google::protobuf::internal::GetOwnedMessage(message_arena, value, submessage_arena);
    }
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }

  _impl_.child_message_ = reinterpret_cast<::test::ParentMessage_ChildMessage*>(value);
  // @@protoc_insertion_point(field_set_allocated:test.ParentMessage.child_message)
}

// .test.ParentMessage.ChildEnum child_enum = 2;
inline void ParentMessage::clear_child_enum() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.child_enum_ = 0;
}
inline ::test::ParentMessage_ChildEnum ParentMessage::child_enum() const {
  // @@protoc_insertion_point(field_get:test.ParentMessage.child_enum)
  return _internal_child_enum();
}
inline void ParentMessage::set_child_enum(::test::ParentMessage_ChildEnum value) {
  _internal_set_child_enum(value);
  // @@protoc_insertion_point(field_set:test.ParentMessage.child_enum)
}
inline ::test::ParentMessage_ChildEnum ParentMessage::_internal_child_enum() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return static_cast<::test::ParentMessage_ChildEnum>(_impl_.child_enum_);
}
inline void ParentMessage::_internal_set_child_enum(::test::ParentMessage_ChildEnum value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.child_enum_ = value;
}

// -------------------------------------------------------------------

// OneOfMessage

// string string_0 = 1;
inline bool OneOfMessage::has_string_0() const {
  return strings_case() == kString0;
}
inline void OneOfMessage::set_has_string_0() {
  _impl_._oneof_case_[0] = kString0;
}
inline void OneOfMessage::clear_string_0() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (strings_case() == kString0) {
    _impl_.strings_.string_0_.Destroy();
    clear_has_strings();
  }
}
inline const std::string& OneOfMessage::string_0() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.OneOfMessage.string_0)
  return _internal_string_0();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void OneOfMessage::set_string_0(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (strings_case() != kString0) {
    clear_strings();

    set_has_string_0();
    _impl_.strings_.string_0_.InitDefault();
  }
  _impl_.strings_.string_0_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:test.OneOfMessage.string_0)
}
inline std::string* OneOfMessage::mutable_string_0() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_string_0();
  // @@protoc_insertion_point(field_mutable:test.OneOfMessage.string_0)
  return _s;
}
inline const std::string& OneOfMessage::_internal_string_0() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  if (strings_case() != kString0) {
    return ::google::protobuf::internal::GetEmptyStringAlreadyInited();
  }
  return _impl_.strings_.string_0_.Get();
}
inline void OneOfMessage::_internal_set_string_0(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (strings_case() != kString0) {
    clear_strings();

    set_has_string_0();
    _impl_.strings_.string_0_.InitDefault();
  }
  _impl_.strings_.string_0_.Set(value, GetArena());
}
inline std::string* OneOfMessage::_internal_mutable_string_0() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (strings_case() != kString0) {
    clear_strings();

    set_has_string_0();
    _impl_.strings_.string_0_.InitDefault();
  }
  return _impl_.strings_.string_0_.Mutable( GetArena());
}
inline std::string* OneOfMessage::release_string_0() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:test.OneOfMessage.string_0)
  if (strings_case() != kString0) {
    return nullptr;
  }
  clear_has_strings();
  return _impl_.strings_.string_0_.Release();
}
inline void OneOfMessage::set_allocated_string_0(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (has_strings()) {
    clear_strings();
  }
  if (value != nullptr) {
    set_has_string_0();
    _impl_.strings_.string_0_.InitAllocated(value, GetArena());
  }
  // @@protoc_insertion_point(field_set_allocated:test.OneOfMessage.string_0)
}

// string string_1 = 2;
inline bool OneOfMessage::has_string_1() const {
  return strings_case() == kString1;
}
inline void OneOfMessage::set_has_string_1() {
  _impl_._oneof_case_[0] = kString1;
}
inline void OneOfMessage::clear_string_1() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (strings_case() == kString1) {
    _impl_.strings_.string_1_.Destroy();
    clear_has_strings();
  }
}
inline const std::string& OneOfMessage::string_1() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.OneOfMessage.string_1)
  return _internal_string_1();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void OneOfMessage::set_string_1(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (strings_case() != kString1) {
    clear_strings();

    set_has_string_1();
    _impl_.strings_.string_1_.InitDefault();
  }
  _impl_.strings_.string_1_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:test.OneOfMessage.string_1)
}
inline std::string* OneOfMessage::mutable_string_1() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_string_1();
  // @@protoc_insertion_point(field_mutable:test.OneOfMessage.string_1)
  return _s;
}
inline const std::string& OneOfMessage::_internal_string_1() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  if (strings_case() != kString1) {
    return ::google::protobuf::internal::GetEmptyStringAlreadyInited();
  }
  return _impl_.strings_.string_1_.Get();
}
inline void OneOfMessage::_internal_set_string_1(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (strings_case() != kString1) {
    clear_strings();

    set_has_string_1();
    _impl_.strings_.string_1_.InitDefault();
  }
  _impl_.strings_.string_1_.Set(value, GetArena());
}
inline std::string* OneOfMessage::_internal_mutable_string_1() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (strings_case() != kString1) {
    clear_strings();

    set_has_string_1();
    _impl_.strings_.string_1_.InitDefault();
  }
  return _impl_.strings_.string_1_.Mutable( GetArena());
}
inline std::string* OneOfMessage::release_string_1() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:test.OneOfMessage.string_1)
  if (strings_case() != kString1) {
    return nullptr;
  }
  clear_has_strings();
  return _impl_.strings_.string_1_.Release();
}
inline void OneOfMessage::set_allocated_string_1(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (has_strings()) {
    clear_strings();
  }
  if (value != nullptr) {
    set_has_string_1();
    _impl_.strings_.string_1_.InitAllocated(value, GetArena());
  }
  // @@protoc_insertion_point(field_set_allocated:test.OneOfMessage.string_1)
}

// .test.OtherMessage message_0 = 3;
inline bool OneOfMessage::has_message_0() const {
  return messages_case() == kMessage0;
}
inline bool OneOfMessage::_internal_has_message_0() const {
  return messages_case() == kMessage0;
}
inline void OneOfMessage::set_has_message_0() {
  _impl_._oneof_case_[1] = kMessage0;
}
inline void OneOfMessage::clear_message_0() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (messages_case() == kMessage0) {
    if (GetArena() == nullptr) {
      delete _impl_.messages_.message_0_;
    } else if (::google::protobuf::internal::DebugHardenClearOneofMessageOnArena()) {
      ::google::protobuf::internal::MaybePoisonAfterClear(_impl_.messages_.message_0_);
    }
    clear_has_messages();
  }
}
inline ::test::OtherMessage* OneOfMessage::release_message_0() {
  // @@protoc_insertion_point(field_release:test.OneOfMessage.message_0)
  if (messages_case() == kMessage0) {
    clear_has_messages();
    auto* temp = _impl_.messages_.message_0_;
    if (GetArena() != nullptr) {
      temp = ::google::protobuf::internal::DuplicateIfNonNull(temp);
    }
    _impl_.messages_.message_0_ = nullptr;
    return temp;
  } else {
    return nullptr;
  }
}
inline const ::test::OtherMessage& OneOfMessage::_internal_message_0() const {
  return messages_case() == kMessage0 ? *_impl_.messages_.message_0_ : reinterpret_cast<::test::OtherMessage&>(::test::_OtherMessage_default_instance_);
}
inline const ::test::OtherMessage& OneOfMessage::message_0() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.OneOfMessage.message_0)
  return _internal_message_0();
}
inline ::test::OtherMessage* OneOfMessage::unsafe_arena_release_message_0() {
  // @@protoc_insertion_point(field_unsafe_arena_release:test.OneOfMessage.message_0)
  if (messages_case() == kMessage0) {
    clear_has_messages();
    auto* temp = _impl_.messages_.message_0_;
    _impl_.messages_.message_0_ = nullptr;
    return temp;
  } else {
    return nullptr;
  }
}
inline void OneOfMessage::unsafe_arena_set_allocated_message_0(::test::OtherMessage* value) {
  // We rely on the oneof clear method to free the earlier contents
  // of this oneof. We can directly use the pointer we're given to
  // set the new value.
  clear_messages();
  if (value) {
    set_has_message_0();
    _impl_.messages_.message_0_ = value;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:test.OneOfMessage.message_0)
}
inline ::test::OtherMessage* OneOfMessage::_internal_mutable_message_0() {
  if (messages_case() != kMessage0) {
    clear_messages();
    set_has_message_0();
    _impl_.messages_.message_0_ =
        ::google::protobuf::Message::DefaultConstruct<::test::OtherMessage>(GetArena());
  }
  return _impl_.messages_.message_0_;
}
inline ::test::OtherMessage* OneOfMessage::mutable_message_0() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::test::OtherMessage* _msg = _internal_mutable_message_0();
  // @@protoc_insertion_point(field_mutable:test.OneOfMessage.message_0)
  return _msg;
}

// .test.OtherMessage message_1 = 4;
inline bool OneOfMessage::has_message_1() const {
  return messages_case() == kMessage1;
}
inline bool OneOfMessage::_internal_has_message_1() const {
  return messages_case() == kMessage1;
}
inline void OneOfMessage::set_has_message_1() {
  _impl_._oneof_case_[1] = kMessage1;
}
inline void OneOfMessage::clear_message_1() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (messages_case() == kMessage1) {
    if (GetArena() == nullptr) {
      delete _impl_.messages_.message_1_;
    } else if (::google::protobuf::internal::DebugHardenClearOneofMessageOnArena()) {
      ::google::protobuf::internal::MaybePoisonAfterClear(_impl_.messages_.message_1_);
    }
    clear_has_messages();
  }
}
inline ::test::OtherMessage* OneOfMessage::release_message_1() {
  // @@protoc_insertion_point(field_release:test.OneOfMessage.message_1)
  if (messages_case() == kMessage1) {
    clear_has_messages();
    auto* temp = _impl_.messages_.message_1_;
    if (GetArena() != nullptr) {
      temp = ::google::protobuf::internal::DuplicateIfNonNull(temp);
    }
    _impl_.messages_.message_1_ = nullptr;
    return temp;
  } else {
    return nullptr;
  }
}
inline const ::test::OtherMessage& OneOfMessage::_internal_message_1() const {
  return messages_case() == kMessage1 ? *_impl_.messages_.message_1_ : reinterpret_cast<::test::OtherMessage&>(::test::_OtherMessage_default_instance_);
}
inline const ::test::OtherMessage& OneOfMessage::message_1() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.OneOfMessage.message_1)
  return _internal_message_1();
}
inline ::test::OtherMessage* OneOfMessage::unsafe_arena_release_message_1() {
  // @@protoc_insertion_point(field_unsafe_arena_release:test.OneOfMessage.message_1)
  if (messages_case() == kMessage1) {
    clear_has_messages();
    auto* temp = _impl_.messages_.message_1_;
    _impl_.messages_.message_1_ = nullptr;
    return temp;
  } else {
    return nullptr;
  }
}
inline void OneOfMessage::unsafe_arena_set_allocated_message_1(::test::OtherMessage* value) {
  // We rely on the oneof clear method to free the earlier contents
  // of this oneof. We can directly use the pointer we're given to
  // set the new value.
  clear_messages();
  if (value) {
    set_has_message_1();
    _impl_.messages_.message_1_ = value;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:test.OneOfMessage.message_1)
}
inline ::test::OtherMessage* OneOfMessage::_internal_mutable_message_1() {
  if (messages_case() != kMessage1) {
    clear_messages();
    set_has_message_1();
    _impl_.messages_.message_1_ =
        ::google::protobuf::Message::DefaultConstruct<::test::OtherMessage>(GetArena());
  }
  return _impl_.messages_.message_1_;
}
inline ::test::OtherMessage* OneOfMessage::mutable_message_1() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::test::OtherMessage* _msg = _internal_mutable_message_1();
  // @@protoc_insertion_point(field_mutable:test.OneOfMessage.message_1)
  return _msg;
}

// bytes bytes_0 = 5;
inline bool OneOfMessage::has_bytes_0() const {
  return bytes_case() == kBytes0;
}
inline void OneOfMessage::set_has_bytes_0() {
  _impl_._oneof_case_[2] = kBytes0;
}
inline void OneOfMessage::clear_bytes_0() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (bytes_case() == kBytes0) {
    _impl_.bytes_.bytes_0_.Destroy();
    clear_has_bytes();
  }
}
inline const std::string& OneOfMessage::bytes_0() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.OneOfMessage.bytes_0)
  return _internal_bytes_0();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void OneOfMessage::set_bytes_0(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (bytes_case() != kBytes0) {
    clear_bytes();

    set_has_bytes_0();
    _impl_.bytes_.bytes_0_.InitDefault();
  }
  _impl_.bytes_.bytes_0_.SetBytes(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:test.OneOfMessage.bytes_0)
}
inline std::string* OneOfMessage::mutable_bytes_0() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_bytes_0();
  // @@protoc_insertion_point(field_mutable:test.OneOfMessage.bytes_0)
  return _s;
}
inline const std::string& OneOfMessage::_internal_bytes_0() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  if (bytes_case() != kBytes0) {
    return ::google::protobuf::internal::GetEmptyStringAlreadyInited();
  }
  return _impl_.bytes_.bytes_0_.Get();
}
inline void OneOfMessage::_internal_set_bytes_0(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (bytes_case() != kBytes0) {
    clear_bytes();

    set_has_bytes_0();
    _impl_.bytes_.bytes_0_.InitDefault();
  }
  _impl_.bytes_.bytes_0_.Set(value, GetArena());
}
inline std::string* OneOfMessage::_internal_mutable_bytes_0() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (bytes_case() != kBytes0) {
    clear_bytes();

    set_has_bytes_0();
    _impl_.bytes_.bytes_0_.InitDefault();
  }
  return _impl_.bytes_.bytes_0_.Mutable( GetArena());
}
inline std::string* OneOfMessage::release_bytes_0() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:test.OneOfMessage.bytes_0)
  if (bytes_case() != kBytes0) {
    return nullptr;
  }
  clear_has_bytes();
  return _impl_.bytes_.bytes_0_.Release();
}
inline void OneOfMessage::set_allocated_bytes_0(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (has_bytes()) {
    clear_bytes();
  }
  if (value != nullptr) {
    set_has_bytes_0();
    _impl_.bytes_.bytes_0_.InitAllocated(value, GetArena());
  }
  // @@protoc_insertion_point(field_set_allocated:test.OneOfMessage.bytes_0)
}

// bytes bytes_1 = 6;
inline bool OneOfMessage::has_bytes_1() const {
  return bytes_case() == kBytes1;
}
inline void OneOfMessage::set_has_bytes_1() {
  _impl_._oneof_case_[2] = kBytes1;
}
inline void OneOfMessage::clear_bytes_1() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (bytes_case() == kBytes1) {
    _impl_.bytes_.bytes_1_.Destroy();
    clear_has_bytes();
  }
}
inline const std::string& OneOfMessage::bytes_1() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.OneOfMessage.bytes_1)
  return _internal_bytes_1();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void OneOfMessage::set_bytes_1(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (bytes_case() != kBytes1) {
    clear_bytes();

    set_has_bytes_1();
    _impl_.bytes_.bytes_1_.InitDefault();
  }
  _impl_.bytes_.bytes_1_.SetBytes(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:test.OneOfMessage.bytes_1)
}
inline std::string* OneOfMessage::mutable_bytes_1() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_bytes_1();
  // @@protoc_insertion_point(field_mutable:test.OneOfMessage.bytes_1)
  return _s;
}
inline const std::string& OneOfMessage::_internal_bytes_1() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  if (bytes_case() != kBytes1) {
    return ::google::protobuf::internal::GetEmptyStringAlreadyInited();
  }
  return _impl_.bytes_.bytes_1_.Get();
}
inline void OneOfMessage::_internal_set_bytes_1(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (bytes_case() != kBytes1) {
    clear_bytes();

    set_has_bytes_1();
    _impl_.bytes_.bytes_1_.InitDefault();
  }
  _impl_.bytes_.bytes_1_.Set(value, GetArena());
}
inline std::string* OneOfMessage::_internal_mutable_bytes_1() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (bytes_case() != kBytes1) {
    clear_bytes();

    set_has_bytes_1();
    _impl_.bytes_.bytes_1_.InitDefault();
  }
  return _impl_.bytes_.bytes_1_.Mutable( GetArena());
}
inline std::string* OneOfMessage::release_bytes_1() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:test.OneOfMessage.bytes_1)
  if (bytes_case() != kBytes1) {
    return nullptr;
  }
  clear_has_bytes();
  return _impl_.bytes_.bytes_1_.Release();
}
inline void OneOfMessage::set_allocated_bytes_1(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (has_bytes()) {
    clear_bytes();
  }
  if (value != nullptr) {
    set_has_bytes_1();
    _impl_.bytes_.bytes_1_.InitAllocated(value, GetArena());
  }
  // @@protoc_insertion_point(field_set_allocated:test.OneOfMessage.bytes_1)
}

inline bool OneOfMessage::has_strings() const {
  return strings_case() != STRINGS_NOT_SET;
}
inline void OneOfMessage::clear_has_strings() {
  _impl_._oneof_case_[0] = STRINGS_NOT_SET;
}
inline bool OneOfMessage::has_messages() const {
  return messages_case() != MESSAGES_NOT_SET;
}
inline void OneOfMessage::clear_has_messages() {
  _impl_._oneof_case_[1] = MESSAGES_NOT_SET;
}
inline bool OneOfMessage::has_bytes() const {
  return bytes_case() != BYTES_NOT_SET;
}
inline void OneOfMessage::clear_has_bytes() {
  _impl_._oneof_case_[2] = BYTES_NOT_SET;
}
inline OneOfMessage::StringsCase OneOfMessage::strings_case() const {
  return OneOfMessage::StringsCase(_impl_._oneof_case_[0]);
}
inline OneOfMessage::MessagesCase OneOfMessage::messages_case() const {
  return OneOfMessage::MessagesCase(_impl_._oneof_case_[1]);
}
inline OneOfMessage::BytesCase OneOfMessage::bytes_case() const {
  return OneOfMessage::BytesCase(_impl_._oneof_case_[2]);
}
// -------------------------------------------------------------------

// OldMessage

// string old_value = 1;
inline void OldMessage::clear_old_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.old_value_.ClearToEmpty();
}
inline const std::string& OldMessage::old_value() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.OldMessage.old_value)
  return _internal_old_value();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void OldMessage::set_old_value(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.old_value_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:test.OldMessage.old_value)
}
inline std::string* OldMessage::mutable_old_value() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_old_value();
  // @@protoc_insertion_point(field_mutable:test.OldMessage.old_value)
  return _s;
}
inline const std::string& OldMessage::_internal_old_value() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.old_value_.Get();
}
inline void OldMessage::_internal_set_old_value(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.old_value_.Set(value, GetArena());
}
inline std::string* OldMessage::_internal_mutable_old_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.old_value_.Mutable( GetArena());
}
inline std::string* OldMessage::release_old_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:test.OldMessage.old_value)
  return _impl_.old_value_.Release();
}
inline void OldMessage::set_allocated_old_value(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.old_value_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.old_value_.IsDefault()) {
          _impl_.old_value_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:test.OldMessage.old_value)
}

// -------------------------------------------------------------------

// NewMessage

// string old_value = 1;
inline void NewMessage::clear_old_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.old_value_.ClearToEmpty();
}
inline const std::string& NewMessage::old_value() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.NewMessage.old_value)
  return _internal_old_value();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void NewMessage::set_old_value(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.old_value_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:test.NewMessage.old_value)
}
inline std::string* NewMessage::mutable_old_value() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_old_value();
  // @@protoc_insertion_point(field_mutable:test.NewMessage.old_value)
  return _s;
}
inline const std::string& NewMessage::_internal_old_value() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.old_value_.Get();
}
inline void NewMessage::_internal_set_old_value(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.old_value_.Set(value, GetArena());
}
inline std::string* NewMessage::_internal_mutable_old_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.old_value_.Mutable( GetArena());
}
inline std::string* NewMessage::release_old_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:test.NewMessage.old_value)
  return _impl_.old_value_.Release();
}
inline void NewMessage::set_allocated_old_value(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.old_value_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.old_value_.IsDefault()) {
          _impl_.old_value_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:test.NewMessage.old_value)
}

// string new_value = 2;
inline void NewMessage::clear_new_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.new_value_.ClearToEmpty();
}
inline const std::string& NewMessage::new_value() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:test.NewMessage.new_value)
  return _internal_new_value();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void NewMessage::set_new_value(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.new_value_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:test.NewMessage.new_value)
}
inline std::string* NewMessage::mutable_new_value() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_new_value();
  // @@protoc_insertion_point(field_mutable:test.NewMessage.new_value)
  return _s;
}
inline const std::string& NewMessage::_internal_new_value() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.new_value_.Get();
}
inline void NewMessage::_internal_set_new_value(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.new_value_.Set(value, GetArena());
}
inline std::string* NewMessage::_internal_mutable_new_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.new_value_.Mutable( GetArena());
}
inline std::string* NewMessage::release_new_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:test.NewMessage.new_value)
  return _impl_.new_value_.Release();
}
inline void NewMessage::set_allocated_new_value(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.new_value_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.new_value_.IsDefault()) {
          _impl_.new_value_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:test.NewMessage.new_value)
}

// -------------------------------------------------------------------

// -------------------------------------------------------------------

// OldEnumMessage

// .test.OldEnumMessage.OldEnum value = 1;
inline void OldEnumMessage::clear_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.value_ = 0;
}
inline ::test::OldEnumMessage_OldEnum OldEnumMessage::value() const {
  // @@protoc_insertion_point(field_get:test.OldEnumMessage.value)
  return _internal_value();
}
inline void OldEnumMessage::set_value(::test::OldEnumMessage_OldEnum value) {
  _internal_set_value(value);
  // @@protoc_insertion_point(field_set:test.OldEnumMessage.value)
}
inline ::test::OldEnumMessage_OldEnum OldEnumMessage::_internal_value() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return static_cast<::test::OldEnumMessage_OldEnum>(_impl_.value_);
}
inline void OldEnumMessage::_internal_set_value(::test::OldEnumMessage_OldEnum value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.value_ = value;
}

// repeated .test.OldEnumMessage.OldEnum repeated_value = 2;
inline int OldEnumMessage::_internal_repeated_value_size() const {
  return _internal_repeated_value().size();
}
inline int OldEnumMessage::repeated_value_size() const {
  return _internal_repeated_value_size();
}
inline void OldEnumMessage::clear_repeated_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.repeated_value_.Clear();
}
inline ::test::OldEnumMessage_OldEnum OldEnumMessage::repeated_value(int index) const {
  // @@protoc_insertion_point(field_get:test.OldEnumMessage.repeated_value)
  return static_cast<::test::OldEnumMessage_OldEnum>(_internal_repeated_value().Get(index));
}
inline void OldEnumMessage::set_repeated_value(int index, ::test::OldEnumMessage_OldEnum value) {
  _internal_mutable_repeated_value()->Set(index, value);
  // @@protoc_insertion_point(field_set:test.OldEnumMessage.repeated_value)
}
inline void OldEnumMessage::add_repeated_value(::test::OldEnumMessage_OldEnum value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_repeated_value()->Add(value);
  // @@protoc_insertion_point(field_add:test.OldEnumMessage.repeated_value)
}
inline const ::google::protobuf::RepeatedField<int>& OldEnumMessage::repeated_value() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.OldEnumMessage.repeated_value)
  return _internal_repeated_value();
}
inline ::google::protobuf::RepeatedField<int>* OldEnumMessage::mutable_repeated_value()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.OldEnumMessage.repeated_value)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_repeated_value();
}
inline const ::google::protobuf::RepeatedField<int>& OldEnumMessage::_internal_repeated_value()
    const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.repeated_value_;
}
inline ::google::protobuf::RepeatedField<int>* OldEnumMessage::_internal_mutable_repeated_value() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.repeated_value_;
}

// map<string, .test.OldEnumMessage.OldEnum> mapped_value = 3;
inline int OldEnumMessage::_internal_mapped_value_size() const {
  return _internal_mapped_value().size();
}
inline int OldEnumMessage::mapped_value_size() const {
  return _internal_mapped_value_size();
}
inline void OldEnumMessage::clear_mapped_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.mapped_value_.Clear();
}
inline const ::google::protobuf::Map<std::string, ::test::OldEnumMessage_OldEnum>& OldEnumMessage::_internal_mapped_value() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.mapped_value_.GetMap();
}
inline const ::google::protobuf::Map<std::string, ::test::OldEnumMessage_OldEnum>& OldEnumMessage::mapped_value() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_map:test.OldEnumMessage.mapped_value)
  return _internal_mapped_value();
}
inline ::google::protobuf::Map<std::string, ::test::OldEnumMessage_OldEnum>* OldEnumMessage::_internal_mutable_mapped_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.mapped_value_.MutableMap();
}
inline ::google::protobuf::Map<std::string, ::test::OldEnumMessage_OldEnum>* OldEnumMessage::mutable_mapped_value() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_map:test.OldEnumMessage.mapped_value)
  return _internal_mutable_mapped_value();
}

// -------------------------------------------------------------------

// -------------------------------------------------------------------

// NewEnumMessage

// .test.NewEnumMessage.NewEnum value = 1;
inline void NewEnumMessage::clear_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.value_ = 0;
}
inline ::test::NewEnumMessage_NewEnum NewEnumMessage::value() const {
  // @@protoc_insertion_point(field_get:test.NewEnumMessage.value)
  return _internal_value();
}
inline void NewEnumMessage::set_value(::test::NewEnumMessage_NewEnum value) {
  _internal_set_value(value);
  // @@protoc_insertion_point(field_set:test.NewEnumMessage.value)
}
inline ::test::NewEnumMessage_NewEnum NewEnumMessage::_internal_value() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return static_cast<::test::NewEnumMessage_NewEnum>(_impl_.value_);
}
inline void NewEnumMessage::_internal_set_value(::test::NewEnumMessage_NewEnum value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.value_ = value;
}

// repeated .test.NewEnumMessage.NewEnum repeated_value = 2;
inline int NewEnumMessage::_internal_repeated_value_size() const {
  return _internal_repeated_value().size();
}
inline int NewEnumMessage::repeated_value_size() const {
  return _internal_repeated_value_size();
}
inline void NewEnumMessage::clear_repeated_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.repeated_value_.Clear();
}
inline ::test::NewEnumMessage_NewEnum NewEnumMessage::repeated_value(int index) const {
  // @@protoc_insertion_point(field_get:test.NewEnumMessage.repeated_value)
  return static_cast<::test::NewEnumMessage_NewEnum>(_internal_repeated_value().Get(index));
}
inline void NewEnumMessage::set_repeated_value(int index, ::test::NewEnumMessage_NewEnum value) {
  _internal_mutable_repeated_value()->Set(index, value);
  // @@protoc_insertion_point(field_set:test.NewEnumMessage.repeated_value)
}
inline void NewEnumMessage::add_repeated_value(::test::NewEnumMessage_NewEnum value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_repeated_value()->Add(value);
  // @@protoc_insertion_point(field_add:test.NewEnumMessage.repeated_value)
}
inline const ::google::protobuf::RepeatedField<int>& NewEnumMessage::repeated_value() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:test.NewEnumMessage.repeated_value)
  return _internal_repeated_value();
}
inline ::google::protobuf::RepeatedField<int>* NewEnumMessage::mutable_repeated_value()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:test.NewEnumMessage.repeated_value)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_repeated_value();
}
inline const ::google::protobuf::RepeatedField<int>& NewEnumMessage::_internal_repeated_value()
    const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.repeated_value_;
}
inline ::google::protobuf::RepeatedField<int>* NewEnumMessage::_internal_mutable_repeated_value() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.repeated_value_;
}

// map<string, .test.NewEnumMessage.NewEnum> mapped_value = 3;
inline int NewEnumMessage::_internal_mapped_value_size() const {
  return _internal_mapped_value().size();
}
inline int NewEnumMessage::mapped_value_size() const {
  return _internal_mapped_value_size();
}
inline void NewEnumMessage::clear_mapped_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.mapped_value_.Clear();
}
inline const ::google::protobuf::Map<std::string, ::test::NewEnumMessage_NewEnum>& NewEnumMessage::_internal_mapped_value() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.mapped_value_.GetMap();
}
inline const ::google::protobuf::Map<std::string, ::test::NewEnumMessage_NewEnum>& NewEnumMessage::mapped_value() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_map:test.NewEnumMessage.mapped_value)
  return _internal_mapped_value();
}
inline ::google::protobuf::Map<std::string, ::test::NewEnumMessage_NewEnum>* NewEnumMessage::_internal_mutable_mapped_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.mapped_value_.MutableMap();
}
inline ::google::protobuf::Map<std::string, ::test::NewEnumMessage_NewEnum>* NewEnumMessage::mutable_mapped_value() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_map:test.NewEnumMessage.mapped_value)
  return _internal_mutable_mapped_value();
}

// -------------------------------------------------------------------

// -------------------------------------------------------------------

// -------------------------------------------------------------------

// -------------------------------------------------------------------

// -------------------------------------------------------------------

// -------------------------------------------------------------------

// -------------------------------------------------------------------

// -------------------------------------------------------------------

// -------------------------------------------------------------------

// MapMessage

// map<string, string> stringToString = 1;
inline int MapMessage::_internal_stringtostring_size() const {
  return _internal_stringtostring().size();
}
inline int MapMessage::stringtostring_size() const {
  return _internal_stringtostring_size();
}
inline void MapMessage::clear_stringtostring() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.stringtostring_.Clear();
}
inline const ::google::protobuf::Map<std::string, std::string>& MapMessage::_internal_stringtostring() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.stringtostring_.GetMap();
}
inline const ::google::protobuf::Map<std::string, std::string>& MapMessage::stringtostring() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_map:test.MapMessage.stringToString)
  return _internal_stringtostring();
}
inline ::google::protobuf::Map<std::string, std::string>* MapMessage::_internal_mutable_stringtostring() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.stringtostring_.MutableMap();
}
inline ::google::protobuf::Map<std::string, std::string>* MapMessage::mutable_stringtostring() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_map:test.MapMessage.stringToString)
  return _internal_mutable_stringtostring();
}

// map<string, int32> stringToNumber = 2;
inline int MapMessage::_internal_stringtonumber_size() const {
  return _internal_stringtonumber().size();
}
inline int MapMessage::stringtonumber_size() const {
  return _internal_stringtonumber_size();
}
inline void MapMessage::clear_stringtonumber() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.stringtonumber_.Clear();
}
inline const ::google::protobuf::Map<std::string, ::int32_t>& MapMessage::_internal_stringtonumber() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.stringtonumber_.GetMap();
}
inline const ::google::protobuf::Map<std::string, ::int32_t>& MapMessage::stringtonumber() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_map:test.MapMessage.stringToNumber)
  return _internal_stringtonumber();
}
inline ::google::protobuf::Map<std::string, ::int32_t>* MapMessage::_internal_mutable_stringtonumber() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.stringtonumber_.MutableMap();
}
inline ::google::protobuf::Map<std::string, ::int32_t>* MapMessage::mutable_stringtonumber() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_map:test.MapMessage.stringToNumber)
  return _internal_mutable_stringtonumber();
}

// map<string, int64> stringToSignedLong = 3;
inline int MapMessage::_internal_stringtosignedlong_size() const {
  return _internal_stringtosignedlong().size();
}
inline int MapMessage::stringtosignedlong_size() const {
  return _internal_stringtosignedlong_size();
}
inline void MapMessage::clear_stringtosignedlong() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.stringtosignedlong_.Clear();
}
inline const ::google::protobuf::Map<std::string, ::int64_t>& MapMessage::_internal_stringtosignedlong() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.stringtosignedlong_.GetMap();
}
inline const ::google::protobuf::Map<std::string, ::int64_t>& MapMessage::stringtosignedlong() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_map:test.MapMessage.stringToSignedLong)
  return _internal_stringtosignedlong();
}
inline ::google::protobuf::Map<std::string, ::int64_t>* MapMessage::_internal_mutable_stringtosignedlong() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.stringtosignedlong_.MutableMap();
}
inline ::google::protobuf::Map<std::string, ::int64_t>* MapMessage::mutable_stringtosignedlong() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_map:test.MapMessage.stringToSignedLong)
  return _internal_mutable_stringtosignedlong();
}

// map<string, uint64> stringToUnsignedLong = 4;
inline int MapMessage::_internal_stringtounsignedlong_size() const {
  return _internal_stringtounsignedlong().size();
}
inline int MapMessage::stringtounsignedlong_size() const {
  return _internal_stringtounsignedlong_size();
}
inline void MapMessage::clear_stringtounsignedlong() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.stringtounsignedlong_.Clear();
}
inline const ::google::protobuf::Map<std::string, ::uint64_t>& MapMessage::_internal_stringtounsignedlong() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.stringtounsignedlong_.GetMap();
}
inline const ::google::protobuf::Map<std::string, ::uint64_t>& MapMessage::stringtounsignedlong() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_map:test.MapMessage.stringToUnsignedLong)
  return _internal_stringtounsignedlong();
}
inline ::google::protobuf::Map<std::string, ::uint64_t>* MapMessage::_internal_mutable_stringtounsignedlong() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.stringtounsignedlong_.MutableMap();
}
inline ::google::protobuf::Map<std::string, ::uint64_t>* MapMessage::mutable_stringtounsignedlong() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_map:test.MapMessage.stringToUnsignedLong)
  return _internal_mutable_stringtounsignedlong();
}

// map<string, double> stringToDouble = 5;
inline int MapMessage::_internal_stringtodouble_size() const {
  return _internal_stringtodouble().size();
}
inline int MapMessage::stringtodouble_size() const {
  return _internal_stringtodouble_size();
}
inline void MapMessage::clear_stringtodouble() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.stringtodouble_.Clear();
}
inline const ::google::protobuf::Map<std::string, double>& MapMessage::_internal_stringtodouble() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.stringtodouble_.GetMap();
}
inline const ::google::protobuf::Map<std::string, double>& MapMessage::stringtodouble() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_map:test.MapMessage.stringToDouble)
  return _internal_stringtodouble();
}
inline ::google::protobuf::Map<std::string, double>* MapMessage::_internal_mutable_stringtodouble() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.stringtodouble_.MutableMap();
}
inline ::google::protobuf::Map<std::string, double>* MapMessage::mutable_stringtodouble() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_map:test.MapMessage.stringToDouble)
  return _internal_mutable_stringtodouble();
}

// map<string, .test.OtherMessage> stringToMessage = 6;
inline int MapMessage::_internal_stringtomessage_size() const {
  return _internal_stringtomessage().size();
}
inline int MapMessage::stringtomessage_size() const {
  return _internal_stringtomessage_size();
}
inline void MapMessage::clear_stringtomessage() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.stringtomessage_.Clear();
}
inline const ::google::protobuf::Map<std::string, ::test::OtherMessage>& MapMessage::_internal_stringtomessage() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.stringtomessage_.GetMap();
}
inline const ::google::protobuf::Map<std::string, ::test::OtherMessage>& MapMessage::stringtomessage() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_map:test.MapMessage.stringToMessage)
  return _internal_stringtomessage();
}
inline ::google::protobuf::Map<std::string, ::test::OtherMessage>* MapMessage::_internal_mutable_stringtomessage() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.stringtomessage_.MutableMap();
}
inline ::google::protobuf::Map<std::string, ::test::OtherMessage>* MapMessage::mutable_stringtomessage() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_map:test.MapMessage.stringToMessage)
  return _internal_mutable_stringtomessage();
}

// map<int32, string> intToString = 7;
inline int MapMessage::_internal_inttostring_size() const {
  return _internal_inttostring().size();
}
inline int MapMessage::inttostring_size() const {
  return _internal_inttostring_size();
}
inline void MapMessage::clear_inttostring() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.inttostring_.Clear();
}
inline const ::google::protobuf::Map<::int32_t, std::string>& MapMessage::_internal_inttostring() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.inttostring_.GetMap();
}
inline const ::google::protobuf::Map<::int32_t, std::string>& MapMessage::inttostring() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_map:test.MapMessage.intToString)
  return _internal_inttostring();
}
inline ::google::protobuf::Map<::int32_t, std::string>* MapMessage::_internal_mutable_inttostring() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.inttostring_.MutableMap();
}
inline ::google::protobuf::Map<::int32_t, std::string>* MapMessage::mutable_inttostring() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_map:test.MapMessage.intToString)
  return _internal_mutable_inttostring();
}

// map<int64, string> longToString = 8;
inline int MapMessage::_internal_longtostring_size() const {
  return _internal_longtostring().size();
}
inline int MapMessage::longtostring_size() const {
  return _internal_longtostring_size();
}
inline void MapMessage::clear_longtostring() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.longtostring_.Clear();
}
inline const ::google::protobuf::Map<::int64_t, std::string>& MapMessage::_internal_longtostring() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.longtostring_.GetMap();
}
inline const ::google::protobuf::Map<::int64_t, std::string>& MapMessage::longtostring() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_map:test.MapMessage.longToString)
  return _internal_longtostring();
}
inline ::google::protobuf::Map<::int64_t, std::string>* MapMessage::_internal_mutable_longtostring() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.longtostring_.MutableMap();
}
inline ::google::protobuf::Map<::int64_t, std::string>* MapMessage::mutable_longtostring() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_map:test.MapMessage.longToString)
  return _internal_mutable_longtostring();
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)
}  // namespace test


namespace google {
namespace protobuf {

template <>
struct is_proto_enum<::test::ParentMessage_ChildEnum> : std::true_type {};
template <>
inline const EnumDescriptor* GetEnumDescriptor<::test::ParentMessage_ChildEnum>() {
  return ::test::ParentMessage_ChildEnum_descriptor();
}
template <>
struct is_proto_enum<::test::OldEnumMessage_OldEnum> : std::true_type {};
template <>
inline const EnumDescriptor* GetEnumDescriptor<::test::OldEnumMessage_OldEnum>() {
  return ::test::OldEnumMessage_OldEnum_descriptor();
}
template <>
struct is_proto_enum<::test::NewEnumMessage_NewEnum> : std::true_type {};
template <>
inline const EnumDescriptor* GetEnumDescriptor<::test::NewEnumMessage_NewEnum>() {
  return ::test::NewEnumMessage_NewEnum_descriptor();
}
template <>
struct is_proto_enum<::test::Enum> : std::true_type {};
template <>
inline const EnumDescriptor* GetEnumDescriptor<::test::Enum>() {
  return ::test::Enum_descriptor();
}

}  // namespace protobuf
}  // namespace google

// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_test_2eproto_2epb_2eh
