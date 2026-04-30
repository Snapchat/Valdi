// Generate by the Snap-modified protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: valdi.proto
// Protobuf C++ Version: 5.27.0

#ifndef GOOGLE_PROTOBUF_INCLUDED_valdi_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_valdi_2eproto_2epb_2eh

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
#include "google/protobuf/generated_enum_util.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_valdi_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_valdi_2eproto {
  static const ::uint32_t offsets[];
};
namespace Valdi {
class CSSRuleIndex;
struct CSSRuleIndexDefaultTypeInternal;
extern CSSRuleIndexDefaultTypeInternal _CSSRuleIndex_default_instance_;
class CSSRuleIndex_AttributeRule;
struct CSSRuleIndex_AttributeRuleDefaultTypeInternal;
extern CSSRuleIndex_AttributeRuleDefaultTypeInternal _CSSRuleIndex_AttributeRule_default_instance_;
class CSSRuleIndex_NthChildRule;
struct CSSRuleIndex_NthChildRuleDefaultTypeInternal;
extern CSSRuleIndex_NthChildRuleDefaultTypeInternal _CSSRuleIndex_NthChildRule_default_instance_;
class ClientAwakeMessage;
struct ClientAwakeMessageDefaultTypeInternal;
extern ClientAwakeMessageDefaultTypeInternal _ClientAwakeMessage_default_instance_;
class DaemonAwakeMessage;
struct DaemonAwakeMessageDefaultTypeInternal;
extern DaemonAwakeMessageDefaultTypeInternal _DaemonAwakeMessage_default_instance_;
class DaemonServiceDiscoveryPayload;
struct DaemonServiceDiscoveryPayloadDefaultTypeInternal;
extern DaemonServiceDiscoveryPayloadDefaultTypeInternal _DaemonServiceDiscoveryPayload_default_instance_;
class DownloadableModuleArtifact;
struct DownloadableModuleArtifactDefaultTypeInternal;
extern DownloadableModuleArtifactDefaultTypeInternal _DownloadableModuleArtifact_default_instance_;
class DownloadableModuleAssets;
struct DownloadableModuleAssetsDefaultTypeInternal;
extern DownloadableModuleAssetsDefaultTypeInternal _DownloadableModuleAssets_default_instance_;
class DownloadableModuleManifest;
struct DownloadableModuleManifestDefaultTypeInternal;
extern DownloadableModuleManifestDefaultTypeInternal _DownloadableModuleManifest_default_instance_;
class NamedStyleNode;
struct NamedStyleNodeDefaultTypeInternal;
extern NamedStyleNodeDefaultTypeInternal _NamedStyleNode_default_instance_;
class NodeAttribute;
struct NodeAttributeDefaultTypeInternal;
extern NodeAttributeDefaultTypeInternal _NodeAttribute_default_instance_;
class StyleDeclaration;
struct StyleDeclarationDefaultTypeInternal;
extern StyleDeclarationDefaultTypeInternal _StyleDeclaration_default_instance_;
class StyleNode;
struct StyleNodeDefaultTypeInternal;
extern StyleNodeDefaultTypeInternal _StyleNode_default_instance_;
class TsnConfig;
struct TsnConfigDefaultTypeInternal;
extern TsnConfigDefaultTypeInternal _TsnConfig_default_instance_;
}  // namespace Valdi
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google

namespace Valdi {
enum NodeAttribute_Type : int {
  NodeAttribute_Type_NODE_ATTRIBUTE_TYPE_STRING = 0,
  NodeAttribute_Type_NODE_ATTRIBUTE_TYPE_INT = 1,
  NodeAttribute_Type_NODE_ATTRIBUTE_TYPE_DOUBLE = 2,
  NodeAttribute_Type_NodeAttribute_Type_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  NodeAttribute_Type_NodeAttribute_Type_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

bool NodeAttribute_Type_IsValid(int value);
extern const uint32_t NodeAttribute_Type_internal_data_[];
constexpr NodeAttribute_Type NodeAttribute_Type_Type_MIN = static_cast<NodeAttribute_Type>(0);
constexpr NodeAttribute_Type NodeAttribute_Type_Type_MAX = static_cast<NodeAttribute_Type>(2);
constexpr int NodeAttribute_Type_Type_ARRAYSIZE = 2 + 1;
const std::string& NodeAttribute_Type_Name(NodeAttribute_Type value);
template <typename T>
const std::string& NodeAttribute_Type_Name(T value) {
  static_assert(std::is_same<T, NodeAttribute_Type>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to Type_Name().");
  return NodeAttribute_Type_Name(static_cast<NodeAttribute_Type>(value));
}
bool NodeAttribute_Type_Parse(absl::string_view name, NodeAttribute_Type* value);
enum CSSRuleIndex_AttributeRule_Type : int {
  CSSRuleIndex_AttributeRule_Type_EQUALS = 0,
  CSSRuleIndex_AttributeRule_Type_CONTAINS_WORD = 1,
  CSSRuleIndex_AttributeRule_Type_STARTS_WITH_WORD = 2,
  CSSRuleIndex_AttributeRule_Type_STARTS_WITH_SUBSTRING = 3,
  CSSRuleIndex_AttributeRule_Type_ENDS_WITH_SUBSTRING = 4,
  CSSRuleIndex_AttributeRule_Type_CONTAINS_SUBSTRING = 5,
  CSSRuleIndex_AttributeRule_Type_HAS_ATTRIBUTE = 6,
  CSSRuleIndex_AttributeRule_Type_CSSRuleIndex_AttributeRule_Type_INT_MIN_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::min(),
  CSSRuleIndex_AttributeRule_Type_CSSRuleIndex_AttributeRule_Type_INT_MAX_SENTINEL_DO_NOT_USE_ =
      std::numeric_limits<::int32_t>::max(),
};

bool CSSRuleIndex_AttributeRule_Type_IsValid(int value);
extern const uint32_t CSSRuleIndex_AttributeRule_Type_internal_data_[];
constexpr CSSRuleIndex_AttributeRule_Type CSSRuleIndex_AttributeRule_Type_Type_MIN = static_cast<CSSRuleIndex_AttributeRule_Type>(0);
constexpr CSSRuleIndex_AttributeRule_Type CSSRuleIndex_AttributeRule_Type_Type_MAX = static_cast<CSSRuleIndex_AttributeRule_Type>(6);
constexpr int CSSRuleIndex_AttributeRule_Type_Type_ARRAYSIZE = 6 + 1;
const std::string& CSSRuleIndex_AttributeRule_Type_Name(CSSRuleIndex_AttributeRule_Type value);
template <typename T>
const std::string& CSSRuleIndex_AttributeRule_Type_Name(T value) {
  static_assert(std::is_same<T, CSSRuleIndex_AttributeRule_Type>::value ||
                    std::is_integral<T>::value,
                "Incorrect type passed to Type_Name().");
  return CSSRuleIndex_AttributeRule_Type_Name(static_cast<CSSRuleIndex_AttributeRule_Type>(value));
}
bool CSSRuleIndex_AttributeRule_Type_Parse(absl::string_view name, CSSRuleIndex_AttributeRule_Type* value);

// ===================================================================


// -------------------------------------------------------------------

class TsnConfig final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:Valdi.TsnConfig) */ {
 public:
  inline TsnConfig() : TsnConfig(nullptr) {}
  ~TsnConfig() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR TsnConfig(
      ::google::protobuf::internal::ConstantInitialized);

  inline TsnConfig(const TsnConfig& from) : TsnConfig(nullptr, from) {}
  inline TsnConfig(TsnConfig&& from) noexcept
      : TsnConfig(nullptr, std::move(from)) {}
  inline TsnConfig& operator=(const TsnConfig& from) {
    CopyFrom(from);
    return *this;
  }
  inline TsnConfig& operator=(TsnConfig&& from) noexcept {
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

  static const TsnConfig& default_instance() {
    return *internal_default_instance();
  }
  static inline const TsnConfig* internal_default_instance() {
    return reinterpret_cast<const TsnConfig*>(
        &_TsnConfig_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 13;
  friend void swap(TsnConfig& a, TsnConfig& b) { a.Swap(&b); }
  inline void Swap(TsnConfig* other) {
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
  void UnsafeArenaSwap(TsnConfig* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  TsnConfig* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<TsnConfig>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const TsnConfig& from);
  void MergeFrom(const TsnConfig& from);
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
  void InternalSwap(TsnConfig* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "Valdi.TsnConfig"; }

 protected:
  explicit TsnConfig(::google::protobuf::Arena* arena);
  TsnConfig(::google::protobuf::Arena* arena, const TsnConfig& from);
  TsnConfig(::google::protobuf::Arena* arena, TsnConfig&& from) noexcept
      : TsnConfig(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kEnabledModulesFieldNumber = 1,
  };
  // repeated string enabled_modules = 1;
  int enabled_modules_size() const;
  private:
  int _internal_enabled_modules_size() const;

  public:
  void clear_enabled_modules() ;
  const std::string& enabled_modules(int index) const;
  std::string* mutable_enabled_modules(int index);
  void set_enabled_modules(int index, const std::string& value);
  void set_enabled_modules(int index, std::string&& value);
  void set_enabled_modules(int index, const char* value);
  void set_enabled_modules(int index, const char* value, std::size_t size);
  void set_enabled_modules(int index, absl::string_view value);
  std::string* add_enabled_modules();
  void add_enabled_modules(const std::string& value);
  void add_enabled_modules(std::string&& value);
  void add_enabled_modules(const char* value);
  void add_enabled_modules(const char* value, std::size_t size);
  void add_enabled_modules(absl::string_view value);
  const ::google::protobuf::RepeatedPtrField<std::string>& enabled_modules() const;
  ::google::protobuf::RepeatedPtrField<std::string>* mutable_enabled_modules();

  private:
  const ::google::protobuf::RepeatedPtrField<std::string>& _internal_enabled_modules() const;
  ::google::protobuf::RepeatedPtrField<std::string>* _internal_mutable_enabled_modules();

  public:
  // @@protoc_insertion_point(class_scope:Valdi.TsnConfig)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      0, 1, 0,
      39, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_TsnConfig_default_instance_;

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
                          const TsnConfig& from_msg);
    ::google::protobuf::RepeatedPtrField<std::string> enabled_modules_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_valdi_2eproto;
};
// -------------------------------------------------------------------

class NodeAttribute final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:Valdi.NodeAttribute) */ {
 public:
  inline NodeAttribute() : NodeAttribute(nullptr) {}
  ~NodeAttribute() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR NodeAttribute(
      ::google::protobuf::internal::ConstantInitialized);

  inline NodeAttribute(const NodeAttribute& from) : NodeAttribute(nullptr, from) {}
  inline NodeAttribute(NodeAttribute&& from) noexcept
      : NodeAttribute(nullptr, std::move(from)) {}
  inline NodeAttribute& operator=(const NodeAttribute& from) {
    CopyFrom(from);
    return *this;
  }
  inline NodeAttribute& operator=(NodeAttribute&& from) noexcept {
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

  static const NodeAttribute& default_instance() {
    return *internal_default_instance();
  }
  static inline const NodeAttribute* internal_default_instance() {
    return reinterpret_cast<const NodeAttribute*>(
        &_NodeAttribute_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(NodeAttribute& a, NodeAttribute& b) { a.Swap(&b); }
  inline void Swap(NodeAttribute* other) {
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
  void UnsafeArenaSwap(NodeAttribute* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  NodeAttribute* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<NodeAttribute>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const NodeAttribute& from);
  void MergeFrom(const NodeAttribute& from);
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
  void InternalSwap(NodeAttribute* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "Valdi.NodeAttribute"; }

 protected:
  explicit NodeAttribute(::google::protobuf::Arena* arena);
  NodeAttribute(::google::protobuf::Arena* arena, const NodeAttribute& from);
  NodeAttribute(::google::protobuf::Arena* arena, NodeAttribute&& from) noexcept
      : NodeAttribute(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------
  using Type = NodeAttribute_Type;
  static constexpr Type NODE_ATTRIBUTE_TYPE_STRING = NodeAttribute_Type_NODE_ATTRIBUTE_TYPE_STRING;
  static constexpr Type NODE_ATTRIBUTE_TYPE_INT = NodeAttribute_Type_NODE_ATTRIBUTE_TYPE_INT;
  static constexpr Type NODE_ATTRIBUTE_TYPE_DOUBLE = NodeAttribute_Type_NODE_ATTRIBUTE_TYPE_DOUBLE;
  static inline bool Type_IsValid(int value) {
    return NodeAttribute_Type_IsValid(value);
  }
  static constexpr Type Type_MIN = NodeAttribute_Type_Type_MIN;
  static constexpr Type Type_MAX = NodeAttribute_Type_Type_MAX;
  static constexpr int Type_ARRAYSIZE = NodeAttribute_Type_Type_ARRAYSIZE;
  template <typename T>
  static inline const std::string& Type_Name(T value) {
    return NodeAttribute_Type_Name(value);
  }
  static inline bool Type_Parse(absl::string_view name, Type* value) {
    return NodeAttribute_Type_Parse(name, value);
  }

  // accessors -------------------------------------------------------
  enum : int {
    kNameFieldNumber = 2,
    kStrValueFieldNumber = 3,
    kIntValueFieldNumber = 4,
    kDoubleValueFieldNumber = 5,
    kTypeFieldNumber = 1,
  };
  // string name = 2;
  void clear_name() ;
  const std::string& name() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_name(Arg_&& arg, Args_... args);
  std::string* mutable_name();
  PROTOBUF_NODISCARD std::string* release_name();
  void set_allocated_name(std::string* value);

  private:
  const std::string& _internal_name() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_name(
      const std::string& value);
  std::string* _internal_mutable_name();

  public:
  // string str_value = 3;
  void clear_str_value() ;
  const std::string& str_value() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_str_value(Arg_&& arg, Args_... args);
  std::string* mutable_str_value();
  PROTOBUF_NODISCARD std::string* release_str_value();
  void set_allocated_str_value(std::string* value);

  private:
  const std::string& _internal_str_value() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_str_value(
      const std::string& value);
  std::string* _internal_mutable_str_value();

  public:
  // int64 int_value = 4;
  void clear_int_value() ;
  ::int64_t int_value() const;
  void set_int_value(::int64_t value);

  private:
  ::int64_t _internal_int_value() const;
  void _internal_set_int_value(::int64_t value);

  public:
  // double double_value = 5;
  void clear_double_value() ;
  double double_value() const;
  void set_double_value(double value);

  private:
  double _internal_double_value() const;
  void _internal_set_double_value(double value);

  public:
  // .Valdi.NodeAttribute.Type type = 1;
  void clear_type() ;
  ::Valdi::NodeAttribute_Type type() const;
  void set_type(::Valdi::NodeAttribute_Type value);

  private:
  ::Valdi::NodeAttribute_Type _internal_type() const;
  void _internal_set_type(::Valdi::NodeAttribute_Type value);

  public:
  // @@protoc_insertion_point(class_scope:Valdi.NodeAttribute)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      3, 5, 0,
      41, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_NodeAttribute_default_instance_;

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
                          const NodeAttribute& from_msg);
    ::google::protobuf::internal::ArenaStringPtr name_;
    ::google::protobuf::internal::ArenaStringPtr str_value_;
    ::int64_t int_value_;
    double double_value_;
    int type_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_valdi_2eproto;
};
// -------------------------------------------------------------------

class DownloadableModuleArtifact final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:Valdi.DownloadableModuleArtifact) */ {
 public:
  inline DownloadableModuleArtifact() : DownloadableModuleArtifact(nullptr) {}
  ~DownloadableModuleArtifact() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR DownloadableModuleArtifact(
      ::google::protobuf::internal::ConstantInitialized);

  inline DownloadableModuleArtifact(const DownloadableModuleArtifact& from) : DownloadableModuleArtifact(nullptr, from) {}
  inline DownloadableModuleArtifact(DownloadableModuleArtifact&& from) noexcept
      : DownloadableModuleArtifact(nullptr, std::move(from)) {}
  inline DownloadableModuleArtifact& operator=(const DownloadableModuleArtifact& from) {
    CopyFrom(from);
    return *this;
  }
  inline DownloadableModuleArtifact& operator=(DownloadableModuleArtifact&& from) noexcept {
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

  static const DownloadableModuleArtifact& default_instance() {
    return *internal_default_instance();
  }
  static inline const DownloadableModuleArtifact* internal_default_instance() {
    return reinterpret_cast<const DownloadableModuleArtifact*>(
        &_DownloadableModuleArtifact_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(DownloadableModuleArtifact& a, DownloadableModuleArtifact& b) { a.Swap(&b); }
  inline void Swap(DownloadableModuleArtifact* other) {
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
  void UnsafeArenaSwap(DownloadableModuleArtifact* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  DownloadableModuleArtifact* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<DownloadableModuleArtifact>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const DownloadableModuleArtifact& from);
  void MergeFrom(const DownloadableModuleArtifact& from);
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
  void InternalSwap(DownloadableModuleArtifact* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "Valdi.DownloadableModuleArtifact"; }

 protected:
  explicit DownloadableModuleArtifact(::google::protobuf::Arena* arena);
  DownloadableModuleArtifact(::google::protobuf::Arena* arena, const DownloadableModuleArtifact& from);
  DownloadableModuleArtifact(::google::protobuf::Arena* arena, DownloadableModuleArtifact&& from) noexcept
      : DownloadableModuleArtifact(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kUrlFieldNumber = 1,
    kSha256DigestFieldNumber = 2,
  };
  // string url = 1;
  void clear_url() ;
  const std::string& url() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_url(Arg_&& arg, Args_... args);
  std::string* mutable_url();
  PROTOBUF_NODISCARD std::string* release_url();
  void set_allocated_url(std::string* value);

  private:
  const std::string& _internal_url() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_url(
      const std::string& value);
  std::string* _internal_mutable_url();

  public:
  // bytes sha256Digest = 2;
  void clear_sha256digest() ;
  const std::string& sha256digest() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_sha256digest(Arg_&& arg, Args_... args);
  std::string* mutable_sha256digest();
  PROTOBUF_NODISCARD std::string* release_sha256digest();
  void set_allocated_sha256digest(std::string* value);

  private:
  const std::string& _internal_sha256digest() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_sha256digest(
      const std::string& value);
  std::string* _internal_mutable_sha256digest();

  public:
  // @@protoc_insertion_point(class_scope:Valdi.DownloadableModuleArtifact)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      1, 2, 0,
      44, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_DownloadableModuleArtifact_default_instance_;

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
                          const DownloadableModuleArtifact& from_msg);
    ::google::protobuf::internal::ArenaStringPtr url_;
    ::google::protobuf::internal::ArenaStringPtr sha256digest_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_valdi_2eproto;
};
// -------------------------------------------------------------------

class DaemonAwakeMessage final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:Valdi.DaemonAwakeMessage) */ {
 public:
  inline DaemonAwakeMessage() : DaemonAwakeMessage(nullptr) {}
  ~DaemonAwakeMessage() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR DaemonAwakeMessage(
      ::google::protobuf::internal::ConstantInitialized);

  inline DaemonAwakeMessage(const DaemonAwakeMessage& from) : DaemonAwakeMessage(nullptr, from) {}
  inline DaemonAwakeMessage(DaemonAwakeMessage&& from) noexcept
      : DaemonAwakeMessage(nullptr, std::move(from)) {}
  inline DaemonAwakeMessage& operator=(const DaemonAwakeMessage& from) {
    CopyFrom(from);
    return *this;
  }
  inline DaemonAwakeMessage& operator=(DaemonAwakeMessage&& from) noexcept {
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

  static const DaemonAwakeMessage& default_instance() {
    return *internal_default_instance();
  }
  static inline const DaemonAwakeMessage* internal_default_instance() {
    return reinterpret_cast<const DaemonAwakeMessage*>(
        &_DaemonAwakeMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 4;
  friend void swap(DaemonAwakeMessage& a, DaemonAwakeMessage& b) { a.Swap(&b); }
  inline void Swap(DaemonAwakeMessage* other) {
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
  void UnsafeArenaSwap(DaemonAwakeMessage* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  DaemonAwakeMessage* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<DaemonAwakeMessage>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const DaemonAwakeMessage& from);
  void MergeFrom(const DaemonAwakeMessage& from);
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
  void InternalSwap(DaemonAwakeMessage* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "Valdi.DaemonAwakeMessage"; }

 protected:
  explicit DaemonAwakeMessage(::google::protobuf::Arena* arena);
  DaemonAwakeMessage(::google::protobuf::Arena* arena, const DaemonAwakeMessage& from);
  DaemonAwakeMessage(::google::protobuf::Arena* arena, DaemonAwakeMessage&& from) noexcept
      : DaemonAwakeMessage(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kServiceAddressesFieldNumber = 1,
    kDeviceIdsFieldNumber = 16,
    kUsernamesFieldNumber = 17,
    kProcessIdFieldNumber = 3,
    kServicePortFieldNumber = 2,
  };
  // repeated string service_addresses = 1;
  int service_addresses_size() const;
  private:
  int _internal_service_addresses_size() const;

  public:
  void clear_service_addresses() ;
  const std::string& service_addresses(int index) const;
  std::string* mutable_service_addresses(int index);
  void set_service_addresses(int index, const std::string& value);
  void set_service_addresses(int index, std::string&& value);
  void set_service_addresses(int index, const char* value);
  void set_service_addresses(int index, const char* value, std::size_t size);
  void set_service_addresses(int index, absl::string_view value);
  std::string* add_service_addresses();
  void add_service_addresses(const std::string& value);
  void add_service_addresses(std::string&& value);
  void add_service_addresses(const char* value);
  void add_service_addresses(const char* value, std::size_t size);
  void add_service_addresses(absl::string_view value);
  const ::google::protobuf::RepeatedPtrField<std::string>& service_addresses() const;
  ::google::protobuf::RepeatedPtrField<std::string>* mutable_service_addresses();

  private:
  const ::google::protobuf::RepeatedPtrField<std::string>& _internal_service_addresses() const;
  ::google::protobuf::RepeatedPtrField<std::string>* _internal_mutable_service_addresses();

  public:
  // repeated string device_ids = 16;
  int device_ids_size() const;
  private:
  int _internal_device_ids_size() const;

  public:
  void clear_device_ids() ;
  const std::string& device_ids(int index) const;
  std::string* mutable_device_ids(int index);
  void set_device_ids(int index, const std::string& value);
  void set_device_ids(int index, std::string&& value);
  void set_device_ids(int index, const char* value);
  void set_device_ids(int index, const char* value, std::size_t size);
  void set_device_ids(int index, absl::string_view value);
  std::string* add_device_ids();
  void add_device_ids(const std::string& value);
  void add_device_ids(std::string&& value);
  void add_device_ids(const char* value);
  void add_device_ids(const char* value, std::size_t size);
  void add_device_ids(absl::string_view value);
  const ::google::protobuf::RepeatedPtrField<std::string>& device_ids() const;
  ::google::protobuf::RepeatedPtrField<std::string>* mutable_device_ids();

  private:
  const ::google::protobuf::RepeatedPtrField<std::string>& _internal_device_ids() const;
  ::google::protobuf::RepeatedPtrField<std::string>* _internal_mutable_device_ids();

  public:
  // repeated string usernames = 17;
  int usernames_size() const;
  private:
  int _internal_usernames_size() const;

  public:
  void clear_usernames() ;
  const std::string& usernames(int index) const;
  std::string* mutable_usernames(int index);
  void set_usernames(int index, const std::string& value);
  void set_usernames(int index, std::string&& value);
  void set_usernames(int index, const char* value);
  void set_usernames(int index, const char* value, std::size_t size);
  void set_usernames(int index, absl::string_view value);
  std::string* add_usernames();
  void add_usernames(const std::string& value);
  void add_usernames(std::string&& value);
  void add_usernames(const char* value);
  void add_usernames(const char* value, std::size_t size);
  void add_usernames(absl::string_view value);
  const ::google::protobuf::RepeatedPtrField<std::string>& usernames() const;
  ::google::protobuf::RepeatedPtrField<std::string>* mutable_usernames();

  private:
  const ::google::protobuf::RepeatedPtrField<std::string>& _internal_usernames() const;
  ::google::protobuf::RepeatedPtrField<std::string>* _internal_mutable_usernames();

  public:
  // string process_id = 3;
  void clear_process_id() ;
  const std::string& process_id() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_process_id(Arg_&& arg, Args_... args);
  std::string* mutable_process_id();
  PROTOBUF_NODISCARD std::string* release_process_id();
  void set_allocated_process_id(std::string* value);

  private:
  const std::string& _internal_process_id() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_process_id(
      const std::string& value);
  std::string* _internal_mutable_process_id();

  public:
  // int32 service_port = 2;
  void clear_service_port() ;
  ::int32_t service_port() const;
  void set_service_port(::int32_t value);

  private:
  ::int32_t _internal_service_port() const;
  void _internal_set_service_port(::int32_t value);

  public:
  // @@protoc_insertion_point(class_scope:Valdi.DaemonAwakeMessage)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      2, 5, 0,
      79, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_DaemonAwakeMessage_default_instance_;

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
                          const DaemonAwakeMessage& from_msg);
    ::google::protobuf::RepeatedPtrField<std::string> service_addresses_;
    ::google::protobuf::RepeatedPtrField<std::string> device_ids_;
    ::google::protobuf::RepeatedPtrField<std::string> usernames_;
    ::google::protobuf::internal::ArenaStringPtr process_id_;
    ::int32_t service_port_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_valdi_2eproto;
};
// -------------------------------------------------------------------

class ClientAwakeMessage final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:Valdi.ClientAwakeMessage) */ {
 public:
  inline ClientAwakeMessage() : ClientAwakeMessage(nullptr) {}
  ~ClientAwakeMessage() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR ClientAwakeMessage(
      ::google::protobuf::internal::ConstantInitialized);

  inline ClientAwakeMessage(const ClientAwakeMessage& from) : ClientAwakeMessage(nullptr, from) {}
  inline ClientAwakeMessage(ClientAwakeMessage&& from) noexcept
      : ClientAwakeMessage(nullptr, std::move(from)) {}
  inline ClientAwakeMessage& operator=(const ClientAwakeMessage& from) {
    CopyFrom(from);
    return *this;
  }
  inline ClientAwakeMessage& operator=(ClientAwakeMessage&& from) noexcept {
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

  static const ClientAwakeMessage& default_instance() {
    return *internal_default_instance();
  }
  static inline const ClientAwakeMessage* internal_default_instance() {
    return reinterpret_cast<const ClientAwakeMessage*>(
        &_ClientAwakeMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 5;
  friend void swap(ClientAwakeMessage& a, ClientAwakeMessage& b) { a.Swap(&b); }
  inline void Swap(ClientAwakeMessage* other) {
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
  void UnsafeArenaSwap(ClientAwakeMessage* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  ClientAwakeMessage* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<ClientAwakeMessage>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const ClientAwakeMessage& from);
  void MergeFrom(const ClientAwakeMessage& from);
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
  void InternalSwap(ClientAwakeMessage* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "Valdi.ClientAwakeMessage"; }

 protected:
  explicit ClientAwakeMessage(::google::protobuf::Arena* arena);
  ClientAwakeMessage(::google::protobuf::Arena* arena, const ClientAwakeMessage& from);
  ClientAwakeMessage(::google::protobuf::Arena* arena, ClientAwakeMessage&& from) noexcept
      : ClientAwakeMessage(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kDeviceIdFieldNumber = 1,
    kUsernameFieldNumber = 2,
  };
  // string device_id = 1;
  void clear_device_id() ;
  const std::string& device_id() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_device_id(Arg_&& arg, Args_... args);
  std::string* mutable_device_id();
  PROTOBUF_NODISCARD std::string* release_device_id();
  void set_allocated_device_id(std::string* value);

  private:
  const std::string& _internal_device_id() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_device_id(
      const std::string& value);
  std::string* _internal_mutable_device_id();

  public:
  // string username = 2;
  void clear_username() ;
  const std::string& username() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_username(Arg_&& arg, Args_... args);
  std::string* mutable_username();
  PROTOBUF_NODISCARD std::string* release_username();
  void set_allocated_username(std::string* value);

  private:
  const std::string& _internal_username() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_username(
      const std::string& value);
  std::string* _internal_mutable_username();

  public:
  // @@protoc_insertion_point(class_scope:Valdi.ClientAwakeMessage)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      1, 2, 0,
      50, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_ClientAwakeMessage_default_instance_;

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
                          const ClientAwakeMessage& from_msg);
    ::google::protobuf::internal::ArenaStringPtr device_id_;
    ::google::protobuf::internal::ArenaStringPtr username_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_valdi_2eproto;
};
// -------------------------------------------------------------------

class StyleDeclaration final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:Valdi.StyleDeclaration) */ {
 public:
  inline StyleDeclaration() : StyleDeclaration(nullptr) {}
  ~StyleDeclaration() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR StyleDeclaration(
      ::google::protobuf::internal::ConstantInitialized);

  inline StyleDeclaration(const StyleDeclaration& from) : StyleDeclaration(nullptr, from) {}
  inline StyleDeclaration(StyleDeclaration&& from) noexcept
      : StyleDeclaration(nullptr, std::move(from)) {}
  inline StyleDeclaration& operator=(const StyleDeclaration& from) {
    CopyFrom(from);
    return *this;
  }
  inline StyleDeclaration& operator=(StyleDeclaration&& from) noexcept {
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

  static const StyleDeclaration& default_instance() {
    return *internal_default_instance();
  }
  static inline const StyleDeclaration* internal_default_instance() {
    return reinterpret_cast<const StyleDeclaration*>(
        &_StyleDeclaration_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 8;
  friend void swap(StyleDeclaration& a, StyleDeclaration& b) { a.Swap(&b); }
  inline void Swap(StyleDeclaration* other) {
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
  void UnsafeArenaSwap(StyleDeclaration* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  StyleDeclaration* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<StyleDeclaration>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const StyleDeclaration& from);
  void MergeFrom(const StyleDeclaration& from);
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
  void InternalSwap(StyleDeclaration* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "Valdi.StyleDeclaration"; }

 protected:
  explicit StyleDeclaration(::google::protobuf::Arena* arena);
  StyleDeclaration(::google::protobuf::Arena* arena, const StyleDeclaration& from);
  StyleDeclaration(::google::protobuf::Arena* arena, StyleDeclaration&& from) noexcept
      : StyleDeclaration(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kAttributeFieldNumber = 1,
    kPriorityFieldNumber = 2,
    kOrderFieldNumber = 3,
    kIdFieldNumber = 4,
  };
  // .Valdi.NodeAttribute attribute = 1;
  bool has_attribute() const;
  void clear_attribute() ;
  const ::Valdi::NodeAttribute& attribute() const;
  PROTOBUF_NODISCARD ::Valdi::NodeAttribute* release_attribute();
  ::Valdi::NodeAttribute* mutable_attribute();
  void set_allocated_attribute(::Valdi::NodeAttribute* value);
  void unsafe_arena_set_allocated_attribute(::Valdi::NodeAttribute* value);
  ::Valdi::NodeAttribute* unsafe_arena_release_attribute();

  private:
  const ::Valdi::NodeAttribute& _internal_attribute() const;
  ::Valdi::NodeAttribute* _internal_mutable_attribute();

  public:
  // int32 priority = 2;
  void clear_priority() ;
  ::int32_t priority() const;
  void set_priority(::int32_t value);

  private:
  ::int32_t _internal_priority() const;
  void _internal_set_priority(::int32_t value);

  public:
  // int32 order = 3;
  void clear_order() ;
  ::int32_t order() const;
  void set_order(::int32_t value);

  private:
  ::int32_t _internal_order() const;
  void _internal_set_order(::int32_t value);

  public:
  // int32 id = 4;
  void clear_id() ;
  ::int32_t id() const;
  void set_id(::int32_t value);

  private:
  ::int32_t _internal_id() const;
  void _internal_set_id(::int32_t value);

  public:
  // @@protoc_insertion_point(class_scope:Valdi.StyleDeclaration)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      2, 4, 1,
      0, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_StyleDeclaration_default_instance_;

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
                          const StyleDeclaration& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    ::Valdi::NodeAttribute* attribute_;
    ::int32_t priority_;
    ::int32_t order_;
    ::int32_t id_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_valdi_2eproto;
};
// -------------------------------------------------------------------

class DownloadableModuleAssets final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:Valdi.DownloadableModuleAssets) */ {
 public:
  inline DownloadableModuleAssets() : DownloadableModuleAssets(nullptr) {}
  ~DownloadableModuleAssets() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR DownloadableModuleAssets(
      ::google::protobuf::internal::ConstantInitialized);

  inline DownloadableModuleAssets(const DownloadableModuleAssets& from) : DownloadableModuleAssets(nullptr, from) {}
  inline DownloadableModuleAssets(DownloadableModuleAssets&& from) noexcept
      : DownloadableModuleAssets(nullptr, std::move(from)) {}
  inline DownloadableModuleAssets& operator=(const DownloadableModuleAssets& from) {
    CopyFrom(from);
    return *this;
  }
  inline DownloadableModuleAssets& operator=(DownloadableModuleAssets&& from) noexcept {
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

  static const DownloadableModuleAssets& default_instance() {
    return *internal_default_instance();
  }
  static inline const DownloadableModuleAssets* internal_default_instance() {
    return reinterpret_cast<const DownloadableModuleAssets*>(
        &_DownloadableModuleAssets_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 2;
  friend void swap(DownloadableModuleAssets& a, DownloadableModuleAssets& b) { a.Swap(&b); }
  inline void Swap(DownloadableModuleAssets* other) {
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
  void UnsafeArenaSwap(DownloadableModuleAssets* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  DownloadableModuleAssets* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<DownloadableModuleAssets>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const DownloadableModuleAssets& from);
  void MergeFrom(const DownloadableModuleAssets& from);
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
  void InternalSwap(DownloadableModuleAssets* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "Valdi.DownloadableModuleAssets"; }

 protected:
  explicit DownloadableModuleAssets(::google::protobuf::Arena* arena);
  DownloadableModuleAssets(::google::protobuf::Arena* arena, const DownloadableModuleAssets& from);
  DownloadableModuleAssets(::google::protobuf::Arena* arena, DownloadableModuleAssets&& from) noexcept
      : DownloadableModuleAssets(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kArtifactFieldNumber = 1,
    kDeviceDensityFieldNumber = 3,
  };
  // .Valdi.DownloadableModuleArtifact artifact = 1;
  bool has_artifact() const;
  void clear_artifact() ;
  const ::Valdi::DownloadableModuleArtifact& artifact() const;
  PROTOBUF_NODISCARD ::Valdi::DownloadableModuleArtifact* release_artifact();
  ::Valdi::DownloadableModuleArtifact* mutable_artifact();
  void set_allocated_artifact(::Valdi::DownloadableModuleArtifact* value);
  void unsafe_arena_set_allocated_artifact(::Valdi::DownloadableModuleArtifact* value);
  ::Valdi::DownloadableModuleArtifact* unsafe_arena_release_artifact();

  private:
  const ::Valdi::DownloadableModuleArtifact& _internal_artifact() const;
  ::Valdi::DownloadableModuleArtifact* _internal_mutable_artifact();

  public:
  // double device_density = 3;
  void clear_device_density() ;
  double device_density() const;
  void set_device_density(double value);

  private:
  double _internal_device_density() const;
  void _internal_set_device_density(double value);

  public:
  // @@protoc_insertion_point(class_scope:Valdi.DownloadableModuleAssets)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      2, 2, 1,
      0, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_DownloadableModuleAssets_default_instance_;

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
                          const DownloadableModuleAssets& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    ::Valdi::DownloadableModuleArtifact* artifact_;
    double device_density_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_valdi_2eproto;
};
// -------------------------------------------------------------------

class DaemonServiceDiscoveryPayload final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:Valdi.DaemonServiceDiscoveryPayload) */ {
 public:
  inline DaemonServiceDiscoveryPayload() : DaemonServiceDiscoveryPayload(nullptr) {}
  ~DaemonServiceDiscoveryPayload() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR DaemonServiceDiscoveryPayload(
      ::google::protobuf::internal::ConstantInitialized);

  inline DaemonServiceDiscoveryPayload(const DaemonServiceDiscoveryPayload& from) : DaemonServiceDiscoveryPayload(nullptr, from) {}
  inline DaemonServiceDiscoveryPayload(DaemonServiceDiscoveryPayload&& from) noexcept
      : DaemonServiceDiscoveryPayload(nullptr, std::move(from)) {}
  inline DaemonServiceDiscoveryPayload& operator=(const DaemonServiceDiscoveryPayload& from) {
    CopyFrom(from);
    return *this;
  }
  inline DaemonServiceDiscoveryPayload& operator=(DaemonServiceDiscoveryPayload&& from) noexcept {
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

  static const DaemonServiceDiscoveryPayload& default_instance() {
    return *internal_default_instance();
  }
  enum MessageCase {
    kDaemonAwakeMessage = 1,
    kClientAwakeMessage = 2,
    MESSAGE_NOT_SET = 0,
  };
  static inline const DaemonServiceDiscoveryPayload* internal_default_instance() {
    return reinterpret_cast<const DaemonServiceDiscoveryPayload*>(
        &_DaemonServiceDiscoveryPayload_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 6;
  friend void swap(DaemonServiceDiscoveryPayload& a, DaemonServiceDiscoveryPayload& b) { a.Swap(&b); }
  inline void Swap(DaemonServiceDiscoveryPayload* other) {
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
  void UnsafeArenaSwap(DaemonServiceDiscoveryPayload* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  DaemonServiceDiscoveryPayload* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<DaemonServiceDiscoveryPayload>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const DaemonServiceDiscoveryPayload& from);
  void MergeFrom(const DaemonServiceDiscoveryPayload& from);
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
  void InternalSwap(DaemonServiceDiscoveryPayload* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "Valdi.DaemonServiceDiscoveryPayload"; }

 protected:
  explicit DaemonServiceDiscoveryPayload(::google::protobuf::Arena* arena);
  DaemonServiceDiscoveryPayload(::google::protobuf::Arena* arena, const DaemonServiceDiscoveryPayload& from);
  DaemonServiceDiscoveryPayload(::google::protobuf::Arena* arena, DaemonServiceDiscoveryPayload&& from) noexcept
      : DaemonServiceDiscoveryPayload(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kDaemonAwakeMessageFieldNumber = 1,
    kClientAwakeMessageFieldNumber = 2,
  };
  // .Valdi.DaemonAwakeMessage daemon_awake_message = 1;
  bool has_daemon_awake_message() const;
  private:
  bool _internal_has_daemon_awake_message() const;

  public:
  void clear_daemon_awake_message() ;
  const ::Valdi::DaemonAwakeMessage& daemon_awake_message() const;
  PROTOBUF_NODISCARD ::Valdi::DaemonAwakeMessage* release_daemon_awake_message();
  ::Valdi::DaemonAwakeMessage* mutable_daemon_awake_message();
  void set_allocated_daemon_awake_message(::Valdi::DaemonAwakeMessage* value);
  void unsafe_arena_set_allocated_daemon_awake_message(::Valdi::DaemonAwakeMessage* value);
  ::Valdi::DaemonAwakeMessage* unsafe_arena_release_daemon_awake_message();

  private:
  const ::Valdi::DaemonAwakeMessage& _internal_daemon_awake_message() const;
  ::Valdi::DaemonAwakeMessage* _internal_mutable_daemon_awake_message();

  public:
  // .Valdi.ClientAwakeMessage client_awake_message = 2;
  bool has_client_awake_message() const;
  private:
  bool _internal_has_client_awake_message() const;

  public:
  void clear_client_awake_message() ;
  const ::Valdi::ClientAwakeMessage& client_awake_message() const;
  PROTOBUF_NODISCARD ::Valdi::ClientAwakeMessage* release_client_awake_message();
  ::Valdi::ClientAwakeMessage* mutable_client_awake_message();
  void set_allocated_client_awake_message(::Valdi::ClientAwakeMessage* value);
  void unsafe_arena_set_allocated_client_awake_message(::Valdi::ClientAwakeMessage* value);
  ::Valdi::ClientAwakeMessage* unsafe_arena_release_client_awake_message();

  private:
  const ::Valdi::ClientAwakeMessage& _internal_client_awake_message() const;
  ::Valdi::ClientAwakeMessage* _internal_mutable_client_awake_message();

  public:
  void clear_message();
  MessageCase message_case() const;
  // @@protoc_insertion_point(class_scope:Valdi.DaemonServiceDiscoveryPayload)
 private:
  class _Internal;
  void set_has_daemon_awake_message();
  void set_has_client_awake_message();
  inline bool has_message() const;
  inline void clear_has_message();
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      0, 2, 2,
      0, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_DaemonServiceDiscoveryPayload_default_instance_;

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
                          const DaemonServiceDiscoveryPayload& from_msg);
    union MessageUnion {
      constexpr MessageUnion() : _constinit_{} {}
      ::google::protobuf::internal::ConstantInitialized _constinit_;
      ::Valdi::DaemonAwakeMessage* daemon_awake_message_;
      ::Valdi::ClientAwakeMessage* client_awake_message_;
    } message_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    ::uint32_t _oneof_case_[1];
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_valdi_2eproto;
};
// -------------------------------------------------------------------

class DownloadableModuleManifest final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:Valdi.DownloadableModuleManifest) */ {
 public:
  inline DownloadableModuleManifest() : DownloadableModuleManifest(nullptr) {}
  ~DownloadableModuleManifest() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR DownloadableModuleManifest(
      ::google::protobuf::internal::ConstantInitialized);

  inline DownloadableModuleManifest(const DownloadableModuleManifest& from) : DownloadableModuleManifest(nullptr, from) {}
  inline DownloadableModuleManifest(DownloadableModuleManifest&& from) noexcept
      : DownloadableModuleManifest(nullptr, std::move(from)) {}
  inline DownloadableModuleManifest& operator=(const DownloadableModuleManifest& from) {
    CopyFrom(from);
    return *this;
  }
  inline DownloadableModuleManifest& operator=(DownloadableModuleManifest&& from) noexcept {
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

  static const DownloadableModuleManifest& default_instance() {
    return *internal_default_instance();
  }
  static inline const DownloadableModuleManifest* internal_default_instance() {
    return reinterpret_cast<const DownloadableModuleManifest*>(
        &_DownloadableModuleManifest_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 3;
  friend void swap(DownloadableModuleManifest& a, DownloadableModuleManifest& b) { a.Swap(&b); }
  inline void Swap(DownloadableModuleManifest* other) {
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
  void UnsafeArenaSwap(DownloadableModuleManifest* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  DownloadableModuleManifest* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<DownloadableModuleManifest>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const DownloadableModuleManifest& from);
  void MergeFrom(const DownloadableModuleManifest& from);
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
  void InternalSwap(DownloadableModuleManifest* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "Valdi.DownloadableModuleManifest"; }

 protected:
  explicit DownloadableModuleManifest(::google::protobuf::Arena* arena);
  DownloadableModuleManifest(::google::protobuf::Arena* arena, const DownloadableModuleManifest& from);
  DownloadableModuleManifest(::google::protobuf::Arena* arena, DownloadableModuleManifest&& from) noexcept
      : DownloadableModuleManifest(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kAssetsFieldNumber = 3,
    kDependenciesFieldNumber = 4,
    kNameFieldNumber = 1,
    kArtifactFieldNumber = 2,
    kHasLocalAssetsFieldNumber = 5,
  };
  // repeated .Valdi.DownloadableModuleAssets assets = 3;
  int assets_size() const;
  private:
  int _internal_assets_size() const;

  public:
  void clear_assets() ;
  ::Valdi::DownloadableModuleAssets* mutable_assets(int index);
  ::google::protobuf::RepeatedPtrField<::Valdi::DownloadableModuleAssets>* mutable_assets();

  private:
  const ::google::protobuf::RepeatedPtrField<::Valdi::DownloadableModuleAssets>& _internal_assets() const;
  ::google::protobuf::RepeatedPtrField<::Valdi::DownloadableModuleAssets>* _internal_mutable_assets();
  public:
  const ::Valdi::DownloadableModuleAssets& assets(int index) const;
  ::Valdi::DownloadableModuleAssets* add_assets();
  const ::google::protobuf::RepeatedPtrField<::Valdi::DownloadableModuleAssets>& assets() const;
  // repeated string dependencies = 4;
  int dependencies_size() const;
  private:
  int _internal_dependencies_size() const;

  public:
  void clear_dependencies() ;
  const std::string& dependencies(int index) const;
  std::string* mutable_dependencies(int index);
  void set_dependencies(int index, const std::string& value);
  void set_dependencies(int index, std::string&& value);
  void set_dependencies(int index, const char* value);
  void set_dependencies(int index, const char* value, std::size_t size);
  void set_dependencies(int index, absl::string_view value);
  std::string* add_dependencies();
  void add_dependencies(const std::string& value);
  void add_dependencies(std::string&& value);
  void add_dependencies(const char* value);
  void add_dependencies(const char* value, std::size_t size);
  void add_dependencies(absl::string_view value);
  const ::google::protobuf::RepeatedPtrField<std::string>& dependencies() const;
  ::google::protobuf::RepeatedPtrField<std::string>* mutable_dependencies();

  private:
  const ::google::protobuf::RepeatedPtrField<std::string>& _internal_dependencies() const;
  ::google::protobuf::RepeatedPtrField<std::string>* _internal_mutable_dependencies();

  public:
  // string name = 1;
  void clear_name() ;
  const std::string& name() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_name(Arg_&& arg, Args_... args);
  std::string* mutable_name();
  PROTOBUF_NODISCARD std::string* release_name();
  void set_allocated_name(std::string* value);

  private:
  const std::string& _internal_name() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_name(
      const std::string& value);
  std::string* _internal_mutable_name();

  public:
  // .Valdi.DownloadableModuleArtifact artifact = 2;
  bool has_artifact() const;
  void clear_artifact() ;
  const ::Valdi::DownloadableModuleArtifact& artifact() const;
  PROTOBUF_NODISCARD ::Valdi::DownloadableModuleArtifact* release_artifact();
  ::Valdi::DownloadableModuleArtifact* mutable_artifact();
  void set_allocated_artifact(::Valdi::DownloadableModuleArtifact* value);
  void unsafe_arena_set_allocated_artifact(::Valdi::DownloadableModuleArtifact* value);
  ::Valdi::DownloadableModuleArtifact* unsafe_arena_release_artifact();

  private:
  const ::Valdi::DownloadableModuleArtifact& _internal_artifact() const;
  ::Valdi::DownloadableModuleArtifact* _internal_mutable_artifact();

  public:
  // bool has_local_assets = 5;
  void clear_has_local_assets() ;
  bool has_local_assets() const;
  void set_has_local_assets(bool value);

  private:
  bool _internal_has_local_assets() const;
  void _internal_set_has_local_assets(bool value);

  public:
  // @@protoc_insertion_point(class_scope:Valdi.DownloadableModuleManifest)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      3, 5, 2,
      57, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_DownloadableModuleManifest_default_instance_;

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
                          const DownloadableModuleManifest& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    ::google::protobuf::RepeatedPtrField< ::Valdi::DownloadableModuleAssets > assets_;
    ::google::protobuf::RepeatedPtrField<std::string> dependencies_;
    ::google::protobuf::internal::ArenaStringPtr name_;
    ::Valdi::DownloadableModuleArtifact* artifact_;
    bool has_local_assets_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_valdi_2eproto;
};
// -------------------------------------------------------------------

class CSSRuleIndex final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:Valdi.CSSRuleIndex) */ {
 public:
  inline CSSRuleIndex() : CSSRuleIndex(nullptr) {}
  ~CSSRuleIndex() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CSSRuleIndex(
      ::google::protobuf::internal::ConstantInitialized);

  inline CSSRuleIndex(const CSSRuleIndex& from) : CSSRuleIndex(nullptr, from) {}
  inline CSSRuleIndex(CSSRuleIndex&& from) noexcept
      : CSSRuleIndex(nullptr, std::move(from)) {}
  inline CSSRuleIndex& operator=(const CSSRuleIndex& from) {
    CopyFrom(from);
    return *this;
  }
  inline CSSRuleIndex& operator=(CSSRuleIndex&& from) noexcept {
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

  static const CSSRuleIndex& default_instance() {
    return *internal_default_instance();
  }
  static inline const CSSRuleIndex* internal_default_instance() {
    return reinterpret_cast<const CSSRuleIndex*>(
        &_CSSRuleIndex_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 12;
  friend void swap(CSSRuleIndex& a, CSSRuleIndex& b) { a.Swap(&b); }
  inline void Swap(CSSRuleIndex* other) {
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
  void UnsafeArenaSwap(CSSRuleIndex* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  CSSRuleIndex* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<CSSRuleIndex>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const CSSRuleIndex& from);
  void MergeFrom(const CSSRuleIndex& from);
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
  void InternalSwap(CSSRuleIndex* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "Valdi.CSSRuleIndex"; }

 protected:
  explicit CSSRuleIndex(::google::protobuf::Arena* arena);
  CSSRuleIndex(::google::protobuf::Arena* arena, const CSSRuleIndex& from);
  CSSRuleIndex(::google::protobuf::Arena* arena, CSSRuleIndex&& from) noexcept
      : CSSRuleIndex(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------
  using AttributeRule = CSSRuleIndex_AttributeRule;
  using NthChildRule = CSSRuleIndex_NthChildRule;

  // accessors -------------------------------------------------------
  enum : int {
    kIdRulesFieldNumber = 1,
    kClassRulesFieldNumber = 2,
    kTagRulesFieldNumber = 3,
    kAttributeRulesFieldNumber = 4,
    kNthChildRulesFieldNumber = 7,
    kFirstChildRuleFieldNumber = 5,
    kLastChildRuleFieldNumber = 6,
    kAncestorRulesFieldNumber = 8,
    kDirectParentRulesFieldNumber = 9,
  };
  // repeated .Valdi.NamedStyleNode id_rules = 1;
  int id_rules_size() const;
  private:
  int _internal_id_rules_size() const;

  public:
  void clear_id_rules() ;
  ::Valdi::NamedStyleNode* mutable_id_rules(int index);
  ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>* mutable_id_rules();

  private:
  const ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>& _internal_id_rules() const;
  ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>* _internal_mutable_id_rules();
  public:
  const ::Valdi::NamedStyleNode& id_rules(int index) const;
  ::Valdi::NamedStyleNode* add_id_rules();
  const ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>& id_rules() const;
  // repeated .Valdi.NamedStyleNode class_rules = 2;
  int class_rules_size() const;
  private:
  int _internal_class_rules_size() const;

  public:
  void clear_class_rules() ;
  ::Valdi::NamedStyleNode* mutable_class_rules(int index);
  ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>* mutable_class_rules();

  private:
  const ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>& _internal_class_rules() const;
  ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>* _internal_mutable_class_rules();
  public:
  const ::Valdi::NamedStyleNode& class_rules(int index) const;
  ::Valdi::NamedStyleNode* add_class_rules();
  const ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>& class_rules() const;
  // repeated .Valdi.NamedStyleNode tag_rules = 3;
  int tag_rules_size() const;
  private:
  int _internal_tag_rules_size() const;

  public:
  void clear_tag_rules() ;
  ::Valdi::NamedStyleNode* mutable_tag_rules(int index);
  ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>* mutable_tag_rules();

  private:
  const ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>& _internal_tag_rules() const;
  ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>* _internal_mutable_tag_rules();
  public:
  const ::Valdi::NamedStyleNode& tag_rules(int index) const;
  ::Valdi::NamedStyleNode* add_tag_rules();
  const ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>& tag_rules() const;
  // repeated .Valdi.CSSRuleIndex.AttributeRule attribute_rules = 4;
  int attribute_rules_size() const;
  private:
  int _internal_attribute_rules_size() const;

  public:
  void clear_attribute_rules() ;
  ::Valdi::CSSRuleIndex_AttributeRule* mutable_attribute_rules(int index);
  ::google::protobuf::RepeatedPtrField<::Valdi::CSSRuleIndex_AttributeRule>* mutable_attribute_rules();

  private:
  const ::google::protobuf::RepeatedPtrField<::Valdi::CSSRuleIndex_AttributeRule>& _internal_attribute_rules() const;
  ::google::protobuf::RepeatedPtrField<::Valdi::CSSRuleIndex_AttributeRule>* _internal_mutable_attribute_rules();
  public:
  const ::Valdi::CSSRuleIndex_AttributeRule& attribute_rules(int index) const;
  ::Valdi::CSSRuleIndex_AttributeRule* add_attribute_rules();
  const ::google::protobuf::RepeatedPtrField<::Valdi::CSSRuleIndex_AttributeRule>& attribute_rules() const;
  // repeated .Valdi.CSSRuleIndex.NthChildRule nth_child_rules = 7;
  int nth_child_rules_size() const;
  private:
  int _internal_nth_child_rules_size() const;

  public:
  void clear_nth_child_rules() ;
  ::Valdi::CSSRuleIndex_NthChildRule* mutable_nth_child_rules(int index);
  ::google::protobuf::RepeatedPtrField<::Valdi::CSSRuleIndex_NthChildRule>* mutable_nth_child_rules();

  private:
  const ::google::protobuf::RepeatedPtrField<::Valdi::CSSRuleIndex_NthChildRule>& _internal_nth_child_rules() const;
  ::google::protobuf::RepeatedPtrField<::Valdi::CSSRuleIndex_NthChildRule>* _internal_mutable_nth_child_rules();
  public:
  const ::Valdi::CSSRuleIndex_NthChildRule& nth_child_rules(int index) const;
  ::Valdi::CSSRuleIndex_NthChildRule* add_nth_child_rules();
  const ::google::protobuf::RepeatedPtrField<::Valdi::CSSRuleIndex_NthChildRule>& nth_child_rules() const;
  // .Valdi.StyleNode first_child_rule = 5;
  bool has_first_child_rule() const;
  void clear_first_child_rule() ;
  const ::Valdi::StyleNode& first_child_rule() const;
  PROTOBUF_NODISCARD ::Valdi::StyleNode* release_first_child_rule();
  ::Valdi::StyleNode* mutable_first_child_rule();
  void set_allocated_first_child_rule(::Valdi::StyleNode* value);
  void unsafe_arena_set_allocated_first_child_rule(::Valdi::StyleNode* value);
  ::Valdi::StyleNode* unsafe_arena_release_first_child_rule();

  private:
  const ::Valdi::StyleNode& _internal_first_child_rule() const;
  ::Valdi::StyleNode* _internal_mutable_first_child_rule();

  public:
  // .Valdi.StyleNode last_child_rule = 6;
  bool has_last_child_rule() const;
  void clear_last_child_rule() ;
  const ::Valdi::StyleNode& last_child_rule() const;
  PROTOBUF_NODISCARD ::Valdi::StyleNode* release_last_child_rule();
  ::Valdi::StyleNode* mutable_last_child_rule();
  void set_allocated_last_child_rule(::Valdi::StyleNode* value);
  void unsafe_arena_set_allocated_last_child_rule(::Valdi::StyleNode* value);
  ::Valdi::StyleNode* unsafe_arena_release_last_child_rule();

  private:
  const ::Valdi::StyleNode& _internal_last_child_rule() const;
  ::Valdi::StyleNode* _internal_mutable_last_child_rule();

  public:
  // .Valdi.CSSRuleIndex ancestor_rules = 8;
  bool has_ancestor_rules() const;
  void clear_ancestor_rules() ;
  const ::Valdi::CSSRuleIndex& ancestor_rules() const;
  PROTOBUF_NODISCARD ::Valdi::CSSRuleIndex* release_ancestor_rules();
  ::Valdi::CSSRuleIndex* mutable_ancestor_rules();
  void set_allocated_ancestor_rules(::Valdi::CSSRuleIndex* value);
  void unsafe_arena_set_allocated_ancestor_rules(::Valdi::CSSRuleIndex* value);
  ::Valdi::CSSRuleIndex* unsafe_arena_release_ancestor_rules();

  private:
  const ::Valdi::CSSRuleIndex& _internal_ancestor_rules() const;
  ::Valdi::CSSRuleIndex* _internal_mutable_ancestor_rules();

  public:
  // .Valdi.CSSRuleIndex direct_parent_rules = 9;
  bool has_direct_parent_rules() const;
  void clear_direct_parent_rules() ;
  const ::Valdi::CSSRuleIndex& direct_parent_rules() const;
  PROTOBUF_NODISCARD ::Valdi::CSSRuleIndex* release_direct_parent_rules();
  ::Valdi::CSSRuleIndex* mutable_direct_parent_rules();
  void set_allocated_direct_parent_rules(::Valdi::CSSRuleIndex* value);
  void unsafe_arena_set_allocated_direct_parent_rules(::Valdi::CSSRuleIndex* value);
  ::Valdi::CSSRuleIndex* unsafe_arena_release_direct_parent_rules();

  private:
  const ::Valdi::CSSRuleIndex& _internal_direct_parent_rules() const;
  ::Valdi::CSSRuleIndex* _internal_mutable_direct_parent_rules();

  public:
  // @@protoc_insertion_point(class_scope:Valdi.CSSRuleIndex)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      4, 9, 9,
      0, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_CSSRuleIndex_default_instance_;

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
                          const CSSRuleIndex& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    ::google::protobuf::RepeatedPtrField< ::Valdi::NamedStyleNode > id_rules_;
    ::google::protobuf::RepeatedPtrField< ::Valdi::NamedStyleNode > class_rules_;
    ::google::protobuf::RepeatedPtrField< ::Valdi::NamedStyleNode > tag_rules_;
    ::google::protobuf::RepeatedPtrField< ::Valdi::CSSRuleIndex_AttributeRule > attribute_rules_;
    ::google::protobuf::RepeatedPtrField< ::Valdi::CSSRuleIndex_NthChildRule > nth_child_rules_;
    ::Valdi::StyleNode* first_child_rule_;
    ::Valdi::StyleNode* last_child_rule_;
    ::Valdi::CSSRuleIndex* ancestor_rules_;
    ::Valdi::CSSRuleIndex* direct_parent_rules_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_valdi_2eproto;
};
// -------------------------------------------------------------------

class CSSRuleIndex_AttributeRule final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:Valdi.CSSRuleIndex.AttributeRule) */ {
 public:
  inline CSSRuleIndex_AttributeRule() : CSSRuleIndex_AttributeRule(nullptr) {}
  ~CSSRuleIndex_AttributeRule() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CSSRuleIndex_AttributeRule(
      ::google::protobuf::internal::ConstantInitialized);

  inline CSSRuleIndex_AttributeRule(const CSSRuleIndex_AttributeRule& from) : CSSRuleIndex_AttributeRule(nullptr, from) {}
  inline CSSRuleIndex_AttributeRule(CSSRuleIndex_AttributeRule&& from) noexcept
      : CSSRuleIndex_AttributeRule(nullptr, std::move(from)) {}
  inline CSSRuleIndex_AttributeRule& operator=(const CSSRuleIndex_AttributeRule& from) {
    CopyFrom(from);
    return *this;
  }
  inline CSSRuleIndex_AttributeRule& operator=(CSSRuleIndex_AttributeRule&& from) noexcept {
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

  static const CSSRuleIndex_AttributeRule& default_instance() {
    return *internal_default_instance();
  }
  static inline const CSSRuleIndex_AttributeRule* internal_default_instance() {
    return reinterpret_cast<const CSSRuleIndex_AttributeRule*>(
        &_CSSRuleIndex_AttributeRule_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 10;
  friend void swap(CSSRuleIndex_AttributeRule& a, CSSRuleIndex_AttributeRule& b) { a.Swap(&b); }
  inline void Swap(CSSRuleIndex_AttributeRule* other) {
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
  void UnsafeArenaSwap(CSSRuleIndex_AttributeRule* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  CSSRuleIndex_AttributeRule* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<CSSRuleIndex_AttributeRule>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const CSSRuleIndex_AttributeRule& from);
  void MergeFrom(const CSSRuleIndex_AttributeRule& from);
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
  void InternalSwap(CSSRuleIndex_AttributeRule* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "Valdi.CSSRuleIndex.AttributeRule"; }

 protected:
  explicit CSSRuleIndex_AttributeRule(::google::protobuf::Arena* arena);
  CSSRuleIndex_AttributeRule(::google::protobuf::Arena* arena, const CSSRuleIndex_AttributeRule& from);
  CSSRuleIndex_AttributeRule(::google::protobuf::Arena* arena, CSSRuleIndex_AttributeRule&& from) noexcept
      : CSSRuleIndex_AttributeRule(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------
  using Type = CSSRuleIndex_AttributeRule_Type;
  static constexpr Type EQUALS = CSSRuleIndex_AttributeRule_Type_EQUALS;
  static constexpr Type CONTAINS_WORD = CSSRuleIndex_AttributeRule_Type_CONTAINS_WORD;
  static constexpr Type STARTS_WITH_WORD = CSSRuleIndex_AttributeRule_Type_STARTS_WITH_WORD;
  static constexpr Type STARTS_WITH_SUBSTRING = CSSRuleIndex_AttributeRule_Type_STARTS_WITH_SUBSTRING;
  static constexpr Type ENDS_WITH_SUBSTRING = CSSRuleIndex_AttributeRule_Type_ENDS_WITH_SUBSTRING;
  static constexpr Type CONTAINS_SUBSTRING = CSSRuleIndex_AttributeRule_Type_CONTAINS_SUBSTRING;
  static constexpr Type HAS_ATTRIBUTE = CSSRuleIndex_AttributeRule_Type_HAS_ATTRIBUTE;
  static inline bool Type_IsValid(int value) {
    return CSSRuleIndex_AttributeRule_Type_IsValid(value);
  }
  static constexpr Type Type_MIN = CSSRuleIndex_AttributeRule_Type_Type_MIN;
  static constexpr Type Type_MAX = CSSRuleIndex_AttributeRule_Type_Type_MAX;
  static constexpr int Type_ARRAYSIZE = CSSRuleIndex_AttributeRule_Type_Type_ARRAYSIZE;
  template <typename T>
  static inline const std::string& Type_Name(T value) {
    return CSSRuleIndex_AttributeRule_Type_Name(value);
  }
  static inline bool Type_Parse(absl::string_view name, Type* value) {
    return CSSRuleIndex_AttributeRule_Type_Parse(name, value);
  }

  // accessors -------------------------------------------------------
  enum : int {
    kAttributeFieldNumber = 2,
    kNodeFieldNumber = 3,
    kTypeFieldNumber = 1,
  };
  // .Valdi.NodeAttribute attribute = 2;
  bool has_attribute() const;
  void clear_attribute() ;
  const ::Valdi::NodeAttribute& attribute() const;
  PROTOBUF_NODISCARD ::Valdi::NodeAttribute* release_attribute();
  ::Valdi::NodeAttribute* mutable_attribute();
  void set_allocated_attribute(::Valdi::NodeAttribute* value);
  void unsafe_arena_set_allocated_attribute(::Valdi::NodeAttribute* value);
  ::Valdi::NodeAttribute* unsafe_arena_release_attribute();

  private:
  const ::Valdi::NodeAttribute& _internal_attribute() const;
  ::Valdi::NodeAttribute* _internal_mutable_attribute();

  public:
  // .Valdi.StyleNode node = 3;
  bool has_node() const;
  void clear_node() ;
  const ::Valdi::StyleNode& node() const;
  PROTOBUF_NODISCARD ::Valdi::StyleNode* release_node();
  ::Valdi::StyleNode* mutable_node();
  void set_allocated_node(::Valdi::StyleNode* value);
  void unsafe_arena_set_allocated_node(::Valdi::StyleNode* value);
  ::Valdi::StyleNode* unsafe_arena_release_node();

  private:
  const ::Valdi::StyleNode& _internal_node() const;
  ::Valdi::StyleNode* _internal_mutable_node();

  public:
  // .Valdi.CSSRuleIndex.AttributeRule.Type type = 1;
  void clear_type() ;
  ::Valdi::CSSRuleIndex_AttributeRule_Type type() const;
  void set_type(::Valdi::CSSRuleIndex_AttributeRule_Type value);

  private:
  ::Valdi::CSSRuleIndex_AttributeRule_Type _internal_type() const;
  void _internal_set_type(::Valdi::CSSRuleIndex_AttributeRule_Type value);

  public:
  // @@protoc_insertion_point(class_scope:Valdi.CSSRuleIndex.AttributeRule)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      2, 3, 2,
      0, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_CSSRuleIndex_AttributeRule_default_instance_;

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
                          const CSSRuleIndex_AttributeRule& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    ::Valdi::NodeAttribute* attribute_;
    ::Valdi::StyleNode* node_;
    int type_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_valdi_2eproto;
};
// -------------------------------------------------------------------

class CSSRuleIndex_NthChildRule final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:Valdi.CSSRuleIndex.NthChildRule) */ {
 public:
  inline CSSRuleIndex_NthChildRule() : CSSRuleIndex_NthChildRule(nullptr) {}
  ~CSSRuleIndex_NthChildRule() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR CSSRuleIndex_NthChildRule(
      ::google::protobuf::internal::ConstantInitialized);

  inline CSSRuleIndex_NthChildRule(const CSSRuleIndex_NthChildRule& from) : CSSRuleIndex_NthChildRule(nullptr, from) {}
  inline CSSRuleIndex_NthChildRule(CSSRuleIndex_NthChildRule&& from) noexcept
      : CSSRuleIndex_NthChildRule(nullptr, std::move(from)) {}
  inline CSSRuleIndex_NthChildRule& operator=(const CSSRuleIndex_NthChildRule& from) {
    CopyFrom(from);
    return *this;
  }
  inline CSSRuleIndex_NthChildRule& operator=(CSSRuleIndex_NthChildRule&& from) noexcept {
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

  static const CSSRuleIndex_NthChildRule& default_instance() {
    return *internal_default_instance();
  }
  static inline const CSSRuleIndex_NthChildRule* internal_default_instance() {
    return reinterpret_cast<const CSSRuleIndex_NthChildRule*>(
        &_CSSRuleIndex_NthChildRule_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 11;
  friend void swap(CSSRuleIndex_NthChildRule& a, CSSRuleIndex_NthChildRule& b) { a.Swap(&b); }
  inline void Swap(CSSRuleIndex_NthChildRule* other) {
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
  void UnsafeArenaSwap(CSSRuleIndex_NthChildRule* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  CSSRuleIndex_NthChildRule* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<CSSRuleIndex_NthChildRule>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const CSSRuleIndex_NthChildRule& from);
  void MergeFrom(const CSSRuleIndex_NthChildRule& from);
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
  void InternalSwap(CSSRuleIndex_NthChildRule* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "Valdi.CSSRuleIndex.NthChildRule"; }

 protected:
  explicit CSSRuleIndex_NthChildRule(::google::protobuf::Arena* arena);
  CSSRuleIndex_NthChildRule(::google::protobuf::Arena* arena, const CSSRuleIndex_NthChildRule& from);
  CSSRuleIndex_NthChildRule(::google::protobuf::Arena* arena, CSSRuleIndex_NthChildRule&& from) noexcept
      : CSSRuleIndex_NthChildRule(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kNodeFieldNumber = 3,
    kNFieldNumber = 1,
    kOffsetFieldNumber = 2,
  };
  // .Valdi.StyleNode node = 3;
  bool has_node() const;
  void clear_node() ;
  const ::Valdi::StyleNode& node() const;
  PROTOBUF_NODISCARD ::Valdi::StyleNode* release_node();
  ::Valdi::StyleNode* mutable_node();
  void set_allocated_node(::Valdi::StyleNode* value);
  void unsafe_arena_set_allocated_node(::Valdi::StyleNode* value);
  ::Valdi::StyleNode* unsafe_arena_release_node();

  private:
  const ::Valdi::StyleNode& _internal_node() const;
  ::Valdi::StyleNode* _internal_mutable_node();

  public:
  // int32 n = 1;
  void clear_n() ;
  ::int32_t n() const;
  void set_n(::int32_t value);

  private:
  ::int32_t _internal_n() const;
  void _internal_set_n(::int32_t value);

  public:
  // int32 offset = 2;
  void clear_offset() ;
  ::int32_t offset() const;
  void set_offset(::int32_t value);

  private:
  ::int32_t _internal_offset() const;
  void _internal_set_offset(::int32_t value);

  public:
  // @@protoc_insertion_point(class_scope:Valdi.CSSRuleIndex.NthChildRule)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      2, 3, 1,
      0, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_CSSRuleIndex_NthChildRule_default_instance_;

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
                          const CSSRuleIndex_NthChildRule& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    ::Valdi::StyleNode* node_;
    ::int32_t n_;
    ::int32_t offset_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_valdi_2eproto;
};
// -------------------------------------------------------------------

class NamedStyleNode final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:Valdi.NamedStyleNode) */ {
 public:
  inline NamedStyleNode() : NamedStyleNode(nullptr) {}
  ~NamedStyleNode() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR NamedStyleNode(
      ::google::protobuf::internal::ConstantInitialized);

  inline NamedStyleNode(const NamedStyleNode& from) : NamedStyleNode(nullptr, from) {}
  inline NamedStyleNode(NamedStyleNode&& from) noexcept
      : NamedStyleNode(nullptr, std::move(from)) {}
  inline NamedStyleNode& operator=(const NamedStyleNode& from) {
    CopyFrom(from);
    return *this;
  }
  inline NamedStyleNode& operator=(NamedStyleNode&& from) noexcept {
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

  static const NamedStyleNode& default_instance() {
    return *internal_default_instance();
  }
  static inline const NamedStyleNode* internal_default_instance() {
    return reinterpret_cast<const NamedStyleNode*>(
        &_NamedStyleNode_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 9;
  friend void swap(NamedStyleNode& a, NamedStyleNode& b) { a.Swap(&b); }
  inline void Swap(NamedStyleNode* other) {
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
  void UnsafeArenaSwap(NamedStyleNode* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  NamedStyleNode* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<NamedStyleNode>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const NamedStyleNode& from);
  void MergeFrom(const NamedStyleNode& from);
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
  void InternalSwap(NamedStyleNode* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "Valdi.NamedStyleNode"; }

 protected:
  explicit NamedStyleNode(::google::protobuf::Arena* arena);
  NamedStyleNode(::google::protobuf::Arena* arena, const NamedStyleNode& from);
  NamedStyleNode(::google::protobuf::Arena* arena, NamedStyleNode&& from) noexcept
      : NamedStyleNode(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kNameFieldNumber = 1,
    kNodeFieldNumber = 2,
  };
  // string name = 1;
  void clear_name() ;
  const std::string& name() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_name(Arg_&& arg, Args_... args);
  std::string* mutable_name();
  PROTOBUF_NODISCARD std::string* release_name();
  void set_allocated_name(std::string* value);

  private:
  const std::string& _internal_name() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_name(
      const std::string& value);
  std::string* _internal_mutable_name();

  public:
  // .Valdi.StyleNode node = 2;
  bool has_node() const;
  void clear_node() ;
  const ::Valdi::StyleNode& node() const;
  PROTOBUF_NODISCARD ::Valdi::StyleNode* release_node();
  ::Valdi::StyleNode* mutable_node();
  void set_allocated_node(::Valdi::StyleNode* value);
  void unsafe_arena_set_allocated_node(::Valdi::StyleNode* value);
  ::Valdi::StyleNode* unsafe_arena_release_node();

  private:
  const ::Valdi::StyleNode& _internal_node() const;
  ::Valdi::StyleNode* _internal_mutable_node();

  public:
  // @@protoc_insertion_point(class_scope:Valdi.NamedStyleNode)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      1, 2, 1,
      33, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_NamedStyleNode_default_instance_;

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
                          const NamedStyleNode& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    ::google::protobuf::internal::ArenaStringPtr name_;
    ::Valdi::StyleNode* node_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_valdi_2eproto;
};
// -------------------------------------------------------------------

class StyleNode final : public ::google::protobuf::MessageLite
/* @@protoc_insertion_point(class_definition:Valdi.StyleNode) */ {
 public:
  inline StyleNode() : StyleNode(nullptr) {}
  ~StyleNode() override;
  template <typename = void>
  explicit PROTOBUF_CONSTEXPR StyleNode(
      ::google::protobuf::internal::ConstantInitialized);

  inline StyleNode(const StyleNode& from) : StyleNode(nullptr, from) {}
  inline StyleNode(StyleNode&& from) noexcept
      : StyleNode(nullptr, std::move(from)) {}
  inline StyleNode& operator=(const StyleNode& from) {
    CopyFrom(from);
    return *this;
  }
  inline StyleNode& operator=(StyleNode&& from) noexcept {
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

  static const StyleNode& default_instance() {
    return *internal_default_instance();
  }
  static inline const StyleNode* internal_default_instance() {
    return reinterpret_cast<const StyleNode*>(
        &_StyleNode_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 7;
  friend void swap(StyleNode& a, StyleNode& b) { a.Swap(&b); }
  inline void Swap(StyleNode* other) {
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
  void UnsafeArenaSwap(StyleNode* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  StyleNode* New(::google::protobuf::Arena* arena = nullptr) const final {
    return ::google::protobuf::MessageLite::DefaultConstruct<StyleNode>(arena);
  }
  void CheckTypeAndMergeFrom(
      const ::google::protobuf::MessageLite& from) final;
  void CopyFrom(const StyleNode& from);
  void MergeFrom(const StyleNode& from);
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
  void InternalSwap(StyleNode* other);
 private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() { return "Valdi.StyleNode"; }

 protected:
  explicit StyleNode(::google::protobuf::Arena* arena);
  StyleNode(::google::protobuf::Arena* arena, const StyleNode& from);
  StyleNode(::google::protobuf::Arena* arena, StyleNode&& from) noexcept
      : StyleNode(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::MessageLite::ClassData* GetClassData() const final;

 public:
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kStylesFieldNumber = 1,
    kRuleIndexFieldNumber = 2,
  };
  // repeated .Valdi.StyleDeclaration styles = 1;
  int styles_size() const;
  private:
  int _internal_styles_size() const;

  public:
  void clear_styles() ;
  ::Valdi::StyleDeclaration* mutable_styles(int index);
  ::google::protobuf::RepeatedPtrField<::Valdi::StyleDeclaration>* mutable_styles();

  private:
  const ::google::protobuf::RepeatedPtrField<::Valdi::StyleDeclaration>& _internal_styles() const;
  ::google::protobuf::RepeatedPtrField<::Valdi::StyleDeclaration>* _internal_mutable_styles();
  public:
  const ::Valdi::StyleDeclaration& styles(int index) const;
  ::Valdi::StyleDeclaration* add_styles();
  const ::google::protobuf::RepeatedPtrField<::Valdi::StyleDeclaration>& styles() const;
  // .Valdi.CSSRuleIndex ruleIndex = 2;
  bool has_ruleindex() const;
  void clear_ruleindex() ;
  const ::Valdi::CSSRuleIndex& ruleindex() const;
  PROTOBUF_NODISCARD ::Valdi::CSSRuleIndex* release_ruleindex();
  ::Valdi::CSSRuleIndex* mutable_ruleindex();
  void set_allocated_ruleindex(::Valdi::CSSRuleIndex* value);
  void unsafe_arena_set_allocated_ruleindex(::Valdi::CSSRuleIndex* value);
  ::Valdi::CSSRuleIndex* unsafe_arena_release_ruleindex();

  private:
  const ::Valdi::CSSRuleIndex& _internal_ruleindex() const;
  ::Valdi::CSSRuleIndex* _internal_mutable_ruleindex();

  public:
  // @@protoc_insertion_point(class_scope:Valdi.StyleNode)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      1, 2, 2,
      0, 2>
      _table_;

  static constexpr const void* _raw_default_instance_ =
      &_StyleNode_default_instance_;

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
                          const StyleNode& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    ::google::protobuf::RepeatedPtrField< ::Valdi::StyleDeclaration > styles_;
    ::Valdi::CSSRuleIndex* ruleindex_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_valdi_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// NodeAttribute

// .Valdi.NodeAttribute.Type type = 1;
inline void NodeAttribute::clear_type() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.type_ = 0;
}
inline ::Valdi::NodeAttribute_Type NodeAttribute::type() const {
  // @@protoc_insertion_point(field_get:Valdi.NodeAttribute.type)
  return _internal_type();
}
inline void NodeAttribute::set_type(::Valdi::NodeAttribute_Type value) {
  _internal_set_type(value);
  // @@protoc_insertion_point(field_set:Valdi.NodeAttribute.type)
}
inline ::Valdi::NodeAttribute_Type NodeAttribute::_internal_type() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return static_cast<::Valdi::NodeAttribute_Type>(_impl_.type_);
}
inline void NodeAttribute::_internal_set_type(::Valdi::NodeAttribute_Type value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.type_ = value;
}

// string name = 2;
inline void NodeAttribute::clear_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.name_.ClearToEmpty();
}
inline const std::string& NodeAttribute::name() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.NodeAttribute.name)
  return _internal_name();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void NodeAttribute::set_name(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.name_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:Valdi.NodeAttribute.name)
}
inline std::string* NodeAttribute::mutable_name() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_name();
  // @@protoc_insertion_point(field_mutable:Valdi.NodeAttribute.name)
  return _s;
}
inline const std::string& NodeAttribute::_internal_name() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.name_.Get();
}
inline void NodeAttribute::_internal_set_name(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.name_.Set(value, GetArena());
}
inline std::string* NodeAttribute::_internal_mutable_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.name_.Mutable( GetArena());
}
inline std::string* NodeAttribute::release_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.NodeAttribute.name)
  return _impl_.name_.Release();
}
inline void NodeAttribute::set_allocated_name(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.name_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.name_.IsDefault()) {
          _impl_.name_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:Valdi.NodeAttribute.name)
}

// string str_value = 3;
inline void NodeAttribute::clear_str_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.str_value_.ClearToEmpty();
}
inline const std::string& NodeAttribute::str_value() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.NodeAttribute.str_value)
  return _internal_str_value();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void NodeAttribute::set_str_value(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.str_value_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:Valdi.NodeAttribute.str_value)
}
inline std::string* NodeAttribute::mutable_str_value() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_str_value();
  // @@protoc_insertion_point(field_mutable:Valdi.NodeAttribute.str_value)
  return _s;
}
inline const std::string& NodeAttribute::_internal_str_value() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.str_value_.Get();
}
inline void NodeAttribute::_internal_set_str_value(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.str_value_.Set(value, GetArena());
}
inline std::string* NodeAttribute::_internal_mutable_str_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.str_value_.Mutable( GetArena());
}
inline std::string* NodeAttribute::release_str_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.NodeAttribute.str_value)
  return _impl_.str_value_.Release();
}
inline void NodeAttribute::set_allocated_str_value(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.str_value_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.str_value_.IsDefault()) {
          _impl_.str_value_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:Valdi.NodeAttribute.str_value)
}

// int64 int_value = 4;
inline void NodeAttribute::clear_int_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.int_value_ = ::int64_t{0};
}
inline ::int64_t NodeAttribute::int_value() const {
  // @@protoc_insertion_point(field_get:Valdi.NodeAttribute.int_value)
  return _internal_int_value();
}
inline void NodeAttribute::set_int_value(::int64_t value) {
  _internal_set_int_value(value);
  // @@protoc_insertion_point(field_set:Valdi.NodeAttribute.int_value)
}
inline ::int64_t NodeAttribute::_internal_int_value() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.int_value_;
}
inline void NodeAttribute::_internal_set_int_value(::int64_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.int_value_ = value;
}

// double double_value = 5;
inline void NodeAttribute::clear_double_value() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.double_value_ = 0;
}
inline double NodeAttribute::double_value() const {
  // @@protoc_insertion_point(field_get:Valdi.NodeAttribute.double_value)
  return _internal_double_value();
}
inline void NodeAttribute::set_double_value(double value) {
  _internal_set_double_value(value);
  // @@protoc_insertion_point(field_set:Valdi.NodeAttribute.double_value)
}
inline double NodeAttribute::_internal_double_value() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.double_value_;
}
inline void NodeAttribute::_internal_set_double_value(double value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.double_value_ = value;
}

// -------------------------------------------------------------------

// DownloadableModuleArtifact

// string url = 1;
inline void DownloadableModuleArtifact::clear_url() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.url_.ClearToEmpty();
}
inline const std::string& DownloadableModuleArtifact::url() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.DownloadableModuleArtifact.url)
  return _internal_url();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void DownloadableModuleArtifact::set_url(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.url_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:Valdi.DownloadableModuleArtifact.url)
}
inline std::string* DownloadableModuleArtifact::mutable_url() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_url();
  // @@protoc_insertion_point(field_mutable:Valdi.DownloadableModuleArtifact.url)
  return _s;
}
inline const std::string& DownloadableModuleArtifact::_internal_url() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.url_.Get();
}
inline void DownloadableModuleArtifact::_internal_set_url(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.url_.Set(value, GetArena());
}
inline std::string* DownloadableModuleArtifact::_internal_mutable_url() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.url_.Mutable( GetArena());
}
inline std::string* DownloadableModuleArtifact::release_url() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.DownloadableModuleArtifact.url)
  return _impl_.url_.Release();
}
inline void DownloadableModuleArtifact::set_allocated_url(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.url_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.url_.IsDefault()) {
          _impl_.url_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:Valdi.DownloadableModuleArtifact.url)
}

// bytes sha256Digest = 2;
inline void DownloadableModuleArtifact::clear_sha256digest() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.sha256digest_.ClearToEmpty();
}
inline const std::string& DownloadableModuleArtifact::sha256digest() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.DownloadableModuleArtifact.sha256Digest)
  return _internal_sha256digest();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void DownloadableModuleArtifact::set_sha256digest(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.sha256digest_.SetBytes(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:Valdi.DownloadableModuleArtifact.sha256Digest)
}
inline std::string* DownloadableModuleArtifact::mutable_sha256digest() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_sha256digest();
  // @@protoc_insertion_point(field_mutable:Valdi.DownloadableModuleArtifact.sha256Digest)
  return _s;
}
inline const std::string& DownloadableModuleArtifact::_internal_sha256digest() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.sha256digest_.Get();
}
inline void DownloadableModuleArtifact::_internal_set_sha256digest(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.sha256digest_.Set(value, GetArena());
}
inline std::string* DownloadableModuleArtifact::_internal_mutable_sha256digest() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.sha256digest_.Mutable( GetArena());
}
inline std::string* DownloadableModuleArtifact::release_sha256digest() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.DownloadableModuleArtifact.sha256Digest)
  return _impl_.sha256digest_.Release();
}
inline void DownloadableModuleArtifact::set_allocated_sha256digest(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.sha256digest_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.sha256digest_.IsDefault()) {
          _impl_.sha256digest_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:Valdi.DownloadableModuleArtifact.sha256Digest)
}

// -------------------------------------------------------------------

// DownloadableModuleAssets

// .Valdi.DownloadableModuleArtifact artifact = 1;
inline bool DownloadableModuleAssets::has_artifact() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.artifact_ != nullptr);
  return value;
}
inline void DownloadableModuleAssets::clear_artifact() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.artifact_ != nullptr) _impl_.artifact_->Clear();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline const ::Valdi::DownloadableModuleArtifact& DownloadableModuleAssets::_internal_artifact() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::Valdi::DownloadableModuleArtifact* p = _impl_.artifact_;
  return p != nullptr ? *p : reinterpret_cast<const ::Valdi::DownloadableModuleArtifact&>(::Valdi::_DownloadableModuleArtifact_default_instance_);
}
inline const ::Valdi::DownloadableModuleArtifact& DownloadableModuleAssets::artifact() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.DownloadableModuleAssets.artifact)
  return _internal_artifact();
}
inline void DownloadableModuleAssets::unsafe_arena_set_allocated_artifact(::Valdi::DownloadableModuleArtifact* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.artifact_);
  }
  _impl_.artifact_ = reinterpret_cast<::Valdi::DownloadableModuleArtifact*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:Valdi.DownloadableModuleAssets.artifact)
}
inline ::Valdi::DownloadableModuleArtifact* DownloadableModuleAssets::release_artifact() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Valdi::DownloadableModuleArtifact* released = _impl_.artifact_;
  _impl_.artifact_ = nullptr;
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
inline ::Valdi::DownloadableModuleArtifact* DownloadableModuleAssets::unsafe_arena_release_artifact() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.DownloadableModuleAssets.artifact)

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Valdi::DownloadableModuleArtifact* temp = _impl_.artifact_;
  _impl_.artifact_ = nullptr;
  return temp;
}
inline ::Valdi::DownloadableModuleArtifact* DownloadableModuleAssets::_internal_mutable_artifact() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.artifact_ == nullptr) {
    auto* p = ::google::protobuf::MessageLite::DefaultConstruct<::Valdi::DownloadableModuleArtifact>(GetArena());
    _impl_.artifact_ = reinterpret_cast<::Valdi::DownloadableModuleArtifact*>(p);
  }
  return _impl_.artifact_;
}
inline ::Valdi::DownloadableModuleArtifact* DownloadableModuleAssets::mutable_artifact() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000001u;
  ::Valdi::DownloadableModuleArtifact* _msg = _internal_mutable_artifact();
  // @@protoc_insertion_point(field_mutable:Valdi.DownloadableModuleAssets.artifact)
  return _msg;
}
inline void DownloadableModuleAssets::set_allocated_artifact(::Valdi::DownloadableModuleArtifact* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete (_impl_.artifact_);
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

  _impl_.artifact_ = reinterpret_cast<::Valdi::DownloadableModuleArtifact*>(value);
  // @@protoc_insertion_point(field_set_allocated:Valdi.DownloadableModuleAssets.artifact)
}

// double device_density = 3;
inline void DownloadableModuleAssets::clear_device_density() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.device_density_ = 0;
}
inline double DownloadableModuleAssets::device_density() const {
  // @@protoc_insertion_point(field_get:Valdi.DownloadableModuleAssets.device_density)
  return _internal_device_density();
}
inline void DownloadableModuleAssets::set_device_density(double value) {
  _internal_set_device_density(value);
  // @@protoc_insertion_point(field_set:Valdi.DownloadableModuleAssets.device_density)
}
inline double DownloadableModuleAssets::_internal_device_density() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.device_density_;
}
inline void DownloadableModuleAssets::_internal_set_device_density(double value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.device_density_ = value;
}

// -------------------------------------------------------------------

// DownloadableModuleManifest

// string name = 1;
inline void DownloadableModuleManifest::clear_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.name_.ClearToEmpty();
}
inline const std::string& DownloadableModuleManifest::name() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.DownloadableModuleManifest.name)
  return _internal_name();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void DownloadableModuleManifest::set_name(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.name_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:Valdi.DownloadableModuleManifest.name)
}
inline std::string* DownloadableModuleManifest::mutable_name() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_name();
  // @@protoc_insertion_point(field_mutable:Valdi.DownloadableModuleManifest.name)
  return _s;
}
inline const std::string& DownloadableModuleManifest::_internal_name() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.name_.Get();
}
inline void DownloadableModuleManifest::_internal_set_name(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.name_.Set(value, GetArena());
}
inline std::string* DownloadableModuleManifest::_internal_mutable_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.name_.Mutable( GetArena());
}
inline std::string* DownloadableModuleManifest::release_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.DownloadableModuleManifest.name)
  return _impl_.name_.Release();
}
inline void DownloadableModuleManifest::set_allocated_name(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.name_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.name_.IsDefault()) {
          _impl_.name_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:Valdi.DownloadableModuleManifest.name)
}

// .Valdi.DownloadableModuleArtifact artifact = 2;
inline bool DownloadableModuleManifest::has_artifact() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.artifact_ != nullptr);
  return value;
}
inline void DownloadableModuleManifest::clear_artifact() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.artifact_ != nullptr) _impl_.artifact_->Clear();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline const ::Valdi::DownloadableModuleArtifact& DownloadableModuleManifest::_internal_artifact() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::Valdi::DownloadableModuleArtifact* p = _impl_.artifact_;
  return p != nullptr ? *p : reinterpret_cast<const ::Valdi::DownloadableModuleArtifact&>(::Valdi::_DownloadableModuleArtifact_default_instance_);
}
inline const ::Valdi::DownloadableModuleArtifact& DownloadableModuleManifest::artifact() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.DownloadableModuleManifest.artifact)
  return _internal_artifact();
}
inline void DownloadableModuleManifest::unsafe_arena_set_allocated_artifact(::Valdi::DownloadableModuleArtifact* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.artifact_);
  }
  _impl_.artifact_ = reinterpret_cast<::Valdi::DownloadableModuleArtifact*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:Valdi.DownloadableModuleManifest.artifact)
}
inline ::Valdi::DownloadableModuleArtifact* DownloadableModuleManifest::release_artifact() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Valdi::DownloadableModuleArtifact* released = _impl_.artifact_;
  _impl_.artifact_ = nullptr;
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
inline ::Valdi::DownloadableModuleArtifact* DownloadableModuleManifest::unsafe_arena_release_artifact() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.DownloadableModuleManifest.artifact)

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Valdi::DownloadableModuleArtifact* temp = _impl_.artifact_;
  _impl_.artifact_ = nullptr;
  return temp;
}
inline ::Valdi::DownloadableModuleArtifact* DownloadableModuleManifest::_internal_mutable_artifact() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.artifact_ == nullptr) {
    auto* p = ::google::protobuf::MessageLite::DefaultConstruct<::Valdi::DownloadableModuleArtifact>(GetArena());
    _impl_.artifact_ = reinterpret_cast<::Valdi::DownloadableModuleArtifact*>(p);
  }
  return _impl_.artifact_;
}
inline ::Valdi::DownloadableModuleArtifact* DownloadableModuleManifest::mutable_artifact() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000001u;
  ::Valdi::DownloadableModuleArtifact* _msg = _internal_mutable_artifact();
  // @@protoc_insertion_point(field_mutable:Valdi.DownloadableModuleManifest.artifact)
  return _msg;
}
inline void DownloadableModuleManifest::set_allocated_artifact(::Valdi::DownloadableModuleArtifact* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete (_impl_.artifact_);
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

  _impl_.artifact_ = reinterpret_cast<::Valdi::DownloadableModuleArtifact*>(value);
  // @@protoc_insertion_point(field_set_allocated:Valdi.DownloadableModuleManifest.artifact)
}

// repeated .Valdi.DownloadableModuleAssets assets = 3;
inline int DownloadableModuleManifest::_internal_assets_size() const {
  return _internal_assets().size();
}
inline int DownloadableModuleManifest::assets_size() const {
  return _internal_assets_size();
}
inline void DownloadableModuleManifest::clear_assets() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.assets_.Clear();
}
inline ::Valdi::DownloadableModuleAssets* DownloadableModuleManifest::mutable_assets(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:Valdi.DownloadableModuleManifest.assets)
  return _internal_mutable_assets()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::Valdi::DownloadableModuleAssets>* DownloadableModuleManifest::mutable_assets()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:Valdi.DownloadableModuleManifest.assets)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_assets();
}
inline const ::Valdi::DownloadableModuleAssets& DownloadableModuleManifest::assets(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.DownloadableModuleManifest.assets)
  return _internal_assets().Get(index);
}
inline ::Valdi::DownloadableModuleAssets* DownloadableModuleManifest::add_assets() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::Valdi::DownloadableModuleAssets* _add = _internal_mutable_assets()->Add();
  // @@protoc_insertion_point(field_add:Valdi.DownloadableModuleManifest.assets)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::Valdi::DownloadableModuleAssets>& DownloadableModuleManifest::assets() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:Valdi.DownloadableModuleManifest.assets)
  return _internal_assets();
}
inline const ::google::protobuf::RepeatedPtrField<::Valdi::DownloadableModuleAssets>&
DownloadableModuleManifest::_internal_assets() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.assets_;
}
inline ::google::protobuf::RepeatedPtrField<::Valdi::DownloadableModuleAssets>*
DownloadableModuleManifest::_internal_mutable_assets() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.assets_;
}

// repeated string dependencies = 4;
inline int DownloadableModuleManifest::_internal_dependencies_size() const {
  return _internal_dependencies().size();
}
inline int DownloadableModuleManifest::dependencies_size() const {
  return _internal_dependencies_size();
}
inline void DownloadableModuleManifest::clear_dependencies() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.dependencies_.Clear();
}
inline std::string* DownloadableModuleManifest::add_dependencies()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  std::string* _s = _internal_mutable_dependencies()->Add();
  // @@protoc_insertion_point(field_add_mutable:Valdi.DownloadableModuleManifest.dependencies)
  return _s;
}
inline const std::string& DownloadableModuleManifest::dependencies(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.DownloadableModuleManifest.dependencies)
  return _internal_dependencies().Get(index);
}
inline std::string* DownloadableModuleManifest::mutable_dependencies(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:Valdi.DownloadableModuleManifest.dependencies)
  return _internal_mutable_dependencies()->Mutable(index);
}
inline void DownloadableModuleManifest::set_dependencies(int index, const std::string& value) {
  _internal_mutable_dependencies()->Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set:Valdi.DownloadableModuleManifest.dependencies)
}
inline void DownloadableModuleManifest::set_dependencies(int index, std::string&& value) {
  _internal_mutable_dependencies()->Mutable(index)->assign(std::move(value));
  // @@protoc_insertion_point(field_set:Valdi.DownloadableModuleManifest.dependencies)
}
inline void DownloadableModuleManifest::set_dependencies(int index, const char* value) {
  ABSL_DCHECK(value != nullptr);
  _internal_mutable_dependencies()->Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set_char:Valdi.DownloadableModuleManifest.dependencies)
}
inline void DownloadableModuleManifest::set_dependencies(int index, const char* value,
                              std::size_t size) {
  _internal_mutable_dependencies()->Mutable(index)->assign(
      reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_set_pointer:Valdi.DownloadableModuleManifest.dependencies)
}
inline void DownloadableModuleManifest::set_dependencies(int index, absl::string_view value) {
  _internal_mutable_dependencies()->Mutable(index)->assign(
      value.data(), value.size());
  // @@protoc_insertion_point(field_set_string_piece:Valdi.DownloadableModuleManifest.dependencies)
}
inline void DownloadableModuleManifest::add_dependencies(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_dependencies()->Add()->assign(value);
  // @@protoc_insertion_point(field_add:Valdi.DownloadableModuleManifest.dependencies)
}
inline void DownloadableModuleManifest::add_dependencies(std::string&& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_dependencies()->Add(std::move(value));
  // @@protoc_insertion_point(field_add:Valdi.DownloadableModuleManifest.dependencies)
}
inline void DownloadableModuleManifest::add_dependencies(const char* value) {
  ABSL_DCHECK(value != nullptr);
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_dependencies()->Add()->assign(value);
  // @@protoc_insertion_point(field_add_char:Valdi.DownloadableModuleManifest.dependencies)
}
inline void DownloadableModuleManifest::add_dependencies(const char* value, std::size_t size) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_dependencies()->Add()->assign(
      reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_add_pointer:Valdi.DownloadableModuleManifest.dependencies)
}
inline void DownloadableModuleManifest::add_dependencies(absl::string_view value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_dependencies()->Add()->assign(value.data(),
                                                     value.size());
  // @@protoc_insertion_point(field_add_string_piece:Valdi.DownloadableModuleManifest.dependencies)
}
inline const ::google::protobuf::RepeatedPtrField<std::string>&
DownloadableModuleManifest::dependencies() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:Valdi.DownloadableModuleManifest.dependencies)
  return _internal_dependencies();
}
inline ::google::protobuf::RepeatedPtrField<std::string>*
DownloadableModuleManifest::mutable_dependencies() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:Valdi.DownloadableModuleManifest.dependencies)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_dependencies();
}
inline const ::google::protobuf::RepeatedPtrField<std::string>&
DownloadableModuleManifest::_internal_dependencies() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.dependencies_;
}
inline ::google::protobuf::RepeatedPtrField<std::string>*
DownloadableModuleManifest::_internal_mutable_dependencies() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.dependencies_;
}

// bool has_local_assets = 5;
inline void DownloadableModuleManifest::clear_has_local_assets() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.has_local_assets_ = false;
}
inline bool DownloadableModuleManifest::has_local_assets() const {
  // @@protoc_insertion_point(field_get:Valdi.DownloadableModuleManifest.has_local_assets)
  return _internal_has_local_assets();
}
inline void DownloadableModuleManifest::set_has_local_assets(bool value) {
  _internal_set_has_local_assets(value);
  // @@protoc_insertion_point(field_set:Valdi.DownloadableModuleManifest.has_local_assets)
}
inline bool DownloadableModuleManifest::_internal_has_local_assets() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.has_local_assets_;
}
inline void DownloadableModuleManifest::_internal_set_has_local_assets(bool value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.has_local_assets_ = value;
}

// -------------------------------------------------------------------

// DaemonAwakeMessage

// repeated string device_ids = 16;
inline int DaemonAwakeMessage::_internal_device_ids_size() const {
  return _internal_device_ids().size();
}
inline int DaemonAwakeMessage::device_ids_size() const {
  return _internal_device_ids_size();
}
inline void DaemonAwakeMessage::clear_device_ids() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.device_ids_.Clear();
}
inline std::string* DaemonAwakeMessage::add_device_ids()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  std::string* _s = _internal_mutable_device_ids()->Add();
  // @@protoc_insertion_point(field_add_mutable:Valdi.DaemonAwakeMessage.device_ids)
  return _s;
}
inline const std::string& DaemonAwakeMessage::device_ids(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.DaemonAwakeMessage.device_ids)
  return _internal_device_ids().Get(index);
}
inline std::string* DaemonAwakeMessage::mutable_device_ids(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:Valdi.DaemonAwakeMessage.device_ids)
  return _internal_mutable_device_ids()->Mutable(index);
}
inline void DaemonAwakeMessage::set_device_ids(int index, const std::string& value) {
  _internal_mutable_device_ids()->Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set:Valdi.DaemonAwakeMessage.device_ids)
}
inline void DaemonAwakeMessage::set_device_ids(int index, std::string&& value) {
  _internal_mutable_device_ids()->Mutable(index)->assign(std::move(value));
  // @@protoc_insertion_point(field_set:Valdi.DaemonAwakeMessage.device_ids)
}
inline void DaemonAwakeMessage::set_device_ids(int index, const char* value) {
  ABSL_DCHECK(value != nullptr);
  _internal_mutable_device_ids()->Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set_char:Valdi.DaemonAwakeMessage.device_ids)
}
inline void DaemonAwakeMessage::set_device_ids(int index, const char* value,
                              std::size_t size) {
  _internal_mutable_device_ids()->Mutable(index)->assign(
      reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_set_pointer:Valdi.DaemonAwakeMessage.device_ids)
}
inline void DaemonAwakeMessage::set_device_ids(int index, absl::string_view value) {
  _internal_mutable_device_ids()->Mutable(index)->assign(
      value.data(), value.size());
  // @@protoc_insertion_point(field_set_string_piece:Valdi.DaemonAwakeMessage.device_ids)
}
inline void DaemonAwakeMessage::add_device_ids(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_device_ids()->Add()->assign(value);
  // @@protoc_insertion_point(field_add:Valdi.DaemonAwakeMessage.device_ids)
}
inline void DaemonAwakeMessage::add_device_ids(std::string&& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_device_ids()->Add(std::move(value));
  // @@protoc_insertion_point(field_add:Valdi.DaemonAwakeMessage.device_ids)
}
inline void DaemonAwakeMessage::add_device_ids(const char* value) {
  ABSL_DCHECK(value != nullptr);
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_device_ids()->Add()->assign(value);
  // @@protoc_insertion_point(field_add_char:Valdi.DaemonAwakeMessage.device_ids)
}
inline void DaemonAwakeMessage::add_device_ids(const char* value, std::size_t size) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_device_ids()->Add()->assign(
      reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_add_pointer:Valdi.DaemonAwakeMessage.device_ids)
}
inline void DaemonAwakeMessage::add_device_ids(absl::string_view value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_device_ids()->Add()->assign(value.data(),
                                                     value.size());
  // @@protoc_insertion_point(field_add_string_piece:Valdi.DaemonAwakeMessage.device_ids)
}
inline const ::google::protobuf::RepeatedPtrField<std::string>&
DaemonAwakeMessage::device_ids() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:Valdi.DaemonAwakeMessage.device_ids)
  return _internal_device_ids();
}
inline ::google::protobuf::RepeatedPtrField<std::string>*
DaemonAwakeMessage::mutable_device_ids() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:Valdi.DaemonAwakeMessage.device_ids)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_device_ids();
}
inline const ::google::protobuf::RepeatedPtrField<std::string>&
DaemonAwakeMessage::_internal_device_ids() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.device_ids_;
}
inline ::google::protobuf::RepeatedPtrField<std::string>*
DaemonAwakeMessage::_internal_mutable_device_ids() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.device_ids_;
}

// repeated string usernames = 17;
inline int DaemonAwakeMessage::_internal_usernames_size() const {
  return _internal_usernames().size();
}
inline int DaemonAwakeMessage::usernames_size() const {
  return _internal_usernames_size();
}
inline void DaemonAwakeMessage::clear_usernames() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.usernames_.Clear();
}
inline std::string* DaemonAwakeMessage::add_usernames()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  std::string* _s = _internal_mutable_usernames()->Add();
  // @@protoc_insertion_point(field_add_mutable:Valdi.DaemonAwakeMessage.usernames)
  return _s;
}
inline const std::string& DaemonAwakeMessage::usernames(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.DaemonAwakeMessage.usernames)
  return _internal_usernames().Get(index);
}
inline std::string* DaemonAwakeMessage::mutable_usernames(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:Valdi.DaemonAwakeMessage.usernames)
  return _internal_mutable_usernames()->Mutable(index);
}
inline void DaemonAwakeMessage::set_usernames(int index, const std::string& value) {
  _internal_mutable_usernames()->Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set:Valdi.DaemonAwakeMessage.usernames)
}
inline void DaemonAwakeMessage::set_usernames(int index, std::string&& value) {
  _internal_mutable_usernames()->Mutable(index)->assign(std::move(value));
  // @@protoc_insertion_point(field_set:Valdi.DaemonAwakeMessage.usernames)
}
inline void DaemonAwakeMessage::set_usernames(int index, const char* value) {
  ABSL_DCHECK(value != nullptr);
  _internal_mutable_usernames()->Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set_char:Valdi.DaemonAwakeMessage.usernames)
}
inline void DaemonAwakeMessage::set_usernames(int index, const char* value,
                              std::size_t size) {
  _internal_mutable_usernames()->Mutable(index)->assign(
      reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_set_pointer:Valdi.DaemonAwakeMessage.usernames)
}
inline void DaemonAwakeMessage::set_usernames(int index, absl::string_view value) {
  _internal_mutable_usernames()->Mutable(index)->assign(
      value.data(), value.size());
  // @@protoc_insertion_point(field_set_string_piece:Valdi.DaemonAwakeMessage.usernames)
}
inline void DaemonAwakeMessage::add_usernames(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_usernames()->Add()->assign(value);
  // @@protoc_insertion_point(field_add:Valdi.DaemonAwakeMessage.usernames)
}
inline void DaemonAwakeMessage::add_usernames(std::string&& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_usernames()->Add(std::move(value));
  // @@protoc_insertion_point(field_add:Valdi.DaemonAwakeMessage.usernames)
}
inline void DaemonAwakeMessage::add_usernames(const char* value) {
  ABSL_DCHECK(value != nullptr);
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_usernames()->Add()->assign(value);
  // @@protoc_insertion_point(field_add_char:Valdi.DaemonAwakeMessage.usernames)
}
inline void DaemonAwakeMessage::add_usernames(const char* value, std::size_t size) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_usernames()->Add()->assign(
      reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_add_pointer:Valdi.DaemonAwakeMessage.usernames)
}
inline void DaemonAwakeMessage::add_usernames(absl::string_view value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_usernames()->Add()->assign(value.data(),
                                                     value.size());
  // @@protoc_insertion_point(field_add_string_piece:Valdi.DaemonAwakeMessage.usernames)
}
inline const ::google::protobuf::RepeatedPtrField<std::string>&
DaemonAwakeMessage::usernames() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:Valdi.DaemonAwakeMessage.usernames)
  return _internal_usernames();
}
inline ::google::protobuf::RepeatedPtrField<std::string>*
DaemonAwakeMessage::mutable_usernames() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:Valdi.DaemonAwakeMessage.usernames)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_usernames();
}
inline const ::google::protobuf::RepeatedPtrField<std::string>&
DaemonAwakeMessage::_internal_usernames() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.usernames_;
}
inline ::google::protobuf::RepeatedPtrField<std::string>*
DaemonAwakeMessage::_internal_mutable_usernames() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.usernames_;
}

// repeated string service_addresses = 1;
inline int DaemonAwakeMessage::_internal_service_addresses_size() const {
  return _internal_service_addresses().size();
}
inline int DaemonAwakeMessage::service_addresses_size() const {
  return _internal_service_addresses_size();
}
inline void DaemonAwakeMessage::clear_service_addresses() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.service_addresses_.Clear();
}
inline std::string* DaemonAwakeMessage::add_service_addresses()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  std::string* _s = _internal_mutable_service_addresses()->Add();
  // @@protoc_insertion_point(field_add_mutable:Valdi.DaemonAwakeMessage.service_addresses)
  return _s;
}
inline const std::string& DaemonAwakeMessage::service_addresses(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.DaemonAwakeMessage.service_addresses)
  return _internal_service_addresses().Get(index);
}
inline std::string* DaemonAwakeMessage::mutable_service_addresses(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:Valdi.DaemonAwakeMessage.service_addresses)
  return _internal_mutable_service_addresses()->Mutable(index);
}
inline void DaemonAwakeMessage::set_service_addresses(int index, const std::string& value) {
  _internal_mutable_service_addresses()->Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set:Valdi.DaemonAwakeMessage.service_addresses)
}
inline void DaemonAwakeMessage::set_service_addresses(int index, std::string&& value) {
  _internal_mutable_service_addresses()->Mutable(index)->assign(std::move(value));
  // @@protoc_insertion_point(field_set:Valdi.DaemonAwakeMessage.service_addresses)
}
inline void DaemonAwakeMessage::set_service_addresses(int index, const char* value) {
  ABSL_DCHECK(value != nullptr);
  _internal_mutable_service_addresses()->Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set_char:Valdi.DaemonAwakeMessage.service_addresses)
}
inline void DaemonAwakeMessage::set_service_addresses(int index, const char* value,
                              std::size_t size) {
  _internal_mutable_service_addresses()->Mutable(index)->assign(
      reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_set_pointer:Valdi.DaemonAwakeMessage.service_addresses)
}
inline void DaemonAwakeMessage::set_service_addresses(int index, absl::string_view value) {
  _internal_mutable_service_addresses()->Mutable(index)->assign(
      value.data(), value.size());
  // @@protoc_insertion_point(field_set_string_piece:Valdi.DaemonAwakeMessage.service_addresses)
}
inline void DaemonAwakeMessage::add_service_addresses(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_service_addresses()->Add()->assign(value);
  // @@protoc_insertion_point(field_add:Valdi.DaemonAwakeMessage.service_addresses)
}
inline void DaemonAwakeMessage::add_service_addresses(std::string&& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_service_addresses()->Add(std::move(value));
  // @@protoc_insertion_point(field_add:Valdi.DaemonAwakeMessage.service_addresses)
}
inline void DaemonAwakeMessage::add_service_addresses(const char* value) {
  ABSL_DCHECK(value != nullptr);
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_service_addresses()->Add()->assign(value);
  // @@protoc_insertion_point(field_add_char:Valdi.DaemonAwakeMessage.service_addresses)
}
inline void DaemonAwakeMessage::add_service_addresses(const char* value, std::size_t size) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_service_addresses()->Add()->assign(
      reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_add_pointer:Valdi.DaemonAwakeMessage.service_addresses)
}
inline void DaemonAwakeMessage::add_service_addresses(absl::string_view value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_service_addresses()->Add()->assign(value.data(),
                                                     value.size());
  // @@protoc_insertion_point(field_add_string_piece:Valdi.DaemonAwakeMessage.service_addresses)
}
inline const ::google::protobuf::RepeatedPtrField<std::string>&
DaemonAwakeMessage::service_addresses() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:Valdi.DaemonAwakeMessage.service_addresses)
  return _internal_service_addresses();
}
inline ::google::protobuf::RepeatedPtrField<std::string>*
DaemonAwakeMessage::mutable_service_addresses() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:Valdi.DaemonAwakeMessage.service_addresses)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_service_addresses();
}
inline const ::google::protobuf::RepeatedPtrField<std::string>&
DaemonAwakeMessage::_internal_service_addresses() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.service_addresses_;
}
inline ::google::protobuf::RepeatedPtrField<std::string>*
DaemonAwakeMessage::_internal_mutable_service_addresses() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.service_addresses_;
}

// int32 service_port = 2;
inline void DaemonAwakeMessage::clear_service_port() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.service_port_ = 0;
}
inline ::int32_t DaemonAwakeMessage::service_port() const {
  // @@protoc_insertion_point(field_get:Valdi.DaemonAwakeMessage.service_port)
  return _internal_service_port();
}
inline void DaemonAwakeMessage::set_service_port(::int32_t value) {
  _internal_set_service_port(value);
  // @@protoc_insertion_point(field_set:Valdi.DaemonAwakeMessage.service_port)
}
inline ::int32_t DaemonAwakeMessage::_internal_service_port() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.service_port_;
}
inline void DaemonAwakeMessage::_internal_set_service_port(::int32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.service_port_ = value;
}

// string process_id = 3;
inline void DaemonAwakeMessage::clear_process_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.process_id_.ClearToEmpty();
}
inline const std::string& DaemonAwakeMessage::process_id() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.DaemonAwakeMessage.process_id)
  return _internal_process_id();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void DaemonAwakeMessage::set_process_id(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.process_id_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:Valdi.DaemonAwakeMessage.process_id)
}
inline std::string* DaemonAwakeMessage::mutable_process_id() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_process_id();
  // @@protoc_insertion_point(field_mutable:Valdi.DaemonAwakeMessage.process_id)
  return _s;
}
inline const std::string& DaemonAwakeMessage::_internal_process_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.process_id_.Get();
}
inline void DaemonAwakeMessage::_internal_set_process_id(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.process_id_.Set(value, GetArena());
}
inline std::string* DaemonAwakeMessage::_internal_mutable_process_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.process_id_.Mutable( GetArena());
}
inline std::string* DaemonAwakeMessage::release_process_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.DaemonAwakeMessage.process_id)
  return _impl_.process_id_.Release();
}
inline void DaemonAwakeMessage::set_allocated_process_id(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.process_id_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.process_id_.IsDefault()) {
          _impl_.process_id_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:Valdi.DaemonAwakeMessage.process_id)
}

// -------------------------------------------------------------------

// ClientAwakeMessage

// string device_id = 1;
inline void ClientAwakeMessage::clear_device_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.device_id_.ClearToEmpty();
}
inline const std::string& ClientAwakeMessage::device_id() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.ClientAwakeMessage.device_id)
  return _internal_device_id();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void ClientAwakeMessage::set_device_id(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.device_id_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:Valdi.ClientAwakeMessage.device_id)
}
inline std::string* ClientAwakeMessage::mutable_device_id() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_device_id();
  // @@protoc_insertion_point(field_mutable:Valdi.ClientAwakeMessage.device_id)
  return _s;
}
inline const std::string& ClientAwakeMessage::_internal_device_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.device_id_.Get();
}
inline void ClientAwakeMessage::_internal_set_device_id(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.device_id_.Set(value, GetArena());
}
inline std::string* ClientAwakeMessage::_internal_mutable_device_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.device_id_.Mutable( GetArena());
}
inline std::string* ClientAwakeMessage::release_device_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.ClientAwakeMessage.device_id)
  return _impl_.device_id_.Release();
}
inline void ClientAwakeMessage::set_allocated_device_id(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.device_id_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.device_id_.IsDefault()) {
          _impl_.device_id_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:Valdi.ClientAwakeMessage.device_id)
}

// string username = 2;
inline void ClientAwakeMessage::clear_username() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.username_.ClearToEmpty();
}
inline const std::string& ClientAwakeMessage::username() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.ClientAwakeMessage.username)
  return _internal_username();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void ClientAwakeMessage::set_username(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.username_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:Valdi.ClientAwakeMessage.username)
}
inline std::string* ClientAwakeMessage::mutable_username() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_username();
  // @@protoc_insertion_point(field_mutable:Valdi.ClientAwakeMessage.username)
  return _s;
}
inline const std::string& ClientAwakeMessage::_internal_username() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.username_.Get();
}
inline void ClientAwakeMessage::_internal_set_username(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.username_.Set(value, GetArena());
}
inline std::string* ClientAwakeMessage::_internal_mutable_username() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.username_.Mutable( GetArena());
}
inline std::string* ClientAwakeMessage::release_username() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.ClientAwakeMessage.username)
  return _impl_.username_.Release();
}
inline void ClientAwakeMessage::set_allocated_username(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.username_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.username_.IsDefault()) {
          _impl_.username_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:Valdi.ClientAwakeMessage.username)
}

// -------------------------------------------------------------------

// DaemonServiceDiscoveryPayload

// .Valdi.DaemonAwakeMessage daemon_awake_message = 1;
inline bool DaemonServiceDiscoveryPayload::has_daemon_awake_message() const {
  return message_case() == kDaemonAwakeMessage;
}
inline bool DaemonServiceDiscoveryPayload::_internal_has_daemon_awake_message() const {
  return message_case() == kDaemonAwakeMessage;
}
inline void DaemonServiceDiscoveryPayload::set_has_daemon_awake_message() {
  _impl_._oneof_case_[0] = kDaemonAwakeMessage;
}
inline void DaemonServiceDiscoveryPayload::clear_daemon_awake_message() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_case() == kDaemonAwakeMessage) {
    if (GetArena() == nullptr) {
      delete _impl_.message_.daemon_awake_message_;
    } else if (::google::protobuf::internal::DebugHardenClearOneofMessageOnArena()) {
      if (_impl_.message_.daemon_awake_message_ != nullptr) {
        _impl_.message_.daemon_awake_message_->Clear();
      }
    }
    clear_has_message();
  }
}
inline ::Valdi::DaemonAwakeMessage* DaemonServiceDiscoveryPayload::release_daemon_awake_message() {
  // @@protoc_insertion_point(field_release:Valdi.DaemonServiceDiscoveryPayload.daemon_awake_message)
  if (message_case() == kDaemonAwakeMessage) {
    clear_has_message();
    auto* temp = _impl_.message_.daemon_awake_message_;
    if (GetArena() != nullptr) {
      temp = ::google::protobuf::internal::DuplicateIfNonNull(temp);
    }
    _impl_.message_.daemon_awake_message_ = nullptr;
    return temp;
  } else {
    return nullptr;
  }
}
inline const ::Valdi::DaemonAwakeMessage& DaemonServiceDiscoveryPayload::_internal_daemon_awake_message() const {
  return message_case() == kDaemonAwakeMessage ? *_impl_.message_.daemon_awake_message_ : reinterpret_cast<::Valdi::DaemonAwakeMessage&>(::Valdi::_DaemonAwakeMessage_default_instance_);
}
inline const ::Valdi::DaemonAwakeMessage& DaemonServiceDiscoveryPayload::daemon_awake_message() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.DaemonServiceDiscoveryPayload.daemon_awake_message)
  return _internal_daemon_awake_message();
}
inline ::Valdi::DaemonAwakeMessage* DaemonServiceDiscoveryPayload::unsafe_arena_release_daemon_awake_message() {
  // @@protoc_insertion_point(field_unsafe_arena_release:Valdi.DaemonServiceDiscoveryPayload.daemon_awake_message)
  if (message_case() == kDaemonAwakeMessage) {
    clear_has_message();
    auto* temp = _impl_.message_.daemon_awake_message_;
    _impl_.message_.daemon_awake_message_ = nullptr;
    return temp;
  } else {
    return nullptr;
  }
}
inline void DaemonServiceDiscoveryPayload::unsafe_arena_set_allocated_daemon_awake_message(::Valdi::DaemonAwakeMessage* value) {
  // We rely on the oneof clear method to free the earlier contents
  // of this oneof. We can directly use the pointer we're given to
  // set the new value.
  clear_message();
  if (value) {
    set_has_daemon_awake_message();
    _impl_.message_.daemon_awake_message_ = value;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:Valdi.DaemonServiceDiscoveryPayload.daemon_awake_message)
}
inline ::Valdi::DaemonAwakeMessage* DaemonServiceDiscoveryPayload::_internal_mutable_daemon_awake_message() {
  if (message_case() != kDaemonAwakeMessage) {
    clear_message();
    set_has_daemon_awake_message();
    _impl_.message_.daemon_awake_message_ =
        ::google::protobuf::MessageLite::DefaultConstruct<::Valdi::DaemonAwakeMessage>(GetArena());
  }
  return _impl_.message_.daemon_awake_message_;
}
inline ::Valdi::DaemonAwakeMessage* DaemonServiceDiscoveryPayload::mutable_daemon_awake_message() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::Valdi::DaemonAwakeMessage* _msg = _internal_mutable_daemon_awake_message();
  // @@protoc_insertion_point(field_mutable:Valdi.DaemonServiceDiscoveryPayload.daemon_awake_message)
  return _msg;
}

// .Valdi.ClientAwakeMessage client_awake_message = 2;
inline bool DaemonServiceDiscoveryPayload::has_client_awake_message() const {
  return message_case() == kClientAwakeMessage;
}
inline bool DaemonServiceDiscoveryPayload::_internal_has_client_awake_message() const {
  return message_case() == kClientAwakeMessage;
}
inline void DaemonServiceDiscoveryPayload::set_has_client_awake_message() {
  _impl_._oneof_case_[0] = kClientAwakeMessage;
}
inline void DaemonServiceDiscoveryPayload::clear_client_awake_message() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_case() == kClientAwakeMessage) {
    if (GetArena() == nullptr) {
      delete _impl_.message_.client_awake_message_;
    } else if (::google::protobuf::internal::DebugHardenClearOneofMessageOnArena()) {
      if (_impl_.message_.client_awake_message_ != nullptr) {
        _impl_.message_.client_awake_message_->Clear();
      }
    }
    clear_has_message();
  }
}
inline ::Valdi::ClientAwakeMessage* DaemonServiceDiscoveryPayload::release_client_awake_message() {
  // @@protoc_insertion_point(field_release:Valdi.DaemonServiceDiscoveryPayload.client_awake_message)
  if (message_case() == kClientAwakeMessage) {
    clear_has_message();
    auto* temp = _impl_.message_.client_awake_message_;
    if (GetArena() != nullptr) {
      temp = ::google::protobuf::internal::DuplicateIfNonNull(temp);
    }
    _impl_.message_.client_awake_message_ = nullptr;
    return temp;
  } else {
    return nullptr;
  }
}
inline const ::Valdi::ClientAwakeMessage& DaemonServiceDiscoveryPayload::_internal_client_awake_message() const {
  return message_case() == kClientAwakeMessage ? *_impl_.message_.client_awake_message_ : reinterpret_cast<::Valdi::ClientAwakeMessage&>(::Valdi::_ClientAwakeMessage_default_instance_);
}
inline const ::Valdi::ClientAwakeMessage& DaemonServiceDiscoveryPayload::client_awake_message() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.DaemonServiceDiscoveryPayload.client_awake_message)
  return _internal_client_awake_message();
}
inline ::Valdi::ClientAwakeMessage* DaemonServiceDiscoveryPayload::unsafe_arena_release_client_awake_message() {
  // @@protoc_insertion_point(field_unsafe_arena_release:Valdi.DaemonServiceDiscoveryPayload.client_awake_message)
  if (message_case() == kClientAwakeMessage) {
    clear_has_message();
    auto* temp = _impl_.message_.client_awake_message_;
    _impl_.message_.client_awake_message_ = nullptr;
    return temp;
  } else {
    return nullptr;
  }
}
inline void DaemonServiceDiscoveryPayload::unsafe_arena_set_allocated_client_awake_message(::Valdi::ClientAwakeMessage* value) {
  // We rely on the oneof clear method to free the earlier contents
  // of this oneof. We can directly use the pointer we're given to
  // set the new value.
  clear_message();
  if (value) {
    set_has_client_awake_message();
    _impl_.message_.client_awake_message_ = value;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:Valdi.DaemonServiceDiscoveryPayload.client_awake_message)
}
inline ::Valdi::ClientAwakeMessage* DaemonServiceDiscoveryPayload::_internal_mutable_client_awake_message() {
  if (message_case() != kClientAwakeMessage) {
    clear_message();
    set_has_client_awake_message();
    _impl_.message_.client_awake_message_ =
        ::google::protobuf::MessageLite::DefaultConstruct<::Valdi::ClientAwakeMessage>(GetArena());
  }
  return _impl_.message_.client_awake_message_;
}
inline ::Valdi::ClientAwakeMessage* DaemonServiceDiscoveryPayload::mutable_client_awake_message() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::Valdi::ClientAwakeMessage* _msg = _internal_mutable_client_awake_message();
  // @@protoc_insertion_point(field_mutable:Valdi.DaemonServiceDiscoveryPayload.client_awake_message)
  return _msg;
}

inline bool DaemonServiceDiscoveryPayload::has_message() const {
  return message_case() != MESSAGE_NOT_SET;
}
inline void DaemonServiceDiscoveryPayload::clear_has_message() {
  _impl_._oneof_case_[0] = MESSAGE_NOT_SET;
}
inline DaemonServiceDiscoveryPayload::MessageCase DaemonServiceDiscoveryPayload::message_case() const {
  return DaemonServiceDiscoveryPayload::MessageCase(_impl_._oneof_case_[0]);
}
// -------------------------------------------------------------------

// StyleNode

// repeated .Valdi.StyleDeclaration styles = 1;
inline int StyleNode::_internal_styles_size() const {
  return _internal_styles().size();
}
inline int StyleNode::styles_size() const {
  return _internal_styles_size();
}
inline void StyleNode::clear_styles() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.styles_.Clear();
}
inline ::Valdi::StyleDeclaration* StyleNode::mutable_styles(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:Valdi.StyleNode.styles)
  return _internal_mutable_styles()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::Valdi::StyleDeclaration>* StyleNode::mutable_styles()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:Valdi.StyleNode.styles)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_styles();
}
inline const ::Valdi::StyleDeclaration& StyleNode::styles(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.StyleNode.styles)
  return _internal_styles().Get(index);
}
inline ::Valdi::StyleDeclaration* StyleNode::add_styles() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::Valdi::StyleDeclaration* _add = _internal_mutable_styles()->Add();
  // @@protoc_insertion_point(field_add:Valdi.StyleNode.styles)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::Valdi::StyleDeclaration>& StyleNode::styles() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:Valdi.StyleNode.styles)
  return _internal_styles();
}
inline const ::google::protobuf::RepeatedPtrField<::Valdi::StyleDeclaration>&
StyleNode::_internal_styles() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.styles_;
}
inline ::google::protobuf::RepeatedPtrField<::Valdi::StyleDeclaration>*
StyleNode::_internal_mutable_styles() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.styles_;
}

// .Valdi.CSSRuleIndex ruleIndex = 2;
inline bool StyleNode::has_ruleindex() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.ruleindex_ != nullptr);
  return value;
}
inline void StyleNode::clear_ruleindex() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.ruleindex_ != nullptr) _impl_.ruleindex_->Clear();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline const ::Valdi::CSSRuleIndex& StyleNode::_internal_ruleindex() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::Valdi::CSSRuleIndex* p = _impl_.ruleindex_;
  return p != nullptr ? *p : reinterpret_cast<const ::Valdi::CSSRuleIndex&>(::Valdi::_CSSRuleIndex_default_instance_);
}
inline const ::Valdi::CSSRuleIndex& StyleNode::ruleindex() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.StyleNode.ruleIndex)
  return _internal_ruleindex();
}
inline void StyleNode::unsafe_arena_set_allocated_ruleindex(::Valdi::CSSRuleIndex* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.ruleindex_);
  }
  _impl_.ruleindex_ = reinterpret_cast<::Valdi::CSSRuleIndex*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:Valdi.StyleNode.ruleIndex)
}
inline ::Valdi::CSSRuleIndex* StyleNode::release_ruleindex() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Valdi::CSSRuleIndex* released = _impl_.ruleindex_;
  _impl_.ruleindex_ = nullptr;
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
inline ::Valdi::CSSRuleIndex* StyleNode::unsafe_arena_release_ruleindex() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.StyleNode.ruleIndex)

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Valdi::CSSRuleIndex* temp = _impl_.ruleindex_;
  _impl_.ruleindex_ = nullptr;
  return temp;
}
inline ::Valdi::CSSRuleIndex* StyleNode::_internal_mutable_ruleindex() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.ruleindex_ == nullptr) {
    auto* p = ::google::protobuf::MessageLite::DefaultConstruct<::Valdi::CSSRuleIndex>(GetArena());
    _impl_.ruleindex_ = reinterpret_cast<::Valdi::CSSRuleIndex*>(p);
  }
  return _impl_.ruleindex_;
}
inline ::Valdi::CSSRuleIndex* StyleNode::mutable_ruleindex() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000001u;
  ::Valdi::CSSRuleIndex* _msg = _internal_mutable_ruleindex();
  // @@protoc_insertion_point(field_mutable:Valdi.StyleNode.ruleIndex)
  return _msg;
}
inline void StyleNode::set_allocated_ruleindex(::Valdi::CSSRuleIndex* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete (_impl_.ruleindex_);
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

  _impl_.ruleindex_ = reinterpret_cast<::Valdi::CSSRuleIndex*>(value);
  // @@protoc_insertion_point(field_set_allocated:Valdi.StyleNode.ruleIndex)
}

// -------------------------------------------------------------------

// StyleDeclaration

// .Valdi.NodeAttribute attribute = 1;
inline bool StyleDeclaration::has_attribute() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.attribute_ != nullptr);
  return value;
}
inline void StyleDeclaration::clear_attribute() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.attribute_ != nullptr) _impl_.attribute_->Clear();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline const ::Valdi::NodeAttribute& StyleDeclaration::_internal_attribute() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::Valdi::NodeAttribute* p = _impl_.attribute_;
  return p != nullptr ? *p : reinterpret_cast<const ::Valdi::NodeAttribute&>(::Valdi::_NodeAttribute_default_instance_);
}
inline const ::Valdi::NodeAttribute& StyleDeclaration::attribute() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.StyleDeclaration.attribute)
  return _internal_attribute();
}
inline void StyleDeclaration::unsafe_arena_set_allocated_attribute(::Valdi::NodeAttribute* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.attribute_);
  }
  _impl_.attribute_ = reinterpret_cast<::Valdi::NodeAttribute*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:Valdi.StyleDeclaration.attribute)
}
inline ::Valdi::NodeAttribute* StyleDeclaration::release_attribute() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Valdi::NodeAttribute* released = _impl_.attribute_;
  _impl_.attribute_ = nullptr;
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
inline ::Valdi::NodeAttribute* StyleDeclaration::unsafe_arena_release_attribute() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.StyleDeclaration.attribute)

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Valdi::NodeAttribute* temp = _impl_.attribute_;
  _impl_.attribute_ = nullptr;
  return temp;
}
inline ::Valdi::NodeAttribute* StyleDeclaration::_internal_mutable_attribute() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.attribute_ == nullptr) {
    auto* p = ::google::protobuf::MessageLite::DefaultConstruct<::Valdi::NodeAttribute>(GetArena());
    _impl_.attribute_ = reinterpret_cast<::Valdi::NodeAttribute*>(p);
  }
  return _impl_.attribute_;
}
inline ::Valdi::NodeAttribute* StyleDeclaration::mutable_attribute() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000001u;
  ::Valdi::NodeAttribute* _msg = _internal_mutable_attribute();
  // @@protoc_insertion_point(field_mutable:Valdi.StyleDeclaration.attribute)
  return _msg;
}
inline void StyleDeclaration::set_allocated_attribute(::Valdi::NodeAttribute* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete (_impl_.attribute_);
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

  _impl_.attribute_ = reinterpret_cast<::Valdi::NodeAttribute*>(value);
  // @@protoc_insertion_point(field_set_allocated:Valdi.StyleDeclaration.attribute)
}

// int32 priority = 2;
inline void StyleDeclaration::clear_priority() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.priority_ = 0;
}
inline ::int32_t StyleDeclaration::priority() const {
  // @@protoc_insertion_point(field_get:Valdi.StyleDeclaration.priority)
  return _internal_priority();
}
inline void StyleDeclaration::set_priority(::int32_t value) {
  _internal_set_priority(value);
  // @@protoc_insertion_point(field_set:Valdi.StyleDeclaration.priority)
}
inline ::int32_t StyleDeclaration::_internal_priority() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.priority_;
}
inline void StyleDeclaration::_internal_set_priority(::int32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.priority_ = value;
}

// int32 order = 3;
inline void StyleDeclaration::clear_order() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.order_ = 0;
}
inline ::int32_t StyleDeclaration::order() const {
  // @@protoc_insertion_point(field_get:Valdi.StyleDeclaration.order)
  return _internal_order();
}
inline void StyleDeclaration::set_order(::int32_t value) {
  _internal_set_order(value);
  // @@protoc_insertion_point(field_set:Valdi.StyleDeclaration.order)
}
inline ::int32_t StyleDeclaration::_internal_order() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.order_;
}
inline void StyleDeclaration::_internal_set_order(::int32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.order_ = value;
}

// int32 id = 4;
inline void StyleDeclaration::clear_id() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.id_ = 0;
}
inline ::int32_t StyleDeclaration::id() const {
  // @@protoc_insertion_point(field_get:Valdi.StyleDeclaration.id)
  return _internal_id();
}
inline void StyleDeclaration::set_id(::int32_t value) {
  _internal_set_id(value);
  // @@protoc_insertion_point(field_set:Valdi.StyleDeclaration.id)
}
inline ::int32_t StyleDeclaration::_internal_id() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.id_;
}
inline void StyleDeclaration::_internal_set_id(::int32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.id_ = value;
}

// -------------------------------------------------------------------

// NamedStyleNode

// string name = 1;
inline void NamedStyleNode::clear_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.name_.ClearToEmpty();
}
inline const std::string& NamedStyleNode::name() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.NamedStyleNode.name)
  return _internal_name();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void NamedStyleNode::set_name(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.name_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:Valdi.NamedStyleNode.name)
}
inline std::string* NamedStyleNode::mutable_name() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_name();
  // @@protoc_insertion_point(field_mutable:Valdi.NamedStyleNode.name)
  return _s;
}
inline const std::string& NamedStyleNode::_internal_name() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.name_.Get();
}
inline void NamedStyleNode::_internal_set_name(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.name_.Set(value, GetArena());
}
inline std::string* NamedStyleNode::_internal_mutable_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _impl_.name_.Mutable( GetArena());
}
inline std::string* NamedStyleNode::release_name() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.NamedStyleNode.name)
  return _impl_.name_.Release();
}
inline void NamedStyleNode::set_allocated_name(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.name_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.name_.IsDefault()) {
          _impl_.name_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:Valdi.NamedStyleNode.name)
}

// .Valdi.StyleNode node = 2;
inline bool NamedStyleNode::has_node() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.node_ != nullptr);
  return value;
}
inline void NamedStyleNode::clear_node() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.node_ != nullptr) _impl_.node_->Clear();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline const ::Valdi::StyleNode& NamedStyleNode::_internal_node() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::Valdi::StyleNode* p = _impl_.node_;
  return p != nullptr ? *p : reinterpret_cast<const ::Valdi::StyleNode&>(::Valdi::_StyleNode_default_instance_);
}
inline const ::Valdi::StyleNode& NamedStyleNode::node() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.NamedStyleNode.node)
  return _internal_node();
}
inline void NamedStyleNode::unsafe_arena_set_allocated_node(::Valdi::StyleNode* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.node_);
  }
  _impl_.node_ = reinterpret_cast<::Valdi::StyleNode*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:Valdi.NamedStyleNode.node)
}
inline ::Valdi::StyleNode* NamedStyleNode::release_node() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Valdi::StyleNode* released = _impl_.node_;
  _impl_.node_ = nullptr;
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
inline ::Valdi::StyleNode* NamedStyleNode::unsafe_arena_release_node() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.NamedStyleNode.node)

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Valdi::StyleNode* temp = _impl_.node_;
  _impl_.node_ = nullptr;
  return temp;
}
inline ::Valdi::StyleNode* NamedStyleNode::_internal_mutable_node() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.node_ == nullptr) {
    auto* p = ::google::protobuf::MessageLite::DefaultConstruct<::Valdi::StyleNode>(GetArena());
    _impl_.node_ = reinterpret_cast<::Valdi::StyleNode*>(p);
  }
  return _impl_.node_;
}
inline ::Valdi::StyleNode* NamedStyleNode::mutable_node() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000001u;
  ::Valdi::StyleNode* _msg = _internal_mutable_node();
  // @@protoc_insertion_point(field_mutable:Valdi.NamedStyleNode.node)
  return _msg;
}
inline void NamedStyleNode::set_allocated_node(::Valdi::StyleNode* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete (_impl_.node_);
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

  _impl_.node_ = reinterpret_cast<::Valdi::StyleNode*>(value);
  // @@protoc_insertion_point(field_set_allocated:Valdi.NamedStyleNode.node)
}

// -------------------------------------------------------------------

// CSSRuleIndex_AttributeRule

// .Valdi.CSSRuleIndex.AttributeRule.Type type = 1;
inline void CSSRuleIndex_AttributeRule::clear_type() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.type_ = 0;
}
inline ::Valdi::CSSRuleIndex_AttributeRule_Type CSSRuleIndex_AttributeRule::type() const {
  // @@protoc_insertion_point(field_get:Valdi.CSSRuleIndex.AttributeRule.type)
  return _internal_type();
}
inline void CSSRuleIndex_AttributeRule::set_type(::Valdi::CSSRuleIndex_AttributeRule_Type value) {
  _internal_set_type(value);
  // @@protoc_insertion_point(field_set:Valdi.CSSRuleIndex.AttributeRule.type)
}
inline ::Valdi::CSSRuleIndex_AttributeRule_Type CSSRuleIndex_AttributeRule::_internal_type() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return static_cast<::Valdi::CSSRuleIndex_AttributeRule_Type>(_impl_.type_);
}
inline void CSSRuleIndex_AttributeRule::_internal_set_type(::Valdi::CSSRuleIndex_AttributeRule_Type value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.type_ = value;
}

// .Valdi.NodeAttribute attribute = 2;
inline bool CSSRuleIndex_AttributeRule::has_attribute() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.attribute_ != nullptr);
  return value;
}
inline void CSSRuleIndex_AttributeRule::clear_attribute() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.attribute_ != nullptr) _impl_.attribute_->Clear();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline const ::Valdi::NodeAttribute& CSSRuleIndex_AttributeRule::_internal_attribute() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::Valdi::NodeAttribute* p = _impl_.attribute_;
  return p != nullptr ? *p : reinterpret_cast<const ::Valdi::NodeAttribute&>(::Valdi::_NodeAttribute_default_instance_);
}
inline const ::Valdi::NodeAttribute& CSSRuleIndex_AttributeRule::attribute() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.CSSRuleIndex.AttributeRule.attribute)
  return _internal_attribute();
}
inline void CSSRuleIndex_AttributeRule::unsafe_arena_set_allocated_attribute(::Valdi::NodeAttribute* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.attribute_);
  }
  _impl_.attribute_ = reinterpret_cast<::Valdi::NodeAttribute*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:Valdi.CSSRuleIndex.AttributeRule.attribute)
}
inline ::Valdi::NodeAttribute* CSSRuleIndex_AttributeRule::release_attribute() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Valdi::NodeAttribute* released = _impl_.attribute_;
  _impl_.attribute_ = nullptr;
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
inline ::Valdi::NodeAttribute* CSSRuleIndex_AttributeRule::unsafe_arena_release_attribute() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.CSSRuleIndex.AttributeRule.attribute)

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Valdi::NodeAttribute* temp = _impl_.attribute_;
  _impl_.attribute_ = nullptr;
  return temp;
}
inline ::Valdi::NodeAttribute* CSSRuleIndex_AttributeRule::_internal_mutable_attribute() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.attribute_ == nullptr) {
    auto* p = ::google::protobuf::MessageLite::DefaultConstruct<::Valdi::NodeAttribute>(GetArena());
    _impl_.attribute_ = reinterpret_cast<::Valdi::NodeAttribute*>(p);
  }
  return _impl_.attribute_;
}
inline ::Valdi::NodeAttribute* CSSRuleIndex_AttributeRule::mutable_attribute() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000001u;
  ::Valdi::NodeAttribute* _msg = _internal_mutable_attribute();
  // @@protoc_insertion_point(field_mutable:Valdi.CSSRuleIndex.AttributeRule.attribute)
  return _msg;
}
inline void CSSRuleIndex_AttributeRule::set_allocated_attribute(::Valdi::NodeAttribute* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete (_impl_.attribute_);
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

  _impl_.attribute_ = reinterpret_cast<::Valdi::NodeAttribute*>(value);
  // @@protoc_insertion_point(field_set_allocated:Valdi.CSSRuleIndex.AttributeRule.attribute)
}

// .Valdi.StyleNode node = 3;
inline bool CSSRuleIndex_AttributeRule::has_node() const {
  bool value = (_impl_._has_bits_[0] & 0x00000002u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.node_ != nullptr);
  return value;
}
inline void CSSRuleIndex_AttributeRule::clear_node() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.node_ != nullptr) _impl_.node_->Clear();
  _impl_._has_bits_[0] &= ~0x00000002u;
}
inline const ::Valdi::StyleNode& CSSRuleIndex_AttributeRule::_internal_node() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::Valdi::StyleNode* p = _impl_.node_;
  return p != nullptr ? *p : reinterpret_cast<const ::Valdi::StyleNode&>(::Valdi::_StyleNode_default_instance_);
}
inline const ::Valdi::StyleNode& CSSRuleIndex_AttributeRule::node() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.CSSRuleIndex.AttributeRule.node)
  return _internal_node();
}
inline void CSSRuleIndex_AttributeRule::unsafe_arena_set_allocated_node(::Valdi::StyleNode* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.node_);
  }
  _impl_.node_ = reinterpret_cast<::Valdi::StyleNode*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000002u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000002u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:Valdi.CSSRuleIndex.AttributeRule.node)
}
inline ::Valdi::StyleNode* CSSRuleIndex_AttributeRule::release_node() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000002u;
  ::Valdi::StyleNode* released = _impl_.node_;
  _impl_.node_ = nullptr;
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
inline ::Valdi::StyleNode* CSSRuleIndex_AttributeRule::unsafe_arena_release_node() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.CSSRuleIndex.AttributeRule.node)

  _impl_._has_bits_[0] &= ~0x00000002u;
  ::Valdi::StyleNode* temp = _impl_.node_;
  _impl_.node_ = nullptr;
  return temp;
}
inline ::Valdi::StyleNode* CSSRuleIndex_AttributeRule::_internal_mutable_node() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.node_ == nullptr) {
    auto* p = ::google::protobuf::MessageLite::DefaultConstruct<::Valdi::StyleNode>(GetArena());
    _impl_.node_ = reinterpret_cast<::Valdi::StyleNode*>(p);
  }
  return _impl_.node_;
}
inline ::Valdi::StyleNode* CSSRuleIndex_AttributeRule::mutable_node() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000002u;
  ::Valdi::StyleNode* _msg = _internal_mutable_node();
  // @@protoc_insertion_point(field_mutable:Valdi.CSSRuleIndex.AttributeRule.node)
  return _msg;
}
inline void CSSRuleIndex_AttributeRule::set_allocated_node(::Valdi::StyleNode* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete (_impl_.node_);
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

  _impl_.node_ = reinterpret_cast<::Valdi::StyleNode*>(value);
  // @@protoc_insertion_point(field_set_allocated:Valdi.CSSRuleIndex.AttributeRule.node)
}

// -------------------------------------------------------------------

// CSSRuleIndex_NthChildRule

// int32 n = 1;
inline void CSSRuleIndex_NthChildRule::clear_n() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.n_ = 0;
}
inline ::int32_t CSSRuleIndex_NthChildRule::n() const {
  // @@protoc_insertion_point(field_get:Valdi.CSSRuleIndex.NthChildRule.n)
  return _internal_n();
}
inline void CSSRuleIndex_NthChildRule::set_n(::int32_t value) {
  _internal_set_n(value);
  // @@protoc_insertion_point(field_set:Valdi.CSSRuleIndex.NthChildRule.n)
}
inline ::int32_t CSSRuleIndex_NthChildRule::_internal_n() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.n_;
}
inline void CSSRuleIndex_NthChildRule::_internal_set_n(::int32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.n_ = value;
}

// int32 offset = 2;
inline void CSSRuleIndex_NthChildRule::clear_offset() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.offset_ = 0;
}
inline ::int32_t CSSRuleIndex_NthChildRule::offset() const {
  // @@protoc_insertion_point(field_get:Valdi.CSSRuleIndex.NthChildRule.offset)
  return _internal_offset();
}
inline void CSSRuleIndex_NthChildRule::set_offset(::int32_t value) {
  _internal_set_offset(value);
  // @@protoc_insertion_point(field_set:Valdi.CSSRuleIndex.NthChildRule.offset)
}
inline ::int32_t CSSRuleIndex_NthChildRule::_internal_offset() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.offset_;
}
inline void CSSRuleIndex_NthChildRule::_internal_set_offset(::int32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.offset_ = value;
}

// .Valdi.StyleNode node = 3;
inline bool CSSRuleIndex_NthChildRule::has_node() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.node_ != nullptr);
  return value;
}
inline void CSSRuleIndex_NthChildRule::clear_node() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.node_ != nullptr) _impl_.node_->Clear();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline const ::Valdi::StyleNode& CSSRuleIndex_NthChildRule::_internal_node() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::Valdi::StyleNode* p = _impl_.node_;
  return p != nullptr ? *p : reinterpret_cast<const ::Valdi::StyleNode&>(::Valdi::_StyleNode_default_instance_);
}
inline const ::Valdi::StyleNode& CSSRuleIndex_NthChildRule::node() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.CSSRuleIndex.NthChildRule.node)
  return _internal_node();
}
inline void CSSRuleIndex_NthChildRule::unsafe_arena_set_allocated_node(::Valdi::StyleNode* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.node_);
  }
  _impl_.node_ = reinterpret_cast<::Valdi::StyleNode*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:Valdi.CSSRuleIndex.NthChildRule.node)
}
inline ::Valdi::StyleNode* CSSRuleIndex_NthChildRule::release_node() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Valdi::StyleNode* released = _impl_.node_;
  _impl_.node_ = nullptr;
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
inline ::Valdi::StyleNode* CSSRuleIndex_NthChildRule::unsafe_arena_release_node() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.CSSRuleIndex.NthChildRule.node)

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Valdi::StyleNode* temp = _impl_.node_;
  _impl_.node_ = nullptr;
  return temp;
}
inline ::Valdi::StyleNode* CSSRuleIndex_NthChildRule::_internal_mutable_node() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.node_ == nullptr) {
    auto* p = ::google::protobuf::MessageLite::DefaultConstruct<::Valdi::StyleNode>(GetArena());
    _impl_.node_ = reinterpret_cast<::Valdi::StyleNode*>(p);
  }
  return _impl_.node_;
}
inline ::Valdi::StyleNode* CSSRuleIndex_NthChildRule::mutable_node() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000001u;
  ::Valdi::StyleNode* _msg = _internal_mutable_node();
  // @@protoc_insertion_point(field_mutable:Valdi.CSSRuleIndex.NthChildRule.node)
  return _msg;
}
inline void CSSRuleIndex_NthChildRule::set_allocated_node(::Valdi::StyleNode* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete (_impl_.node_);
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

  _impl_.node_ = reinterpret_cast<::Valdi::StyleNode*>(value);
  // @@protoc_insertion_point(field_set_allocated:Valdi.CSSRuleIndex.NthChildRule.node)
}

// -------------------------------------------------------------------

// CSSRuleIndex

// repeated .Valdi.NamedStyleNode id_rules = 1;
inline int CSSRuleIndex::_internal_id_rules_size() const {
  return _internal_id_rules().size();
}
inline int CSSRuleIndex::id_rules_size() const {
  return _internal_id_rules_size();
}
inline void CSSRuleIndex::clear_id_rules() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.id_rules_.Clear();
}
inline ::Valdi::NamedStyleNode* CSSRuleIndex::mutable_id_rules(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:Valdi.CSSRuleIndex.id_rules)
  return _internal_mutable_id_rules()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>* CSSRuleIndex::mutable_id_rules()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:Valdi.CSSRuleIndex.id_rules)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_id_rules();
}
inline const ::Valdi::NamedStyleNode& CSSRuleIndex::id_rules(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.CSSRuleIndex.id_rules)
  return _internal_id_rules().Get(index);
}
inline ::Valdi::NamedStyleNode* CSSRuleIndex::add_id_rules() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::Valdi::NamedStyleNode* _add = _internal_mutable_id_rules()->Add();
  // @@protoc_insertion_point(field_add:Valdi.CSSRuleIndex.id_rules)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>& CSSRuleIndex::id_rules() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:Valdi.CSSRuleIndex.id_rules)
  return _internal_id_rules();
}
inline const ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>&
CSSRuleIndex::_internal_id_rules() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.id_rules_;
}
inline ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>*
CSSRuleIndex::_internal_mutable_id_rules() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.id_rules_;
}

// repeated .Valdi.NamedStyleNode class_rules = 2;
inline int CSSRuleIndex::_internal_class_rules_size() const {
  return _internal_class_rules().size();
}
inline int CSSRuleIndex::class_rules_size() const {
  return _internal_class_rules_size();
}
inline void CSSRuleIndex::clear_class_rules() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.class_rules_.Clear();
}
inline ::Valdi::NamedStyleNode* CSSRuleIndex::mutable_class_rules(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:Valdi.CSSRuleIndex.class_rules)
  return _internal_mutable_class_rules()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>* CSSRuleIndex::mutable_class_rules()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:Valdi.CSSRuleIndex.class_rules)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_class_rules();
}
inline const ::Valdi::NamedStyleNode& CSSRuleIndex::class_rules(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.CSSRuleIndex.class_rules)
  return _internal_class_rules().Get(index);
}
inline ::Valdi::NamedStyleNode* CSSRuleIndex::add_class_rules() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::Valdi::NamedStyleNode* _add = _internal_mutable_class_rules()->Add();
  // @@protoc_insertion_point(field_add:Valdi.CSSRuleIndex.class_rules)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>& CSSRuleIndex::class_rules() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:Valdi.CSSRuleIndex.class_rules)
  return _internal_class_rules();
}
inline const ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>&
CSSRuleIndex::_internal_class_rules() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.class_rules_;
}
inline ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>*
CSSRuleIndex::_internal_mutable_class_rules() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.class_rules_;
}

// repeated .Valdi.NamedStyleNode tag_rules = 3;
inline int CSSRuleIndex::_internal_tag_rules_size() const {
  return _internal_tag_rules().size();
}
inline int CSSRuleIndex::tag_rules_size() const {
  return _internal_tag_rules_size();
}
inline void CSSRuleIndex::clear_tag_rules() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.tag_rules_.Clear();
}
inline ::Valdi::NamedStyleNode* CSSRuleIndex::mutable_tag_rules(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:Valdi.CSSRuleIndex.tag_rules)
  return _internal_mutable_tag_rules()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>* CSSRuleIndex::mutable_tag_rules()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:Valdi.CSSRuleIndex.tag_rules)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_tag_rules();
}
inline const ::Valdi::NamedStyleNode& CSSRuleIndex::tag_rules(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.CSSRuleIndex.tag_rules)
  return _internal_tag_rules().Get(index);
}
inline ::Valdi::NamedStyleNode* CSSRuleIndex::add_tag_rules() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::Valdi::NamedStyleNode* _add = _internal_mutable_tag_rules()->Add();
  // @@protoc_insertion_point(field_add:Valdi.CSSRuleIndex.tag_rules)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>& CSSRuleIndex::tag_rules() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:Valdi.CSSRuleIndex.tag_rules)
  return _internal_tag_rules();
}
inline const ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>&
CSSRuleIndex::_internal_tag_rules() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.tag_rules_;
}
inline ::google::protobuf::RepeatedPtrField<::Valdi::NamedStyleNode>*
CSSRuleIndex::_internal_mutable_tag_rules() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.tag_rules_;
}

// repeated .Valdi.CSSRuleIndex.AttributeRule attribute_rules = 4;
inline int CSSRuleIndex::_internal_attribute_rules_size() const {
  return _internal_attribute_rules().size();
}
inline int CSSRuleIndex::attribute_rules_size() const {
  return _internal_attribute_rules_size();
}
inline void CSSRuleIndex::clear_attribute_rules() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.attribute_rules_.Clear();
}
inline ::Valdi::CSSRuleIndex_AttributeRule* CSSRuleIndex::mutable_attribute_rules(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:Valdi.CSSRuleIndex.attribute_rules)
  return _internal_mutable_attribute_rules()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::Valdi::CSSRuleIndex_AttributeRule>* CSSRuleIndex::mutable_attribute_rules()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:Valdi.CSSRuleIndex.attribute_rules)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_attribute_rules();
}
inline const ::Valdi::CSSRuleIndex_AttributeRule& CSSRuleIndex::attribute_rules(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.CSSRuleIndex.attribute_rules)
  return _internal_attribute_rules().Get(index);
}
inline ::Valdi::CSSRuleIndex_AttributeRule* CSSRuleIndex::add_attribute_rules() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::Valdi::CSSRuleIndex_AttributeRule* _add = _internal_mutable_attribute_rules()->Add();
  // @@protoc_insertion_point(field_add:Valdi.CSSRuleIndex.attribute_rules)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::Valdi::CSSRuleIndex_AttributeRule>& CSSRuleIndex::attribute_rules() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:Valdi.CSSRuleIndex.attribute_rules)
  return _internal_attribute_rules();
}
inline const ::google::protobuf::RepeatedPtrField<::Valdi::CSSRuleIndex_AttributeRule>&
CSSRuleIndex::_internal_attribute_rules() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.attribute_rules_;
}
inline ::google::protobuf::RepeatedPtrField<::Valdi::CSSRuleIndex_AttributeRule>*
CSSRuleIndex::_internal_mutable_attribute_rules() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.attribute_rules_;
}

// .Valdi.StyleNode first_child_rule = 5;
inline bool CSSRuleIndex::has_first_child_rule() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.first_child_rule_ != nullptr);
  return value;
}
inline void CSSRuleIndex::clear_first_child_rule() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.first_child_rule_ != nullptr) _impl_.first_child_rule_->Clear();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline const ::Valdi::StyleNode& CSSRuleIndex::_internal_first_child_rule() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::Valdi::StyleNode* p = _impl_.first_child_rule_;
  return p != nullptr ? *p : reinterpret_cast<const ::Valdi::StyleNode&>(::Valdi::_StyleNode_default_instance_);
}
inline const ::Valdi::StyleNode& CSSRuleIndex::first_child_rule() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.CSSRuleIndex.first_child_rule)
  return _internal_first_child_rule();
}
inline void CSSRuleIndex::unsafe_arena_set_allocated_first_child_rule(::Valdi::StyleNode* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.first_child_rule_);
  }
  _impl_.first_child_rule_ = reinterpret_cast<::Valdi::StyleNode*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:Valdi.CSSRuleIndex.first_child_rule)
}
inline ::Valdi::StyleNode* CSSRuleIndex::release_first_child_rule() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Valdi::StyleNode* released = _impl_.first_child_rule_;
  _impl_.first_child_rule_ = nullptr;
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
inline ::Valdi::StyleNode* CSSRuleIndex::unsafe_arena_release_first_child_rule() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.CSSRuleIndex.first_child_rule)

  _impl_._has_bits_[0] &= ~0x00000001u;
  ::Valdi::StyleNode* temp = _impl_.first_child_rule_;
  _impl_.first_child_rule_ = nullptr;
  return temp;
}
inline ::Valdi::StyleNode* CSSRuleIndex::_internal_mutable_first_child_rule() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.first_child_rule_ == nullptr) {
    auto* p = ::google::protobuf::MessageLite::DefaultConstruct<::Valdi::StyleNode>(GetArena());
    _impl_.first_child_rule_ = reinterpret_cast<::Valdi::StyleNode*>(p);
  }
  return _impl_.first_child_rule_;
}
inline ::Valdi::StyleNode* CSSRuleIndex::mutable_first_child_rule() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000001u;
  ::Valdi::StyleNode* _msg = _internal_mutable_first_child_rule();
  // @@protoc_insertion_point(field_mutable:Valdi.CSSRuleIndex.first_child_rule)
  return _msg;
}
inline void CSSRuleIndex::set_allocated_first_child_rule(::Valdi::StyleNode* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete (_impl_.first_child_rule_);
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

  _impl_.first_child_rule_ = reinterpret_cast<::Valdi::StyleNode*>(value);
  // @@protoc_insertion_point(field_set_allocated:Valdi.CSSRuleIndex.first_child_rule)
}

// .Valdi.StyleNode last_child_rule = 6;
inline bool CSSRuleIndex::has_last_child_rule() const {
  bool value = (_impl_._has_bits_[0] & 0x00000002u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.last_child_rule_ != nullptr);
  return value;
}
inline void CSSRuleIndex::clear_last_child_rule() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.last_child_rule_ != nullptr) _impl_.last_child_rule_->Clear();
  _impl_._has_bits_[0] &= ~0x00000002u;
}
inline const ::Valdi::StyleNode& CSSRuleIndex::_internal_last_child_rule() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::Valdi::StyleNode* p = _impl_.last_child_rule_;
  return p != nullptr ? *p : reinterpret_cast<const ::Valdi::StyleNode&>(::Valdi::_StyleNode_default_instance_);
}
inline const ::Valdi::StyleNode& CSSRuleIndex::last_child_rule() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.CSSRuleIndex.last_child_rule)
  return _internal_last_child_rule();
}
inline void CSSRuleIndex::unsafe_arena_set_allocated_last_child_rule(::Valdi::StyleNode* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.last_child_rule_);
  }
  _impl_.last_child_rule_ = reinterpret_cast<::Valdi::StyleNode*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000002u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000002u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:Valdi.CSSRuleIndex.last_child_rule)
}
inline ::Valdi::StyleNode* CSSRuleIndex::release_last_child_rule() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000002u;
  ::Valdi::StyleNode* released = _impl_.last_child_rule_;
  _impl_.last_child_rule_ = nullptr;
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
inline ::Valdi::StyleNode* CSSRuleIndex::unsafe_arena_release_last_child_rule() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.CSSRuleIndex.last_child_rule)

  _impl_._has_bits_[0] &= ~0x00000002u;
  ::Valdi::StyleNode* temp = _impl_.last_child_rule_;
  _impl_.last_child_rule_ = nullptr;
  return temp;
}
inline ::Valdi::StyleNode* CSSRuleIndex::_internal_mutable_last_child_rule() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.last_child_rule_ == nullptr) {
    auto* p = ::google::protobuf::MessageLite::DefaultConstruct<::Valdi::StyleNode>(GetArena());
    _impl_.last_child_rule_ = reinterpret_cast<::Valdi::StyleNode*>(p);
  }
  return _impl_.last_child_rule_;
}
inline ::Valdi::StyleNode* CSSRuleIndex::mutable_last_child_rule() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000002u;
  ::Valdi::StyleNode* _msg = _internal_mutable_last_child_rule();
  // @@protoc_insertion_point(field_mutable:Valdi.CSSRuleIndex.last_child_rule)
  return _msg;
}
inline void CSSRuleIndex::set_allocated_last_child_rule(::Valdi::StyleNode* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete (_impl_.last_child_rule_);
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

  _impl_.last_child_rule_ = reinterpret_cast<::Valdi::StyleNode*>(value);
  // @@protoc_insertion_point(field_set_allocated:Valdi.CSSRuleIndex.last_child_rule)
}

// repeated .Valdi.CSSRuleIndex.NthChildRule nth_child_rules = 7;
inline int CSSRuleIndex::_internal_nth_child_rules_size() const {
  return _internal_nth_child_rules().size();
}
inline int CSSRuleIndex::nth_child_rules_size() const {
  return _internal_nth_child_rules_size();
}
inline void CSSRuleIndex::clear_nth_child_rules() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.nth_child_rules_.Clear();
}
inline ::Valdi::CSSRuleIndex_NthChildRule* CSSRuleIndex::mutable_nth_child_rules(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:Valdi.CSSRuleIndex.nth_child_rules)
  return _internal_mutable_nth_child_rules()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::Valdi::CSSRuleIndex_NthChildRule>* CSSRuleIndex::mutable_nth_child_rules()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:Valdi.CSSRuleIndex.nth_child_rules)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_nth_child_rules();
}
inline const ::Valdi::CSSRuleIndex_NthChildRule& CSSRuleIndex::nth_child_rules(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.CSSRuleIndex.nth_child_rules)
  return _internal_nth_child_rules().Get(index);
}
inline ::Valdi::CSSRuleIndex_NthChildRule* CSSRuleIndex::add_nth_child_rules() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::Valdi::CSSRuleIndex_NthChildRule* _add = _internal_mutable_nth_child_rules()->Add();
  // @@protoc_insertion_point(field_add:Valdi.CSSRuleIndex.nth_child_rules)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::Valdi::CSSRuleIndex_NthChildRule>& CSSRuleIndex::nth_child_rules() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:Valdi.CSSRuleIndex.nth_child_rules)
  return _internal_nth_child_rules();
}
inline const ::google::protobuf::RepeatedPtrField<::Valdi::CSSRuleIndex_NthChildRule>&
CSSRuleIndex::_internal_nth_child_rules() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.nth_child_rules_;
}
inline ::google::protobuf::RepeatedPtrField<::Valdi::CSSRuleIndex_NthChildRule>*
CSSRuleIndex::_internal_mutable_nth_child_rules() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.nth_child_rules_;
}

// .Valdi.CSSRuleIndex ancestor_rules = 8;
inline bool CSSRuleIndex::has_ancestor_rules() const {
  bool value = (_impl_._has_bits_[0] & 0x00000004u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.ancestor_rules_ != nullptr);
  return value;
}
inline void CSSRuleIndex::clear_ancestor_rules() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.ancestor_rules_ != nullptr) _impl_.ancestor_rules_->Clear();
  _impl_._has_bits_[0] &= ~0x00000004u;
}
inline const ::Valdi::CSSRuleIndex& CSSRuleIndex::_internal_ancestor_rules() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::Valdi::CSSRuleIndex* p = _impl_.ancestor_rules_;
  return p != nullptr ? *p : reinterpret_cast<const ::Valdi::CSSRuleIndex&>(::Valdi::_CSSRuleIndex_default_instance_);
}
inline const ::Valdi::CSSRuleIndex& CSSRuleIndex::ancestor_rules() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.CSSRuleIndex.ancestor_rules)
  return _internal_ancestor_rules();
}
inline void CSSRuleIndex::unsafe_arena_set_allocated_ancestor_rules(::Valdi::CSSRuleIndex* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.ancestor_rules_);
  }
  _impl_.ancestor_rules_ = reinterpret_cast<::Valdi::CSSRuleIndex*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000004u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000004u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:Valdi.CSSRuleIndex.ancestor_rules)
}
inline ::Valdi::CSSRuleIndex* CSSRuleIndex::release_ancestor_rules() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000004u;
  ::Valdi::CSSRuleIndex* released = _impl_.ancestor_rules_;
  _impl_.ancestor_rules_ = nullptr;
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
inline ::Valdi::CSSRuleIndex* CSSRuleIndex::unsafe_arena_release_ancestor_rules() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.CSSRuleIndex.ancestor_rules)

  _impl_._has_bits_[0] &= ~0x00000004u;
  ::Valdi::CSSRuleIndex* temp = _impl_.ancestor_rules_;
  _impl_.ancestor_rules_ = nullptr;
  return temp;
}
inline ::Valdi::CSSRuleIndex* CSSRuleIndex::_internal_mutable_ancestor_rules() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.ancestor_rules_ == nullptr) {
    auto* p = ::google::protobuf::MessageLite::DefaultConstruct<::Valdi::CSSRuleIndex>(GetArena());
    _impl_.ancestor_rules_ = reinterpret_cast<::Valdi::CSSRuleIndex*>(p);
  }
  return _impl_.ancestor_rules_;
}
inline ::Valdi::CSSRuleIndex* CSSRuleIndex::mutable_ancestor_rules() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000004u;
  ::Valdi::CSSRuleIndex* _msg = _internal_mutable_ancestor_rules();
  // @@protoc_insertion_point(field_mutable:Valdi.CSSRuleIndex.ancestor_rules)
  return _msg;
}
inline void CSSRuleIndex::set_allocated_ancestor_rules(::Valdi::CSSRuleIndex* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete (_impl_.ancestor_rules_);
  }

  if (value != nullptr) {
    ::google::protobuf::Arena* submessage_arena = (value)->GetArena();
    if (message_arena != submessage_arena) {
      value = ::google::protobuf::internal::GetOwnedMessage(message_arena, value, submessage_arena);
    }
    _impl_._has_bits_[0] |= 0x00000004u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000004u;
  }

  _impl_.ancestor_rules_ = reinterpret_cast<::Valdi::CSSRuleIndex*>(value);
  // @@protoc_insertion_point(field_set_allocated:Valdi.CSSRuleIndex.ancestor_rules)
}

// .Valdi.CSSRuleIndex direct_parent_rules = 9;
inline bool CSSRuleIndex::has_direct_parent_rules() const {
  bool value = (_impl_._has_bits_[0] & 0x00000008u) != 0;
  PROTOBUF_ASSUME(!value || _impl_.direct_parent_rules_ != nullptr);
  return value;
}
inline void CSSRuleIndex::clear_direct_parent_rules() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.direct_parent_rules_ != nullptr) _impl_.direct_parent_rules_->Clear();
  _impl_._has_bits_[0] &= ~0x00000008u;
}
inline const ::Valdi::CSSRuleIndex& CSSRuleIndex::_internal_direct_parent_rules() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  const ::Valdi::CSSRuleIndex* p = _impl_.direct_parent_rules_;
  return p != nullptr ? *p : reinterpret_cast<const ::Valdi::CSSRuleIndex&>(::Valdi::_CSSRuleIndex_default_instance_);
}
inline const ::Valdi::CSSRuleIndex& CSSRuleIndex::direct_parent_rules() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.CSSRuleIndex.direct_parent_rules)
  return _internal_direct_parent_rules();
}
inline void CSSRuleIndex::unsafe_arena_set_allocated_direct_parent_rules(::Valdi::CSSRuleIndex* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (GetArena() == nullptr) {
    delete reinterpret_cast<::google::protobuf::MessageLite*>(_impl_.direct_parent_rules_);
  }
  _impl_.direct_parent_rules_ = reinterpret_cast<::Valdi::CSSRuleIndex*>(value);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000008u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000008u;
  }
  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:Valdi.CSSRuleIndex.direct_parent_rules)
}
inline ::Valdi::CSSRuleIndex* CSSRuleIndex::release_direct_parent_rules() {
  ::google::protobuf::internal::TSanWrite(&_impl_);

  _impl_._has_bits_[0] &= ~0x00000008u;
  ::Valdi::CSSRuleIndex* released = _impl_.direct_parent_rules_;
  _impl_.direct_parent_rules_ = nullptr;
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
inline ::Valdi::CSSRuleIndex* CSSRuleIndex::unsafe_arena_release_direct_parent_rules() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:Valdi.CSSRuleIndex.direct_parent_rules)

  _impl_._has_bits_[0] &= ~0x00000008u;
  ::Valdi::CSSRuleIndex* temp = _impl_.direct_parent_rules_;
  _impl_.direct_parent_rules_ = nullptr;
  return temp;
}
inline ::Valdi::CSSRuleIndex* CSSRuleIndex::_internal_mutable_direct_parent_rules() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (_impl_.direct_parent_rules_ == nullptr) {
    auto* p = ::google::protobuf::MessageLite::DefaultConstruct<::Valdi::CSSRuleIndex>(GetArena());
    _impl_.direct_parent_rules_ = reinterpret_cast<::Valdi::CSSRuleIndex*>(p);
  }
  return _impl_.direct_parent_rules_;
}
inline ::Valdi::CSSRuleIndex* CSSRuleIndex::mutable_direct_parent_rules() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  _impl_._has_bits_[0] |= 0x00000008u;
  ::Valdi::CSSRuleIndex* _msg = _internal_mutable_direct_parent_rules();
  // @@protoc_insertion_point(field_mutable:Valdi.CSSRuleIndex.direct_parent_rules)
  return _msg;
}
inline void CSSRuleIndex::set_allocated_direct_parent_rules(::Valdi::CSSRuleIndex* value) {
  ::google::protobuf::Arena* message_arena = GetArena();
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (message_arena == nullptr) {
    delete (_impl_.direct_parent_rules_);
  }

  if (value != nullptr) {
    ::google::protobuf::Arena* submessage_arena = (value)->GetArena();
    if (message_arena != submessage_arena) {
      value = ::google::protobuf::internal::GetOwnedMessage(message_arena, value, submessage_arena);
    }
    _impl_._has_bits_[0] |= 0x00000008u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000008u;
  }

  _impl_.direct_parent_rules_ = reinterpret_cast<::Valdi::CSSRuleIndex*>(value);
  // @@protoc_insertion_point(field_set_allocated:Valdi.CSSRuleIndex.direct_parent_rules)
}

// -------------------------------------------------------------------

// TsnConfig

// repeated string enabled_modules = 1;
inline int TsnConfig::_internal_enabled_modules_size() const {
  return _internal_enabled_modules().size();
}
inline int TsnConfig::enabled_modules_size() const {
  return _internal_enabled_modules_size();
}
inline void TsnConfig::clear_enabled_modules() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.enabled_modules_.Clear();
}
inline std::string* TsnConfig::add_enabled_modules()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  std::string* _s = _internal_mutable_enabled_modules()->Add();
  // @@protoc_insertion_point(field_add_mutable:Valdi.TsnConfig.enabled_modules)
  return _s;
}
inline const std::string& TsnConfig::enabled_modules(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:Valdi.TsnConfig.enabled_modules)
  return _internal_enabled_modules().Get(index);
}
inline std::string* TsnConfig::mutable_enabled_modules(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:Valdi.TsnConfig.enabled_modules)
  return _internal_mutable_enabled_modules()->Mutable(index);
}
inline void TsnConfig::set_enabled_modules(int index, const std::string& value) {
  _internal_mutable_enabled_modules()->Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set:Valdi.TsnConfig.enabled_modules)
}
inline void TsnConfig::set_enabled_modules(int index, std::string&& value) {
  _internal_mutable_enabled_modules()->Mutable(index)->assign(std::move(value));
  // @@protoc_insertion_point(field_set:Valdi.TsnConfig.enabled_modules)
}
inline void TsnConfig::set_enabled_modules(int index, const char* value) {
  ABSL_DCHECK(value != nullptr);
  _internal_mutable_enabled_modules()->Mutable(index)->assign(value);
  // @@protoc_insertion_point(field_set_char:Valdi.TsnConfig.enabled_modules)
}
inline void TsnConfig::set_enabled_modules(int index, const char* value,
                              std::size_t size) {
  _internal_mutable_enabled_modules()->Mutable(index)->assign(
      reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_set_pointer:Valdi.TsnConfig.enabled_modules)
}
inline void TsnConfig::set_enabled_modules(int index, absl::string_view value) {
  _internal_mutable_enabled_modules()->Mutable(index)->assign(
      value.data(), value.size());
  // @@protoc_insertion_point(field_set_string_piece:Valdi.TsnConfig.enabled_modules)
}
inline void TsnConfig::add_enabled_modules(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_enabled_modules()->Add()->assign(value);
  // @@protoc_insertion_point(field_add:Valdi.TsnConfig.enabled_modules)
}
inline void TsnConfig::add_enabled_modules(std::string&& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_enabled_modules()->Add(std::move(value));
  // @@protoc_insertion_point(field_add:Valdi.TsnConfig.enabled_modules)
}
inline void TsnConfig::add_enabled_modules(const char* value) {
  ABSL_DCHECK(value != nullptr);
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_enabled_modules()->Add()->assign(value);
  // @@protoc_insertion_point(field_add_char:Valdi.TsnConfig.enabled_modules)
}
inline void TsnConfig::add_enabled_modules(const char* value, std::size_t size) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_enabled_modules()->Add()->assign(
      reinterpret_cast<const char*>(value), size);
  // @@protoc_insertion_point(field_add_pointer:Valdi.TsnConfig.enabled_modules)
}
inline void TsnConfig::add_enabled_modules(absl::string_view value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _internal_mutable_enabled_modules()->Add()->assign(value.data(),
                                                     value.size());
  // @@protoc_insertion_point(field_add_string_piece:Valdi.TsnConfig.enabled_modules)
}
inline const ::google::protobuf::RepeatedPtrField<std::string>&
TsnConfig::enabled_modules() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:Valdi.TsnConfig.enabled_modules)
  return _internal_enabled_modules();
}
inline ::google::protobuf::RepeatedPtrField<std::string>*
TsnConfig::mutable_enabled_modules() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:Valdi.TsnConfig.enabled_modules)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_enabled_modules();
}
inline const ::google::protobuf::RepeatedPtrField<std::string>&
TsnConfig::_internal_enabled_modules() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.enabled_modules_;
}
inline ::google::protobuf::RepeatedPtrField<std::string>*
TsnConfig::_internal_mutable_enabled_modules() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.enabled_modules_;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)
}  // namespace Valdi


namespace google {
namespace protobuf {

template <>
struct is_proto_enum<::Valdi::NodeAttribute_Type> : std::true_type {};
template <>
struct is_proto_enum<::Valdi::CSSRuleIndex_AttributeRule_Type> : std::true_type {};

}  // namespace protobuf
}  // namespace google

// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_valdi_2eproto_2epb_2eh
