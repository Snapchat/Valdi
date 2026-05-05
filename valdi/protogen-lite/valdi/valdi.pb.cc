// Generate by the Snap-modified protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: valdi.proto
// Protobuf C++ Version: 5.27.0

#include "valdi.pb.h"

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
namespace Valdi {

inline constexpr TsnConfig::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : enabled_modules_{},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR TsnConfig::TsnConfig(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct TsnConfigDefaultTypeInternal {
  PROTOBUF_CONSTEXPR TsnConfigDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~TsnConfigDefaultTypeInternal() {}
  union {
    TsnConfig _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 TsnConfigDefaultTypeInternal _TsnConfig_default_instance_;

inline constexpr NodeAttribute::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : name_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        str_value_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        int_value_{::int64_t{0}},
        double_value_{0},
        type_{static_cast< ::Valdi::NodeAttribute_Type >(0)},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR NodeAttribute::NodeAttribute(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct NodeAttributeDefaultTypeInternal {
  PROTOBUF_CONSTEXPR NodeAttributeDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~NodeAttributeDefaultTypeInternal() {}
  union {
    NodeAttribute _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 NodeAttributeDefaultTypeInternal _NodeAttribute_default_instance_;

inline constexpr DownloadableModuleArtifact::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : url_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        sha256digest_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR DownloadableModuleArtifact::DownloadableModuleArtifact(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct DownloadableModuleArtifactDefaultTypeInternal {
  PROTOBUF_CONSTEXPR DownloadableModuleArtifactDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~DownloadableModuleArtifactDefaultTypeInternal() {}
  union {
    DownloadableModuleArtifact _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 DownloadableModuleArtifactDefaultTypeInternal _DownloadableModuleArtifact_default_instance_;

inline constexpr DaemonAwakeMessage::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : service_addresses_{},
        device_ids_{},
        usernames_{},
        process_id_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        service_port_{0},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR DaemonAwakeMessage::DaemonAwakeMessage(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct DaemonAwakeMessageDefaultTypeInternal {
  PROTOBUF_CONSTEXPR DaemonAwakeMessageDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~DaemonAwakeMessageDefaultTypeInternal() {}
  union {
    DaemonAwakeMessage _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 DaemonAwakeMessageDefaultTypeInternal _DaemonAwakeMessage_default_instance_;

inline constexpr ClientAwakeMessage::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : device_id_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        username_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR ClientAwakeMessage::ClientAwakeMessage(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct ClientAwakeMessageDefaultTypeInternal {
  PROTOBUF_CONSTEXPR ClientAwakeMessageDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~ClientAwakeMessageDefaultTypeInternal() {}
  union {
    ClientAwakeMessage _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 ClientAwakeMessageDefaultTypeInternal _ClientAwakeMessage_default_instance_;

inline constexpr StyleDeclaration::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        attribute_{nullptr},
        priority_{0},
        order_{0},
        id_{0} {}

template <typename>
PROTOBUF_CONSTEXPR StyleDeclaration::StyleDeclaration(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct StyleDeclarationDefaultTypeInternal {
  PROTOBUF_CONSTEXPR StyleDeclarationDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~StyleDeclarationDefaultTypeInternal() {}
  union {
    StyleDeclaration _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 StyleDeclarationDefaultTypeInternal _StyleDeclaration_default_instance_;

inline constexpr DownloadableModuleAssets::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        artifact_{nullptr},
        device_density_{0} {}

template <typename>
PROTOBUF_CONSTEXPR DownloadableModuleAssets::DownloadableModuleAssets(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct DownloadableModuleAssetsDefaultTypeInternal {
  PROTOBUF_CONSTEXPR DownloadableModuleAssetsDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~DownloadableModuleAssetsDefaultTypeInternal() {}
  union {
    DownloadableModuleAssets _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 DownloadableModuleAssetsDefaultTypeInternal _DownloadableModuleAssets_default_instance_;

inline constexpr DaemonServiceDiscoveryPayload::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : message_{},
        _cached_size_{0},
        _oneof_case_{} {}

template <typename>
PROTOBUF_CONSTEXPR DaemonServiceDiscoveryPayload::DaemonServiceDiscoveryPayload(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct DaemonServiceDiscoveryPayloadDefaultTypeInternal {
  PROTOBUF_CONSTEXPR DaemonServiceDiscoveryPayloadDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~DaemonServiceDiscoveryPayloadDefaultTypeInternal() {}
  union {
    DaemonServiceDiscoveryPayload _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 DaemonServiceDiscoveryPayloadDefaultTypeInternal _DaemonServiceDiscoveryPayload_default_instance_;

inline constexpr DownloadableModuleManifest::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        assets_{},
        dependencies_{},
        name_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        artifact_{nullptr},
        has_local_assets_{false} {}

template <typename>
PROTOBUF_CONSTEXPR DownloadableModuleManifest::DownloadableModuleManifest(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct DownloadableModuleManifestDefaultTypeInternal {
  PROTOBUF_CONSTEXPR DownloadableModuleManifestDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~DownloadableModuleManifestDefaultTypeInternal() {}
  union {
    DownloadableModuleManifest _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 DownloadableModuleManifestDefaultTypeInternal _DownloadableModuleManifest_default_instance_;

inline constexpr CSSRuleIndex::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        id_rules_{},
        class_rules_{},
        tag_rules_{},
        attribute_rules_{},
        nth_child_rules_{},
        first_child_rule_{nullptr},
        last_child_rule_{nullptr},
        ancestor_rules_{nullptr},
        direct_parent_rules_{nullptr} {}

template <typename>
PROTOBUF_CONSTEXPR CSSRuleIndex::CSSRuleIndex(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct CSSRuleIndexDefaultTypeInternal {
  PROTOBUF_CONSTEXPR CSSRuleIndexDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~CSSRuleIndexDefaultTypeInternal() {}
  union {
    CSSRuleIndex _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 CSSRuleIndexDefaultTypeInternal _CSSRuleIndex_default_instance_;

inline constexpr CSSRuleIndex_AttributeRule::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        attribute_{nullptr},
        node_{nullptr},
        type_{static_cast< ::Valdi::CSSRuleIndex_AttributeRule_Type >(0)} {}

template <typename>
PROTOBUF_CONSTEXPR CSSRuleIndex_AttributeRule::CSSRuleIndex_AttributeRule(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct CSSRuleIndex_AttributeRuleDefaultTypeInternal {
  PROTOBUF_CONSTEXPR CSSRuleIndex_AttributeRuleDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~CSSRuleIndex_AttributeRuleDefaultTypeInternal() {}
  union {
    CSSRuleIndex_AttributeRule _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 CSSRuleIndex_AttributeRuleDefaultTypeInternal _CSSRuleIndex_AttributeRule_default_instance_;

inline constexpr CSSRuleIndex_NthChildRule::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        node_{nullptr},
        n_{0},
        offset_{0} {}

template <typename>
PROTOBUF_CONSTEXPR CSSRuleIndex_NthChildRule::CSSRuleIndex_NthChildRule(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct CSSRuleIndex_NthChildRuleDefaultTypeInternal {
  PROTOBUF_CONSTEXPR CSSRuleIndex_NthChildRuleDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~CSSRuleIndex_NthChildRuleDefaultTypeInternal() {}
  union {
    CSSRuleIndex_NthChildRule _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 CSSRuleIndex_NthChildRuleDefaultTypeInternal _CSSRuleIndex_NthChildRule_default_instance_;

inline constexpr NamedStyleNode::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        name_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        node_{nullptr} {}

template <typename>
PROTOBUF_CONSTEXPR NamedStyleNode::NamedStyleNode(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct NamedStyleNodeDefaultTypeInternal {
  PROTOBUF_CONSTEXPR NamedStyleNodeDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~NamedStyleNodeDefaultTypeInternal() {}
  union {
    NamedStyleNode _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 NamedStyleNodeDefaultTypeInternal _NamedStyleNode_default_instance_;

inline constexpr StyleNode::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        styles_{},
        ruleindex_{nullptr} {}

template <typename>
PROTOBUF_CONSTEXPR StyleNode::StyleNode(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct StyleNodeDefaultTypeInternal {
  PROTOBUF_CONSTEXPR StyleNodeDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~StyleNodeDefaultTypeInternal() {}
  union {
    StyleNode _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 StyleNodeDefaultTypeInternal _StyleNode_default_instance_;
}  // namespace Valdi
namespace Valdi {
PROTOBUF_CONSTINIT const uint32_t NodeAttribute_Type_internal_data_[] = {
    196608u, 0u, };
bool NodeAttribute_Type_IsValid(int value) {
  return 0 <= value && value <= 2;
}
static ::google::protobuf::internal::ExplicitlyConstructed<std::string>
    NodeAttribute_Type_strings[3] = {};

static const char NodeAttribute_Type_names[] = {
    "NODE_ATTRIBUTE_TYPE_DOUBLE"
    "NODE_ATTRIBUTE_TYPE_INT"
    "NODE_ATTRIBUTE_TYPE_STRING"
};

static const ::google::protobuf::internal::EnumEntry NodeAttribute_Type_entries[] =
    {
        {{&NodeAttribute_Type_names[0], 26}, 2},
        {{&NodeAttribute_Type_names[26], 23}, 1},
        {{&NodeAttribute_Type_names[49], 26}, 0},
};

static const int NodeAttribute_Type_entries_by_number[] = {
    2,  // 0 -> NODE_ATTRIBUTE_TYPE_STRING
    1,  // 1 -> NODE_ATTRIBUTE_TYPE_INT
    0,  // 2 -> NODE_ATTRIBUTE_TYPE_DOUBLE
};

const std::string& NodeAttribute_Type_Name(NodeAttribute_Type value) {
  static const bool kDummy =
      ::google::protobuf::internal::InitializeEnumStrings(
          NodeAttribute_Type_entries, NodeAttribute_Type_entries_by_number,
          3, NodeAttribute_Type_strings);
  (void)kDummy;

  int idx = ::google::protobuf::internal::LookUpEnumName(
      NodeAttribute_Type_entries, NodeAttribute_Type_entries_by_number, 3,
      value);
  return idx == -1 ? ::google::protobuf::internal::GetEmptyString()
                   : NodeAttribute_Type_strings[idx].get();
}

bool NodeAttribute_Type_Parse(absl::string_view name, NodeAttribute_Type* value) {
  int int_value;
  bool success = ::google::protobuf::internal::LookUpEnumValue(
      NodeAttribute_Type_entries, 3, name, &int_value);
  if (success) {
    *value = static_cast<NodeAttribute_Type>(int_value);
  }
  return success;
}
#if (__cplusplus < 201703) && \
  (!defined(_MSC_VER) || (_MSC_VER >= 1900 && _MSC_VER < 1912))

constexpr NodeAttribute_Type NodeAttribute::NODE_ATTRIBUTE_TYPE_STRING;
constexpr NodeAttribute_Type NodeAttribute::NODE_ATTRIBUTE_TYPE_INT;
constexpr NodeAttribute_Type NodeAttribute::NODE_ATTRIBUTE_TYPE_DOUBLE;
constexpr NodeAttribute_Type NodeAttribute::Type_MIN;
constexpr NodeAttribute_Type NodeAttribute::Type_MAX;
constexpr int NodeAttribute::Type_ARRAYSIZE;

#endif  // (__cplusplus < 201703) &&
        // (!defined(_MSC_VER) || (_MSC_VER >= 1900 && _MSC_VER < 1912))
PROTOBUF_CONSTINIT const uint32_t CSSRuleIndex_AttributeRule_Type_internal_data_[] = {
    458752u, 0u, };
bool CSSRuleIndex_AttributeRule_Type_IsValid(int value) {
  return 0 <= value && value <= 6;
}
static ::google::protobuf::internal::ExplicitlyConstructed<std::string>
    CSSRuleIndex_AttributeRule_Type_strings[7] = {};

static const char CSSRuleIndex_AttributeRule_Type_names[] = {
    "CONTAINS_SUBSTRING"
    "CONTAINS_WORD"
    "ENDS_WITH_SUBSTRING"
    "EQUALS"
    "HAS_ATTRIBUTE"
    "STARTS_WITH_SUBSTRING"
    "STARTS_WITH_WORD"
};

static const ::google::protobuf::internal::EnumEntry CSSRuleIndex_AttributeRule_Type_entries[] =
    {
        {{&CSSRuleIndex_AttributeRule_Type_names[0], 18}, 5},
        {{&CSSRuleIndex_AttributeRule_Type_names[18], 13}, 1},
        {{&CSSRuleIndex_AttributeRule_Type_names[31], 19}, 4},
        {{&CSSRuleIndex_AttributeRule_Type_names[50], 6}, 0},
        {{&CSSRuleIndex_AttributeRule_Type_names[56], 13}, 6},
        {{&CSSRuleIndex_AttributeRule_Type_names[69], 21}, 3},
        {{&CSSRuleIndex_AttributeRule_Type_names[90], 16}, 2},
};

static const int CSSRuleIndex_AttributeRule_Type_entries_by_number[] = {
    3,  // 0 -> EQUALS
    1,  // 1 -> CONTAINS_WORD
    6,  // 2 -> STARTS_WITH_WORD
    5,  // 3 -> STARTS_WITH_SUBSTRING
    2,  // 4 -> ENDS_WITH_SUBSTRING
    0,  // 5 -> CONTAINS_SUBSTRING
    4,  // 6 -> HAS_ATTRIBUTE
};

const std::string& CSSRuleIndex_AttributeRule_Type_Name(CSSRuleIndex_AttributeRule_Type value) {
  static const bool kDummy =
      ::google::protobuf::internal::InitializeEnumStrings(
          CSSRuleIndex_AttributeRule_Type_entries, CSSRuleIndex_AttributeRule_Type_entries_by_number,
          7, CSSRuleIndex_AttributeRule_Type_strings);
  (void)kDummy;

  int idx = ::google::protobuf::internal::LookUpEnumName(
      CSSRuleIndex_AttributeRule_Type_entries, CSSRuleIndex_AttributeRule_Type_entries_by_number, 7,
      value);
  return idx == -1 ? ::google::protobuf::internal::GetEmptyString()
                   : CSSRuleIndex_AttributeRule_Type_strings[idx].get();
}

bool CSSRuleIndex_AttributeRule_Type_Parse(absl::string_view name, CSSRuleIndex_AttributeRule_Type* value) {
  int int_value;
  bool success = ::google::protobuf::internal::LookUpEnumValue(
      CSSRuleIndex_AttributeRule_Type_entries, 7, name, &int_value);
  if (success) {
    *value = static_cast<CSSRuleIndex_AttributeRule_Type>(int_value);
  }
  return success;
}
#if (__cplusplus < 201703) && \
  (!defined(_MSC_VER) || (_MSC_VER >= 1900 && _MSC_VER < 1912))

constexpr CSSRuleIndex_AttributeRule_Type CSSRuleIndex_AttributeRule::EQUALS;
constexpr CSSRuleIndex_AttributeRule_Type CSSRuleIndex_AttributeRule::CONTAINS_WORD;
constexpr CSSRuleIndex_AttributeRule_Type CSSRuleIndex_AttributeRule::STARTS_WITH_WORD;
constexpr CSSRuleIndex_AttributeRule_Type CSSRuleIndex_AttributeRule::STARTS_WITH_SUBSTRING;
constexpr CSSRuleIndex_AttributeRule_Type CSSRuleIndex_AttributeRule::ENDS_WITH_SUBSTRING;
constexpr CSSRuleIndex_AttributeRule_Type CSSRuleIndex_AttributeRule::CONTAINS_SUBSTRING;
constexpr CSSRuleIndex_AttributeRule_Type CSSRuleIndex_AttributeRule::HAS_ATTRIBUTE;
constexpr CSSRuleIndex_AttributeRule_Type CSSRuleIndex_AttributeRule::Type_MIN;
constexpr CSSRuleIndex_AttributeRule_Type CSSRuleIndex_AttributeRule::Type_MAX;
constexpr int CSSRuleIndex_AttributeRule::Type_ARRAYSIZE;

#endif  // (__cplusplus < 201703) &&
        // (!defined(_MSC_VER) || (_MSC_VER >= 1900 && _MSC_VER < 1912))
// ===================================================================

class NodeAttribute::_Internal {
 public:
};

NodeAttribute::NodeAttribute(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:Valdi.NodeAttribute)
}
inline PROTOBUF_NDEBUG_INLINE NodeAttribute::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::Valdi::NodeAttribute& from_msg)
      : name_(arena, from.name_),
        str_value_(arena, from.str_value_),
        _cached_size_{0} {}

NodeAttribute::NodeAttribute(
    ::google::protobuf::Arena* arena,
    const NodeAttribute& from)
    : ::google::protobuf::MessageLite(arena) {
  NodeAttribute* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  ::memcpy(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, int_value_),
           reinterpret_cast<const char *>(&from._impl_) +
               offsetof(Impl_, int_value_),
           offsetof(Impl_, type_) -
               offsetof(Impl_, int_value_) +
               sizeof(Impl_::type_));

  // @@protoc_insertion_point(copy_constructor:Valdi.NodeAttribute)
}
inline PROTOBUF_NDEBUG_INLINE NodeAttribute::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : name_(arena),
        str_value_(arena),
        _cached_size_{0} {}

inline void NodeAttribute::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, int_value_),
           0,
           offsetof(Impl_, type_) -
               offsetof(Impl_, int_value_) +
               sizeof(Impl_::type_));
}
NodeAttribute::~NodeAttribute() {
  // @@protoc_insertion_point(destructor:Valdi.NodeAttribute)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void NodeAttribute::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.name_.Destroy();
  _impl_.str_value_.Destroy();
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
NodeAttribute::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<20> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(NodeAttribute, _impl_._cached_size_),
              true,
          },
          "Valdi.NodeAttribute",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<3, 5, 0, 41, 2> NodeAttribute::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    5, 56,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967264,  // skipmap
    offsetof(decltype(_table_), field_entries),
    5,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    &_NodeAttribute_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::Valdi::NodeAttribute>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // .Valdi.NodeAttribute.Type type = 1;
    {::_pbi::TcParser::FastV32S1,
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(NodeAttribute, _impl_.type_)}},
    // string name = 2;
    {::_pbi::TcParser::FastUS1,
     {18, 63, 0, PROTOBUF_FIELD_OFFSET(NodeAttribute, _impl_.name_)}},
    // string str_value = 3;
    {::_pbi::TcParser::FastUS1,
     {26, 63, 0, PROTOBUF_FIELD_OFFSET(NodeAttribute, _impl_.str_value_)}},
    // int64 int_value = 4;
    {::_pbi::TcParser::FastV64S1,
     {32, 63, 0, PROTOBUF_FIELD_OFFSET(NodeAttribute, _impl_.int_value_)}},
    // double double_value = 5;
    {::_pbi::TcParser::FastF64S1,
     {41, 63, 0, PROTOBUF_FIELD_OFFSET(NodeAttribute, _impl_.double_value_)}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
  }}, {{
    65535, 65535
  }}, {{
    // .Valdi.NodeAttribute.Type type = 1;
    {PROTOBUF_FIELD_OFFSET(NodeAttribute, _impl_.type_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kOpenEnum)},
    // string name = 2;
    {PROTOBUF_FIELD_OFFSET(NodeAttribute, _impl_.name_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // string str_value = 3;
    {PROTOBUF_FIELD_OFFSET(NodeAttribute, _impl_.str_value_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // int64 int_value = 4;
    {PROTOBUF_FIELD_OFFSET(NodeAttribute, _impl_.int_value_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kInt64)},
    // double double_value = 5;
    {PROTOBUF_FIELD_OFFSET(NodeAttribute, _impl_.double_value_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kDouble)},
  }},
  // no aux_entries
  {{
    "\23\0\4\11\0\0\0\0"
    "Valdi.NodeAttribute"
    "name"
    "str_value"
  }},
};

PROTOBUF_NOINLINE void NodeAttribute::Clear() {
// @@protoc_insertion_point(message_clear_start:Valdi.NodeAttribute)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.name_.ClearToEmpty();
  _impl_.str_value_.ClearToEmpty();
  ::memset(&_impl_.int_value_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.type_) -
      reinterpret_cast<char*>(&_impl_.int_value_)) + sizeof(_impl_.type_));
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* NodeAttribute::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:Valdi.NodeAttribute)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // .Valdi.NodeAttribute.Type type = 1;
  if (this->_internal_type() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteEnumToArray(
        1, this->_internal_type(), target);
  }

  // string name = 2;
  if (!this->_internal_name().empty()) {
    const std::string& _s = this->_internal_name();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "Valdi.NodeAttribute.name");
    target = stream->WriteStringMaybeAliased(2, _s, target);
  }

  // string str_value = 3;
  if (!this->_internal_str_value().empty()) {
    const std::string& _s = this->_internal_str_value();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "Valdi.NodeAttribute.str_value");
    target = stream->WriteStringMaybeAliased(3, _s, target);
  }

  // int64 int_value = 4;
  if (this->_internal_int_value() != 0) {
    target = ::google::protobuf::internal::WireFormatLite::
        WriteInt64ToArrayWithField<4>(
            stream, this->_internal_int_value(), target);
  }

  // double double_value = 5;
  static_assert(sizeof(::uint64_t) == sizeof(double),
                "Code assumes ::uint64_t and double are the same size.");
  double tmp_double_value = this->_internal_double_value();
  ::uint64_t raw_double_value;
  memcpy(&raw_double_value, &tmp_double_value, sizeof(tmp_double_value));
  if (raw_double_value != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteDoubleToArray(
        5, this->_internal_double_value(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:Valdi.NodeAttribute)
  return target;
}

::size_t NodeAttribute::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:Valdi.NodeAttribute)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // string name = 2;
  if (!this->_internal_name().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_name());
  }

  // string str_value = 3;
  if (!this->_internal_str_value().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_str_value());
  }

  // int64 int_value = 4;
  if (this->_internal_int_value() != 0) {
    total_size += ::_pbi::WireFormatLite::Int64SizePlusOne(
        this->_internal_int_value());
  }

  // double double_value = 5;
  static_assert(sizeof(::uint64_t) == sizeof(double),
                "Code assumes ::uint64_t and double are the same size.");
  double tmp_double_value = this->_internal_double_value();
  ::uint64_t raw_double_value;
  memcpy(&raw_double_value, &tmp_double_value, sizeof(tmp_double_value));
  if (raw_double_value != 0) {
    total_size += 9;
  }

  // .Valdi.NodeAttribute.Type type = 1;
  if (this->_internal_type() != 0) {
    total_size += 1 +
                  ::_pbi::WireFormatLite::EnumSize(this->_internal_type());
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void NodeAttribute::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const NodeAttribute*>(
      &from));
}

void NodeAttribute::MergeFrom(const NodeAttribute& from) {
  NodeAttribute* const _this = this;
  // @@protoc_insertion_point(class_specific_merge_from_start:Valdi.NodeAttribute)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_name().empty()) {
    _this->_internal_set_name(from._internal_name());
  }
  if (!from._internal_str_value().empty()) {
    _this->_internal_set_str_value(from._internal_str_value());
  }
  if (from._internal_int_value() != 0) {
    _this->_impl_.int_value_ = from._impl_.int_value_;
  }
  static_assert(sizeof(::uint64_t) == sizeof(double),
                "Code assumes ::uint64_t and double are the same size.");
  double tmp_double_value = from._internal_double_value();
  ::uint64_t raw_double_value;
  memcpy(&raw_double_value, &tmp_double_value, sizeof(tmp_double_value));
  if (raw_double_value != 0) {
    _this->_impl_.double_value_ = from._impl_.double_value_;
  }
  if (from._internal_type() != 0) {
    _this->_impl_.type_ = from._impl_.type_;
  }
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void NodeAttribute::CopyFrom(const NodeAttribute& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Valdi.NodeAttribute)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void NodeAttribute::InternalSwap(NodeAttribute* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.name_, &other->_impl_.name_, arena);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.str_value_, &other->_impl_.str_value_, arena);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(NodeAttribute, _impl_.type_)
      + sizeof(NodeAttribute::_impl_.type_)
      - PROTOBUF_FIELD_OFFSET(NodeAttribute, _impl_.int_value_)>(
          reinterpret_cast<char*>(&_impl_.int_value_),
          reinterpret_cast<char*>(&other->_impl_.int_value_));
}

// ===================================================================

class DownloadableModuleArtifact::_Internal {
 public:
};

DownloadableModuleArtifact::DownloadableModuleArtifact(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:Valdi.DownloadableModuleArtifact)
}
inline PROTOBUF_NDEBUG_INLINE DownloadableModuleArtifact::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::Valdi::DownloadableModuleArtifact& from_msg)
      : url_(arena, from.url_),
        sha256digest_(arena, from.sha256digest_),
        _cached_size_{0} {}

DownloadableModuleArtifact::DownloadableModuleArtifact(
    ::google::protobuf::Arena* arena,
    const DownloadableModuleArtifact& from)
    : ::google::protobuf::MessageLite(arena) {
  DownloadableModuleArtifact* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:Valdi.DownloadableModuleArtifact)
}
inline PROTOBUF_NDEBUG_INLINE DownloadableModuleArtifact::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : url_(arena),
        sha256digest_(arena),
        _cached_size_{0} {}

inline void DownloadableModuleArtifact::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
DownloadableModuleArtifact::~DownloadableModuleArtifact() {
  // @@protoc_insertion_point(destructor:Valdi.DownloadableModuleArtifact)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void DownloadableModuleArtifact::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.url_.Destroy();
  _impl_.sha256digest_.Destroy();
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
DownloadableModuleArtifact::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<33> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(DownloadableModuleArtifact, _impl_._cached_size_),
              true,
          },
          "Valdi.DownloadableModuleArtifact",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 0, 44, 2> DownloadableModuleArtifact::_table_ = {
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
    &_DownloadableModuleArtifact_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::Valdi::DownloadableModuleArtifact>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // bytes sha256Digest = 2;
    {::_pbi::TcParser::FastBS1,
     {18, 63, 0, PROTOBUF_FIELD_OFFSET(DownloadableModuleArtifact, _impl_.sha256digest_)}},
    // string url = 1;
    {::_pbi::TcParser::FastUS1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(DownloadableModuleArtifact, _impl_.url_)}},
  }}, {{
    65535, 65535
  }}, {{
    // string url = 1;
    {PROTOBUF_FIELD_OFFSET(DownloadableModuleArtifact, _impl_.url_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // bytes sha256Digest = 2;
    {PROTOBUF_FIELD_OFFSET(DownloadableModuleArtifact, _impl_.sha256digest_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kBytes | ::_fl::kRepAString)},
  }},
  // no aux_entries
  {{
    "\40\3\0\0\0\0\0\0"
    "Valdi.DownloadableModuleArtifact"
    "url"
  }},
};

PROTOBUF_NOINLINE void DownloadableModuleArtifact::Clear() {
// @@protoc_insertion_point(message_clear_start:Valdi.DownloadableModuleArtifact)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.url_.ClearToEmpty();
  _impl_.sha256digest_.ClearToEmpty();
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* DownloadableModuleArtifact::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:Valdi.DownloadableModuleArtifact)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // string url = 1;
  if (!this->_internal_url().empty()) {
    const std::string& _s = this->_internal_url();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "Valdi.DownloadableModuleArtifact.url");
    target = stream->WriteStringMaybeAliased(1, _s, target);
  }

  // bytes sha256Digest = 2;
  if (!this->_internal_sha256digest().empty()) {
    const std::string& _s = this->_internal_sha256digest();
    target = stream->WriteBytesMaybeAliased(2, _s, target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:Valdi.DownloadableModuleArtifact)
  return target;
}

::size_t DownloadableModuleArtifact::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:Valdi.DownloadableModuleArtifact)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // string url = 1;
  if (!this->_internal_url().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_url());
  }

  // bytes sha256Digest = 2;
  if (!this->_internal_sha256digest().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::BytesSize(
                                    this->_internal_sha256digest());
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void DownloadableModuleArtifact::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const DownloadableModuleArtifact*>(
      &from));
}

void DownloadableModuleArtifact::MergeFrom(const DownloadableModuleArtifact& from) {
  DownloadableModuleArtifact* const _this = this;
  // @@protoc_insertion_point(class_specific_merge_from_start:Valdi.DownloadableModuleArtifact)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_url().empty()) {
    _this->_internal_set_url(from._internal_url());
  }
  if (!from._internal_sha256digest().empty()) {
    _this->_internal_set_sha256digest(from._internal_sha256digest());
  }
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void DownloadableModuleArtifact::CopyFrom(const DownloadableModuleArtifact& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Valdi.DownloadableModuleArtifact)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void DownloadableModuleArtifact::InternalSwap(DownloadableModuleArtifact* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.url_, &other->_impl_.url_, arena);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.sha256digest_, &other->_impl_.sha256digest_, arena);
}

// ===================================================================

class DownloadableModuleAssets::_Internal {
 public:
  using HasBits =
      decltype(std::declval<DownloadableModuleAssets>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
      8 * PROTOBUF_FIELD_OFFSET(DownloadableModuleAssets, _impl_._has_bits_);
};

DownloadableModuleAssets::DownloadableModuleAssets(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:Valdi.DownloadableModuleAssets)
}
inline PROTOBUF_NDEBUG_INLINE DownloadableModuleAssets::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::Valdi::DownloadableModuleAssets& from_msg)
      : _has_bits_{from._has_bits_},
        _cached_size_{0} {}

DownloadableModuleAssets::DownloadableModuleAssets(
    ::google::protobuf::Arena* arena,
    const DownloadableModuleAssets& from)
    : ::google::protobuf::MessageLite(arena) {
  DownloadableModuleAssets* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  ::uint32_t cached_has_bits = _impl_._has_bits_[0];
  _impl_.artifact_ = (cached_has_bits & 0x00000001u) ? ::google::protobuf::MessageLite::CopyConstruct<::Valdi::DownloadableModuleArtifact>(
                              arena, *from._impl_.artifact_)
                        : nullptr;
  _impl_.device_density_ = from._impl_.device_density_;

  // @@protoc_insertion_point(copy_constructor:Valdi.DownloadableModuleAssets)
}
inline PROTOBUF_NDEBUG_INLINE DownloadableModuleAssets::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void DownloadableModuleAssets::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, artifact_),
           0,
           offsetof(Impl_, device_density_) -
               offsetof(Impl_, artifact_) +
               sizeof(Impl_::device_density_));
}
DownloadableModuleAssets::~DownloadableModuleAssets() {
  // @@protoc_insertion_point(destructor:Valdi.DownloadableModuleAssets)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void DownloadableModuleAssets::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  delete _impl_.artifact_;
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
DownloadableModuleAssets::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<31> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(DownloadableModuleAssets, _impl_._cached_size_),
              true,
          },
          "Valdi.DownloadableModuleAssets",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 2, 1, 0, 2> DownloadableModuleAssets::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(DownloadableModuleAssets, _impl_._has_bits_),
    0, // no _extensions_
    3, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967290,  // skipmap
    offsetof(decltype(_table_), field_entries),
    2,  // num_field_entries
    1,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_DownloadableModuleAssets_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::Valdi::DownloadableModuleAssets>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // .Valdi.DownloadableModuleArtifact artifact = 1;
    {::_pbi::TcParser::FastMtS1,
     {10, 0, 0, PROTOBUF_FIELD_OFFSET(DownloadableModuleAssets, _impl_.artifact_)}},
    {::_pbi::TcParser::MiniParse, {}},
    // double device_density = 3;
    {::_pbi::TcParser::FastF64S1,
     {25, 63, 0, PROTOBUF_FIELD_OFFSET(DownloadableModuleAssets, _impl_.device_density_)}},
  }}, {{
    65535, 65535
  }}, {{
    // .Valdi.DownloadableModuleArtifact artifact = 1;
    {PROTOBUF_FIELD_OFFSET(DownloadableModuleAssets, _impl_.artifact_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
    // double device_density = 3;
    {PROTOBUF_FIELD_OFFSET(DownloadableModuleAssets, _impl_.device_density_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kDouble)},
  }}, {{
    {::_pbi::TcParser::GetTable<::Valdi::DownloadableModuleArtifact>()},
  }}, {{
  }},
};

PROTOBUF_NOINLINE void DownloadableModuleAssets::Clear() {
// @@protoc_insertion_point(message_clear_start:Valdi.DownloadableModuleAssets)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(_impl_.artifact_ != nullptr);
    _impl_.artifact_->Clear();
  }
  _impl_.device_density_ = 0;
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* DownloadableModuleAssets::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:Valdi.DownloadableModuleAssets)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  cached_has_bits = _impl_._has_bits_[0];
  // .Valdi.DownloadableModuleArtifact artifact = 1;
  if (cached_has_bits & 0x00000001u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        1, *_impl_.artifact_, _impl_.artifact_->GetCachedSize(), target, stream);
  }

  // double device_density = 3;
  static_assert(sizeof(::uint64_t) == sizeof(double),
                "Code assumes ::uint64_t and double are the same size.");
  double tmp_device_density = this->_internal_device_density();
  ::uint64_t raw_device_density;
  memcpy(&raw_device_density, &tmp_device_density, sizeof(tmp_device_density));
  if (raw_device_density != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteDoubleToArray(
        3, this->_internal_device_density(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:Valdi.DownloadableModuleAssets)
  return target;
}

::size_t DownloadableModuleAssets::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:Valdi.DownloadableModuleAssets)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // .Valdi.DownloadableModuleArtifact artifact = 1;
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    total_size +=
        1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.artifact_);
  }

  // double device_density = 3;
  static_assert(sizeof(::uint64_t) == sizeof(double),
                "Code assumes ::uint64_t and double are the same size.");
  double tmp_device_density = this->_internal_device_density();
  ::uint64_t raw_device_density;
  memcpy(&raw_device_density, &tmp_device_density, sizeof(tmp_device_density));
  if (raw_device_density != 0) {
    total_size += 9;
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void DownloadableModuleAssets::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const DownloadableModuleAssets*>(
      &from));
}

void DownloadableModuleAssets::MergeFrom(const DownloadableModuleAssets& from) {
  DownloadableModuleAssets* const _this = this;
  ::google::protobuf::Arena* arena = _this->GetArena();
  // @@protoc_insertion_point(class_specific_merge_from_start:Valdi.DownloadableModuleAssets)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  cached_has_bits = from._impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(from._impl_.artifact_ != nullptr);
    if (_this->_impl_.artifact_ == nullptr) {
      _this->_impl_.artifact_ =
          ::google::protobuf::MessageLite::CopyConstruct<::Valdi::DownloadableModuleArtifact>(arena, *from._impl_.artifact_);
    } else {
      _this->_impl_.artifact_->MergeFrom(*from._impl_.artifact_);
    }
  }
  static_assert(sizeof(::uint64_t) == sizeof(double),
                "Code assumes ::uint64_t and double are the same size.");
  double tmp_device_density = from._internal_device_density();
  ::uint64_t raw_device_density;
  memcpy(&raw_device_density, &tmp_device_density, sizeof(tmp_device_density));
  if (raw_device_density != 0) {
    _this->_impl_.device_density_ = from._impl_.device_density_;
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void DownloadableModuleAssets::CopyFrom(const DownloadableModuleAssets& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Valdi.DownloadableModuleAssets)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void DownloadableModuleAssets::InternalSwap(DownloadableModuleAssets* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(DownloadableModuleAssets, _impl_.device_density_)
      + sizeof(DownloadableModuleAssets::_impl_.device_density_)
      - PROTOBUF_FIELD_OFFSET(DownloadableModuleAssets, _impl_.artifact_)>(
          reinterpret_cast<char*>(&_impl_.artifact_),
          reinterpret_cast<char*>(&other->_impl_.artifact_));
}

// ===================================================================

class DownloadableModuleManifest::_Internal {
 public:
  using HasBits =
      decltype(std::declval<DownloadableModuleManifest>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
      8 * PROTOBUF_FIELD_OFFSET(DownloadableModuleManifest, _impl_._has_bits_);
};

DownloadableModuleManifest::DownloadableModuleManifest(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:Valdi.DownloadableModuleManifest)
}
inline PROTOBUF_NDEBUG_INLINE DownloadableModuleManifest::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::Valdi::DownloadableModuleManifest& from_msg)
      : _has_bits_{from._has_bits_},
        _cached_size_{0},
        assets_{visibility, arena, from.assets_},
        dependencies_{visibility, arena, from.dependencies_},
        name_(arena, from.name_) {}

DownloadableModuleManifest::DownloadableModuleManifest(
    ::google::protobuf::Arena* arena,
    const DownloadableModuleManifest& from)
    : ::google::protobuf::MessageLite(arena) {
  DownloadableModuleManifest* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  ::uint32_t cached_has_bits = _impl_._has_bits_[0];
  _impl_.artifact_ = (cached_has_bits & 0x00000001u) ? ::google::protobuf::MessageLite::CopyConstruct<::Valdi::DownloadableModuleArtifact>(
                              arena, *from._impl_.artifact_)
                        : nullptr;
  _impl_.has_local_assets_ = from._impl_.has_local_assets_;

  // @@protoc_insertion_point(copy_constructor:Valdi.DownloadableModuleManifest)
}
inline PROTOBUF_NDEBUG_INLINE DownloadableModuleManifest::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0},
        assets_{visibility, arena},
        dependencies_{visibility, arena},
        name_(arena) {}

inline void DownloadableModuleManifest::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, artifact_),
           0,
           offsetof(Impl_, has_local_assets_) -
               offsetof(Impl_, artifact_) +
               sizeof(Impl_::has_local_assets_));
}
DownloadableModuleManifest::~DownloadableModuleManifest() {
  // @@protoc_insertion_point(destructor:Valdi.DownloadableModuleManifest)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void DownloadableModuleManifest::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.name_.Destroy();
  delete _impl_.artifact_;
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
DownloadableModuleManifest::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<33> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(DownloadableModuleManifest, _impl_._cached_size_),
              true,
          },
          "Valdi.DownloadableModuleManifest",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<3, 5, 2, 57, 2> DownloadableModuleManifest::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(DownloadableModuleManifest, _impl_._has_bits_),
    0, // no _extensions_
    5, 56,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967264,  // skipmap
    offsetof(decltype(_table_), field_entries),
    5,  // num_field_entries
    2,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_DownloadableModuleManifest_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::Valdi::DownloadableModuleManifest>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // string name = 1;
    {::_pbi::TcParser::FastUS1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(DownloadableModuleManifest, _impl_.name_)}},
    // .Valdi.DownloadableModuleArtifact artifact = 2;
    {::_pbi::TcParser::FastMtS1,
     {18, 0, 0, PROTOBUF_FIELD_OFFSET(DownloadableModuleManifest, _impl_.artifact_)}},
    // repeated .Valdi.DownloadableModuleAssets assets = 3;
    {::_pbi::TcParser::FastMtR1,
     {26, 63, 1, PROTOBUF_FIELD_OFFSET(DownloadableModuleManifest, _impl_.assets_)}},
    // repeated string dependencies = 4;
    {::_pbi::TcParser::FastUR1,
     {34, 63, 0, PROTOBUF_FIELD_OFFSET(DownloadableModuleManifest, _impl_.dependencies_)}},
    // bool has_local_assets = 5;
    {::_pbi::TcParser::FastV8S1,
     {40, 63, 0, PROTOBUF_FIELD_OFFSET(DownloadableModuleManifest, _impl_.has_local_assets_)}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
  }}, {{
    65535, 65535
  }}, {{
    // string name = 1;
    {PROTOBUF_FIELD_OFFSET(DownloadableModuleManifest, _impl_.name_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // .Valdi.DownloadableModuleArtifact artifact = 2;
    {PROTOBUF_FIELD_OFFSET(DownloadableModuleManifest, _impl_.artifact_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
    // repeated .Valdi.DownloadableModuleAssets assets = 3;
    {PROTOBUF_FIELD_OFFSET(DownloadableModuleManifest, _impl_.assets_), -1, 1,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
    // repeated string dependencies = 4;
    {PROTOBUF_FIELD_OFFSET(DownloadableModuleManifest, _impl_.dependencies_), -1, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kUtf8String | ::_fl::kRepSString)},
    // bool has_local_assets = 5;
    {PROTOBUF_FIELD_OFFSET(DownloadableModuleManifest, _impl_.has_local_assets_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kBool)},
  }}, {{
    {::_pbi::TcParser::GetTable<::Valdi::DownloadableModuleArtifact>()},
    {::_pbi::TcParser::GetTable<::Valdi::DownloadableModuleAssets>()},
  }}, {{
    "\40\4\0\0\14\0\0\0"
    "Valdi.DownloadableModuleManifest"
    "name"
    "dependencies"
  }},
};

PROTOBUF_NOINLINE void DownloadableModuleManifest::Clear() {
// @@protoc_insertion_point(message_clear_start:Valdi.DownloadableModuleManifest)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.assets_.Clear();
  _impl_.dependencies_.Clear();
  _impl_.name_.ClearToEmpty();
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(_impl_.artifact_ != nullptr);
    _impl_.artifact_->Clear();
  }
  _impl_.has_local_assets_ = false;
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* DownloadableModuleManifest::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:Valdi.DownloadableModuleManifest)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // string name = 1;
  if (!this->_internal_name().empty()) {
    const std::string& _s = this->_internal_name();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "Valdi.DownloadableModuleManifest.name");
    target = stream->WriteStringMaybeAliased(1, _s, target);
  }

  cached_has_bits = _impl_._has_bits_[0];
  // .Valdi.DownloadableModuleArtifact artifact = 2;
  if (cached_has_bits & 0x00000001u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        2, *_impl_.artifact_, _impl_.artifact_->GetCachedSize(), target, stream);
  }

  // repeated .Valdi.DownloadableModuleAssets assets = 3;
  for (unsigned i = 0, n = static_cast<unsigned>(
                           this->_internal_assets_size());
       i < n; i++) {
    const auto& repfield = this->_internal_assets().Get(i);
    target =
        ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
            3, repfield, repfield.GetCachedSize(),
            target, stream);
  }

  // repeated string dependencies = 4;
  for (int i = 0, n = this->_internal_dependencies_size(); i < n; ++i) {
    const auto& s = this->_internal_dependencies().Get(i);
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        s.data(), static_cast<int>(s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "Valdi.DownloadableModuleManifest.dependencies");
    target = stream->WriteString(4, s, target);
  }

  // bool has_local_assets = 5;
  if (this->_internal_has_local_assets() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteBoolToArray(
        5, this->_internal_has_local_assets(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:Valdi.DownloadableModuleManifest)
  return target;
}

::size_t DownloadableModuleManifest::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:Valdi.DownloadableModuleManifest)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // repeated .Valdi.DownloadableModuleAssets assets = 3;
  total_size += 1UL * this->_internal_assets_size();
  for (const auto& msg : this->_internal_assets()) {
    total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  // repeated string dependencies = 4;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_dependencies().size());
  for (int i = 0, n = _internal_dependencies().size(); i < n; ++i) {
    total_size += ::google::protobuf::internal::WireFormatLite::StringSize(
        _internal_dependencies().Get(i));
  }
  // string name = 1;
  if (!this->_internal_name().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_name());
  }

  // .Valdi.DownloadableModuleArtifact artifact = 2;
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    total_size +=
        1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.artifact_);
  }

  // bool has_local_assets = 5;
  if (this->_internal_has_local_assets() != 0) {
    total_size += 2;
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void DownloadableModuleManifest::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const DownloadableModuleManifest*>(
      &from));
}

void DownloadableModuleManifest::MergeFrom(const DownloadableModuleManifest& from) {
  DownloadableModuleManifest* const _this = this;
  ::google::protobuf::Arena* arena = _this->GetArena();
  // @@protoc_insertion_point(class_specific_merge_from_start:Valdi.DownloadableModuleManifest)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_assets()->MergeFrom(
      from._internal_assets());
  _this->_internal_mutable_dependencies()->MergeFrom(from._internal_dependencies());
  if (!from._internal_name().empty()) {
    _this->_internal_set_name(from._internal_name());
  }
  cached_has_bits = from._impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(from._impl_.artifact_ != nullptr);
    if (_this->_impl_.artifact_ == nullptr) {
      _this->_impl_.artifact_ =
          ::google::protobuf::MessageLite::CopyConstruct<::Valdi::DownloadableModuleArtifact>(arena, *from._impl_.artifact_);
    } else {
      _this->_impl_.artifact_->MergeFrom(*from._impl_.artifact_);
    }
  }
  if (from._internal_has_local_assets() != 0) {
    _this->_impl_.has_local_assets_ = from._impl_.has_local_assets_;
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void DownloadableModuleManifest::CopyFrom(const DownloadableModuleManifest& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Valdi.DownloadableModuleManifest)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void DownloadableModuleManifest::InternalSwap(DownloadableModuleManifest* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  _impl_.assets_.InternalSwap(&other->_impl_.assets_);
  _impl_.dependencies_.InternalSwap(&other->_impl_.dependencies_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.name_, &other->_impl_.name_, arena);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(DownloadableModuleManifest, _impl_.has_local_assets_)
      + sizeof(DownloadableModuleManifest::_impl_.has_local_assets_)
      - PROTOBUF_FIELD_OFFSET(DownloadableModuleManifest, _impl_.artifact_)>(
          reinterpret_cast<char*>(&_impl_.artifact_),
          reinterpret_cast<char*>(&other->_impl_.artifact_));
}

// ===================================================================

class DaemonAwakeMessage::_Internal {
 public:
};

DaemonAwakeMessage::DaemonAwakeMessage(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:Valdi.DaemonAwakeMessage)
}
inline PROTOBUF_NDEBUG_INLINE DaemonAwakeMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::Valdi::DaemonAwakeMessage& from_msg)
      : service_addresses_{visibility, arena, from.service_addresses_},
        device_ids_{visibility, arena, from.device_ids_},
        usernames_{visibility, arena, from.usernames_},
        process_id_(arena, from.process_id_),
        _cached_size_{0} {}

DaemonAwakeMessage::DaemonAwakeMessage(
    ::google::protobuf::Arena* arena,
    const DaemonAwakeMessage& from)
    : ::google::protobuf::MessageLite(arena) {
  DaemonAwakeMessage* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  _impl_.service_port_ = from._impl_.service_port_;

  // @@protoc_insertion_point(copy_constructor:Valdi.DaemonAwakeMessage)
}
inline PROTOBUF_NDEBUG_INLINE DaemonAwakeMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : service_addresses_{visibility, arena},
        device_ids_{visibility, arena},
        usernames_{visibility, arena},
        process_id_(arena),
        _cached_size_{0} {}

inline void DaemonAwakeMessage::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.service_port_ = {};
}
DaemonAwakeMessage::~DaemonAwakeMessage() {
  // @@protoc_insertion_point(destructor:Valdi.DaemonAwakeMessage)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void DaemonAwakeMessage::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.process_id_.Destroy();
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
DaemonAwakeMessage::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<25> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(DaemonAwakeMessage, _impl_._cached_size_),
              true,
          },
          "Valdi.DaemonAwakeMessage",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 5, 0, 79, 2> DaemonAwakeMessage::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    17, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294868984,  // skipmap
    offsetof(decltype(_table_), field_entries),
    5,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    &_DaemonAwakeMessage_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::Valdi::DaemonAwakeMessage>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // repeated string device_ids = 16;
    {::_pbi::TcParser::FastUR2,
     {386, 63, 0, PROTOBUF_FIELD_OFFSET(DaemonAwakeMessage, _impl_.device_ids_)}},
    // repeated string service_addresses = 1;
    {::_pbi::TcParser::FastUR1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(DaemonAwakeMessage, _impl_.service_addresses_)}},
    // int32 service_port = 2;
    {::_pbi::TcParser::FastV32S1,
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(DaemonAwakeMessage, _impl_.service_port_)}},
    // string process_id = 3;
    {::_pbi::TcParser::FastUS1,
     {26, 63, 0, PROTOBUF_FIELD_OFFSET(DaemonAwakeMessage, _impl_.process_id_)}},
  }}, {{
    65535, 65535
  }}, {{
    // repeated string service_addresses = 1;
    {PROTOBUF_FIELD_OFFSET(DaemonAwakeMessage, _impl_.service_addresses_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kUtf8String | ::_fl::kRepSString)},
    // int32 service_port = 2;
    {PROTOBUF_FIELD_OFFSET(DaemonAwakeMessage, _impl_.service_port_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kInt32)},
    // string process_id = 3;
    {PROTOBUF_FIELD_OFFSET(DaemonAwakeMessage, _impl_.process_id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // repeated string device_ids = 16;
    {PROTOBUF_FIELD_OFFSET(DaemonAwakeMessage, _impl_.device_ids_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kUtf8String | ::_fl::kRepSString)},
    // repeated string usernames = 17;
    {PROTOBUF_FIELD_OFFSET(DaemonAwakeMessage, _impl_.usernames_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kUtf8String | ::_fl::kRepSString)},
  }},
  // no aux_entries
  {{
    "\30\21\0\12\12\11\0\0"
    "Valdi.DaemonAwakeMessage"
    "service_addresses"
    "process_id"
    "device_ids"
    "usernames"
  }},
};

PROTOBUF_NOINLINE void DaemonAwakeMessage::Clear() {
// @@protoc_insertion_point(message_clear_start:Valdi.DaemonAwakeMessage)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.service_addresses_.Clear();
  _impl_.device_ids_.Clear();
  _impl_.usernames_.Clear();
  _impl_.process_id_.ClearToEmpty();
  _impl_.service_port_ = 0;
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* DaemonAwakeMessage::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:Valdi.DaemonAwakeMessage)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // repeated string service_addresses = 1;
  for (int i = 0, n = this->_internal_service_addresses_size(); i < n; ++i) {
    const auto& s = this->_internal_service_addresses().Get(i);
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        s.data(), static_cast<int>(s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "Valdi.DaemonAwakeMessage.service_addresses");
    target = stream->WriteString(1, s, target);
  }

  // int32 service_port = 2;
  if (this->_internal_service_port() != 0) {
    target = ::google::protobuf::internal::WireFormatLite::
        WriteInt32ToArrayWithField<2>(
            stream, this->_internal_service_port(), target);
  }

  // string process_id = 3;
  if (!this->_internal_process_id().empty()) {
    const std::string& _s = this->_internal_process_id();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "Valdi.DaemonAwakeMessage.process_id");
    target = stream->WriteStringMaybeAliased(3, _s, target);
  }

  // repeated string device_ids = 16;
  for (int i = 0, n = this->_internal_device_ids_size(); i < n; ++i) {
    const auto& s = this->_internal_device_ids().Get(i);
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        s.data(), static_cast<int>(s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "Valdi.DaemonAwakeMessage.device_ids");
    target = stream->WriteString(16, s, target);
  }

  // repeated string usernames = 17;
  for (int i = 0, n = this->_internal_usernames_size(); i < n; ++i) {
    const auto& s = this->_internal_usernames().Get(i);
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        s.data(), static_cast<int>(s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "Valdi.DaemonAwakeMessage.usernames");
    target = stream->WriteString(17, s, target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:Valdi.DaemonAwakeMessage)
  return target;
}

::size_t DaemonAwakeMessage::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:Valdi.DaemonAwakeMessage)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // repeated string service_addresses = 1;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_service_addresses().size());
  for (int i = 0, n = _internal_service_addresses().size(); i < n; ++i) {
    total_size += ::google::protobuf::internal::WireFormatLite::StringSize(
        _internal_service_addresses().Get(i));
  }
  // repeated string device_ids = 16;
  total_size += 2 * ::google::protobuf::internal::FromIntSize(_internal_device_ids().size());
  for (int i = 0, n = _internal_device_ids().size(); i < n; ++i) {
    total_size += ::google::protobuf::internal::WireFormatLite::StringSize(
        _internal_device_ids().Get(i));
  }
  // repeated string usernames = 17;
  total_size += 2 * ::google::protobuf::internal::FromIntSize(_internal_usernames().size());
  for (int i = 0, n = _internal_usernames().size(); i < n; ++i) {
    total_size += ::google::protobuf::internal::WireFormatLite::StringSize(
        _internal_usernames().Get(i));
  }
  // string process_id = 3;
  if (!this->_internal_process_id().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_process_id());
  }

  // int32 service_port = 2;
  if (this->_internal_service_port() != 0) {
    total_size += ::_pbi::WireFormatLite::Int32SizePlusOne(
        this->_internal_service_port());
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void DaemonAwakeMessage::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const DaemonAwakeMessage*>(
      &from));
}

void DaemonAwakeMessage::MergeFrom(const DaemonAwakeMessage& from) {
  DaemonAwakeMessage* const _this = this;
  // @@protoc_insertion_point(class_specific_merge_from_start:Valdi.DaemonAwakeMessage)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_service_addresses()->MergeFrom(from._internal_service_addresses());
  _this->_internal_mutable_device_ids()->MergeFrom(from._internal_device_ids());
  _this->_internal_mutable_usernames()->MergeFrom(from._internal_usernames());
  if (!from._internal_process_id().empty()) {
    _this->_internal_set_process_id(from._internal_process_id());
  }
  if (from._internal_service_port() != 0) {
    _this->_impl_.service_port_ = from._impl_.service_port_;
  }
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void DaemonAwakeMessage::CopyFrom(const DaemonAwakeMessage& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Valdi.DaemonAwakeMessage)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void DaemonAwakeMessage::InternalSwap(DaemonAwakeMessage* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.service_addresses_.InternalSwap(&other->_impl_.service_addresses_);
  _impl_.device_ids_.InternalSwap(&other->_impl_.device_ids_);
  _impl_.usernames_.InternalSwap(&other->_impl_.usernames_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.process_id_, &other->_impl_.process_id_, arena);
        swap(_impl_.service_port_, other->_impl_.service_port_);
}

// ===================================================================

class ClientAwakeMessage::_Internal {
 public:
};

ClientAwakeMessage::ClientAwakeMessage(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:Valdi.ClientAwakeMessage)
}
inline PROTOBUF_NDEBUG_INLINE ClientAwakeMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::Valdi::ClientAwakeMessage& from_msg)
      : device_id_(arena, from.device_id_),
        username_(arena, from.username_),
        _cached_size_{0} {}

ClientAwakeMessage::ClientAwakeMessage(
    ::google::protobuf::Arena* arena,
    const ClientAwakeMessage& from)
    : ::google::protobuf::MessageLite(arena) {
  ClientAwakeMessage* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:Valdi.ClientAwakeMessage)
}
inline PROTOBUF_NDEBUG_INLINE ClientAwakeMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : device_id_(arena),
        username_(arena),
        _cached_size_{0} {}

inline void ClientAwakeMessage::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
ClientAwakeMessage::~ClientAwakeMessage() {
  // @@protoc_insertion_point(destructor:Valdi.ClientAwakeMessage)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void ClientAwakeMessage::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.device_id_.Destroy();
  _impl_.username_.Destroy();
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
ClientAwakeMessage::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<25> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(ClientAwakeMessage, _impl_._cached_size_),
              true,
          },
          "Valdi.ClientAwakeMessage",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 0, 50, 2> ClientAwakeMessage::_table_ = {
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
    &_ClientAwakeMessage_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::Valdi::ClientAwakeMessage>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // string username = 2;
    {::_pbi::TcParser::FastUS1,
     {18, 63, 0, PROTOBUF_FIELD_OFFSET(ClientAwakeMessage, _impl_.username_)}},
    // string device_id = 1;
    {::_pbi::TcParser::FastUS1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(ClientAwakeMessage, _impl_.device_id_)}},
  }}, {{
    65535, 65535
  }}, {{
    // string device_id = 1;
    {PROTOBUF_FIELD_OFFSET(ClientAwakeMessage, _impl_.device_id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // string username = 2;
    {PROTOBUF_FIELD_OFFSET(ClientAwakeMessage, _impl_.username_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
  }},
  // no aux_entries
  {{
    "\30\11\10\0\0\0\0\0"
    "Valdi.ClientAwakeMessage"
    "device_id"
    "username"
  }},
};

PROTOBUF_NOINLINE void ClientAwakeMessage::Clear() {
// @@protoc_insertion_point(message_clear_start:Valdi.ClientAwakeMessage)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.device_id_.ClearToEmpty();
  _impl_.username_.ClearToEmpty();
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* ClientAwakeMessage::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:Valdi.ClientAwakeMessage)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // string device_id = 1;
  if (!this->_internal_device_id().empty()) {
    const std::string& _s = this->_internal_device_id();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "Valdi.ClientAwakeMessage.device_id");
    target = stream->WriteStringMaybeAliased(1, _s, target);
  }

  // string username = 2;
  if (!this->_internal_username().empty()) {
    const std::string& _s = this->_internal_username();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "Valdi.ClientAwakeMessage.username");
    target = stream->WriteStringMaybeAliased(2, _s, target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:Valdi.ClientAwakeMessage)
  return target;
}

::size_t ClientAwakeMessage::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:Valdi.ClientAwakeMessage)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // string device_id = 1;
  if (!this->_internal_device_id().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_device_id());
  }

  // string username = 2;
  if (!this->_internal_username().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_username());
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void ClientAwakeMessage::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const ClientAwakeMessage*>(
      &from));
}

void ClientAwakeMessage::MergeFrom(const ClientAwakeMessage& from) {
  ClientAwakeMessage* const _this = this;
  // @@protoc_insertion_point(class_specific_merge_from_start:Valdi.ClientAwakeMessage)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_device_id().empty()) {
    _this->_internal_set_device_id(from._internal_device_id());
  }
  if (!from._internal_username().empty()) {
    _this->_internal_set_username(from._internal_username());
  }
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void ClientAwakeMessage::CopyFrom(const ClientAwakeMessage& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Valdi.ClientAwakeMessage)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void ClientAwakeMessage::InternalSwap(ClientAwakeMessage* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.device_id_, &other->_impl_.device_id_, arena);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.username_, &other->_impl_.username_, arena);
}

// ===================================================================

class DaemonServiceDiscoveryPayload::_Internal {
 public:
  static constexpr ::int32_t kOneofCaseOffset =
      PROTOBUF_FIELD_OFFSET(::Valdi::DaemonServiceDiscoveryPayload, _impl_._oneof_case_);
};

void DaemonServiceDiscoveryPayload::set_allocated_daemon_awake_message(::Valdi::DaemonAwakeMessage* daemon_awake_message) {
  ::google::protobuf::Arena* message_arena = GetArena();
  clear_message();
  if (daemon_awake_message) {
    ::google::protobuf::Arena* submessage_arena = daemon_awake_message->GetArena();
    if (message_arena != submessage_arena) {
      daemon_awake_message = ::google::protobuf::internal::GetOwnedMessage(message_arena, daemon_awake_message, submessage_arena);
    }
    set_has_daemon_awake_message();
    _impl_.message_.daemon_awake_message_ = daemon_awake_message;
  }
  // @@protoc_insertion_point(field_set_allocated:Valdi.DaemonServiceDiscoveryPayload.daemon_awake_message)
}
void DaemonServiceDiscoveryPayload::set_allocated_client_awake_message(::Valdi::ClientAwakeMessage* client_awake_message) {
  ::google::protobuf::Arena* message_arena = GetArena();
  clear_message();
  if (client_awake_message) {
    ::google::protobuf::Arena* submessage_arena = client_awake_message->GetArena();
    if (message_arena != submessage_arena) {
      client_awake_message = ::google::protobuf::internal::GetOwnedMessage(message_arena, client_awake_message, submessage_arena);
    }
    set_has_client_awake_message();
    _impl_.message_.client_awake_message_ = client_awake_message;
  }
  // @@protoc_insertion_point(field_set_allocated:Valdi.DaemonServiceDiscoveryPayload.client_awake_message)
}
DaemonServiceDiscoveryPayload::DaemonServiceDiscoveryPayload(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:Valdi.DaemonServiceDiscoveryPayload)
}
inline PROTOBUF_NDEBUG_INLINE DaemonServiceDiscoveryPayload::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::Valdi::DaemonServiceDiscoveryPayload& from_msg)
      : message_{},
        _cached_size_{0},
        _oneof_case_{from._oneof_case_[0]} {}

DaemonServiceDiscoveryPayload::DaemonServiceDiscoveryPayload(
    ::google::protobuf::Arena* arena,
    const DaemonServiceDiscoveryPayload& from)
    : ::google::protobuf::MessageLite(arena) {
  DaemonServiceDiscoveryPayload* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  switch (message_case()) {
    case MESSAGE_NOT_SET:
      break;
      case kDaemonAwakeMessage:
        _impl_.message_.daemon_awake_message_ = ::google::protobuf::MessageLite::CopyConstruct<::Valdi::DaemonAwakeMessage>(arena, *from._impl_.message_.daemon_awake_message_);
        break;
      case kClientAwakeMessage:
        _impl_.message_.client_awake_message_ = ::google::protobuf::MessageLite::CopyConstruct<::Valdi::ClientAwakeMessage>(arena, *from._impl_.message_.client_awake_message_);
        break;
  }

  // @@protoc_insertion_point(copy_constructor:Valdi.DaemonServiceDiscoveryPayload)
}
inline PROTOBUF_NDEBUG_INLINE DaemonServiceDiscoveryPayload::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : message_{},
        _cached_size_{0},
        _oneof_case_{} {}

inline void DaemonServiceDiscoveryPayload::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
DaemonServiceDiscoveryPayload::~DaemonServiceDiscoveryPayload() {
  // @@protoc_insertion_point(destructor:Valdi.DaemonServiceDiscoveryPayload)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void DaemonServiceDiscoveryPayload::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  if (has_message()) {
    clear_message();
  }
  _impl_.~Impl_();
}

void DaemonServiceDiscoveryPayload::clear_message() {
// @@protoc_insertion_point(one_of_clear_start:Valdi.DaemonServiceDiscoveryPayload)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  switch (message_case()) {
    case kDaemonAwakeMessage: {
      if (GetArena() == nullptr) {
        delete _impl_.message_.daemon_awake_message_;
      } else if (::google::protobuf::internal::DebugHardenClearOneofMessageOnArena()) {
        if (_impl_.message_.daemon_awake_message_ != nullptr) {
          _impl_.message_.daemon_awake_message_->Clear();
        }
      }
      break;
    }
    case kClientAwakeMessage: {
      if (GetArena() == nullptr) {
        delete _impl_.message_.client_awake_message_;
      } else if (::google::protobuf::internal::DebugHardenClearOneofMessageOnArena()) {
        if (_impl_.message_.client_awake_message_ != nullptr) {
          _impl_.message_.client_awake_message_->Clear();
        }
      }
      break;
    }
    case MESSAGE_NOT_SET: {
      break;
    }
  }
  _impl_._oneof_case_[0] = MESSAGE_NOT_SET;
}


const ::google::protobuf::MessageLite::ClassData*
DaemonServiceDiscoveryPayload::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<36> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(DaemonServiceDiscoveryPayload, _impl_._cached_size_),
              true,
          },
          "Valdi.DaemonServiceDiscoveryPayload",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 2, 2, 0, 2> DaemonServiceDiscoveryPayload::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    2, 0,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967292,  // skipmap
    offsetof(decltype(_table_), field_entries),
    2,  // num_field_entries
    2,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_DaemonServiceDiscoveryPayload_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::Valdi::DaemonServiceDiscoveryPayload>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
  }}, {{
    65535, 65535
  }}, {{
    // .Valdi.DaemonAwakeMessage daemon_awake_message = 1;
    {PROTOBUF_FIELD_OFFSET(DaemonServiceDiscoveryPayload, _impl_.message_.daemon_awake_message_), _Internal::kOneofCaseOffset + 0, 0,
    (0 | ::_fl::kFcOneof | ::_fl::kMessage | ::_fl::kTvTable)},
    // .Valdi.ClientAwakeMessage client_awake_message = 2;
    {PROTOBUF_FIELD_OFFSET(DaemonServiceDiscoveryPayload, _impl_.message_.client_awake_message_), _Internal::kOneofCaseOffset + 0, 1,
    (0 | ::_fl::kFcOneof | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::Valdi::DaemonAwakeMessage>()},
    {::_pbi::TcParser::GetTable<::Valdi::ClientAwakeMessage>()},
  }}, {{
  }},
};

PROTOBUF_NOINLINE void DaemonServiceDiscoveryPayload::Clear() {
// @@protoc_insertion_point(message_clear_start:Valdi.DaemonServiceDiscoveryPayload)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  clear_message();
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* DaemonServiceDiscoveryPayload::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:Valdi.DaemonServiceDiscoveryPayload)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  switch (message_case()) {
    case kDaemonAwakeMessage: {
      target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
          1, *_impl_.message_.daemon_awake_message_, _impl_.message_.daemon_awake_message_->GetCachedSize(), target, stream);
      break;
    }
    case kClientAwakeMessage: {
      target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
          2, *_impl_.message_.client_awake_message_, _impl_.message_.client_awake_message_->GetCachedSize(), target, stream);
      break;
    }
    default:
      break;
  }
  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:Valdi.DaemonServiceDiscoveryPayload)
  return target;
}

::size_t DaemonServiceDiscoveryPayload::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:Valdi.DaemonServiceDiscoveryPayload)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  switch (message_case()) {
    // .Valdi.DaemonAwakeMessage daemon_awake_message = 1;
    case kDaemonAwakeMessage: {
      total_size +=
          1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.message_.daemon_awake_message_);
      break;
    }
    // .Valdi.ClientAwakeMessage client_awake_message = 2;
    case kClientAwakeMessage: {
      total_size +=
          1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.message_.client_awake_message_);
      break;
    }
    case MESSAGE_NOT_SET: {
      break;
    }
  }
  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void DaemonServiceDiscoveryPayload::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const DaemonServiceDiscoveryPayload*>(
      &from));
}

void DaemonServiceDiscoveryPayload::MergeFrom(const DaemonServiceDiscoveryPayload& from) {
  DaemonServiceDiscoveryPayload* const _this = this;
  ::google::protobuf::Arena* arena = _this->GetArena();
  // @@protoc_insertion_point(class_specific_merge_from_start:Valdi.DaemonServiceDiscoveryPayload)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (const uint32_t oneof_from_case = from._impl_._oneof_case_[0]) {
    const uint32_t oneof_to_case = _this->_impl_._oneof_case_[0];
    const bool oneof_needs_init = oneof_to_case != oneof_from_case;
    if (oneof_needs_init) {
      if (oneof_to_case != 0) {
        _this->clear_message();
      }
      _this->_impl_._oneof_case_[0] = oneof_from_case;
    }

    switch (oneof_from_case) {
      case kDaemonAwakeMessage: {
        if (oneof_needs_init) {
          _this->_impl_.message_.daemon_awake_message_ =
              ::google::protobuf::MessageLite::CopyConstruct<::Valdi::DaemonAwakeMessage>(arena, *from._impl_.message_.daemon_awake_message_);
        } else {
          _this->_impl_.message_.daemon_awake_message_->MergeFrom(from._internal_daemon_awake_message());
        }
        break;
      }
      case kClientAwakeMessage: {
        if (oneof_needs_init) {
          _this->_impl_.message_.client_awake_message_ =
              ::google::protobuf::MessageLite::CopyConstruct<::Valdi::ClientAwakeMessage>(arena, *from._impl_.message_.client_awake_message_);
        } else {
          _this->_impl_.message_.client_awake_message_->MergeFrom(from._internal_client_awake_message());
        }
        break;
      }
      case MESSAGE_NOT_SET:
        break;
    }
  }
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void DaemonServiceDiscoveryPayload::CopyFrom(const DaemonServiceDiscoveryPayload& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Valdi.DaemonServiceDiscoveryPayload)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void DaemonServiceDiscoveryPayload::InternalSwap(DaemonServiceDiscoveryPayload* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_.message_, other->_impl_.message_);
  swap(_impl_._oneof_case_[0], other->_impl_._oneof_case_[0]);
}

// ===================================================================

class StyleNode::_Internal {
 public:
  using HasBits =
      decltype(std::declval<StyleNode>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
      8 * PROTOBUF_FIELD_OFFSET(StyleNode, _impl_._has_bits_);
};

StyleNode::StyleNode(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:Valdi.StyleNode)
}
inline PROTOBUF_NDEBUG_INLINE StyleNode::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::Valdi::StyleNode& from_msg)
      : _has_bits_{from._has_bits_},
        _cached_size_{0},
        styles_{visibility, arena, from.styles_} {}

StyleNode::StyleNode(
    ::google::protobuf::Arena* arena,
    const StyleNode& from)
    : ::google::protobuf::MessageLite(arena) {
  StyleNode* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  ::uint32_t cached_has_bits = _impl_._has_bits_[0];
  _impl_.ruleindex_ = (cached_has_bits & 0x00000001u) ? ::google::protobuf::MessageLite::CopyConstruct<::Valdi::CSSRuleIndex>(
                              arena, *from._impl_.ruleindex_)
                        : nullptr;

  // @@protoc_insertion_point(copy_constructor:Valdi.StyleNode)
}
inline PROTOBUF_NDEBUG_INLINE StyleNode::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0},
        styles_{visibility, arena} {}

inline void StyleNode::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.ruleindex_ = {};
}
StyleNode::~StyleNode() {
  // @@protoc_insertion_point(destructor:Valdi.StyleNode)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void StyleNode::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  delete _impl_.ruleindex_;
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
StyleNode::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<16> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(StyleNode, _impl_._cached_size_),
              true,
          },
          "Valdi.StyleNode",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 2, 0, 2> StyleNode::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(StyleNode, _impl_._has_bits_),
    0, // no _extensions_
    2, 8,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967292,  // skipmap
    offsetof(decltype(_table_), field_entries),
    2,  // num_field_entries
    2,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_StyleNode_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::Valdi::StyleNode>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // .Valdi.CSSRuleIndex ruleIndex = 2;
    {::_pbi::TcParser::FastMtS1,
     {18, 0, 1, PROTOBUF_FIELD_OFFSET(StyleNode, _impl_.ruleindex_)}},
    // repeated .Valdi.StyleDeclaration styles = 1;
    {::_pbi::TcParser::FastMtR1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(StyleNode, _impl_.styles_)}},
  }}, {{
    65535, 65535
  }}, {{
    // repeated .Valdi.StyleDeclaration styles = 1;
    {PROTOBUF_FIELD_OFFSET(StyleNode, _impl_.styles_), -1, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
    // .Valdi.CSSRuleIndex ruleIndex = 2;
    {PROTOBUF_FIELD_OFFSET(StyleNode, _impl_.ruleindex_), _Internal::kHasBitsOffset + 0, 1,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::Valdi::StyleDeclaration>()},
    {::_pbi::TcParser::GetTable<::Valdi::CSSRuleIndex>()},
  }}, {{
  }},
};

PROTOBUF_NOINLINE void StyleNode::Clear() {
// @@protoc_insertion_point(message_clear_start:Valdi.StyleNode)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.styles_.Clear();
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(_impl_.ruleindex_ != nullptr);
    _impl_.ruleindex_->Clear();
  }
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* StyleNode::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:Valdi.StyleNode)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // repeated .Valdi.StyleDeclaration styles = 1;
  for (unsigned i = 0, n = static_cast<unsigned>(
                           this->_internal_styles_size());
       i < n; i++) {
    const auto& repfield = this->_internal_styles().Get(i);
    target =
        ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
            1, repfield, repfield.GetCachedSize(),
            target, stream);
  }

  cached_has_bits = _impl_._has_bits_[0];
  // .Valdi.CSSRuleIndex ruleIndex = 2;
  if (cached_has_bits & 0x00000001u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        2, *_impl_.ruleindex_, _impl_.ruleindex_->GetCachedSize(), target, stream);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:Valdi.StyleNode)
  return target;
}

::size_t StyleNode::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:Valdi.StyleNode)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // repeated .Valdi.StyleDeclaration styles = 1;
  total_size += 1UL * this->_internal_styles_size();
  for (const auto& msg : this->_internal_styles()) {
    total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  // .Valdi.CSSRuleIndex ruleIndex = 2;
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    total_size +=
        1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.ruleindex_);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void StyleNode::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const StyleNode*>(
      &from));
}

void StyleNode::MergeFrom(const StyleNode& from) {
  StyleNode* const _this = this;
  ::google::protobuf::Arena* arena = _this->GetArena();
  // @@protoc_insertion_point(class_specific_merge_from_start:Valdi.StyleNode)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_styles()->MergeFrom(
      from._internal_styles());
  cached_has_bits = from._impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(from._impl_.ruleindex_ != nullptr);
    if (_this->_impl_.ruleindex_ == nullptr) {
      _this->_impl_.ruleindex_ =
          ::google::protobuf::MessageLite::CopyConstruct<::Valdi::CSSRuleIndex>(arena, *from._impl_.ruleindex_);
    } else {
      _this->_impl_.ruleindex_->MergeFrom(*from._impl_.ruleindex_);
    }
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void StyleNode::CopyFrom(const StyleNode& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Valdi.StyleNode)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void StyleNode::InternalSwap(StyleNode* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  _impl_.styles_.InternalSwap(&other->_impl_.styles_);
  swap(_impl_.ruleindex_, other->_impl_.ruleindex_);
}

// ===================================================================

class StyleDeclaration::_Internal {
 public:
  using HasBits =
      decltype(std::declval<StyleDeclaration>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
      8 * PROTOBUF_FIELD_OFFSET(StyleDeclaration, _impl_._has_bits_);
};

StyleDeclaration::StyleDeclaration(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:Valdi.StyleDeclaration)
}
inline PROTOBUF_NDEBUG_INLINE StyleDeclaration::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::Valdi::StyleDeclaration& from_msg)
      : _has_bits_{from._has_bits_},
        _cached_size_{0} {}

StyleDeclaration::StyleDeclaration(
    ::google::protobuf::Arena* arena,
    const StyleDeclaration& from)
    : ::google::protobuf::MessageLite(arena) {
  StyleDeclaration* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  ::uint32_t cached_has_bits = _impl_._has_bits_[0];
  _impl_.attribute_ = (cached_has_bits & 0x00000001u) ? ::google::protobuf::MessageLite::CopyConstruct<::Valdi::NodeAttribute>(
                              arena, *from._impl_.attribute_)
                        : nullptr;
  ::memcpy(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, priority_),
           reinterpret_cast<const char *>(&from._impl_) +
               offsetof(Impl_, priority_),
           offsetof(Impl_, id_) -
               offsetof(Impl_, priority_) +
               sizeof(Impl_::id_));

  // @@protoc_insertion_point(copy_constructor:Valdi.StyleDeclaration)
}
inline PROTOBUF_NDEBUG_INLINE StyleDeclaration::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void StyleDeclaration::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, attribute_),
           0,
           offsetof(Impl_, id_) -
               offsetof(Impl_, attribute_) +
               sizeof(Impl_::id_));
}
StyleDeclaration::~StyleDeclaration() {
  // @@protoc_insertion_point(destructor:Valdi.StyleDeclaration)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void StyleDeclaration::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  delete _impl_.attribute_;
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
StyleDeclaration::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<23> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(StyleDeclaration, _impl_._cached_size_),
              true,
          },
          "Valdi.StyleDeclaration",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 4, 1, 0, 2> StyleDeclaration::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(StyleDeclaration, _impl_._has_bits_),
    0, // no _extensions_
    4, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967280,  // skipmap
    offsetof(decltype(_table_), field_entries),
    4,  // num_field_entries
    1,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_StyleDeclaration_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::Valdi::StyleDeclaration>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // int32 id = 4;
    {::_pbi::TcParser::FastV32S1,
     {32, 63, 0, PROTOBUF_FIELD_OFFSET(StyleDeclaration, _impl_.id_)}},
    // .Valdi.NodeAttribute attribute = 1;
    {::_pbi::TcParser::FastMtS1,
     {10, 0, 0, PROTOBUF_FIELD_OFFSET(StyleDeclaration, _impl_.attribute_)}},
    // int32 priority = 2;
    {::_pbi::TcParser::FastV32S1,
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(StyleDeclaration, _impl_.priority_)}},
    // int32 order = 3;
    {::_pbi::TcParser::FastV32S1,
     {24, 63, 0, PROTOBUF_FIELD_OFFSET(StyleDeclaration, _impl_.order_)}},
  }}, {{
    65535, 65535
  }}, {{
    // .Valdi.NodeAttribute attribute = 1;
    {PROTOBUF_FIELD_OFFSET(StyleDeclaration, _impl_.attribute_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
    // int32 priority = 2;
    {PROTOBUF_FIELD_OFFSET(StyleDeclaration, _impl_.priority_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kInt32)},
    // int32 order = 3;
    {PROTOBUF_FIELD_OFFSET(StyleDeclaration, _impl_.order_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kInt32)},
    // int32 id = 4;
    {PROTOBUF_FIELD_OFFSET(StyleDeclaration, _impl_.id_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kInt32)},
  }}, {{
    {::_pbi::TcParser::GetTable<::Valdi::NodeAttribute>()},
  }}, {{
  }},
};

PROTOBUF_NOINLINE void StyleDeclaration::Clear() {
// @@protoc_insertion_point(message_clear_start:Valdi.StyleDeclaration)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(_impl_.attribute_ != nullptr);
    _impl_.attribute_->Clear();
  }
  ::memset(&_impl_.priority_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.id_) -
      reinterpret_cast<char*>(&_impl_.priority_)) + sizeof(_impl_.id_));
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* StyleDeclaration::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:Valdi.StyleDeclaration)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  cached_has_bits = _impl_._has_bits_[0];
  // .Valdi.NodeAttribute attribute = 1;
  if (cached_has_bits & 0x00000001u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        1, *_impl_.attribute_, _impl_.attribute_->GetCachedSize(), target, stream);
  }

  // int32 priority = 2;
  if (this->_internal_priority() != 0) {
    target = ::google::protobuf::internal::WireFormatLite::
        WriteInt32ToArrayWithField<2>(
            stream, this->_internal_priority(), target);
  }

  // int32 order = 3;
  if (this->_internal_order() != 0) {
    target = ::google::protobuf::internal::WireFormatLite::
        WriteInt32ToArrayWithField<3>(
            stream, this->_internal_order(), target);
  }

  // int32 id = 4;
  if (this->_internal_id() != 0) {
    target = ::google::protobuf::internal::WireFormatLite::
        WriteInt32ToArrayWithField<4>(
            stream, this->_internal_id(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:Valdi.StyleDeclaration)
  return target;
}

::size_t StyleDeclaration::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:Valdi.StyleDeclaration)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // .Valdi.NodeAttribute attribute = 1;
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    total_size +=
        1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.attribute_);
  }

  // int32 priority = 2;
  if (this->_internal_priority() != 0) {
    total_size += ::_pbi::WireFormatLite::Int32SizePlusOne(
        this->_internal_priority());
  }

  // int32 order = 3;
  if (this->_internal_order() != 0) {
    total_size += ::_pbi::WireFormatLite::Int32SizePlusOne(
        this->_internal_order());
  }

  // int32 id = 4;
  if (this->_internal_id() != 0) {
    total_size += ::_pbi::WireFormatLite::Int32SizePlusOne(
        this->_internal_id());
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void StyleDeclaration::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const StyleDeclaration*>(
      &from));
}

void StyleDeclaration::MergeFrom(const StyleDeclaration& from) {
  StyleDeclaration* const _this = this;
  ::google::protobuf::Arena* arena = _this->GetArena();
  // @@protoc_insertion_point(class_specific_merge_from_start:Valdi.StyleDeclaration)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  cached_has_bits = from._impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(from._impl_.attribute_ != nullptr);
    if (_this->_impl_.attribute_ == nullptr) {
      _this->_impl_.attribute_ =
          ::google::protobuf::MessageLite::CopyConstruct<::Valdi::NodeAttribute>(arena, *from._impl_.attribute_);
    } else {
      _this->_impl_.attribute_->MergeFrom(*from._impl_.attribute_);
    }
  }
  if (from._internal_priority() != 0) {
    _this->_impl_.priority_ = from._impl_.priority_;
  }
  if (from._internal_order() != 0) {
    _this->_impl_.order_ = from._impl_.order_;
  }
  if (from._internal_id() != 0) {
    _this->_impl_.id_ = from._impl_.id_;
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void StyleDeclaration::CopyFrom(const StyleDeclaration& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Valdi.StyleDeclaration)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void StyleDeclaration::InternalSwap(StyleDeclaration* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(StyleDeclaration, _impl_.id_)
      + sizeof(StyleDeclaration::_impl_.id_)
      - PROTOBUF_FIELD_OFFSET(StyleDeclaration, _impl_.attribute_)>(
          reinterpret_cast<char*>(&_impl_.attribute_),
          reinterpret_cast<char*>(&other->_impl_.attribute_));
}

// ===================================================================

class NamedStyleNode::_Internal {
 public:
  using HasBits =
      decltype(std::declval<NamedStyleNode>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
      8 * PROTOBUF_FIELD_OFFSET(NamedStyleNode, _impl_._has_bits_);
};

NamedStyleNode::NamedStyleNode(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:Valdi.NamedStyleNode)
}
inline PROTOBUF_NDEBUG_INLINE NamedStyleNode::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::Valdi::NamedStyleNode& from_msg)
      : _has_bits_{from._has_bits_},
        _cached_size_{0},
        name_(arena, from.name_) {}

NamedStyleNode::NamedStyleNode(
    ::google::protobuf::Arena* arena,
    const NamedStyleNode& from)
    : ::google::protobuf::MessageLite(arena) {
  NamedStyleNode* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  ::uint32_t cached_has_bits = _impl_._has_bits_[0];
  _impl_.node_ = (cached_has_bits & 0x00000001u) ? ::google::protobuf::MessageLite::CopyConstruct<::Valdi::StyleNode>(
                              arena, *from._impl_.node_)
                        : nullptr;

  // @@protoc_insertion_point(copy_constructor:Valdi.NamedStyleNode)
}
inline PROTOBUF_NDEBUG_INLINE NamedStyleNode::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0},
        name_(arena) {}

inline void NamedStyleNode::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.node_ = {};
}
NamedStyleNode::~NamedStyleNode() {
  // @@protoc_insertion_point(destructor:Valdi.NamedStyleNode)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void NamedStyleNode::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.name_.Destroy();
  delete _impl_.node_;
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
NamedStyleNode::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<21> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(NamedStyleNode, _impl_._cached_size_),
              true,
          },
          "Valdi.NamedStyleNode",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 1, 33, 2> NamedStyleNode::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(NamedStyleNode, _impl_._has_bits_),
    0, // no _extensions_
    2, 8,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967292,  // skipmap
    offsetof(decltype(_table_), field_entries),
    2,  // num_field_entries
    1,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_NamedStyleNode_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::Valdi::NamedStyleNode>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // .Valdi.StyleNode node = 2;
    {::_pbi::TcParser::FastMtS1,
     {18, 0, 0, PROTOBUF_FIELD_OFFSET(NamedStyleNode, _impl_.node_)}},
    // string name = 1;
    {::_pbi::TcParser::FastUS1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(NamedStyleNode, _impl_.name_)}},
  }}, {{
    65535, 65535
  }}, {{
    // string name = 1;
    {PROTOBUF_FIELD_OFFSET(NamedStyleNode, _impl_.name_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // .Valdi.StyleNode node = 2;
    {PROTOBUF_FIELD_OFFSET(NamedStyleNode, _impl_.node_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::Valdi::StyleNode>()},
  }}, {{
    "\24\4\0\0\0\0\0\0"
    "Valdi.NamedStyleNode"
    "name"
  }},
};

PROTOBUF_NOINLINE void NamedStyleNode::Clear() {
// @@protoc_insertion_point(message_clear_start:Valdi.NamedStyleNode)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.name_.ClearToEmpty();
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(_impl_.node_ != nullptr);
    _impl_.node_->Clear();
  }
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* NamedStyleNode::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:Valdi.NamedStyleNode)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // string name = 1;
  if (!this->_internal_name().empty()) {
    const std::string& _s = this->_internal_name();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "Valdi.NamedStyleNode.name");
    target = stream->WriteStringMaybeAliased(1, _s, target);
  }

  cached_has_bits = _impl_._has_bits_[0];
  // .Valdi.StyleNode node = 2;
  if (cached_has_bits & 0x00000001u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        2, *_impl_.node_, _impl_.node_->GetCachedSize(), target, stream);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:Valdi.NamedStyleNode)
  return target;
}

::size_t NamedStyleNode::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:Valdi.NamedStyleNode)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // string name = 1;
  if (!this->_internal_name().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_name());
  }

  // .Valdi.StyleNode node = 2;
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    total_size +=
        1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.node_);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void NamedStyleNode::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const NamedStyleNode*>(
      &from));
}

void NamedStyleNode::MergeFrom(const NamedStyleNode& from) {
  NamedStyleNode* const _this = this;
  ::google::protobuf::Arena* arena = _this->GetArena();
  // @@protoc_insertion_point(class_specific_merge_from_start:Valdi.NamedStyleNode)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_name().empty()) {
    _this->_internal_set_name(from._internal_name());
  }
  cached_has_bits = from._impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(from._impl_.node_ != nullptr);
    if (_this->_impl_.node_ == nullptr) {
      _this->_impl_.node_ =
          ::google::protobuf::MessageLite::CopyConstruct<::Valdi::StyleNode>(arena, *from._impl_.node_);
    } else {
      _this->_impl_.node_->MergeFrom(*from._impl_.node_);
    }
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void NamedStyleNode::CopyFrom(const NamedStyleNode& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Valdi.NamedStyleNode)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void NamedStyleNode::InternalSwap(NamedStyleNode* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.name_, &other->_impl_.name_, arena);
  swap(_impl_.node_, other->_impl_.node_);
}

// ===================================================================

class CSSRuleIndex_AttributeRule::_Internal {
 public:
  using HasBits =
      decltype(std::declval<CSSRuleIndex_AttributeRule>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
      8 * PROTOBUF_FIELD_OFFSET(CSSRuleIndex_AttributeRule, _impl_._has_bits_);
};

CSSRuleIndex_AttributeRule::CSSRuleIndex_AttributeRule(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:Valdi.CSSRuleIndex.AttributeRule)
}
inline PROTOBUF_NDEBUG_INLINE CSSRuleIndex_AttributeRule::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::Valdi::CSSRuleIndex_AttributeRule& from_msg)
      : _has_bits_{from._has_bits_},
        _cached_size_{0} {}

CSSRuleIndex_AttributeRule::CSSRuleIndex_AttributeRule(
    ::google::protobuf::Arena* arena,
    const CSSRuleIndex_AttributeRule& from)
    : ::google::protobuf::MessageLite(arena) {
  CSSRuleIndex_AttributeRule* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  ::uint32_t cached_has_bits = _impl_._has_bits_[0];
  _impl_.attribute_ = (cached_has_bits & 0x00000001u) ? ::google::protobuf::MessageLite::CopyConstruct<::Valdi::NodeAttribute>(
                              arena, *from._impl_.attribute_)
                        : nullptr;
  _impl_.node_ = (cached_has_bits & 0x00000002u) ? ::google::protobuf::MessageLite::CopyConstruct<::Valdi::StyleNode>(
                              arena, *from._impl_.node_)
                        : nullptr;
  _impl_.type_ = from._impl_.type_;

  // @@protoc_insertion_point(copy_constructor:Valdi.CSSRuleIndex.AttributeRule)
}
inline PROTOBUF_NDEBUG_INLINE CSSRuleIndex_AttributeRule::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void CSSRuleIndex_AttributeRule::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, attribute_),
           0,
           offsetof(Impl_, type_) -
               offsetof(Impl_, attribute_) +
               sizeof(Impl_::type_));
}
CSSRuleIndex_AttributeRule::~CSSRuleIndex_AttributeRule() {
  // @@protoc_insertion_point(destructor:Valdi.CSSRuleIndex.AttributeRule)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void CSSRuleIndex_AttributeRule::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  delete _impl_.attribute_;
  delete _impl_.node_;
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
CSSRuleIndex_AttributeRule::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<33> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(CSSRuleIndex_AttributeRule, _impl_._cached_size_),
              true,
          },
          "Valdi.CSSRuleIndex.AttributeRule",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 3, 2, 0, 2> CSSRuleIndex_AttributeRule::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(CSSRuleIndex_AttributeRule, _impl_._has_bits_),
    0, // no _extensions_
    3, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967288,  // skipmap
    offsetof(decltype(_table_), field_entries),
    3,  // num_field_entries
    2,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_CSSRuleIndex_AttributeRule_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::Valdi::CSSRuleIndex_AttributeRule>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // .Valdi.CSSRuleIndex.AttributeRule.Type type = 1;
    {::_pbi::TcParser::FastV32S1,
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(CSSRuleIndex_AttributeRule, _impl_.type_)}},
    // .Valdi.NodeAttribute attribute = 2;
    {::_pbi::TcParser::FastMtS1,
     {18, 0, 0, PROTOBUF_FIELD_OFFSET(CSSRuleIndex_AttributeRule, _impl_.attribute_)}},
    // .Valdi.StyleNode node = 3;
    {::_pbi::TcParser::FastMtS1,
     {26, 1, 1, PROTOBUF_FIELD_OFFSET(CSSRuleIndex_AttributeRule, _impl_.node_)}},
  }}, {{
    65535, 65535
  }}, {{
    // .Valdi.CSSRuleIndex.AttributeRule.Type type = 1;
    {PROTOBUF_FIELD_OFFSET(CSSRuleIndex_AttributeRule, _impl_.type_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kOpenEnum)},
    // .Valdi.NodeAttribute attribute = 2;
    {PROTOBUF_FIELD_OFFSET(CSSRuleIndex_AttributeRule, _impl_.attribute_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
    // .Valdi.StyleNode node = 3;
    {PROTOBUF_FIELD_OFFSET(CSSRuleIndex_AttributeRule, _impl_.node_), _Internal::kHasBitsOffset + 1, 1,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::Valdi::NodeAttribute>()},
    {::_pbi::TcParser::GetTable<::Valdi::StyleNode>()},
  }}, {{
  }},
};

PROTOBUF_NOINLINE void CSSRuleIndex_AttributeRule::Clear() {
// @@protoc_insertion_point(message_clear_start:Valdi.CSSRuleIndex.AttributeRule)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000003u) {
    if (cached_has_bits & 0x00000001u) {
      ABSL_DCHECK(_impl_.attribute_ != nullptr);
      _impl_.attribute_->Clear();
    }
    if (cached_has_bits & 0x00000002u) {
      ABSL_DCHECK(_impl_.node_ != nullptr);
      _impl_.node_->Clear();
    }
  }
  _impl_.type_ = 0;
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* CSSRuleIndex_AttributeRule::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:Valdi.CSSRuleIndex.AttributeRule)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // .Valdi.CSSRuleIndex.AttributeRule.Type type = 1;
  if (this->_internal_type() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteEnumToArray(
        1, this->_internal_type(), target);
  }

  cached_has_bits = _impl_._has_bits_[0];
  // .Valdi.NodeAttribute attribute = 2;
  if (cached_has_bits & 0x00000001u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        2, *_impl_.attribute_, _impl_.attribute_->GetCachedSize(), target, stream);
  }

  // .Valdi.StyleNode node = 3;
  if (cached_has_bits & 0x00000002u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        3, *_impl_.node_, _impl_.node_->GetCachedSize(), target, stream);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:Valdi.CSSRuleIndex.AttributeRule)
  return target;
}

::size_t CSSRuleIndex_AttributeRule::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:Valdi.CSSRuleIndex.AttributeRule)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000003u) {
    // .Valdi.NodeAttribute attribute = 2;
    if (cached_has_bits & 0x00000001u) {
      total_size +=
          1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.attribute_);
    }

    // .Valdi.StyleNode node = 3;
    if (cached_has_bits & 0x00000002u) {
      total_size +=
          1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.node_);
    }

  }
  // .Valdi.CSSRuleIndex.AttributeRule.Type type = 1;
  if (this->_internal_type() != 0) {
    total_size += 1 +
                  ::_pbi::WireFormatLite::EnumSize(this->_internal_type());
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void CSSRuleIndex_AttributeRule::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const CSSRuleIndex_AttributeRule*>(
      &from));
}

void CSSRuleIndex_AttributeRule::MergeFrom(const CSSRuleIndex_AttributeRule& from) {
  CSSRuleIndex_AttributeRule* const _this = this;
  ::google::protobuf::Arena* arena = _this->GetArena();
  // @@protoc_insertion_point(class_specific_merge_from_start:Valdi.CSSRuleIndex.AttributeRule)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  cached_has_bits = from._impl_._has_bits_[0];
  if (cached_has_bits & 0x00000003u) {
    if (cached_has_bits & 0x00000001u) {
      ABSL_DCHECK(from._impl_.attribute_ != nullptr);
      if (_this->_impl_.attribute_ == nullptr) {
        _this->_impl_.attribute_ =
            ::google::protobuf::MessageLite::CopyConstruct<::Valdi::NodeAttribute>(arena, *from._impl_.attribute_);
      } else {
        _this->_impl_.attribute_->MergeFrom(*from._impl_.attribute_);
      }
    }
    if (cached_has_bits & 0x00000002u) {
      ABSL_DCHECK(from._impl_.node_ != nullptr);
      if (_this->_impl_.node_ == nullptr) {
        _this->_impl_.node_ =
            ::google::protobuf::MessageLite::CopyConstruct<::Valdi::StyleNode>(arena, *from._impl_.node_);
      } else {
        _this->_impl_.node_->MergeFrom(*from._impl_.node_);
      }
    }
  }
  if (from._internal_type() != 0) {
    _this->_impl_.type_ = from._impl_.type_;
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void CSSRuleIndex_AttributeRule::CopyFrom(const CSSRuleIndex_AttributeRule& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Valdi.CSSRuleIndex.AttributeRule)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void CSSRuleIndex_AttributeRule::InternalSwap(CSSRuleIndex_AttributeRule* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(CSSRuleIndex_AttributeRule, _impl_.type_)
      + sizeof(CSSRuleIndex_AttributeRule::_impl_.type_)
      - PROTOBUF_FIELD_OFFSET(CSSRuleIndex_AttributeRule, _impl_.attribute_)>(
          reinterpret_cast<char*>(&_impl_.attribute_),
          reinterpret_cast<char*>(&other->_impl_.attribute_));
}

// ===================================================================

class CSSRuleIndex_NthChildRule::_Internal {
 public:
  using HasBits =
      decltype(std::declval<CSSRuleIndex_NthChildRule>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
      8 * PROTOBUF_FIELD_OFFSET(CSSRuleIndex_NthChildRule, _impl_._has_bits_);
};

CSSRuleIndex_NthChildRule::CSSRuleIndex_NthChildRule(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:Valdi.CSSRuleIndex.NthChildRule)
}
inline PROTOBUF_NDEBUG_INLINE CSSRuleIndex_NthChildRule::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::Valdi::CSSRuleIndex_NthChildRule& from_msg)
      : _has_bits_{from._has_bits_},
        _cached_size_{0} {}

CSSRuleIndex_NthChildRule::CSSRuleIndex_NthChildRule(
    ::google::protobuf::Arena* arena,
    const CSSRuleIndex_NthChildRule& from)
    : ::google::protobuf::MessageLite(arena) {
  CSSRuleIndex_NthChildRule* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  ::uint32_t cached_has_bits = _impl_._has_bits_[0];
  _impl_.node_ = (cached_has_bits & 0x00000001u) ? ::google::protobuf::MessageLite::CopyConstruct<::Valdi::StyleNode>(
                              arena, *from._impl_.node_)
                        : nullptr;
  ::memcpy(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, n_),
           reinterpret_cast<const char *>(&from._impl_) +
               offsetof(Impl_, n_),
           offsetof(Impl_, offset_) -
               offsetof(Impl_, n_) +
               sizeof(Impl_::offset_));

  // @@protoc_insertion_point(copy_constructor:Valdi.CSSRuleIndex.NthChildRule)
}
inline PROTOBUF_NDEBUG_INLINE CSSRuleIndex_NthChildRule::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void CSSRuleIndex_NthChildRule::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, node_),
           0,
           offsetof(Impl_, offset_) -
               offsetof(Impl_, node_) +
               sizeof(Impl_::offset_));
}
CSSRuleIndex_NthChildRule::~CSSRuleIndex_NthChildRule() {
  // @@protoc_insertion_point(destructor:Valdi.CSSRuleIndex.NthChildRule)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void CSSRuleIndex_NthChildRule::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  delete _impl_.node_;
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
CSSRuleIndex_NthChildRule::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<32> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(CSSRuleIndex_NthChildRule, _impl_._cached_size_),
              true,
          },
          "Valdi.CSSRuleIndex.NthChildRule",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 3, 1, 0, 2> CSSRuleIndex_NthChildRule::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(CSSRuleIndex_NthChildRule, _impl_._has_bits_),
    0, // no _extensions_
    3, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967288,  // skipmap
    offsetof(decltype(_table_), field_entries),
    3,  // num_field_entries
    1,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_CSSRuleIndex_NthChildRule_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::Valdi::CSSRuleIndex_NthChildRule>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // int32 n = 1;
    {::_pbi::TcParser::FastV32S1,
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(CSSRuleIndex_NthChildRule, _impl_.n_)}},
    // int32 offset = 2;
    {::_pbi::TcParser::FastV32S1,
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(CSSRuleIndex_NthChildRule, _impl_.offset_)}},
    // .Valdi.StyleNode node = 3;
    {::_pbi::TcParser::FastMtS1,
     {26, 0, 0, PROTOBUF_FIELD_OFFSET(CSSRuleIndex_NthChildRule, _impl_.node_)}},
  }}, {{
    65535, 65535
  }}, {{
    // int32 n = 1;
    {PROTOBUF_FIELD_OFFSET(CSSRuleIndex_NthChildRule, _impl_.n_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kInt32)},
    // int32 offset = 2;
    {PROTOBUF_FIELD_OFFSET(CSSRuleIndex_NthChildRule, _impl_.offset_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kInt32)},
    // .Valdi.StyleNode node = 3;
    {PROTOBUF_FIELD_OFFSET(CSSRuleIndex_NthChildRule, _impl_.node_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::Valdi::StyleNode>()},
  }}, {{
  }},
};

PROTOBUF_NOINLINE void CSSRuleIndex_NthChildRule::Clear() {
// @@protoc_insertion_point(message_clear_start:Valdi.CSSRuleIndex.NthChildRule)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(_impl_.node_ != nullptr);
    _impl_.node_->Clear();
  }
  ::memset(&_impl_.n_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.offset_) -
      reinterpret_cast<char*>(&_impl_.n_)) + sizeof(_impl_.offset_));
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* CSSRuleIndex_NthChildRule::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:Valdi.CSSRuleIndex.NthChildRule)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // int32 n = 1;
  if (this->_internal_n() != 0) {
    target = ::google::protobuf::internal::WireFormatLite::
        WriteInt32ToArrayWithField<1>(
            stream, this->_internal_n(), target);
  }

  // int32 offset = 2;
  if (this->_internal_offset() != 0) {
    target = ::google::protobuf::internal::WireFormatLite::
        WriteInt32ToArrayWithField<2>(
            stream, this->_internal_offset(), target);
  }

  cached_has_bits = _impl_._has_bits_[0];
  // .Valdi.StyleNode node = 3;
  if (cached_has_bits & 0x00000001u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        3, *_impl_.node_, _impl_.node_->GetCachedSize(), target, stream);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:Valdi.CSSRuleIndex.NthChildRule)
  return target;
}

::size_t CSSRuleIndex_NthChildRule::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:Valdi.CSSRuleIndex.NthChildRule)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // .Valdi.StyleNode node = 3;
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    total_size +=
        1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.node_);
  }

  // int32 n = 1;
  if (this->_internal_n() != 0) {
    total_size += ::_pbi::WireFormatLite::Int32SizePlusOne(
        this->_internal_n());
  }

  // int32 offset = 2;
  if (this->_internal_offset() != 0) {
    total_size += ::_pbi::WireFormatLite::Int32SizePlusOne(
        this->_internal_offset());
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void CSSRuleIndex_NthChildRule::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const CSSRuleIndex_NthChildRule*>(
      &from));
}

void CSSRuleIndex_NthChildRule::MergeFrom(const CSSRuleIndex_NthChildRule& from) {
  CSSRuleIndex_NthChildRule* const _this = this;
  ::google::protobuf::Arena* arena = _this->GetArena();
  // @@protoc_insertion_point(class_specific_merge_from_start:Valdi.CSSRuleIndex.NthChildRule)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  cached_has_bits = from._impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(from._impl_.node_ != nullptr);
    if (_this->_impl_.node_ == nullptr) {
      _this->_impl_.node_ =
          ::google::protobuf::MessageLite::CopyConstruct<::Valdi::StyleNode>(arena, *from._impl_.node_);
    } else {
      _this->_impl_.node_->MergeFrom(*from._impl_.node_);
    }
  }
  if (from._internal_n() != 0) {
    _this->_impl_.n_ = from._impl_.n_;
  }
  if (from._internal_offset() != 0) {
    _this->_impl_.offset_ = from._impl_.offset_;
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void CSSRuleIndex_NthChildRule::CopyFrom(const CSSRuleIndex_NthChildRule& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Valdi.CSSRuleIndex.NthChildRule)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void CSSRuleIndex_NthChildRule::InternalSwap(CSSRuleIndex_NthChildRule* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(CSSRuleIndex_NthChildRule, _impl_.offset_)
      + sizeof(CSSRuleIndex_NthChildRule::_impl_.offset_)
      - PROTOBUF_FIELD_OFFSET(CSSRuleIndex_NthChildRule, _impl_.node_)>(
          reinterpret_cast<char*>(&_impl_.node_),
          reinterpret_cast<char*>(&other->_impl_.node_));
}

// ===================================================================

class CSSRuleIndex::_Internal {
 public:
  using HasBits =
      decltype(std::declval<CSSRuleIndex>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
      8 * PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_._has_bits_);
};

CSSRuleIndex::CSSRuleIndex(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:Valdi.CSSRuleIndex)
}
inline PROTOBUF_NDEBUG_INLINE CSSRuleIndex::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::Valdi::CSSRuleIndex& from_msg)
      : _has_bits_{from._has_bits_},
        _cached_size_{0},
        id_rules_{visibility, arena, from.id_rules_},
        class_rules_{visibility, arena, from.class_rules_},
        tag_rules_{visibility, arena, from.tag_rules_},
        attribute_rules_{visibility, arena, from.attribute_rules_},
        nth_child_rules_{visibility, arena, from.nth_child_rules_} {}

CSSRuleIndex::CSSRuleIndex(
    ::google::protobuf::Arena* arena,
    const CSSRuleIndex& from)
    : ::google::protobuf::MessageLite(arena) {
  CSSRuleIndex* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  ::uint32_t cached_has_bits = _impl_._has_bits_[0];
  _impl_.first_child_rule_ = (cached_has_bits & 0x00000001u) ? ::google::protobuf::MessageLite::CopyConstruct<::Valdi::StyleNode>(
                              arena, *from._impl_.first_child_rule_)
                        : nullptr;
  _impl_.last_child_rule_ = (cached_has_bits & 0x00000002u) ? ::google::protobuf::MessageLite::CopyConstruct<::Valdi::StyleNode>(
                              arena, *from._impl_.last_child_rule_)
                        : nullptr;
  _impl_.ancestor_rules_ = (cached_has_bits & 0x00000004u) ? ::google::protobuf::MessageLite::CopyConstruct<::Valdi::CSSRuleIndex>(
                              arena, *from._impl_.ancestor_rules_)
                        : nullptr;
  _impl_.direct_parent_rules_ = (cached_has_bits & 0x00000008u) ? ::google::protobuf::MessageLite::CopyConstruct<::Valdi::CSSRuleIndex>(
                              arena, *from._impl_.direct_parent_rules_)
                        : nullptr;

  // @@protoc_insertion_point(copy_constructor:Valdi.CSSRuleIndex)
}
inline PROTOBUF_NDEBUG_INLINE CSSRuleIndex::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0},
        id_rules_{visibility, arena},
        class_rules_{visibility, arena},
        tag_rules_{visibility, arena},
        attribute_rules_{visibility, arena},
        nth_child_rules_{visibility, arena} {}

inline void CSSRuleIndex::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, first_child_rule_),
           0,
           offsetof(Impl_, direct_parent_rules_) -
               offsetof(Impl_, first_child_rule_) +
               sizeof(Impl_::direct_parent_rules_));
}
CSSRuleIndex::~CSSRuleIndex() {
  // @@protoc_insertion_point(destructor:Valdi.CSSRuleIndex)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void CSSRuleIndex::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  delete _impl_.first_child_rule_;
  delete _impl_.last_child_rule_;
  delete _impl_.ancestor_rules_;
  delete _impl_.direct_parent_rules_;
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
CSSRuleIndex::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<19> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_._cached_size_),
              true,
          },
          "Valdi.CSSRuleIndex",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<4, 9, 9, 0, 2> CSSRuleIndex::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_._has_bits_),
    0, // no _extensions_
    9, 120,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294966784,  // skipmap
    offsetof(decltype(_table_), field_entries),
    9,  // num_field_entries
    9,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_CSSRuleIndex_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::Valdi::CSSRuleIndex>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // repeated .Valdi.NamedStyleNode id_rules = 1;
    {::_pbi::TcParser::FastMtR1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.id_rules_)}},
    // repeated .Valdi.NamedStyleNode class_rules = 2;
    {::_pbi::TcParser::FastMtR1,
     {18, 63, 1, PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.class_rules_)}},
    // repeated .Valdi.NamedStyleNode tag_rules = 3;
    {::_pbi::TcParser::FastMtR1,
     {26, 63, 2, PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.tag_rules_)}},
    // repeated .Valdi.CSSRuleIndex.AttributeRule attribute_rules = 4;
    {::_pbi::TcParser::FastMtR1,
     {34, 63, 3, PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.attribute_rules_)}},
    // .Valdi.StyleNode first_child_rule = 5;
    {::_pbi::TcParser::FastMtS1,
     {42, 0, 4, PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.first_child_rule_)}},
    // .Valdi.StyleNode last_child_rule = 6;
    {::_pbi::TcParser::FastMtS1,
     {50, 1, 5, PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.last_child_rule_)}},
    // repeated .Valdi.CSSRuleIndex.NthChildRule nth_child_rules = 7;
    {::_pbi::TcParser::FastMtR1,
     {58, 63, 6, PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.nth_child_rules_)}},
    // .Valdi.CSSRuleIndex ancestor_rules = 8;
    {::_pbi::TcParser::FastMtS1,
     {66, 2, 7, PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.ancestor_rules_)}},
    // .Valdi.CSSRuleIndex direct_parent_rules = 9;
    {::_pbi::TcParser::FastMtS1,
     {74, 3, 8, PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.direct_parent_rules_)}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
  }}, {{
    65535, 65535
  }}, {{
    // repeated .Valdi.NamedStyleNode id_rules = 1;
    {PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.id_rules_), -1, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
    // repeated .Valdi.NamedStyleNode class_rules = 2;
    {PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.class_rules_), -1, 1,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
    // repeated .Valdi.NamedStyleNode tag_rules = 3;
    {PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.tag_rules_), -1, 2,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
    // repeated .Valdi.CSSRuleIndex.AttributeRule attribute_rules = 4;
    {PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.attribute_rules_), -1, 3,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
    // .Valdi.StyleNode first_child_rule = 5;
    {PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.first_child_rule_), _Internal::kHasBitsOffset + 0, 4,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
    // .Valdi.StyleNode last_child_rule = 6;
    {PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.last_child_rule_), _Internal::kHasBitsOffset + 1, 5,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
    // repeated .Valdi.CSSRuleIndex.NthChildRule nth_child_rules = 7;
    {PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.nth_child_rules_), -1, 6,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
    // .Valdi.CSSRuleIndex ancestor_rules = 8;
    {PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.ancestor_rules_), _Internal::kHasBitsOffset + 2, 7,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
    // .Valdi.CSSRuleIndex direct_parent_rules = 9;
    {PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.direct_parent_rules_), _Internal::kHasBitsOffset + 3, 8,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::Valdi::NamedStyleNode>()},
    {::_pbi::TcParser::GetTable<::Valdi::NamedStyleNode>()},
    {::_pbi::TcParser::GetTable<::Valdi::NamedStyleNode>()},
    {::_pbi::TcParser::GetTable<::Valdi::CSSRuleIndex_AttributeRule>()},
    {::_pbi::TcParser::GetTable<::Valdi::StyleNode>()},
    {::_pbi::TcParser::GetTable<::Valdi::StyleNode>()},
    {::_pbi::TcParser::GetTable<::Valdi::CSSRuleIndex_NthChildRule>()},
    {::_pbi::TcParser::GetTable<::Valdi::CSSRuleIndex>()},
    {::_pbi::TcParser::GetTable<::Valdi::CSSRuleIndex>()},
  }}, {{
  }},
};

PROTOBUF_NOINLINE void CSSRuleIndex::Clear() {
// @@protoc_insertion_point(message_clear_start:Valdi.CSSRuleIndex)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.id_rules_.Clear();
  _impl_.class_rules_.Clear();
  _impl_.tag_rules_.Clear();
  _impl_.attribute_rules_.Clear();
  _impl_.nth_child_rules_.Clear();
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x0000000fu) {
    if (cached_has_bits & 0x00000001u) {
      ABSL_DCHECK(_impl_.first_child_rule_ != nullptr);
      _impl_.first_child_rule_->Clear();
    }
    if (cached_has_bits & 0x00000002u) {
      ABSL_DCHECK(_impl_.last_child_rule_ != nullptr);
      _impl_.last_child_rule_->Clear();
    }
    if (cached_has_bits & 0x00000004u) {
      ABSL_DCHECK(_impl_.ancestor_rules_ != nullptr);
      _impl_.ancestor_rules_->Clear();
    }
    if (cached_has_bits & 0x00000008u) {
      ABSL_DCHECK(_impl_.direct_parent_rules_ != nullptr);
      _impl_.direct_parent_rules_->Clear();
    }
  }
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* CSSRuleIndex::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:Valdi.CSSRuleIndex)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // repeated .Valdi.NamedStyleNode id_rules = 1;
  for (unsigned i = 0, n = static_cast<unsigned>(
                           this->_internal_id_rules_size());
       i < n; i++) {
    const auto& repfield = this->_internal_id_rules().Get(i);
    target =
        ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
            1, repfield, repfield.GetCachedSize(),
            target, stream);
  }

  // repeated .Valdi.NamedStyleNode class_rules = 2;
  for (unsigned i = 0, n = static_cast<unsigned>(
                           this->_internal_class_rules_size());
       i < n; i++) {
    const auto& repfield = this->_internal_class_rules().Get(i);
    target =
        ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
            2, repfield, repfield.GetCachedSize(),
            target, stream);
  }

  // repeated .Valdi.NamedStyleNode tag_rules = 3;
  for (unsigned i = 0, n = static_cast<unsigned>(
                           this->_internal_tag_rules_size());
       i < n; i++) {
    const auto& repfield = this->_internal_tag_rules().Get(i);
    target =
        ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
            3, repfield, repfield.GetCachedSize(),
            target, stream);
  }

  // repeated .Valdi.CSSRuleIndex.AttributeRule attribute_rules = 4;
  for (unsigned i = 0, n = static_cast<unsigned>(
                           this->_internal_attribute_rules_size());
       i < n; i++) {
    const auto& repfield = this->_internal_attribute_rules().Get(i);
    target =
        ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
            4, repfield, repfield.GetCachedSize(),
            target, stream);
  }

  cached_has_bits = _impl_._has_bits_[0];
  // .Valdi.StyleNode first_child_rule = 5;
  if (cached_has_bits & 0x00000001u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        5, *_impl_.first_child_rule_, _impl_.first_child_rule_->GetCachedSize(), target, stream);
  }

  // .Valdi.StyleNode last_child_rule = 6;
  if (cached_has_bits & 0x00000002u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        6, *_impl_.last_child_rule_, _impl_.last_child_rule_->GetCachedSize(), target, stream);
  }

  // repeated .Valdi.CSSRuleIndex.NthChildRule nth_child_rules = 7;
  for (unsigned i = 0, n = static_cast<unsigned>(
                           this->_internal_nth_child_rules_size());
       i < n; i++) {
    const auto& repfield = this->_internal_nth_child_rules().Get(i);
    target =
        ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
            7, repfield, repfield.GetCachedSize(),
            target, stream);
  }

  // .Valdi.CSSRuleIndex ancestor_rules = 8;
  if (cached_has_bits & 0x00000004u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        8, *_impl_.ancestor_rules_, _impl_.ancestor_rules_->GetCachedSize(), target, stream);
  }

  // .Valdi.CSSRuleIndex direct_parent_rules = 9;
  if (cached_has_bits & 0x00000008u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        9, *_impl_.direct_parent_rules_, _impl_.direct_parent_rules_->GetCachedSize(), target, stream);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:Valdi.CSSRuleIndex)
  return target;
}

::size_t CSSRuleIndex::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:Valdi.CSSRuleIndex)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // repeated .Valdi.NamedStyleNode id_rules = 1;
  total_size += 1UL * this->_internal_id_rules_size();
  for (const auto& msg : this->_internal_id_rules()) {
    total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  // repeated .Valdi.NamedStyleNode class_rules = 2;
  total_size += 1UL * this->_internal_class_rules_size();
  for (const auto& msg : this->_internal_class_rules()) {
    total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  // repeated .Valdi.NamedStyleNode tag_rules = 3;
  total_size += 1UL * this->_internal_tag_rules_size();
  for (const auto& msg : this->_internal_tag_rules()) {
    total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  // repeated .Valdi.CSSRuleIndex.AttributeRule attribute_rules = 4;
  total_size += 1UL * this->_internal_attribute_rules_size();
  for (const auto& msg : this->_internal_attribute_rules()) {
    total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  // repeated .Valdi.CSSRuleIndex.NthChildRule nth_child_rules = 7;
  total_size += 1UL * this->_internal_nth_child_rules_size();
  for (const auto& msg : this->_internal_nth_child_rules()) {
    total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x0000000fu) {
    // .Valdi.StyleNode first_child_rule = 5;
    if (cached_has_bits & 0x00000001u) {
      total_size +=
          1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.first_child_rule_);
    }

    // .Valdi.StyleNode last_child_rule = 6;
    if (cached_has_bits & 0x00000002u) {
      total_size +=
          1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.last_child_rule_);
    }

    // .Valdi.CSSRuleIndex ancestor_rules = 8;
    if (cached_has_bits & 0x00000004u) {
      total_size +=
          1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.ancestor_rules_);
    }

    // .Valdi.CSSRuleIndex direct_parent_rules = 9;
    if (cached_has_bits & 0x00000008u) {
      total_size +=
          1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.direct_parent_rules_);
    }

  }
  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void CSSRuleIndex::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const CSSRuleIndex*>(
      &from));
}

void CSSRuleIndex::MergeFrom(const CSSRuleIndex& from) {
  CSSRuleIndex* const _this = this;
  ::google::protobuf::Arena* arena = _this->GetArena();
  // @@protoc_insertion_point(class_specific_merge_from_start:Valdi.CSSRuleIndex)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_id_rules()->MergeFrom(
      from._internal_id_rules());
  _this->_internal_mutable_class_rules()->MergeFrom(
      from._internal_class_rules());
  _this->_internal_mutable_tag_rules()->MergeFrom(
      from._internal_tag_rules());
  _this->_internal_mutable_attribute_rules()->MergeFrom(
      from._internal_attribute_rules());
  _this->_internal_mutable_nth_child_rules()->MergeFrom(
      from._internal_nth_child_rules());
  cached_has_bits = from._impl_._has_bits_[0];
  if (cached_has_bits & 0x0000000fu) {
    if (cached_has_bits & 0x00000001u) {
      ABSL_DCHECK(from._impl_.first_child_rule_ != nullptr);
      if (_this->_impl_.first_child_rule_ == nullptr) {
        _this->_impl_.first_child_rule_ =
            ::google::protobuf::MessageLite::CopyConstruct<::Valdi::StyleNode>(arena, *from._impl_.first_child_rule_);
      } else {
        _this->_impl_.first_child_rule_->MergeFrom(*from._impl_.first_child_rule_);
      }
    }
    if (cached_has_bits & 0x00000002u) {
      ABSL_DCHECK(from._impl_.last_child_rule_ != nullptr);
      if (_this->_impl_.last_child_rule_ == nullptr) {
        _this->_impl_.last_child_rule_ =
            ::google::protobuf::MessageLite::CopyConstruct<::Valdi::StyleNode>(arena, *from._impl_.last_child_rule_);
      } else {
        _this->_impl_.last_child_rule_->MergeFrom(*from._impl_.last_child_rule_);
      }
    }
    if (cached_has_bits & 0x00000004u) {
      ABSL_DCHECK(from._impl_.ancestor_rules_ != nullptr);
      if (_this->_impl_.ancestor_rules_ == nullptr) {
        _this->_impl_.ancestor_rules_ =
            ::google::protobuf::MessageLite::CopyConstruct<::Valdi::CSSRuleIndex>(arena, *from._impl_.ancestor_rules_);
      } else {
        _this->_impl_.ancestor_rules_->MergeFrom(*from._impl_.ancestor_rules_);
      }
    }
    if (cached_has_bits & 0x00000008u) {
      ABSL_DCHECK(from._impl_.direct_parent_rules_ != nullptr);
      if (_this->_impl_.direct_parent_rules_ == nullptr) {
        _this->_impl_.direct_parent_rules_ =
            ::google::protobuf::MessageLite::CopyConstruct<::Valdi::CSSRuleIndex>(arena, *from._impl_.direct_parent_rules_);
      } else {
        _this->_impl_.direct_parent_rules_->MergeFrom(*from._impl_.direct_parent_rules_);
      }
    }
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void CSSRuleIndex::CopyFrom(const CSSRuleIndex& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Valdi.CSSRuleIndex)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void CSSRuleIndex::InternalSwap(CSSRuleIndex* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  _impl_.id_rules_.InternalSwap(&other->_impl_.id_rules_);
  _impl_.class_rules_.InternalSwap(&other->_impl_.class_rules_);
  _impl_.tag_rules_.InternalSwap(&other->_impl_.tag_rules_);
  _impl_.attribute_rules_.InternalSwap(&other->_impl_.attribute_rules_);
  _impl_.nth_child_rules_.InternalSwap(&other->_impl_.nth_child_rules_);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.direct_parent_rules_)
      + sizeof(CSSRuleIndex::_impl_.direct_parent_rules_)
      - PROTOBUF_FIELD_OFFSET(CSSRuleIndex, _impl_.first_child_rule_)>(
          reinterpret_cast<char*>(&_impl_.first_child_rule_),
          reinterpret_cast<char*>(&other->_impl_.first_child_rule_));
}

// ===================================================================

class TsnConfig::_Internal {
 public:
};

TsnConfig::TsnConfig(::google::protobuf::Arena* arena)
    : ::google::protobuf::MessageLite(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:Valdi.TsnConfig)
}
inline PROTOBUF_NDEBUG_INLINE TsnConfig::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::Valdi::TsnConfig& from_msg)
      : enabled_modules_{visibility, arena, from.enabled_modules_},
        _cached_size_{0} {}

TsnConfig::TsnConfig(
    ::google::protobuf::Arena* arena,
    const TsnConfig& from)
    : ::google::protobuf::MessageLite(arena) {
  TsnConfig* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<std::string>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:Valdi.TsnConfig)
}
inline PROTOBUF_NDEBUG_INLINE TsnConfig::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : enabled_modules_{visibility, arena},
        _cached_size_{0} {}

inline void TsnConfig::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
TsnConfig::~TsnConfig() {
  // @@protoc_insertion_point(destructor:Valdi.TsnConfig)
  _internal_metadata_.Delete<std::string>();
  SharedDtor();
}
inline void TsnConfig::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
TsnConfig::GetClassData() const {
  PROTOBUF_CONSTINIT static const ClassDataLite<16> _data_ =
      {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(TsnConfig, _impl_._cached_size_),
              true,
          },
          "Valdi.TsnConfig",
      };

  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 0, 39, 2> TsnConfig::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    1, 0,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967294,  // skipmap
    offsetof(decltype(_table_), field_entries),
    1,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    &_TsnConfig_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallbackLite,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::Valdi::TsnConfig>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // repeated string enabled_modules = 1;
    {::_pbi::TcParser::FastUR1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(TsnConfig, _impl_.enabled_modules_)}},
  }}, {{
    65535, 65535
  }}, {{
    // repeated string enabled_modules = 1;
    {PROTOBUF_FIELD_OFFSET(TsnConfig, _impl_.enabled_modules_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kUtf8String | ::_fl::kRepSString)},
  }},
  // no aux_entries
  {{
    "\17\17\0\0\0\0\0\0"
    "Valdi.TsnConfig"
    "enabled_modules"
  }},
};

PROTOBUF_NOINLINE void TsnConfig::Clear() {
// @@protoc_insertion_point(message_clear_start:Valdi.TsnConfig)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.enabled_modules_.Clear();
  _internal_metadata_.Clear<std::string>();
}

::uint8_t* TsnConfig::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:Valdi.TsnConfig)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // repeated string enabled_modules = 1;
  for (int i = 0, n = this->_internal_enabled_modules_size(); i < n; ++i) {
    const auto& s = this->_internal_enabled_modules().Get(i);
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        s.data(), static_cast<int>(s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "Valdi.TsnConfig.enabled_modules");
    target = stream->WriteString(1, s, target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target = stream->WriteRaw(
        _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).data(),
        static_cast<int>(_internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size()), target);
  }
  // @@protoc_insertion_point(serialize_to_array_end:Valdi.TsnConfig)
  return target;
}

::size_t TsnConfig::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:Valdi.TsnConfig)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // repeated string enabled_modules = 1;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_enabled_modules().size());
  for (int i = 0, n = _internal_enabled_modules().size(); i < n; ++i) {
    total_size += ::google::protobuf::internal::WireFormatLite::StringSize(
        _internal_enabled_modules().Get(i));
  }
  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    total_size += _internal_metadata_.unknown_fields<std::string>(::google::protobuf::internal::GetEmptyString).size();
  }
  _impl_._cached_size_.Set(::_pbi::ToCachedSize(total_size));
  return total_size;
}

void TsnConfig::CheckTypeAndMergeFrom(
    const ::google::protobuf::MessageLite& from) {
  MergeFrom(*::_pbi::DownCast<const TsnConfig*>(
      &from));
}

void TsnConfig::MergeFrom(const TsnConfig& from) {
  TsnConfig* const _this = this;
  // @@protoc_insertion_point(class_specific_merge_from_start:Valdi.TsnConfig)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_enabled_modules()->MergeFrom(from._internal_enabled_modules());
  _this->_internal_metadata_.MergeFrom<std::string>(from._internal_metadata_);
}

void TsnConfig::CopyFrom(const TsnConfig& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Valdi.TsnConfig)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void TsnConfig::InternalSwap(TsnConfig* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.enabled_modules_.InternalSwap(&other->_impl_.enabled_modules_);
}

// @@protoc_insertion_point(namespace_scope)
}  // namespace Valdi
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
#include "google/protobuf/port_undef.inc"
