// Generate by the Snap-modified protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: test.proto
// Protobuf C++ Version: 5.27.0

#include "test.pb.h"

#include <algorithm>
#include <type_traits>
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/generated_message_tctable_impl.h"
#include "google/protobuf/extension_set.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/reflection_ops.h"
#include "google/protobuf/wire_format.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"
PROTOBUF_PRAGMA_INIT_SEG
namespace _pb = ::google::protobuf;
namespace _pbi = ::google::protobuf::internal;
namespace _fl = ::google::protobuf::internal::field_layout;
namespace test {

inline constexpr ParentMessage_ChildMessage::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : value_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR ParentMessage_ChildMessage::ParentMessage_ChildMessage(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct ParentMessage_ChildMessageDefaultTypeInternal {
  PROTOBUF_CONSTEXPR ParentMessage_ChildMessageDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~ParentMessage_ChildMessageDefaultTypeInternal() {}
  union {
    ParentMessage_ChildMessage _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 ParentMessage_ChildMessageDefaultTypeInternal _ParentMessage_ChildMessage_default_instance_;

inline constexpr OtherMessage::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : value_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR OtherMessage::OtherMessage(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct OtherMessageDefaultTypeInternal {
  PROTOBUF_CONSTEXPR OtherMessageDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~OtherMessageDefaultTypeInternal() {}
  union {
    OtherMessage _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 OtherMessageDefaultTypeInternal _OtherMessage_default_instance_;

inline constexpr OldMessage::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : old_value_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR OldMessage::OldMessage(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct OldMessageDefaultTypeInternal {
  PROTOBUF_CONSTEXPR OldMessageDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~OldMessageDefaultTypeInternal() {}
  union {
    OldMessage _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 OldMessageDefaultTypeInternal _OldMessage_default_instance_;
      template <typename>
PROTOBUF_CONSTEXPR OldEnumMessage_MappedValueEntry_DoNotUse::OldEnumMessage_MappedValueEntry_DoNotUse(::_pbi::ConstantInitialized) {}
struct OldEnumMessage_MappedValueEntry_DoNotUseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR OldEnumMessage_MappedValueEntry_DoNotUseDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~OldEnumMessage_MappedValueEntry_DoNotUseDefaultTypeInternal() {}
  union {
    OldEnumMessage_MappedValueEntry_DoNotUse _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 OldEnumMessage_MappedValueEntry_DoNotUseDefaultTypeInternal _OldEnumMessage_MappedValueEntry_DoNotUse_default_instance_;

inline constexpr NewMessage::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : old_value_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        new_value_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR NewMessage::NewMessage(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct NewMessageDefaultTypeInternal {
  PROTOBUF_CONSTEXPR NewMessageDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~NewMessageDefaultTypeInternal() {}
  union {
    NewMessage _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 NewMessageDefaultTypeInternal _NewMessage_default_instance_;
      template <typename>
PROTOBUF_CONSTEXPR NewEnumMessage_MappedValueEntry_DoNotUse::NewEnumMessage_MappedValueEntry_DoNotUse(::_pbi::ConstantInitialized) {}
struct NewEnumMessage_MappedValueEntry_DoNotUseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR NewEnumMessage_MappedValueEntry_DoNotUseDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~NewEnumMessage_MappedValueEntry_DoNotUseDefaultTypeInternal() {}
  union {
    NewEnumMessage_MappedValueEntry_DoNotUse _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 NewEnumMessage_MappedValueEntry_DoNotUseDefaultTypeInternal _NewEnumMessage_MappedValueEntry_DoNotUse_default_instance_;
      template <typename>
PROTOBUF_CONSTEXPR MapMessage_StringToUnsignedLongEntry_DoNotUse::MapMessage_StringToUnsignedLongEntry_DoNotUse(::_pbi::ConstantInitialized) {}
struct MapMessage_StringToUnsignedLongEntry_DoNotUseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR MapMessage_StringToUnsignedLongEntry_DoNotUseDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~MapMessage_StringToUnsignedLongEntry_DoNotUseDefaultTypeInternal() {}
  union {
    MapMessage_StringToUnsignedLongEntry_DoNotUse _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 MapMessage_StringToUnsignedLongEntry_DoNotUseDefaultTypeInternal _MapMessage_StringToUnsignedLongEntry_DoNotUse_default_instance_;
      template <typename>
PROTOBUF_CONSTEXPR MapMessage_StringToStringEntry_DoNotUse::MapMessage_StringToStringEntry_DoNotUse(::_pbi::ConstantInitialized) {}
struct MapMessage_StringToStringEntry_DoNotUseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR MapMessage_StringToStringEntry_DoNotUseDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~MapMessage_StringToStringEntry_DoNotUseDefaultTypeInternal() {}
  union {
    MapMessage_StringToStringEntry_DoNotUse _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 MapMessage_StringToStringEntry_DoNotUseDefaultTypeInternal _MapMessage_StringToStringEntry_DoNotUse_default_instance_;
      template <typename>
PROTOBUF_CONSTEXPR MapMessage_StringToSignedLongEntry_DoNotUse::MapMessage_StringToSignedLongEntry_DoNotUse(::_pbi::ConstantInitialized) {}
struct MapMessage_StringToSignedLongEntry_DoNotUseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR MapMessage_StringToSignedLongEntry_DoNotUseDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~MapMessage_StringToSignedLongEntry_DoNotUseDefaultTypeInternal() {}
  union {
    MapMessage_StringToSignedLongEntry_DoNotUse _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 MapMessage_StringToSignedLongEntry_DoNotUseDefaultTypeInternal _MapMessage_StringToSignedLongEntry_DoNotUse_default_instance_;
      template <typename>
PROTOBUF_CONSTEXPR MapMessage_StringToNumberEntry_DoNotUse::MapMessage_StringToNumberEntry_DoNotUse(::_pbi::ConstantInitialized) {}
struct MapMessage_StringToNumberEntry_DoNotUseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR MapMessage_StringToNumberEntry_DoNotUseDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~MapMessage_StringToNumberEntry_DoNotUseDefaultTypeInternal() {}
  union {
    MapMessage_StringToNumberEntry_DoNotUse _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 MapMessage_StringToNumberEntry_DoNotUseDefaultTypeInternal _MapMessage_StringToNumberEntry_DoNotUse_default_instance_;
      template <typename>
PROTOBUF_CONSTEXPR MapMessage_StringToDoubleEntry_DoNotUse::MapMessage_StringToDoubleEntry_DoNotUse(::_pbi::ConstantInitialized) {}
struct MapMessage_StringToDoubleEntry_DoNotUseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR MapMessage_StringToDoubleEntry_DoNotUseDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~MapMessage_StringToDoubleEntry_DoNotUseDefaultTypeInternal() {}
  union {
    MapMessage_StringToDoubleEntry_DoNotUse _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 MapMessage_StringToDoubleEntry_DoNotUseDefaultTypeInternal _MapMessage_StringToDoubleEntry_DoNotUse_default_instance_;
      template <typename>
PROTOBUF_CONSTEXPR MapMessage_LongToStringEntry_DoNotUse::MapMessage_LongToStringEntry_DoNotUse(::_pbi::ConstantInitialized) {}
struct MapMessage_LongToStringEntry_DoNotUseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR MapMessage_LongToStringEntry_DoNotUseDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~MapMessage_LongToStringEntry_DoNotUseDefaultTypeInternal() {}
  union {
    MapMessage_LongToStringEntry_DoNotUse _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 MapMessage_LongToStringEntry_DoNotUseDefaultTypeInternal _MapMessage_LongToStringEntry_DoNotUse_default_instance_;
      template <typename>
PROTOBUF_CONSTEXPR MapMessage_IntToStringEntry_DoNotUse::MapMessage_IntToStringEntry_DoNotUse(::_pbi::ConstantInitialized) {}
struct MapMessage_IntToStringEntry_DoNotUseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR MapMessage_IntToStringEntry_DoNotUseDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~MapMessage_IntToStringEntry_DoNotUseDefaultTypeInternal() {}
  union {
    MapMessage_IntToStringEntry_DoNotUse _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 MapMessage_IntToStringEntry_DoNotUseDefaultTypeInternal _MapMessage_IntToStringEntry_DoNotUse_default_instance_;

inline constexpr RepeatedMessage::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : int32_{},
        _int32_cached_byte_size_{0},
        int64_{},
        _int64_cached_byte_size_{0},
        uint32_{},
        _uint32_cached_byte_size_{0},
        uint64_{},
        _uint64_cached_byte_size_{0},
        sint32_{},
        _sint32_cached_byte_size_{0},
        sint64_{},
        _sint64_cached_byte_size_{0},
        fixed32_{},
        fixed64_{},
        sfixed32_{},
        sfixed64_{},
        float__{},
        double__{},
        bool__{},
        string_{},
        bytes_{},
        enum__{},
        _enum__cached_byte_size_{0},
        self_message_{},
        other_message_{},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR RepeatedMessage::RepeatedMessage(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct RepeatedMessageDefaultTypeInternal {
  PROTOBUF_CONSTEXPR RepeatedMessageDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~RepeatedMessageDefaultTypeInternal() {}
  union {
    RepeatedMessage _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 RepeatedMessageDefaultTypeInternal _RepeatedMessage_default_instance_;

inline constexpr ParentMessage::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        child_message_{nullptr},
        child_enum_{static_cast< ::test::ParentMessage_ChildEnum >(0)} {}

template <typename>
PROTOBUF_CONSTEXPR ParentMessage::ParentMessage(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct ParentMessageDefaultTypeInternal {
  PROTOBUF_CONSTEXPR ParentMessageDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~ParentMessageDefaultTypeInternal() {}
  union {
    ParentMessage _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 ParentMessageDefaultTypeInternal _ParentMessage_default_instance_;

inline constexpr OneOfMessage::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : strings_{},
        messages_{},
        bytes_{},
        _cached_size_{0},
        _oneof_case_{} {}

template <typename>
PROTOBUF_CONSTEXPR OneOfMessage::OneOfMessage(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct OneOfMessageDefaultTypeInternal {
  PROTOBUF_CONSTEXPR OneOfMessageDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~OneOfMessageDefaultTypeInternal() {}
  union {
    OneOfMessage _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 OneOfMessageDefaultTypeInternal _OneOfMessage_default_instance_;

inline constexpr OldEnumMessage::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : repeated_value_{},
        _repeated_value_cached_byte_size_{0},
        mapped_value_{},
        value_{static_cast< ::test::OldEnumMessage_OldEnum >(0)},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR OldEnumMessage::OldEnumMessage(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct OldEnumMessageDefaultTypeInternal {
  PROTOBUF_CONSTEXPR OldEnumMessageDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~OldEnumMessageDefaultTypeInternal() {}
  union {
    OldEnumMessage _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 OldEnumMessageDefaultTypeInternal _OldEnumMessage_default_instance_;

inline constexpr NewEnumMessage::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : repeated_value_{},
        _repeated_value_cached_byte_size_{0},
        mapped_value_{},
        value_{static_cast< ::test::NewEnumMessage_NewEnum >(0)},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR NewEnumMessage::NewEnumMessage(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct NewEnumMessageDefaultTypeInternal {
  PROTOBUF_CONSTEXPR NewEnumMessageDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~NewEnumMessageDefaultTypeInternal() {}
  union {
    NewEnumMessage _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 NewEnumMessageDefaultTypeInternal _NewEnumMessage_default_instance_;

inline constexpr Message::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        string_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        bytes_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        self_message_{nullptr},
        other_message_{nullptr},
        int64_{::int64_t{0}},
        int32_{0},
        uint32_{0u},
        uint64_{::uint64_t{0u}},
        sint64_{::int64_t{0}},
        sint32_{0},
        fixed32_{0u},
        fixed64_{::uint64_t{0u}},
        sfixed64_{::int64_t{0}},
        sfixed32_{0},
        float__{0},
        double__{0},
        bool__{false},
        enum__{static_cast< ::test::Enum >(0)} {}

template <typename>
PROTOBUF_CONSTEXPR Message::Message(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct MessageDefaultTypeInternal {
  PROTOBUF_CONSTEXPR MessageDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~MessageDefaultTypeInternal() {}
  union {
    Message _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 MessageDefaultTypeInternal _Message_default_instance_;
      template <typename>
PROTOBUF_CONSTEXPR MapMessage_StringToMessageEntry_DoNotUse::MapMessage_StringToMessageEntry_DoNotUse(::_pbi::ConstantInitialized) {}
struct MapMessage_StringToMessageEntry_DoNotUseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR MapMessage_StringToMessageEntry_DoNotUseDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~MapMessage_StringToMessageEntry_DoNotUseDefaultTypeInternal() {}
  union {
    MapMessage_StringToMessageEntry_DoNotUse _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 MapMessage_StringToMessageEntry_DoNotUseDefaultTypeInternal _MapMessage_StringToMessageEntry_DoNotUse_default_instance_;

inline constexpr MapMessage::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : stringtostring_{},
        stringtonumber_{},
        stringtosignedlong_{},
        stringtounsignedlong_{},
        stringtodouble_{},
        stringtomessage_{},
        inttostring_{},
        longtostring_{},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR MapMessage::MapMessage(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct MapMessageDefaultTypeInternal {
  PROTOBUF_CONSTEXPR MapMessageDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~MapMessageDefaultTypeInternal() {}
  union {
    MapMessage _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 MapMessageDefaultTypeInternal _MapMessage_default_instance_;
}  // namespace test
static const ::_pb::EnumDescriptor* file_level_enum_descriptors_test_2eproto[4];
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_test_2eproto = nullptr;
const ::uint32_t
    TableStruct_test_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::test::OtherMessage, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::OtherMessage, _impl_.value_),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_._has_bits_),
        PROTOBUF_FIELD_OFFSET(::test::Message, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.int32_),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.int64_),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.uint32_),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.uint64_),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.sint32_),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.sint64_),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.fixed32_),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.fixed64_),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.sfixed32_),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.sfixed64_),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.float__),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.double__),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.bool__),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.string_),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.bytes_),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.enum__),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.self_message_),
        PROTOBUF_FIELD_OFFSET(::test::Message, _impl_.other_message_),
        ~0u,
        ~0u,
        ~0u,
        ~0u,
        ~0u,
        ~0u,
        ~0u,
        ~0u,
        ~0u,
        ~0u,
        ~0u,
        ~0u,
        ~0u,
        ~0u,
        ~0u,
        ~0u,
        0,
        1,
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.int32_),
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.int64_),
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.uint32_),
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.uint64_),
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.sint32_),
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.sint64_),
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.fixed32_),
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.fixed64_),
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.sfixed32_),
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.sfixed64_),
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.float__),
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.double__),
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.bool__),
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.string_),
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.bytes_),
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.enum__),
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.self_message_),
        PROTOBUF_FIELD_OFFSET(::test::RepeatedMessage, _impl_.other_message_),
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::test::ParentMessage_ChildMessage, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::ParentMessage_ChildMessage, _impl_.value_),
        PROTOBUF_FIELD_OFFSET(::test::ParentMessage, _impl_._has_bits_),
        PROTOBUF_FIELD_OFFSET(::test::ParentMessage, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::ParentMessage, _impl_.child_message_),
        PROTOBUF_FIELD_OFFSET(::test::ParentMessage, _impl_.child_enum_),
        0,
        ~0u,
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::test::OneOfMessage, _internal_metadata_),
        ~0u,  // no _extensions_
        PROTOBUF_FIELD_OFFSET(::test::OneOfMessage, _impl_._oneof_case_[0]),
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        ::_pbi::kInvalidFieldOffsetTag,
        ::_pbi::kInvalidFieldOffsetTag,
        ::_pbi::kInvalidFieldOffsetTag,
        ::_pbi::kInvalidFieldOffsetTag,
        ::_pbi::kInvalidFieldOffsetTag,
        ::_pbi::kInvalidFieldOffsetTag,
        PROTOBUF_FIELD_OFFSET(::test::OneOfMessage, _impl_.strings_),
        PROTOBUF_FIELD_OFFSET(::test::OneOfMessage, _impl_.messages_),
        PROTOBUF_FIELD_OFFSET(::test::OneOfMessage, _impl_.bytes_),
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::test::OldMessage, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::OldMessage, _impl_.old_value_),
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::test::NewMessage, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::NewMessage, _impl_.old_value_),
        PROTOBUF_FIELD_OFFSET(::test::NewMessage, _impl_.new_value_),
        PROTOBUF_FIELD_OFFSET(::test::OldEnumMessage_MappedValueEntry_DoNotUse, _has_bits_),
        PROTOBUF_FIELD_OFFSET(::test::OldEnumMessage_MappedValueEntry_DoNotUse, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::OldEnumMessage_MappedValueEntry_DoNotUse, key_),
        PROTOBUF_FIELD_OFFSET(::test::OldEnumMessage_MappedValueEntry_DoNotUse, value_),
        0,
        1,
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::test::OldEnumMessage, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::OldEnumMessage, _impl_.value_),
        PROTOBUF_FIELD_OFFSET(::test::OldEnumMessage, _impl_.repeated_value_),
        PROTOBUF_FIELD_OFFSET(::test::OldEnumMessage, _impl_.mapped_value_),
        PROTOBUF_FIELD_OFFSET(::test::NewEnumMessage_MappedValueEntry_DoNotUse, _has_bits_),
        PROTOBUF_FIELD_OFFSET(::test::NewEnumMessage_MappedValueEntry_DoNotUse, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::NewEnumMessage_MappedValueEntry_DoNotUse, key_),
        PROTOBUF_FIELD_OFFSET(::test::NewEnumMessage_MappedValueEntry_DoNotUse, value_),
        0,
        1,
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::test::NewEnumMessage, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::NewEnumMessage, _impl_.value_),
        PROTOBUF_FIELD_OFFSET(::test::NewEnumMessage, _impl_.repeated_value_),
        PROTOBUF_FIELD_OFFSET(::test::NewEnumMessage, _impl_.mapped_value_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToStringEntry_DoNotUse, _has_bits_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToStringEntry_DoNotUse, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToStringEntry_DoNotUse, key_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToStringEntry_DoNotUse, value_),
        0,
        1,
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToNumberEntry_DoNotUse, _has_bits_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToNumberEntry_DoNotUse, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToNumberEntry_DoNotUse, key_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToNumberEntry_DoNotUse, value_),
        0,
        1,
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToSignedLongEntry_DoNotUse, _has_bits_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToSignedLongEntry_DoNotUse, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToSignedLongEntry_DoNotUse, key_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToSignedLongEntry_DoNotUse, value_),
        0,
        1,
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToUnsignedLongEntry_DoNotUse, _has_bits_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToUnsignedLongEntry_DoNotUse, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToUnsignedLongEntry_DoNotUse, key_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToUnsignedLongEntry_DoNotUse, value_),
        0,
        1,
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToDoubleEntry_DoNotUse, _has_bits_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToDoubleEntry_DoNotUse, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToDoubleEntry_DoNotUse, key_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToDoubleEntry_DoNotUse, value_),
        0,
        1,
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToMessageEntry_DoNotUse, _has_bits_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToMessageEntry_DoNotUse, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToMessageEntry_DoNotUse, key_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_StringToMessageEntry_DoNotUse, value_),
        0,
        1,
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_IntToStringEntry_DoNotUse, _has_bits_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_IntToStringEntry_DoNotUse, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_IntToStringEntry_DoNotUse, key_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_IntToStringEntry_DoNotUse, value_),
        0,
        1,
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_LongToStringEntry_DoNotUse, _has_bits_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_LongToStringEntry_DoNotUse, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_LongToStringEntry_DoNotUse, key_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage_LongToStringEntry_DoNotUse, value_),
        0,
        1,
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::test::MapMessage, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::test::MapMessage, _impl_.stringtostring_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage, _impl_.stringtonumber_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage, _impl_.stringtosignedlong_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage, _impl_.stringtounsignedlong_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage, _impl_.stringtodouble_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage, _impl_.stringtomessage_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage, _impl_.inttostring_),
        PROTOBUF_FIELD_OFFSET(::test::MapMessage, _impl_.longtostring_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::test::OtherMessage)},
        {9, 35, -1, sizeof(::test::Message)},
        {53, -1, -1, sizeof(::test::RepeatedMessage)},
        {79, -1, -1, sizeof(::test::ParentMessage_ChildMessage)},
        {88, 98, -1, sizeof(::test::ParentMessage)},
        {100, -1, -1, sizeof(::test::OneOfMessage)},
        {117, -1, -1, sizeof(::test::OldMessage)},
        {126, -1, -1, sizeof(::test::NewMessage)},
        {136, 146, -1, sizeof(::test::OldEnumMessage_MappedValueEntry_DoNotUse)},
        {148, -1, -1, sizeof(::test::OldEnumMessage)},
        {159, 169, -1, sizeof(::test::NewEnumMessage_MappedValueEntry_DoNotUse)},
        {171, -1, -1, sizeof(::test::NewEnumMessage)},
        {182, 192, -1, sizeof(::test::MapMessage_StringToStringEntry_DoNotUse)},
        {194, 204, -1, sizeof(::test::MapMessage_StringToNumberEntry_DoNotUse)},
        {206, 216, -1, sizeof(::test::MapMessage_StringToSignedLongEntry_DoNotUse)},
        {218, 228, -1, sizeof(::test::MapMessage_StringToUnsignedLongEntry_DoNotUse)},
        {230, 240, -1, sizeof(::test::MapMessage_StringToDoubleEntry_DoNotUse)},
        {242, 252, -1, sizeof(::test::MapMessage_StringToMessageEntry_DoNotUse)},
        {254, 264, -1, sizeof(::test::MapMessage_IntToStringEntry_DoNotUse)},
        {266, 276, -1, sizeof(::test::MapMessage_LongToStringEntry_DoNotUse)},
        {278, -1, -1, sizeof(::test::MapMessage)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::test::_OtherMessage_default_instance_._instance,
    &::test::_Message_default_instance_._instance,
    &::test::_RepeatedMessage_default_instance_._instance,
    &::test::_ParentMessage_ChildMessage_default_instance_._instance,
    &::test::_ParentMessage_default_instance_._instance,
    &::test::_OneOfMessage_default_instance_._instance,
    &::test::_OldMessage_default_instance_._instance,
    &::test::_NewMessage_default_instance_._instance,
    &::test::_OldEnumMessage_MappedValueEntry_DoNotUse_default_instance_._instance,
    &::test::_OldEnumMessage_default_instance_._instance,
    &::test::_NewEnumMessage_MappedValueEntry_DoNotUse_default_instance_._instance,
    &::test::_NewEnumMessage_default_instance_._instance,
    &::test::_MapMessage_StringToStringEntry_DoNotUse_default_instance_._instance,
    &::test::_MapMessage_StringToNumberEntry_DoNotUse_default_instance_._instance,
    &::test::_MapMessage_StringToSignedLongEntry_DoNotUse_default_instance_._instance,
    &::test::_MapMessage_StringToUnsignedLongEntry_DoNotUse_default_instance_._instance,
    &::test::_MapMessage_StringToDoubleEntry_DoNotUse_default_instance_._instance,
    &::test::_MapMessage_StringToMessageEntry_DoNotUse_default_instance_._instance,
    &::test::_MapMessage_IntToStringEntry_DoNotUse_default_instance_._instance,
    &::test::_MapMessage_LongToStringEntry_DoNotUse_default_instance_._instance,
    &::test::_MapMessage_default_instance_._instance,
};
const char descriptor_table_protodef_test_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\ntest.proto\022\004test\"\035\n\014OtherMessage\022\r\n\005va"
    "lue\030\001 \001(\t\"\343\002\n\007Message\022\r\n\005int32\030\001 \001(\005\022\r\n\005"
    "int64\030\002 \001(\003\022\016\n\006uint32\030\003 \001(\r\022\016\n\006uint64\030\004 "
    "\001(\004\022\016\n\006sint32\030\005 \001(\021\022\016\n\006sint64\030\006 \001(\022\022\017\n\007f"
    "ixed32\030\007 \001(\007\022\017\n\007fixed64\030\010 \001(\006\022\020\n\010sfixed3"
    "2\030\t \001(\017\022\020\n\010sfixed64\030\n \001(\020\022\r\n\005float\030\013 \001(\002"
    "\022\016\n\006double\030\014 \001(\001\022\014\n\004bool\030\r \001(\010\022\016\n\006string"
    "\030\016 \001(\t\022\r\n\005bytes\030\017 \001(\014\022\030\n\004enum\030\020 \001(\0162\n.te"
    "st.Enum\022#\n\014self_message\030\021 \001(\0132\r.test.Mes"
    "sage\022)\n\rother_message\030\022 \001(\0132\022.test.Other"
    "Message\"\363\002\n\017RepeatedMessage\022\r\n\005int32\030\001 \003"
    "(\005\022\r\n\005int64\030\002 \003(\003\022\016\n\006uint32\030\003 \003(\r\022\016\n\006uin"
    "t64\030\004 \003(\004\022\016\n\006sint32\030\005 \003(\021\022\016\n\006sint64\030\006 \003("
    "\022\022\017\n\007fixed32\030\007 \003(\007\022\017\n\007fixed64\030\010 \003(\006\022\020\n\010s"
    "fixed32\030\t \003(\017\022\020\n\010sfixed64\030\n \003(\020\022\r\n\005float"
    "\030\013 \003(\002\022\016\n\006double\030\014 \003(\001\022\014\n\004bool\030\r \003(\010\022\016\n\006"
    "string\030\016 \003(\t\022\r\n\005bytes\030\017 \003(\014\022\030\n\004enum\030\020 \003("
    "\0162\n.test.Enum\022+\n\014self_message\030\021 \003(\0132\025.te"
    "st.RepeatedMessage\022)\n\rother_message\030\022 \003("
    "\0132\022.test.OtherMessage\"\301\001\n\rParentMessage\022"
    "7\n\rchild_message\030\001 \001(\0132 .test.ParentMess"
    "age.ChildMessage\0221\n\nchild_enum\030\002 \001(\0162\035.t"
    "est.ParentMessage.ChildEnum\032\035\n\014ChildMess"
    "age\022\r\n\005value\030\001 \001(\t\"%\n\tChildEnum\022\013\n\007VALUE"
    "_0\020\000\022\013\n\007VALUE_1\020\001\"\316\001\n\014OneOfMessage\022\022\n\010st"
    "ring_0\030\001 \001(\tH\000\022\022\n\010string_1\030\002 \001(\tH\000\022\'\n\tme"
    "ssage_0\030\003 \001(\0132\022.test.OtherMessageH\001\022\'\n\tm"
    "essage_1\030\004 \001(\0132\022.test.OtherMessageH\001\022\021\n\007"
    "bytes_0\030\005 \001(\014H\002\022\021\n\007bytes_1\030\006 \001(\014H\002B\t\n\007st"
    "ringsB\n\n\010messagesB\007\n\005bytes\"\037\n\nOldMessage"
    "\022\021\n\told_value\030\001 \001(\t\"2\n\nNewMessage\022\021\n\told"
    "_value\030\001 \001(\t\022\021\n\tnew_value\030\002 \001(\t\"\247\002\n\016OldE"
    "numMessage\022+\n\005value\030\001 \001(\0162\034.test.OldEnum"
    "Message.OldEnum\0224\n\016repeated_value\030\002 \003(\0162"
    "\034.test.OldEnumMessage.OldEnum\022;\n\014mapped_"
    "value\030\003 \003(\0132%.test.OldEnumMessage.Mapped"
    "ValueEntry\032P\n\020MappedValueEntry\022\013\n\003key\030\001 "
    "\001(\t\022+\n\005value\030\002 \001(\0162\034.test.OldEnumMessage"
    ".OldEnum:\0028\001\"#\n\007OldEnum\022\013\n\007VALUE_0\020\000\022\013\n\007"
    "VALUE_1\020\001\"\303\002\n\016NewEnumMessage\022+\n\005value\030\001 "
    "\001(\0162\034.test.NewEnumMessage.NewEnum\0224\n\016rep"
    "eated_value\030\002 \003(\0162\034.test.NewEnumMessage."
    "NewEnum\022;\n\014mapped_value\030\003 \003(\0132%.test.New"
    "EnumMessage.MappedValueEntry\032P\n\020MappedVa"
    "lueEntry\022\013\n\003key\030\001 \001(\t\022+\n\005value\030\002 \001(\0162\034.t"
    "est.NewEnumMessage.NewEnum:\0028\001\"\?\n\007NewEnu"
    "m\022\013\n\007VALUE_0\020\000\022\013\n\007VALUE_1\020\001\022\014\n\007VALUE_2\020\310"
    "\001\022\014\n\007VALUE_3\020\254\002\"\332\007\n\nMapMessage\022<\n\016string"
    "ToString\030\001 \003(\0132$.test.MapMessage.StringT"
    "oStringEntry\022<\n\016stringToNumber\030\002 \003(\0132$.t"
    "est.MapMessage.StringToNumberEntry\022D\n\022st"
    "ringToSignedLong\030\003 \003(\0132(.test.MapMessage"
    ".StringToSignedLongEntry\022H\n\024stringToUnsi"
    "gnedLong\030\004 \003(\0132*.test.MapMessage.StringT"
    "oUnsignedLongEntry\022<\n\016stringToDouble\030\005 \003"
    "(\0132$.test.MapMessage.StringToDoubleEntry"
    "\022>\n\017stringToMessage\030\006 \003(\0132%.test.MapMess"
    "age.StringToMessageEntry\0226\n\013intToString\030"
    "\007 \003(\0132!.test.MapMessage.IntToStringEntry"
    "\0228\n\014longToString\030\010 \003(\0132\".test.MapMessage"
    ".LongToStringEntry\0325\n\023StringToStringEntr"
    "y\022\013\n\003key\030\001 \001(\t\022\r\n\005value\030\002 \001(\t:\0028\001\0325\n\023Str"
    "ingToNumberEntry\022\013\n\003key\030\001 \001(\t\022\r\n\005value\030\002"
    " \001(\005:\0028\001\0329\n\027StringToSignedLongEntry\022\013\n\003k"
    "ey\030\001 \001(\t\022\r\n\005value\030\002 \001(\003:\0028\001\032;\n\031StringToU"
    "nsignedLongEntry\022\013\n\003key\030\001 \001(\t\022\r\n\005value\030\002"
    " \001(\004:\0028\001\0325\n\023StringToDoubleEntry\022\013\n\003key\030\001"
    " \001(\t\022\r\n\005value\030\002 \001(\001:\0028\001\032J\n\024StringToMessa"
    "geEntry\022\013\n\003key\030\001 \001(\t\022!\n\005value\030\002 \001(\0132\022.te"
    "st.OtherMessage:\0028\001\0322\n\020IntToStringEntry\022"
    "\013\n\003key\030\001 \001(\005\022\r\n\005value\030\002 \001(\t:\0028\001\0323\n\021LongT"
    "oStringEntry\022\013\n\003key\030\001 \001(\003\022\r\n\005value\030\002 \001(\t"
    ":\0028\001* \n\004Enum\022\013\n\007VALUE_0\020\000\022\013\n\007VALUE_1\020\001b\006"
    "proto3"
};
static ::absl::once_flag descriptor_table_test_2eproto_once;
PROTOBUF_CONSTINIT const ::_pbi::DescriptorTable descriptor_table_test_2eproto = {
    false,
    false,
    2926,
    descriptor_table_protodef_test_2eproto,
    "test.proto",
    &descriptor_table_test_2eproto_once,
    nullptr,
    0,
    21,
    schemas,
    file_default_instances,
    TableStruct_test_2eproto::offsets,
    file_level_enum_descriptors_test_2eproto,
    file_level_service_descriptors_test_2eproto,
};
namespace test {
const ::google::protobuf::EnumDescriptor* ParentMessage_ChildEnum_descriptor() {
  ::google::protobuf::internal::AssignDescriptors(&descriptor_table_test_2eproto);
  return file_level_enum_descriptors_test_2eproto[0];
}
PROTOBUF_CONSTINIT const uint32_t ParentMessage_ChildEnum_internal_data_[] = {
    131072u, 0u, };
bool ParentMessage_ChildEnum_IsValid(int value) {
  return 0 <= value && value <= 1;
}
#if (__cplusplus < 201703) && \
  (!defined(_MSC_VER) || (_MSC_VER >= 1900 && _MSC_VER < 1912))

constexpr ParentMessage_ChildEnum ParentMessage::VALUE_0;
constexpr ParentMessage_ChildEnum ParentMessage::VALUE_1;
constexpr ParentMessage_ChildEnum ParentMessage::ChildEnum_MIN;
constexpr ParentMessage_ChildEnum ParentMessage::ChildEnum_MAX;
constexpr int ParentMessage::ChildEnum_ARRAYSIZE;

#endif  // (__cplusplus < 201703) &&
        // (!defined(_MSC_VER) || (_MSC_VER >= 1900 && _MSC_VER < 1912))
const ::google::protobuf::EnumDescriptor* OldEnumMessage_OldEnum_descriptor() {
  ::google::protobuf::internal::AssignDescriptors(&descriptor_table_test_2eproto);
  return file_level_enum_descriptors_test_2eproto[1];
}
PROTOBUF_CONSTINIT const uint32_t OldEnumMessage_OldEnum_internal_data_[] = {
    131072u, 0u, };
bool OldEnumMessage_OldEnum_IsValid(int value) {
  return 0 <= value && value <= 1;
}
#if (__cplusplus < 201703) && \
  (!defined(_MSC_VER) || (_MSC_VER >= 1900 && _MSC_VER < 1912))

constexpr OldEnumMessage_OldEnum OldEnumMessage::VALUE_0;
constexpr OldEnumMessage_OldEnum OldEnumMessage::VALUE_1;
constexpr OldEnumMessage_OldEnum OldEnumMessage::OldEnum_MIN;
constexpr OldEnumMessage_OldEnum OldEnumMessage::OldEnum_MAX;
constexpr int OldEnumMessage::OldEnum_ARRAYSIZE;

#endif  // (__cplusplus < 201703) &&
        // (!defined(_MSC_VER) || (_MSC_VER >= 1900 && _MSC_VER < 1912))
const ::google::protobuf::EnumDescriptor* NewEnumMessage_NewEnum_descriptor() {
  ::google::protobuf::internal::AssignDescriptors(&descriptor_table_test_2eproto);
  return file_level_enum_descriptors_test_2eproto[2];
}
PROTOBUF_CONSTINIT const uint32_t NewEnumMessage_NewEnum_internal_data_[] = {
    131072u, 131072u, 300u, 200u, };
bool NewEnumMessage_NewEnum_IsValid(int value) {
  return ::_pbi::ValidateEnum(value, NewEnumMessage_NewEnum_internal_data_);
}
#if (__cplusplus < 201703) && \
  (!defined(_MSC_VER) || (_MSC_VER >= 1900 && _MSC_VER < 1912))

constexpr NewEnumMessage_NewEnum NewEnumMessage::VALUE_0;
constexpr NewEnumMessage_NewEnum NewEnumMessage::VALUE_1;
constexpr NewEnumMessage_NewEnum NewEnumMessage::VALUE_2;
constexpr NewEnumMessage_NewEnum NewEnumMessage::VALUE_3;
constexpr NewEnumMessage_NewEnum NewEnumMessage::NewEnum_MIN;
constexpr NewEnumMessage_NewEnum NewEnumMessage::NewEnum_MAX;
constexpr int NewEnumMessage::NewEnum_ARRAYSIZE;

#endif  // (__cplusplus < 201703) &&
        // (!defined(_MSC_VER) || (_MSC_VER >= 1900 && _MSC_VER < 1912))
const ::google::protobuf::EnumDescriptor* Enum_descriptor() {
  ::google::protobuf::internal::AssignDescriptors(&descriptor_table_test_2eproto);
  return file_level_enum_descriptors_test_2eproto[3];
}
PROTOBUF_CONSTINIT const uint32_t Enum_internal_data_[] = {
    131072u, 0u, };
bool Enum_IsValid(int value) {
  return 0 <= value && value <= 1;
}
// ===================================================================

class OtherMessage::_Internal {
 public:
};

OtherMessage::OtherMessage(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:test.OtherMessage)
}
inline PROTOBUF_NDEBUG_INLINE OtherMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::test::OtherMessage& from_msg)
      : value_(arena, from.value_),
        _cached_size_{0} {}

OtherMessage::OtherMessage(
    ::google::protobuf::Arena* arena,
    const OtherMessage& from)
    : ::google::protobuf::Message(arena) {
  OtherMessage* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:test.OtherMessage)
}
inline PROTOBUF_NDEBUG_INLINE OtherMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : value_(arena),
        _cached_size_{0} {}

inline void OtherMessage::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
OtherMessage::~OtherMessage() {
  // @@protoc_insertion_point(destructor:test.OtherMessage)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void OtherMessage::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.value_.Destroy();
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
OtherMessage::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(OtherMessage, _impl_._cached_size_),
              false,
          },
          &OtherMessage::MergeImpl,
          &OtherMessage::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 0, 31, 2> OtherMessage::_table_ = {
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
    &_OtherMessage_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::test::OtherMessage>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // string value = 1;
    {::_pbi::TcParser::FastUS1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(OtherMessage, _impl_.value_)}},
  }}, {{
    65535, 65535
  }}, {{
    // string value = 1;
    {PROTOBUF_FIELD_OFFSET(OtherMessage, _impl_.value_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
  }},
  // no aux_entries
  {{
    "\21\5\0\0\0\0\0\0"
    "test.OtherMessage"
    "value"
  }},
};

PROTOBUF_NOINLINE void OtherMessage::Clear() {
// @@protoc_insertion_point(message_clear_start:test.OtherMessage)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.value_.ClearToEmpty();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

::uint8_t* OtherMessage::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:test.OtherMessage)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // string value = 1;
  if (!this->_internal_value().empty()) {
    const std::string& _s = this->_internal_value();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.OtherMessage.value");
    target = stream->WriteStringMaybeAliased(1, _s, target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:test.OtherMessage)
  return target;
}

::size_t OtherMessage::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:test.OtherMessage)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // string value = 1;
  if (!this->_internal_value().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_value());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void OtherMessage::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<OtherMessage*>(&to_msg);
  auto& from = static_cast<const OtherMessage&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:test.OtherMessage)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_value().empty()) {
    _this->_internal_set_value(from._internal_value());
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void OtherMessage::CopyFrom(const OtherMessage& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:test.OtherMessage)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void OtherMessage::InternalSwap(OtherMessage* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.value_, &other->_impl_.value_, arena);
}

::google::protobuf::Metadata OtherMessage::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

class Message::_Internal {
 public:
  using HasBits =
      decltype(std::declval<Message>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
      8 * PROTOBUF_FIELD_OFFSET(Message, _impl_._has_bits_);
};

Message::Message(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:test.Message)
}
inline PROTOBUF_NDEBUG_INLINE Message::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::test::Message& from_msg)
      : _has_bits_{from._has_bits_},
        _cached_size_{0},
        string_(arena, from.string_),
        bytes_(arena, from.bytes_) {}

Message::Message(
    ::google::protobuf::Arena* arena,
    const Message& from)
    : ::google::protobuf::Message(arena) {
  Message* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  ::uint32_t cached_has_bits = _impl_._has_bits_[0];
  _impl_.self_message_ = (cached_has_bits & 0x00000001u) ? ::google::protobuf::Message::CopyConstruct<::test::Message>(
                              arena, *from._impl_.self_message_)
                        : nullptr;
  _impl_.other_message_ = (cached_has_bits & 0x00000002u) ? ::google::protobuf::Message::CopyConstruct<::test::OtherMessage>(
                              arena, *from._impl_.other_message_)
                        : nullptr;
  ::memcpy(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, int64_),
           reinterpret_cast<const char *>(&from._impl_) +
               offsetof(Impl_, int64_),
           offsetof(Impl_, enum__) -
               offsetof(Impl_, int64_) +
               sizeof(Impl_::enum__));

  // @@protoc_insertion_point(copy_constructor:test.Message)
}
inline PROTOBUF_NDEBUG_INLINE Message::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0},
        string_(arena),
        bytes_(arena) {}

inline void Message::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, self_message_),
           0,
           offsetof(Impl_, enum__) -
               offsetof(Impl_, self_message_) +
               sizeof(Impl_::enum__));
}
Message::~Message() {
  // @@protoc_insertion_point(destructor:test.Message)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void Message::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.string_.Destroy();
  _impl_.bytes_.Destroy();
  delete _impl_.self_message_;
  delete _impl_.other_message_;
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
Message::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(Message, _impl_._cached_size_),
              false,
          },
          &Message::MergeImpl,
          &Message::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<5, 18, 2, 43, 2> Message::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(Message, _impl_._has_bits_),
    0, // no _extensions_
    18, 248,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294705152,  // skipmap
    offsetof(decltype(_table_), field_entries),
    18,  // num_field_entries
    2,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_Message_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::test::Message>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // int32 int32 = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(Message, _impl_.int32_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(Message, _impl_.int32_)}},
    // int64 int64 = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(Message, _impl_.int64_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(Message, _impl_.int64_)}},
    // uint32 uint32 = 3;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(Message, _impl_.uint32_), 63>(),
     {24, 63, 0, PROTOBUF_FIELD_OFFSET(Message, _impl_.uint32_)}},
    // uint64 uint64 = 4;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(Message, _impl_.uint64_), 63>(),
     {32, 63, 0, PROTOBUF_FIELD_OFFSET(Message, _impl_.uint64_)}},
    // sint32 sint32 = 5;
    {::_pbi::TcParser::FastZ32S1,
     {40, 63, 0, PROTOBUF_FIELD_OFFSET(Message, _impl_.sint32_)}},
    // sint64 sint64 = 6;
    {::_pbi::TcParser::FastZ64S1,
     {48, 63, 0, PROTOBUF_FIELD_OFFSET(Message, _impl_.sint64_)}},
    // fixed32 fixed32 = 7;
    {::_pbi::TcParser::FastF32S1,
     {61, 63, 0, PROTOBUF_FIELD_OFFSET(Message, _impl_.fixed32_)}},
    // fixed64 fixed64 = 8;
    {::_pbi::TcParser::FastF64S1,
     {65, 63, 0, PROTOBUF_FIELD_OFFSET(Message, _impl_.fixed64_)}},
    // sfixed32 sfixed32 = 9;
    {::_pbi::TcParser::FastF32S1,
     {77, 63, 0, PROTOBUF_FIELD_OFFSET(Message, _impl_.sfixed32_)}},
    // sfixed64 sfixed64 = 10;
    {::_pbi::TcParser::FastF64S1,
     {81, 63, 0, PROTOBUF_FIELD_OFFSET(Message, _impl_.sfixed64_)}},
    // float float = 11;
    {::_pbi::TcParser::FastF32S1,
     {93, 63, 0, PROTOBUF_FIELD_OFFSET(Message, _impl_.float__)}},
    // double double = 12;
    {::_pbi::TcParser::FastF64S1,
     {97, 63, 0, PROTOBUF_FIELD_OFFSET(Message, _impl_.double__)}},
    // bool bool = 13;
    {::_pbi::TcParser::SingularVarintNoZag1<bool, offsetof(Message, _impl_.bool__), 63>(),
     {104, 63, 0, PROTOBUF_FIELD_OFFSET(Message, _impl_.bool__)}},
    // string string = 14;
    {::_pbi::TcParser::FastUS1,
     {114, 63, 0, PROTOBUF_FIELD_OFFSET(Message, _impl_.string_)}},
    // bytes bytes = 15;
    {::_pbi::TcParser::FastBS1,
     {122, 63, 0, PROTOBUF_FIELD_OFFSET(Message, _impl_.bytes_)}},
    // .test.Enum enum = 16;
    {::_pbi::TcParser::FastV32S2,
     {384, 63, 0, PROTOBUF_FIELD_OFFSET(Message, _impl_.enum__)}},
    // .test.Message self_message = 17;
    {::_pbi::TcParser::FastMtS2,
     {394, 0, 0, PROTOBUF_FIELD_OFFSET(Message, _impl_.self_message_)}},
    // .test.OtherMessage other_message = 18;
    {::_pbi::TcParser::FastMtS2,
     {402, 1, 1, PROTOBUF_FIELD_OFFSET(Message, _impl_.other_message_)}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
  }}, {{
    65535, 65535
  }}, {{
    // int32 int32 = 1;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.int32_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kInt32)},
    // int64 int64 = 2;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.int64_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kInt64)},
    // uint32 uint32 = 3;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.uint32_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt32)},
    // uint64 uint64 = 4;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.uint64_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUInt64)},
    // sint32 sint32 = 5;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.sint32_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kSInt32)},
    // sint64 sint64 = 6;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.sint64_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kSInt64)},
    // fixed32 fixed32 = 7;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.fixed32_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kFixed32)},
    // fixed64 fixed64 = 8;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.fixed64_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kFixed64)},
    // sfixed32 sfixed32 = 9;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.sfixed32_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kSFixed32)},
    // sfixed64 sfixed64 = 10;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.sfixed64_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kSFixed64)},
    // float float = 11;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.float__), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kFloat)},
    // double double = 12;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.double__), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kDouble)},
    // bool bool = 13;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.bool__), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kBool)},
    // string string = 14;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.string_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // bytes bytes = 15;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.bytes_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kBytes | ::_fl::kRepAString)},
    // .test.Enum enum = 16;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.enum__), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kOpenEnum)},
    // .test.Message self_message = 17;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.self_message_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
    // .test.OtherMessage other_message = 18;
    {PROTOBUF_FIELD_OFFSET(Message, _impl_.other_message_), _Internal::kHasBitsOffset + 1, 1,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::test::Message>()},
    {::_pbi::TcParser::GetTable<::test::OtherMessage>()},
  }}, {{
    "\14\0\0\0\0\0\0\0\0\0\0\0\0\0\6\0\0\0\0\0\0\0\0\0"
    "test.Message"
    "string"
  }},
};

PROTOBUF_NOINLINE void Message::Clear() {
// @@protoc_insertion_point(message_clear_start:test.Message)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.string_.ClearToEmpty();
  _impl_.bytes_.ClearToEmpty();
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000003u) {
    if (cached_has_bits & 0x00000001u) {
      ABSL_DCHECK(_impl_.self_message_ != nullptr);
      _impl_.self_message_->Clear();
    }
    if (cached_has_bits & 0x00000002u) {
      ABSL_DCHECK(_impl_.other_message_ != nullptr);
      _impl_.other_message_->Clear();
    }
  }
  ::memset(&_impl_.int64_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.enum__) -
      reinterpret_cast<char*>(&_impl_.int64_)) + sizeof(_impl_.enum__));
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

::uint8_t* Message::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:test.Message)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // int32 int32 = 1;
  if (this->_internal_int32() != 0) {
    target = ::google::protobuf::internal::WireFormatLite::
        WriteInt32ToArrayWithField<1>(
            stream, this->_internal_int32(), target);
  }

  // int64 int64 = 2;
  if (this->_internal_int64() != 0) {
    target = ::google::protobuf::internal::WireFormatLite::
        WriteInt64ToArrayWithField<2>(
            stream, this->_internal_int64(), target);
  }

  // uint32 uint32 = 3;
  if (this->_internal_uint32() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt32ToArray(
        3, this->_internal_uint32(), target);
  }

  // uint64 uint64 = 4;
  if (this->_internal_uint64() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteUInt64ToArray(
        4, this->_internal_uint64(), target);
  }

  // sint32 sint32 = 5;
  if (this->_internal_sint32() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteSInt32ToArray(
        5, this->_internal_sint32(), target);
  }

  // sint64 sint64 = 6;
  if (this->_internal_sint64() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteSInt64ToArray(
        6, this->_internal_sint64(), target);
  }

  // fixed32 fixed32 = 7;
  if (this->_internal_fixed32() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteFixed32ToArray(
        7, this->_internal_fixed32(), target);
  }

  // fixed64 fixed64 = 8;
  if (this->_internal_fixed64() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteFixed64ToArray(
        8, this->_internal_fixed64(), target);
  }

  // sfixed32 sfixed32 = 9;
  if (this->_internal_sfixed32() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteSFixed32ToArray(
        9, this->_internal_sfixed32(), target);
  }

  // sfixed64 sfixed64 = 10;
  if (this->_internal_sfixed64() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteSFixed64ToArray(
        10, this->_internal_sfixed64(), target);
  }

  // float float = 11;
  static_assert(sizeof(::uint32_t) == sizeof(float),
                "Code assumes ::uint32_t and float are the same size.");
  float tmp_float_ = this->_internal_float_();
  ::uint32_t raw_float_;
  memcpy(&raw_float_, &tmp_float_, sizeof(tmp_float_));
  if (raw_float_ != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteFloatToArray(
        11, this->_internal_float_(), target);
  }

  // double double = 12;
  static_assert(sizeof(::uint64_t) == sizeof(double),
                "Code assumes ::uint64_t and double are the same size.");
  double tmp_double_ = this->_internal_double_();
  ::uint64_t raw_double_;
  memcpy(&raw_double_, &tmp_double_, sizeof(tmp_double_));
  if (raw_double_ != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteDoubleToArray(
        12, this->_internal_double_(), target);
  }

  // bool bool = 13;
  if (this->_internal_bool_() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteBoolToArray(
        13, this->_internal_bool_(), target);
  }

  // string string = 14;
  if (!this->_internal_string().empty()) {
    const std::string& _s = this->_internal_string();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.Message.string");
    target = stream->WriteStringMaybeAliased(14, _s, target);
  }

  // bytes bytes = 15;
  if (!this->_internal_bytes().empty()) {
    const std::string& _s = this->_internal_bytes();
    target = stream->WriteBytesMaybeAliased(15, _s, target);
  }

  // .test.Enum enum = 16;
  if (this->_internal_enum_() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteEnumToArray(
        16, this->_internal_enum_(), target);
  }

  cached_has_bits = _impl_._has_bits_[0];
  // .test.Message self_message = 17;
  if (cached_has_bits & 0x00000001u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        17, *_impl_.self_message_, _impl_.self_message_->GetCachedSize(), target, stream);
  }

  // .test.OtherMessage other_message = 18;
  if (cached_has_bits & 0x00000002u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        18, *_impl_.other_message_, _impl_.other_message_->GetCachedSize(), target, stream);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:test.Message)
  return target;
}

::size_t Message::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:test.Message)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // string string = 14;
  if (!this->_internal_string().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_string());
  }

  // bytes bytes = 15;
  if (!this->_internal_bytes().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::BytesSize(
                                    this->_internal_bytes());
  }

  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000003u) {
    // .test.Message self_message = 17;
    if (cached_has_bits & 0x00000001u) {
      total_size +=
          2 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.self_message_);
    }

    // .test.OtherMessage other_message = 18;
    if (cached_has_bits & 0x00000002u) {
      total_size +=
          2 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.other_message_);
    }

  }
  // int64 int64 = 2;
  if (this->_internal_int64() != 0) {
    total_size += ::_pbi::WireFormatLite::Int64SizePlusOne(
        this->_internal_int64());
  }

  // int32 int32 = 1;
  if (this->_internal_int32() != 0) {
    total_size += ::_pbi::WireFormatLite::Int32SizePlusOne(
        this->_internal_int32());
  }

  // uint32 uint32 = 3;
  if (this->_internal_uint32() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt32SizePlusOne(
        this->_internal_uint32());
  }

  // uint64 uint64 = 4;
  if (this->_internal_uint64() != 0) {
    total_size += ::_pbi::WireFormatLite::UInt64SizePlusOne(
        this->_internal_uint64());
  }

  // sint64 sint64 = 6;
  if (this->_internal_sint64() != 0) {
    total_size += ::_pbi::WireFormatLite::SInt64SizePlusOne(
        this->_internal_sint64());
  }

  // sint32 sint32 = 5;
  if (this->_internal_sint32() != 0) {
    total_size += ::_pbi::WireFormatLite::SInt32SizePlusOne(
        this->_internal_sint32());
  }

  // fixed32 fixed32 = 7;
  if (this->_internal_fixed32() != 0) {
    total_size += 5;
  }

  // fixed64 fixed64 = 8;
  if (this->_internal_fixed64() != 0) {
    total_size += 9;
  }

  // sfixed64 sfixed64 = 10;
  if (this->_internal_sfixed64() != 0) {
    total_size += 9;
  }

  // sfixed32 sfixed32 = 9;
  if (this->_internal_sfixed32() != 0) {
    total_size += 5;
  }

  // float float = 11;
  static_assert(sizeof(::uint32_t) == sizeof(float),
                "Code assumes ::uint32_t and float are the same size.");
  float tmp_float_ = this->_internal_float_();
  ::uint32_t raw_float_;
  memcpy(&raw_float_, &tmp_float_, sizeof(tmp_float_));
  if (raw_float_ != 0) {
    total_size += 5;
  }

  // double double = 12;
  static_assert(sizeof(::uint64_t) == sizeof(double),
                "Code assumes ::uint64_t and double are the same size.");
  double tmp_double_ = this->_internal_double_();
  ::uint64_t raw_double_;
  memcpy(&raw_double_, &tmp_double_, sizeof(tmp_double_));
  if (raw_double_ != 0) {
    total_size += 9;
  }

  // bool bool = 13;
  if (this->_internal_bool_() != 0) {
    total_size += 2;
  }

  // .test.Enum enum = 16;
  if (this->_internal_enum_() != 0) {
    total_size += 2 +
                  ::_pbi::WireFormatLite::EnumSize(this->_internal_enum_());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void Message::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<Message*>(&to_msg);
  auto& from = static_cast<const Message&>(from_msg);
  ::google::protobuf::Arena* arena = _this->GetArena();
  // @@protoc_insertion_point(class_specific_merge_from_start:test.Message)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_string().empty()) {
    _this->_internal_set_string(from._internal_string());
  }
  if (!from._internal_bytes().empty()) {
    _this->_internal_set_bytes(from._internal_bytes());
  }
  cached_has_bits = from._impl_._has_bits_[0];
  if (cached_has_bits & 0x00000003u) {
    if (cached_has_bits & 0x00000001u) {
      ABSL_DCHECK(from._impl_.self_message_ != nullptr);
      if (_this->_impl_.self_message_ == nullptr) {
        _this->_impl_.self_message_ =
            ::google::protobuf::Message::CopyConstruct<::test::Message>(arena, *from._impl_.self_message_);
      } else {
        _this->_impl_.self_message_->MergeFrom(*from._impl_.self_message_);
      }
    }
    if (cached_has_bits & 0x00000002u) {
      ABSL_DCHECK(from._impl_.other_message_ != nullptr);
      if (_this->_impl_.other_message_ == nullptr) {
        _this->_impl_.other_message_ =
            ::google::protobuf::Message::CopyConstruct<::test::OtherMessage>(arena, *from._impl_.other_message_);
      } else {
        _this->_impl_.other_message_->MergeFrom(*from._impl_.other_message_);
      }
    }
  }
  if (from._internal_int64() != 0) {
    _this->_impl_.int64_ = from._impl_.int64_;
  }
  if (from._internal_int32() != 0) {
    _this->_impl_.int32_ = from._impl_.int32_;
  }
  if (from._internal_uint32() != 0) {
    _this->_impl_.uint32_ = from._impl_.uint32_;
  }
  if (from._internal_uint64() != 0) {
    _this->_impl_.uint64_ = from._impl_.uint64_;
  }
  if (from._internal_sint64() != 0) {
    _this->_impl_.sint64_ = from._impl_.sint64_;
  }
  if (from._internal_sint32() != 0) {
    _this->_impl_.sint32_ = from._impl_.sint32_;
  }
  if (from._internal_fixed32() != 0) {
    _this->_impl_.fixed32_ = from._impl_.fixed32_;
  }
  if (from._internal_fixed64() != 0) {
    _this->_impl_.fixed64_ = from._impl_.fixed64_;
  }
  if (from._internal_sfixed64() != 0) {
    _this->_impl_.sfixed64_ = from._impl_.sfixed64_;
  }
  if (from._internal_sfixed32() != 0) {
    _this->_impl_.sfixed32_ = from._impl_.sfixed32_;
  }
  static_assert(sizeof(::uint32_t) == sizeof(float),
                "Code assumes ::uint32_t and float are the same size.");
  float tmp_float_ = from._internal_float_();
  ::uint32_t raw_float_;
  memcpy(&raw_float_, &tmp_float_, sizeof(tmp_float_));
  if (raw_float_ != 0) {
    _this->_impl_.float__ = from._impl_.float__;
  }
  static_assert(sizeof(::uint64_t) == sizeof(double),
                "Code assumes ::uint64_t and double are the same size.");
  double tmp_double_ = from._internal_double_();
  ::uint64_t raw_double_;
  memcpy(&raw_double_, &tmp_double_, sizeof(tmp_double_));
  if (raw_double_ != 0) {
    _this->_impl_.double__ = from._impl_.double__;
  }
  if (from._internal_bool_() != 0) {
    _this->_impl_.bool__ = from._impl_.bool__;
  }
  if (from._internal_enum_() != 0) {
    _this->_impl_.enum__ = from._impl_.enum__;
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void Message::CopyFrom(const Message& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:test.Message)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void Message::InternalSwap(Message* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.string_, &other->_impl_.string_, arena);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.bytes_, &other->_impl_.bytes_, arena);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(Message, _impl_.enum__)
      + sizeof(Message::_impl_.enum__)
      - PROTOBUF_FIELD_OFFSET(Message, _impl_.self_message_)>(
          reinterpret_cast<char*>(&_impl_.self_message_),
          reinterpret_cast<char*>(&other->_impl_.self_message_));
}

::google::protobuf::Metadata Message::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

class RepeatedMessage::_Internal {
 public:
};

RepeatedMessage::RepeatedMessage(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:test.RepeatedMessage)
}
inline PROTOBUF_NDEBUG_INLINE RepeatedMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::test::RepeatedMessage& from_msg)
      : int32_{visibility, arena, from.int32_},
        _int32_cached_byte_size_{0},
        int64_{visibility, arena, from.int64_},
        _int64_cached_byte_size_{0},
        uint32_{visibility, arena, from.uint32_},
        _uint32_cached_byte_size_{0},
        uint64_{visibility, arena, from.uint64_},
        _uint64_cached_byte_size_{0},
        sint32_{visibility, arena, from.sint32_},
        _sint32_cached_byte_size_{0},
        sint64_{visibility, arena, from.sint64_},
        _sint64_cached_byte_size_{0},
        fixed32_{visibility, arena, from.fixed32_},
        fixed64_{visibility, arena, from.fixed64_},
        sfixed32_{visibility, arena, from.sfixed32_},
        sfixed64_{visibility, arena, from.sfixed64_},
        float__{visibility, arena, from.float__},
        double__{visibility, arena, from.double__},
        bool__{visibility, arena, from.bool__},
        string_{visibility, arena, from.string_},
        bytes_{visibility, arena, from.bytes_},
        enum__{visibility, arena, from.enum__},
        _enum__cached_byte_size_{0},
        self_message_{visibility, arena, from.self_message_},
        other_message_{visibility, arena, from.other_message_},
        _cached_size_{0} {}

RepeatedMessage::RepeatedMessage(
    ::google::protobuf::Arena* arena,
    const RepeatedMessage& from)
    : ::google::protobuf::Message(arena) {
  RepeatedMessage* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:test.RepeatedMessage)
}
inline PROTOBUF_NDEBUG_INLINE RepeatedMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : int32_{visibility, arena},
        _int32_cached_byte_size_{0},
        int64_{visibility, arena},
        _int64_cached_byte_size_{0},
        uint32_{visibility, arena},
        _uint32_cached_byte_size_{0},
        uint64_{visibility, arena},
        _uint64_cached_byte_size_{0},
        sint32_{visibility, arena},
        _sint32_cached_byte_size_{0},
        sint64_{visibility, arena},
        _sint64_cached_byte_size_{0},
        fixed32_{visibility, arena},
        fixed64_{visibility, arena},
        sfixed32_{visibility, arena},
        sfixed64_{visibility, arena},
        float__{visibility, arena},
        double__{visibility, arena},
        bool__{visibility, arena},
        string_{visibility, arena},
        bytes_{visibility, arena},
        enum__{visibility, arena},
        _enum__cached_byte_size_{0},
        self_message_{visibility, arena},
        other_message_{visibility, arena},
        _cached_size_{0} {}

inline void RepeatedMessage::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
RepeatedMessage::~RepeatedMessage() {
  // @@protoc_insertion_point(destructor:test.RepeatedMessage)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void RepeatedMessage::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
RepeatedMessage::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_._cached_size_),
              false,
          },
          &RepeatedMessage::MergeImpl,
          &RepeatedMessage::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<5, 18, 2, 51, 2> RepeatedMessage::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    18, 248,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294705152,  // skipmap
    offsetof(decltype(_table_), field_entries),
    18,  // num_field_entries
    2,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_RepeatedMessage_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::test::RepeatedMessage>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // repeated int32 int32 = 1;
    {::_pbi::TcParser::FastV32P1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.int32_)}},
    // repeated int64 int64 = 2;
    {::_pbi::TcParser::FastV64P1,
     {18, 63, 0, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.int64_)}},
    // repeated uint32 uint32 = 3;
    {::_pbi::TcParser::FastV32P1,
     {26, 63, 0, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.uint32_)}},
    // repeated uint64 uint64 = 4;
    {::_pbi::TcParser::FastV64P1,
     {34, 63, 0, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.uint64_)}},
    // repeated sint32 sint32 = 5;
    {::_pbi::TcParser::FastZ32P1,
     {42, 63, 0, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.sint32_)}},
    // repeated sint64 sint64 = 6;
    {::_pbi::TcParser::FastZ64P1,
     {50, 63, 0, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.sint64_)}},
    // repeated fixed32 fixed32 = 7;
    {::_pbi::TcParser::FastF32P1,
     {58, 63, 0, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.fixed32_)}},
    // repeated fixed64 fixed64 = 8;
    {::_pbi::TcParser::FastF64P1,
     {66, 63, 0, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.fixed64_)}},
    // repeated sfixed32 sfixed32 = 9;
    {::_pbi::TcParser::FastF32P1,
     {74, 63, 0, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.sfixed32_)}},
    // repeated sfixed64 sfixed64 = 10;
    {::_pbi::TcParser::FastF64P1,
     {82, 63, 0, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.sfixed64_)}},
    // repeated float float = 11;
    {::_pbi::TcParser::FastF32P1,
     {90, 63, 0, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.float__)}},
    // repeated double double = 12;
    {::_pbi::TcParser::FastF64P1,
     {98, 63, 0, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.double__)}},
    // repeated bool bool = 13;
    {::_pbi::TcParser::FastV8P1,
     {106, 63, 0, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.bool__)}},
    // repeated string string = 14;
    {::_pbi::TcParser::FastUR1,
     {114, 63, 0, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.string_)}},
    // repeated bytes bytes = 15;
    {::_pbi::TcParser::FastBR1,
     {122, 63, 0, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.bytes_)}},
    // repeated .test.Enum enum = 16;
    {::_pbi::TcParser::FastV32P2,
     {386, 63, 0, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.enum__)}},
    // repeated .test.RepeatedMessage self_message = 17;
    {::_pbi::TcParser::FastMtR2,
     {394, 63, 0, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.self_message_)}},
    // repeated .test.OtherMessage other_message = 18;
    {::_pbi::TcParser::FastMtR2,
     {402, 63, 1, PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.other_message_)}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
    {::_pbi::TcParser::MiniParse, {}},
  }}, {{
    65535, 65535
  }}, {{
    // repeated int32 int32 = 1;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.int32_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedInt32)},
    // repeated int64 int64 = 2;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.int64_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedInt64)},
    // repeated uint32 uint32 = 3;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.uint32_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedUInt32)},
    // repeated uint64 uint64 = 4;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.uint64_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedUInt64)},
    // repeated sint32 sint32 = 5;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.sint32_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedSInt32)},
    // repeated sint64 sint64 = 6;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.sint64_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedSInt64)},
    // repeated fixed32 fixed32 = 7;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.fixed32_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedFixed32)},
    // repeated fixed64 fixed64 = 8;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.fixed64_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedFixed64)},
    // repeated sfixed32 sfixed32 = 9;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.sfixed32_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedSFixed32)},
    // repeated sfixed64 sfixed64 = 10;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.sfixed64_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedSFixed64)},
    // repeated float float = 11;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.float__), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedFloat)},
    // repeated double double = 12;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.double__), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedDouble)},
    // repeated bool bool = 13;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.bool__), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedBool)},
    // repeated string string = 14;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.string_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kUtf8String | ::_fl::kRepSString)},
    // repeated bytes bytes = 15;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.bytes_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kBytes | ::_fl::kRepSString)},
    // repeated .test.Enum enum = 16;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.enum__), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedOpenEnum)},
    // repeated .test.RepeatedMessage self_message = 17;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.self_message_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
    // repeated .test.OtherMessage other_message = 18;
    {PROTOBUF_FIELD_OFFSET(RepeatedMessage, _impl_.other_message_), 0, 1,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::test::RepeatedMessage>()},
    {::_pbi::TcParser::GetTable<::test::OtherMessage>()},
  }}, {{
    "\24\0\0\0\0\0\0\0\0\0\0\0\0\0\6\0\0\0\0\0\0\0\0\0"
    "test.RepeatedMessage"
    "string"
  }},
};

PROTOBUF_NOINLINE void RepeatedMessage::Clear() {
// @@protoc_insertion_point(message_clear_start:test.RepeatedMessage)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.int32_.Clear();
  _impl_.int64_.Clear();
  _impl_.uint32_.Clear();
  _impl_.uint64_.Clear();
  _impl_.sint32_.Clear();
  _impl_.sint64_.Clear();
  _impl_.fixed32_.Clear();
  _impl_.fixed64_.Clear();
  _impl_.sfixed32_.Clear();
  _impl_.sfixed64_.Clear();
  _impl_.float__.Clear();
  _impl_.double__.Clear();
  _impl_.bool__.Clear();
  _impl_.string_.Clear();
  _impl_.bytes_.Clear();
  _impl_.enum__.Clear();
  _impl_.self_message_.Clear();
  _impl_.other_message_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

::uint8_t* RepeatedMessage::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:test.RepeatedMessage)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // repeated int32 int32 = 1;
  {
    int byte_size = _impl_._int32_cached_byte_size_.Get();
    if (byte_size > 0) {
      target = stream->WriteInt32Packed(
          1, _internal_int32(), byte_size, target);
    }
  }

  // repeated int64 int64 = 2;
  {
    int byte_size = _impl_._int64_cached_byte_size_.Get();
    if (byte_size > 0) {
      target = stream->WriteInt64Packed(
          2, _internal_int64(), byte_size, target);
    }
  }

  // repeated uint32 uint32 = 3;
  {
    int byte_size = _impl_._uint32_cached_byte_size_.Get();
    if (byte_size > 0) {
      target = stream->WriteUInt32Packed(
          3, _internal_uint32(), byte_size, target);
    }
  }

  // repeated uint64 uint64 = 4;
  {
    int byte_size = _impl_._uint64_cached_byte_size_.Get();
    if (byte_size > 0) {
      target = stream->WriteUInt64Packed(
          4, _internal_uint64(), byte_size, target);
    }
  }

  // repeated sint32 sint32 = 5;
  {
    int byte_size = _impl_._sint32_cached_byte_size_.Get();
    if (byte_size > 0) {
      target = stream->WriteSInt32Packed(
          5, _internal_sint32(), byte_size, target);
    }
  }

  // repeated sint64 sint64 = 6;
  {
    int byte_size = _impl_._sint64_cached_byte_size_.Get();
    if (byte_size > 0) {
      target = stream->WriteSInt64Packed(
          6, _internal_sint64(), byte_size, target);
    }
  }

  // repeated fixed32 fixed32 = 7;
  if (this->_internal_fixed32_size() > 0) {
    target = stream->WriteFixedPacked(7, _internal_fixed32(), target);
  }

  // repeated fixed64 fixed64 = 8;
  if (this->_internal_fixed64_size() > 0) {
    target = stream->WriteFixedPacked(8, _internal_fixed64(), target);
  }

  // repeated sfixed32 sfixed32 = 9;
  if (this->_internal_sfixed32_size() > 0) {
    target = stream->WriteFixedPacked(9, _internal_sfixed32(), target);
  }

  // repeated sfixed64 sfixed64 = 10;
  if (this->_internal_sfixed64_size() > 0) {
    target = stream->WriteFixedPacked(10, _internal_sfixed64(), target);
  }

  // repeated float float = 11;
  if (this->_internal_float__size() > 0) {
    target = stream->WriteFixedPacked(11, _internal_float_(), target);
  }

  // repeated double double = 12;
  if (this->_internal_double__size() > 0) {
    target = stream->WriteFixedPacked(12, _internal_double_(), target);
  }

  // repeated bool bool = 13;
  if (this->_internal_bool__size() > 0) {
    target = stream->WriteFixedPacked(13, _internal_bool_(), target);
  }

  // repeated string string = 14;
  for (int i = 0, n = this->_internal_string_size(); i < n; ++i) {
    const auto& s = this->_internal_string().Get(i);
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        s.data(), static_cast<int>(s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.RepeatedMessage.string");
    target = stream->WriteString(14, s, target);
  }

  // repeated bytes bytes = 15;
  for (int i = 0, n = this->_internal_bytes_size(); i < n; ++i) {
    const auto& s = this->_internal_bytes().Get(i);
    target = stream->WriteBytes(15, s, target);
  }

  // repeated .test.Enum enum = 16;
  {
    std::size_t byte_size = _impl_._enum__cached_byte_size_.Get();
    if (byte_size > 0) {
      target = stream->WriteEnumPacked(16, _internal_enum_(),
                                       byte_size, target);
    }
  }

  // repeated .test.RepeatedMessage self_message = 17;
  for (unsigned i = 0, n = static_cast<unsigned>(
                           this->_internal_self_message_size());
       i < n; i++) {
    const auto& repfield = this->_internal_self_message().Get(i);
    target =
        ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
            17, repfield, repfield.GetCachedSize(),
            target, stream);
  }

  // repeated .test.OtherMessage other_message = 18;
  for (unsigned i = 0, n = static_cast<unsigned>(
                           this->_internal_other_message_size());
       i < n; i++) {
    const auto& repfield = this->_internal_other_message().Get(i);
    target =
        ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
            18, repfield, repfield.GetCachedSize(),
            target, stream);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:test.RepeatedMessage)
  return target;
}

::size_t RepeatedMessage::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:test.RepeatedMessage)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // repeated int32 int32 = 1;
  {
    std::size_t data_size = ::_pbi::WireFormatLite::Int32Size(
        this->_internal_int32())
    ;
    _impl_._int32_cached_byte_size_.Set(::_pbi::ToCachedSize(data_size));
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // repeated int64 int64 = 2;
  {
    std::size_t data_size = ::_pbi::WireFormatLite::Int64Size(
        this->_internal_int64())
    ;
    _impl_._int64_cached_byte_size_.Set(::_pbi::ToCachedSize(data_size));
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // repeated uint32 uint32 = 3;
  {
    std::size_t data_size = ::_pbi::WireFormatLite::UInt32Size(
        this->_internal_uint32())
    ;
    _impl_._uint32_cached_byte_size_.Set(::_pbi::ToCachedSize(data_size));
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // repeated uint64 uint64 = 4;
  {
    std::size_t data_size = ::_pbi::WireFormatLite::UInt64Size(
        this->_internal_uint64())
    ;
    _impl_._uint64_cached_byte_size_.Set(::_pbi::ToCachedSize(data_size));
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // repeated sint32 sint32 = 5;
  {
    std::size_t data_size = ::_pbi::WireFormatLite::SInt32Size(
        this->_internal_sint32())
    ;
    _impl_._sint32_cached_byte_size_.Set(::_pbi::ToCachedSize(data_size));
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // repeated sint64 sint64 = 6;
  {
    std::size_t data_size = ::_pbi::WireFormatLite::SInt64Size(
        this->_internal_sint64())
    ;
    _impl_._sint64_cached_byte_size_.Set(::_pbi::ToCachedSize(data_size));
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // repeated fixed32 fixed32 = 7;
  {
    std::size_t data_size = std::size_t{4} *
        ::_pbi::FromIntSize(this->_internal_fixed32_size())
    ;
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // repeated fixed64 fixed64 = 8;
  {
    std::size_t data_size = std::size_t{8} *
        ::_pbi::FromIntSize(this->_internal_fixed64_size())
    ;
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // repeated sfixed32 sfixed32 = 9;
  {
    std::size_t data_size = std::size_t{4} *
        ::_pbi::FromIntSize(this->_internal_sfixed32_size())
    ;
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // repeated sfixed64 sfixed64 = 10;
  {
    std::size_t data_size = std::size_t{8} *
        ::_pbi::FromIntSize(this->_internal_sfixed64_size())
    ;
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // repeated float float = 11;
  {
    std::size_t data_size = std::size_t{4} *
        ::_pbi::FromIntSize(this->_internal_float__size())
    ;
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // repeated double double = 12;
  {
    std::size_t data_size = std::size_t{8} *
        ::_pbi::FromIntSize(this->_internal_double__size())
    ;
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // repeated bool bool = 13;
  {
    std::size_t data_size = std::size_t{1} *
        ::_pbi::FromIntSize(this->_internal_bool__size())
    ;
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // repeated string string = 14;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_string().size());
  for (int i = 0, n = _internal_string().size(); i < n; ++i) {
    total_size += ::google::protobuf::internal::WireFormatLite::StringSize(
        _internal_string().Get(i));
  }
  // repeated bytes bytes = 15;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_bytes().size());
  for (int i = 0, n = _internal_bytes().size(); i < n; ++i) {
    total_size += ::google::protobuf::internal::WireFormatLite::BytesSize(
        _internal_bytes().Get(i));
  }
  // repeated .test.Enum enum = 16;
  {
    std::size_t data_size = 0;
    auto count = static_cast<std::size_t>(this->_internal_enum__size());

    for (std::size_t i = 0; i < count; ++i) {
      data_size += ::_pbi::WireFormatLite::EnumSize(
          this->_internal_enum_().Get(static_cast<int>(i)));
    }
    total_size += data_size;
    if (data_size > 0) {
      total_size += 2;
      total_size += ::_pbi::WireFormatLite::Int32Size(
          static_cast<int32_t>(data_size));
    }
    _impl_._enum__cached_byte_size_.Set(::_pbi::ToCachedSize(data_size));
  }
  // repeated .test.RepeatedMessage self_message = 17;
  total_size += 2UL * this->_internal_self_message_size();
  for (const auto& msg : this->_internal_self_message()) {
    total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  // repeated .test.OtherMessage other_message = 18;
  total_size += 2UL * this->_internal_other_message_size();
  for (const auto& msg : this->_internal_other_message()) {
    total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void RepeatedMessage::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<RepeatedMessage*>(&to_msg);
  auto& from = static_cast<const RepeatedMessage&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:test.RepeatedMessage)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_int32()->MergeFrom(from._internal_int32());
  _this->_internal_mutable_int64()->MergeFrom(from._internal_int64());
  _this->_internal_mutable_uint32()->MergeFrom(from._internal_uint32());
  _this->_internal_mutable_uint64()->MergeFrom(from._internal_uint64());
  _this->_internal_mutable_sint32()->MergeFrom(from._internal_sint32());
  _this->_internal_mutable_sint64()->MergeFrom(from._internal_sint64());
  _this->_internal_mutable_fixed32()->MergeFrom(from._internal_fixed32());
  _this->_internal_mutable_fixed64()->MergeFrom(from._internal_fixed64());
  _this->_internal_mutable_sfixed32()->MergeFrom(from._internal_sfixed32());
  _this->_internal_mutable_sfixed64()->MergeFrom(from._internal_sfixed64());
  _this->_internal_mutable_float_()->MergeFrom(from._internal_float_());
  _this->_internal_mutable_double_()->MergeFrom(from._internal_double_());
  _this->_internal_mutable_bool_()->MergeFrom(from._internal_bool_());
  _this->_internal_mutable_string()->MergeFrom(from._internal_string());
  _this->_internal_mutable_bytes()->MergeFrom(from._internal_bytes());
  _this->_internal_mutable_enum_()->MergeFrom(from._internal_enum_());
  _this->_internal_mutable_self_message()->MergeFrom(
      from._internal_self_message());
  _this->_internal_mutable_other_message()->MergeFrom(
      from._internal_other_message());
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void RepeatedMessage::CopyFrom(const RepeatedMessage& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:test.RepeatedMessage)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void RepeatedMessage::InternalSwap(RepeatedMessage* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.int32_.InternalSwap(&other->_impl_.int32_);
  _impl_.int64_.InternalSwap(&other->_impl_.int64_);
  _impl_.uint32_.InternalSwap(&other->_impl_.uint32_);
  _impl_.uint64_.InternalSwap(&other->_impl_.uint64_);
  _impl_.sint32_.InternalSwap(&other->_impl_.sint32_);
  _impl_.sint64_.InternalSwap(&other->_impl_.sint64_);
  _impl_.fixed32_.InternalSwap(&other->_impl_.fixed32_);
  _impl_.fixed64_.InternalSwap(&other->_impl_.fixed64_);
  _impl_.sfixed32_.InternalSwap(&other->_impl_.sfixed32_);
  _impl_.sfixed64_.InternalSwap(&other->_impl_.sfixed64_);
  _impl_.float__.InternalSwap(&other->_impl_.float__);
  _impl_.double__.InternalSwap(&other->_impl_.double__);
  _impl_.bool__.InternalSwap(&other->_impl_.bool__);
  _impl_.string_.InternalSwap(&other->_impl_.string_);
  _impl_.bytes_.InternalSwap(&other->_impl_.bytes_);
  _impl_.enum__.InternalSwap(&other->_impl_.enum__);
  _impl_.self_message_.InternalSwap(&other->_impl_.self_message_);
  _impl_.other_message_.InternalSwap(&other->_impl_.other_message_);
}

::google::protobuf::Metadata RepeatedMessage::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

class ParentMessage_ChildMessage::_Internal {
 public:
};

ParentMessage_ChildMessage::ParentMessage_ChildMessage(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:test.ParentMessage.ChildMessage)
}
inline PROTOBUF_NDEBUG_INLINE ParentMessage_ChildMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::test::ParentMessage_ChildMessage& from_msg)
      : value_(arena, from.value_),
        _cached_size_{0} {}

ParentMessage_ChildMessage::ParentMessage_ChildMessage(
    ::google::protobuf::Arena* arena,
    const ParentMessage_ChildMessage& from)
    : ::google::protobuf::Message(arena) {
  ParentMessage_ChildMessage* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:test.ParentMessage.ChildMessage)
}
inline PROTOBUF_NDEBUG_INLINE ParentMessage_ChildMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : value_(arena),
        _cached_size_{0} {}

inline void ParentMessage_ChildMessage::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
ParentMessage_ChildMessage::~ParentMessage_ChildMessage() {
  // @@protoc_insertion_point(destructor:test.ParentMessage.ChildMessage)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void ParentMessage_ChildMessage::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.value_.Destroy();
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
ParentMessage_ChildMessage::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(ParentMessage_ChildMessage, _impl_._cached_size_),
              false,
          },
          &ParentMessage_ChildMessage::MergeImpl,
          &ParentMessage_ChildMessage::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 0, 45, 2> ParentMessage_ChildMessage::_table_ = {
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
    &_ParentMessage_ChildMessage_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::test::ParentMessage_ChildMessage>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // string value = 1;
    {::_pbi::TcParser::FastUS1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(ParentMessage_ChildMessage, _impl_.value_)}},
  }}, {{
    65535, 65535
  }}, {{
    // string value = 1;
    {PROTOBUF_FIELD_OFFSET(ParentMessage_ChildMessage, _impl_.value_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
  }},
  // no aux_entries
  {{
    "\37\5\0\0\0\0\0\0"
    "test.ParentMessage.ChildMessage"
    "value"
  }},
};

PROTOBUF_NOINLINE void ParentMessage_ChildMessage::Clear() {
// @@protoc_insertion_point(message_clear_start:test.ParentMessage.ChildMessage)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.value_.ClearToEmpty();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

::uint8_t* ParentMessage_ChildMessage::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:test.ParentMessage.ChildMessage)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // string value = 1;
  if (!this->_internal_value().empty()) {
    const std::string& _s = this->_internal_value();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.ParentMessage.ChildMessage.value");
    target = stream->WriteStringMaybeAliased(1, _s, target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:test.ParentMessage.ChildMessage)
  return target;
}

::size_t ParentMessage_ChildMessage::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:test.ParentMessage.ChildMessage)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // string value = 1;
  if (!this->_internal_value().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_value());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void ParentMessage_ChildMessage::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<ParentMessage_ChildMessage*>(&to_msg);
  auto& from = static_cast<const ParentMessage_ChildMessage&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:test.ParentMessage.ChildMessage)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_value().empty()) {
    _this->_internal_set_value(from._internal_value());
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void ParentMessage_ChildMessage::CopyFrom(const ParentMessage_ChildMessage& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:test.ParentMessage.ChildMessage)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void ParentMessage_ChildMessage::InternalSwap(ParentMessage_ChildMessage* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.value_, &other->_impl_.value_, arena);
}

::google::protobuf::Metadata ParentMessage_ChildMessage::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

class ParentMessage::_Internal {
 public:
  using HasBits =
      decltype(std::declval<ParentMessage>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
      8 * PROTOBUF_FIELD_OFFSET(ParentMessage, _impl_._has_bits_);
};

ParentMessage::ParentMessage(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:test.ParentMessage)
}
inline PROTOBUF_NDEBUG_INLINE ParentMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::test::ParentMessage& from_msg)
      : _has_bits_{from._has_bits_},
        _cached_size_{0} {}

ParentMessage::ParentMessage(
    ::google::protobuf::Arena* arena,
    const ParentMessage& from)
    : ::google::protobuf::Message(arena) {
  ParentMessage* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  ::uint32_t cached_has_bits = _impl_._has_bits_[0];
  _impl_.child_message_ = (cached_has_bits & 0x00000001u) ? ::google::protobuf::Message::CopyConstruct<::test::ParentMessage_ChildMessage>(
                              arena, *from._impl_.child_message_)
                        : nullptr;
  _impl_.child_enum_ = from._impl_.child_enum_;

  // @@protoc_insertion_point(copy_constructor:test.ParentMessage)
}
inline PROTOBUF_NDEBUG_INLINE ParentMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void ParentMessage::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, child_message_),
           0,
           offsetof(Impl_, child_enum_) -
               offsetof(Impl_, child_message_) +
               sizeof(Impl_::child_enum_));
}
ParentMessage::~ParentMessage() {
  // @@protoc_insertion_point(destructor:test.ParentMessage)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void ParentMessage::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  delete _impl_.child_message_;
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
ParentMessage::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(ParentMessage, _impl_._cached_size_),
              false,
          },
          &ParentMessage::MergeImpl,
          &ParentMessage::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 1, 0, 2> ParentMessage::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(ParentMessage, _impl_._has_bits_),
    0, // no _extensions_
    2, 8,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967292,  // skipmap
    offsetof(decltype(_table_), field_entries),
    2,  // num_field_entries
    1,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_ParentMessage_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::test::ParentMessage>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // .test.ParentMessage.ChildEnum child_enum = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(ParentMessage, _impl_.child_enum_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(ParentMessage, _impl_.child_enum_)}},
    // .test.ParentMessage.ChildMessage child_message = 1;
    {::_pbi::TcParser::FastMtS1,
     {10, 0, 0, PROTOBUF_FIELD_OFFSET(ParentMessage, _impl_.child_message_)}},
  }}, {{
    65535, 65535
  }}, {{
    // .test.ParentMessage.ChildMessage child_message = 1;
    {PROTOBUF_FIELD_OFFSET(ParentMessage, _impl_.child_message_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
    // .test.ParentMessage.ChildEnum child_enum = 2;
    {PROTOBUF_FIELD_OFFSET(ParentMessage, _impl_.child_enum_), -1, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kOpenEnum)},
  }}, {{
    {::_pbi::TcParser::GetTable<::test::ParentMessage_ChildMessage>()},
  }}, {{
  }},
};

PROTOBUF_NOINLINE void ParentMessage::Clear() {
// @@protoc_insertion_point(message_clear_start:test.ParentMessage)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(_impl_.child_message_ != nullptr);
    _impl_.child_message_->Clear();
  }
  _impl_.child_enum_ = 0;
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

::uint8_t* ParentMessage::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:test.ParentMessage)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  cached_has_bits = _impl_._has_bits_[0];
  // .test.ParentMessage.ChildMessage child_message = 1;
  if (cached_has_bits & 0x00000001u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        1, *_impl_.child_message_, _impl_.child_message_->GetCachedSize(), target, stream);
  }

  // .test.ParentMessage.ChildEnum child_enum = 2;
  if (this->_internal_child_enum() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteEnumToArray(
        2, this->_internal_child_enum(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:test.ParentMessage)
  return target;
}

::size_t ParentMessage::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:test.ParentMessage)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // .test.ParentMessage.ChildMessage child_message = 1;
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    total_size +=
        1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.child_message_);
  }

  // .test.ParentMessage.ChildEnum child_enum = 2;
  if (this->_internal_child_enum() != 0) {
    total_size += 1 +
                  ::_pbi::WireFormatLite::EnumSize(this->_internal_child_enum());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void ParentMessage::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<ParentMessage*>(&to_msg);
  auto& from = static_cast<const ParentMessage&>(from_msg);
  ::google::protobuf::Arena* arena = _this->GetArena();
  // @@protoc_insertion_point(class_specific_merge_from_start:test.ParentMessage)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  cached_has_bits = from._impl_._has_bits_[0];
  if (cached_has_bits & 0x00000001u) {
    ABSL_DCHECK(from._impl_.child_message_ != nullptr);
    if (_this->_impl_.child_message_ == nullptr) {
      _this->_impl_.child_message_ =
          ::google::protobuf::Message::CopyConstruct<::test::ParentMessage_ChildMessage>(arena, *from._impl_.child_message_);
    } else {
      _this->_impl_.child_message_->MergeFrom(*from._impl_.child_message_);
    }
  }
  if (from._internal_child_enum() != 0) {
    _this->_impl_.child_enum_ = from._impl_.child_enum_;
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void ParentMessage::CopyFrom(const ParentMessage& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:test.ParentMessage)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void ParentMessage::InternalSwap(ParentMessage* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(ParentMessage, _impl_.child_enum_)
      + sizeof(ParentMessage::_impl_.child_enum_)
      - PROTOBUF_FIELD_OFFSET(ParentMessage, _impl_.child_message_)>(
          reinterpret_cast<char*>(&_impl_.child_message_),
          reinterpret_cast<char*>(&other->_impl_.child_message_));
}

::google::protobuf::Metadata ParentMessage::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

class OneOfMessage::_Internal {
 public:
  static constexpr ::int32_t kOneofCaseOffset =
      PROTOBUF_FIELD_OFFSET(::test::OneOfMessage, _impl_._oneof_case_);
};

void OneOfMessage::set_allocated_message_0(::test::OtherMessage* message_0) {
  ::google::protobuf::Arena* message_arena = GetArena();
  clear_messages();
  if (message_0) {
    ::google::protobuf::Arena* submessage_arena = message_0->GetArena();
    if (message_arena != submessage_arena) {
      message_0 = ::google::protobuf::internal::GetOwnedMessage(message_arena, message_0, submessage_arena);
    }
    set_has_message_0();
    _impl_.messages_.message_0_ = message_0;
  }
  // @@protoc_insertion_point(field_set_allocated:test.OneOfMessage.message_0)
}
void OneOfMessage::set_allocated_message_1(::test::OtherMessage* message_1) {
  ::google::protobuf::Arena* message_arena = GetArena();
  clear_messages();
  if (message_1) {
    ::google::protobuf::Arena* submessage_arena = message_1->GetArena();
    if (message_arena != submessage_arena) {
      message_1 = ::google::protobuf::internal::GetOwnedMessage(message_arena, message_1, submessage_arena);
    }
    set_has_message_1();
    _impl_.messages_.message_1_ = message_1;
  }
  // @@protoc_insertion_point(field_set_allocated:test.OneOfMessage.message_1)
}
OneOfMessage::OneOfMessage(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:test.OneOfMessage)
}
inline PROTOBUF_NDEBUG_INLINE OneOfMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::test::OneOfMessage& from_msg)
      : strings_{},
        messages_{},
        bytes_{},
        _cached_size_{0},
        _oneof_case_{from._oneof_case_[0], from._oneof_case_[1], from._oneof_case_[2]} {}

OneOfMessage::OneOfMessage(
    ::google::protobuf::Arena* arena,
    const OneOfMessage& from)
    : ::google::protobuf::Message(arena) {
  OneOfMessage* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  switch (strings_case()) {
    case STRINGS_NOT_SET:
      break;
      case kString0:
        new (&_impl_.strings_.string_0_) decltype(_impl_.strings_.string_0_){arena, from._impl_.strings_.string_0_};
        break;
      case kString1:
        new (&_impl_.strings_.string_1_) decltype(_impl_.strings_.string_1_){arena, from._impl_.strings_.string_1_};
        break;
  }
  switch (messages_case()) {
    case MESSAGES_NOT_SET:
      break;
      case kMessage0:
        _impl_.messages_.message_0_ = ::google::protobuf::Message::CopyConstruct<::test::OtherMessage>(arena, *from._impl_.messages_.message_0_);
        break;
      case kMessage1:
        _impl_.messages_.message_1_ = ::google::protobuf::Message::CopyConstruct<::test::OtherMessage>(arena, *from._impl_.messages_.message_1_);
        break;
  }
  switch (bytes_case()) {
    case BYTES_NOT_SET:
      break;
      case kBytes0:
        new (&_impl_.bytes_.bytes_0_) decltype(_impl_.bytes_.bytes_0_){arena, from._impl_.bytes_.bytes_0_};
        break;
      case kBytes1:
        new (&_impl_.bytes_.bytes_1_) decltype(_impl_.bytes_.bytes_1_){arena, from._impl_.bytes_.bytes_1_};
        break;
  }

  // @@protoc_insertion_point(copy_constructor:test.OneOfMessage)
}
inline PROTOBUF_NDEBUG_INLINE OneOfMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : strings_{},
        messages_{},
        bytes_{},
        _cached_size_{0},
        _oneof_case_{} {}

inline void OneOfMessage::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
OneOfMessage::~OneOfMessage() {
  // @@protoc_insertion_point(destructor:test.OneOfMessage)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void OneOfMessage::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  if (has_strings()) {
    clear_strings();
  }
  if (has_messages()) {
    clear_messages();
  }
  if (has_bytes()) {
    clear_bytes();
  }
  _impl_.~Impl_();
}

void OneOfMessage::clear_strings() {
// @@protoc_insertion_point(one_of_clear_start:test.OneOfMessage)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  switch (strings_case()) {
    case kString0: {
      _impl_.strings_.string_0_.Destroy();
      break;
    }
    case kString1: {
      _impl_.strings_.string_1_.Destroy();
      break;
    }
    case STRINGS_NOT_SET: {
      break;
    }
  }
  _impl_._oneof_case_[0] = STRINGS_NOT_SET;
}

void OneOfMessage::clear_messages() {
// @@protoc_insertion_point(one_of_clear_start:test.OneOfMessage)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  switch (messages_case()) {
    case kMessage0: {
      if (GetArena() == nullptr) {
        delete _impl_.messages_.message_0_;
      } else if (::google::protobuf::internal::DebugHardenClearOneofMessageOnArena()) {
        ::google::protobuf::internal::MaybePoisonAfterClear(_impl_.messages_.message_0_);
      }
      break;
    }
    case kMessage1: {
      if (GetArena() == nullptr) {
        delete _impl_.messages_.message_1_;
      } else if (::google::protobuf::internal::DebugHardenClearOneofMessageOnArena()) {
        ::google::protobuf::internal::MaybePoisonAfterClear(_impl_.messages_.message_1_);
      }
      break;
    }
    case MESSAGES_NOT_SET: {
      break;
    }
  }
  _impl_._oneof_case_[1] = MESSAGES_NOT_SET;
}

void OneOfMessage::clear_bytes() {
// @@protoc_insertion_point(one_of_clear_start:test.OneOfMessage)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  switch (bytes_case()) {
    case kBytes0: {
      _impl_.bytes_.bytes_0_.Destroy();
      break;
    }
    case kBytes1: {
      _impl_.bytes_.bytes_1_.Destroy();
      break;
    }
    case BYTES_NOT_SET: {
      break;
    }
  }
  _impl_._oneof_case_[2] = BYTES_NOT_SET;
}


const ::google::protobuf::MessageLite::ClassData*
OneOfMessage::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(OneOfMessage, _impl_._cached_size_),
              false,
          },
          &OneOfMessage::MergeImpl,
          &OneOfMessage::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 6, 2, 42, 2> OneOfMessage::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    6, 0,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967232,  // skipmap
    offsetof(decltype(_table_), field_entries),
    6,  // num_field_entries
    2,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_OneOfMessage_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::test::OneOfMessage>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
  }}, {{
    65535, 65535
  }}, {{
    // string string_0 = 1;
    {PROTOBUF_FIELD_OFFSET(OneOfMessage, _impl_.strings_.string_0_), _Internal::kOneofCaseOffset + 0, 0,
    (0 | ::_fl::kFcOneof | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // string string_1 = 2;
    {PROTOBUF_FIELD_OFFSET(OneOfMessage, _impl_.strings_.string_1_), _Internal::kOneofCaseOffset + 0, 0,
    (0 | ::_fl::kFcOneof | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // .test.OtherMessage message_0 = 3;
    {PROTOBUF_FIELD_OFFSET(OneOfMessage, _impl_.messages_.message_0_), _Internal::kOneofCaseOffset + 4, 0,
    (0 | ::_fl::kFcOneof | ::_fl::kMessage | ::_fl::kTvTable)},
    // .test.OtherMessage message_1 = 4;
    {PROTOBUF_FIELD_OFFSET(OneOfMessage, _impl_.messages_.message_1_), _Internal::kOneofCaseOffset + 4, 1,
    (0 | ::_fl::kFcOneof | ::_fl::kMessage | ::_fl::kTvTable)},
    // bytes bytes_0 = 5;
    {PROTOBUF_FIELD_OFFSET(OneOfMessage, _impl_.bytes_.bytes_0_), _Internal::kOneofCaseOffset + 8, 0,
    (0 | ::_fl::kFcOneof | ::_fl::kBytes | ::_fl::kRepAString)},
    // bytes bytes_1 = 6;
    {PROTOBUF_FIELD_OFFSET(OneOfMessage, _impl_.bytes_.bytes_1_), _Internal::kOneofCaseOffset + 8, 0,
    (0 | ::_fl::kFcOneof | ::_fl::kBytes | ::_fl::kRepAString)},
  }}, {{
    {::_pbi::TcParser::GetTable<::test::OtherMessage>()},
    {::_pbi::TcParser::GetTable<::test::OtherMessage>()},
  }}, {{
    "\21\10\10\0\0\0\0\0"
    "test.OneOfMessage"
    "string_0"
    "string_1"
  }},
};

PROTOBUF_NOINLINE void OneOfMessage::Clear() {
// @@protoc_insertion_point(message_clear_start:test.OneOfMessage)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  clear_strings();
  clear_messages();
  clear_bytes();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

::uint8_t* OneOfMessage::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:test.OneOfMessage)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  switch (strings_case()) {
    case kString0: {
      const std::string& _s = this->_internal_string_0();
      ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
          _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.OneOfMessage.string_0");
      target = stream->WriteStringMaybeAliased(1, _s, target);
      break;
    }
    case kString1: {
      const std::string& _s = this->_internal_string_1();
      ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
          _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.OneOfMessage.string_1");
      target = stream->WriteStringMaybeAliased(2, _s, target);
      break;
    }
    default:
      break;
  }
  switch (messages_case()) {
    case kMessage0: {
      target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
          3, *_impl_.messages_.message_0_, _impl_.messages_.message_0_->GetCachedSize(), target, stream);
      break;
    }
    case kMessage1: {
      target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
          4, *_impl_.messages_.message_1_, _impl_.messages_.message_1_->GetCachedSize(), target, stream);
      break;
    }
    default:
      break;
  }
  switch (bytes_case()) {
    case kBytes0: {
      const std::string& _s = this->_internal_bytes_0();
      target = stream->WriteBytesMaybeAliased(5, _s, target);
      break;
    }
    case kBytes1: {
      const std::string& _s = this->_internal_bytes_1();
      target = stream->WriteBytesMaybeAliased(6, _s, target);
      break;
    }
    default:
      break;
  }
  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:test.OneOfMessage)
  return target;
}

::size_t OneOfMessage::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:test.OneOfMessage)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  switch (strings_case()) {
    // string string_0 = 1;
    case kString0: {
      total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                      this->_internal_string_0());
      break;
    }
    // string string_1 = 2;
    case kString1: {
      total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                      this->_internal_string_1());
      break;
    }
    case STRINGS_NOT_SET: {
      break;
    }
  }
  switch (messages_case()) {
    // .test.OtherMessage message_0 = 3;
    case kMessage0: {
      total_size +=
          1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.messages_.message_0_);
      break;
    }
    // .test.OtherMessage message_1 = 4;
    case kMessage1: {
      total_size +=
          1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.messages_.message_1_);
      break;
    }
    case MESSAGES_NOT_SET: {
      break;
    }
  }
  switch (bytes_case()) {
    // bytes bytes_0 = 5;
    case kBytes0: {
      total_size += 1 + ::google::protobuf::internal::WireFormatLite::BytesSize(
                                      this->_internal_bytes_0());
      break;
    }
    // bytes bytes_1 = 6;
    case kBytes1: {
      total_size += 1 + ::google::protobuf::internal::WireFormatLite::BytesSize(
                                      this->_internal_bytes_1());
      break;
    }
    case BYTES_NOT_SET: {
      break;
    }
  }
  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void OneOfMessage::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<OneOfMessage*>(&to_msg);
  auto& from = static_cast<const OneOfMessage&>(from_msg);
  ::google::protobuf::Arena* arena = _this->GetArena();
  // @@protoc_insertion_point(class_specific_merge_from_start:test.OneOfMessage)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (const uint32_t oneof_from_case = from._impl_._oneof_case_[0]) {
    const uint32_t oneof_to_case = _this->_impl_._oneof_case_[0];
    const bool oneof_needs_init = oneof_to_case != oneof_from_case;
    if (oneof_needs_init) {
      if (oneof_to_case != 0) {
        _this->clear_strings();
      }
      _this->_impl_._oneof_case_[0] = oneof_from_case;
    }

    switch (oneof_from_case) {
      case kString0: {
        if (oneof_needs_init) {
          _this->_impl_.strings_.string_0_.InitDefault();
        }
        _this->_impl_.strings_.string_0_.Set(from._internal_string_0(), arena);
        break;
      }
      case kString1: {
        if (oneof_needs_init) {
          _this->_impl_.strings_.string_1_.InitDefault();
        }
        _this->_impl_.strings_.string_1_.Set(from._internal_string_1(), arena);
        break;
      }
      case STRINGS_NOT_SET:
        break;
    }
  }
  if (const uint32_t oneof_from_case = from._impl_._oneof_case_[1]) {
    const uint32_t oneof_to_case = _this->_impl_._oneof_case_[1];
    const bool oneof_needs_init = oneof_to_case != oneof_from_case;
    if (oneof_needs_init) {
      if (oneof_to_case != 0) {
        _this->clear_messages();
      }
      _this->_impl_._oneof_case_[1] = oneof_from_case;
    }

    switch (oneof_from_case) {
      case kMessage0: {
        if (oneof_needs_init) {
          _this->_impl_.messages_.message_0_ =
              ::google::protobuf::Message::CopyConstruct<::test::OtherMessage>(arena, *from._impl_.messages_.message_0_);
        } else {
          _this->_impl_.messages_.message_0_->MergeFrom(from._internal_message_0());
        }
        break;
      }
      case kMessage1: {
        if (oneof_needs_init) {
          _this->_impl_.messages_.message_1_ =
              ::google::protobuf::Message::CopyConstruct<::test::OtherMessage>(arena, *from._impl_.messages_.message_1_);
        } else {
          _this->_impl_.messages_.message_1_->MergeFrom(from._internal_message_1());
        }
        break;
      }
      case MESSAGES_NOT_SET:
        break;
    }
  }
  if (const uint32_t oneof_from_case = from._impl_._oneof_case_[2]) {
    const uint32_t oneof_to_case = _this->_impl_._oneof_case_[2];
    const bool oneof_needs_init = oneof_to_case != oneof_from_case;
    if (oneof_needs_init) {
      if (oneof_to_case != 0) {
        _this->clear_bytes();
      }
      _this->_impl_._oneof_case_[2] = oneof_from_case;
    }

    switch (oneof_from_case) {
      case kBytes0: {
        if (oneof_needs_init) {
          _this->_impl_.bytes_.bytes_0_.InitDefault();
        }
        _this->_impl_.bytes_.bytes_0_.Set(from._internal_bytes_0(), arena);
        break;
      }
      case kBytes1: {
        if (oneof_needs_init) {
          _this->_impl_.bytes_.bytes_1_.InitDefault();
        }
        _this->_impl_.bytes_.bytes_1_.Set(from._internal_bytes_1(), arena);
        break;
      }
      case BYTES_NOT_SET:
        break;
    }
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void OneOfMessage::CopyFrom(const OneOfMessage& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:test.OneOfMessage)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void OneOfMessage::InternalSwap(OneOfMessage* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_.strings_, other->_impl_.strings_);
  swap(_impl_.messages_, other->_impl_.messages_);
  swap(_impl_.bytes_, other->_impl_.bytes_);
  swap(_impl_._oneof_case_[0], other->_impl_._oneof_case_[0]);
  swap(_impl_._oneof_case_[1], other->_impl_._oneof_case_[1]);
  swap(_impl_._oneof_case_[2], other->_impl_._oneof_case_[2]);
}

::google::protobuf::Metadata OneOfMessage::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

class OldMessage::_Internal {
 public:
};

OldMessage::OldMessage(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:test.OldMessage)
}
inline PROTOBUF_NDEBUG_INLINE OldMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::test::OldMessage& from_msg)
      : old_value_(arena, from.old_value_),
        _cached_size_{0} {}

OldMessage::OldMessage(
    ::google::protobuf::Arena* arena,
    const OldMessage& from)
    : ::google::protobuf::Message(arena) {
  OldMessage* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:test.OldMessage)
}
inline PROTOBUF_NDEBUG_INLINE OldMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : old_value_(arena),
        _cached_size_{0} {}

inline void OldMessage::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
OldMessage::~OldMessage() {
  // @@protoc_insertion_point(destructor:test.OldMessage)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void OldMessage::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.old_value_.Destroy();
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
OldMessage::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(OldMessage, _impl_._cached_size_),
              false,
          },
          &OldMessage::MergeImpl,
          &OldMessage::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 0, 33, 2> OldMessage::_table_ = {
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
    &_OldMessage_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::test::OldMessage>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // string old_value = 1;
    {::_pbi::TcParser::FastUS1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(OldMessage, _impl_.old_value_)}},
  }}, {{
    65535, 65535
  }}, {{
    // string old_value = 1;
    {PROTOBUF_FIELD_OFFSET(OldMessage, _impl_.old_value_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
  }},
  // no aux_entries
  {{
    "\17\11\0\0\0\0\0\0"
    "test.OldMessage"
    "old_value"
  }},
};

PROTOBUF_NOINLINE void OldMessage::Clear() {
// @@protoc_insertion_point(message_clear_start:test.OldMessage)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.old_value_.ClearToEmpty();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

::uint8_t* OldMessage::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:test.OldMessage)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // string old_value = 1;
  if (!this->_internal_old_value().empty()) {
    const std::string& _s = this->_internal_old_value();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.OldMessage.old_value");
    target = stream->WriteStringMaybeAliased(1, _s, target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:test.OldMessage)
  return target;
}

::size_t OldMessage::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:test.OldMessage)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // string old_value = 1;
  if (!this->_internal_old_value().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_old_value());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void OldMessage::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<OldMessage*>(&to_msg);
  auto& from = static_cast<const OldMessage&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:test.OldMessage)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_old_value().empty()) {
    _this->_internal_set_old_value(from._internal_old_value());
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void OldMessage::CopyFrom(const OldMessage& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:test.OldMessage)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void OldMessage::InternalSwap(OldMessage* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.old_value_, &other->_impl_.old_value_, arena);
}

::google::protobuf::Metadata OldMessage::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

class NewMessage::_Internal {
 public:
};

NewMessage::NewMessage(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:test.NewMessage)
}
inline PROTOBUF_NDEBUG_INLINE NewMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::test::NewMessage& from_msg)
      : old_value_(arena, from.old_value_),
        new_value_(arena, from.new_value_),
        _cached_size_{0} {}

NewMessage::NewMessage(
    ::google::protobuf::Arena* arena,
    const NewMessage& from)
    : ::google::protobuf::Message(arena) {
  NewMessage* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:test.NewMessage)
}
inline PROTOBUF_NDEBUG_INLINE NewMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : old_value_(arena),
        new_value_(arena),
        _cached_size_{0} {}

inline void NewMessage::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
NewMessage::~NewMessage() {
  // @@protoc_insertion_point(destructor:test.NewMessage)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void NewMessage::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.old_value_.Destroy();
  _impl_.new_value_.Destroy();
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
NewMessage::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(NewMessage, _impl_._cached_size_),
              false,
          },
          &NewMessage::MergeImpl,
          &NewMessage::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 0, 42, 2> NewMessage::_table_ = {
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
    &_NewMessage_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::test::NewMessage>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // string new_value = 2;
    {::_pbi::TcParser::FastUS1,
     {18, 63, 0, PROTOBUF_FIELD_OFFSET(NewMessage, _impl_.new_value_)}},
    // string old_value = 1;
    {::_pbi::TcParser::FastUS1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(NewMessage, _impl_.old_value_)}},
  }}, {{
    65535, 65535
  }}, {{
    // string old_value = 1;
    {PROTOBUF_FIELD_OFFSET(NewMessage, _impl_.old_value_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // string new_value = 2;
    {PROTOBUF_FIELD_OFFSET(NewMessage, _impl_.new_value_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
  }},
  // no aux_entries
  {{
    "\17\11\11\0\0\0\0\0"
    "test.NewMessage"
    "old_value"
    "new_value"
  }},
};

PROTOBUF_NOINLINE void NewMessage::Clear() {
// @@protoc_insertion_point(message_clear_start:test.NewMessage)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.old_value_.ClearToEmpty();
  _impl_.new_value_.ClearToEmpty();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

::uint8_t* NewMessage::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:test.NewMessage)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // string old_value = 1;
  if (!this->_internal_old_value().empty()) {
    const std::string& _s = this->_internal_old_value();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.NewMessage.old_value");
    target = stream->WriteStringMaybeAliased(1, _s, target);
  }

  // string new_value = 2;
  if (!this->_internal_new_value().empty()) {
    const std::string& _s = this->_internal_new_value();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.NewMessage.new_value");
    target = stream->WriteStringMaybeAliased(2, _s, target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:test.NewMessage)
  return target;
}

::size_t NewMessage::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:test.NewMessage)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // string old_value = 1;
  if (!this->_internal_old_value().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_old_value());
  }

  // string new_value = 2;
  if (!this->_internal_new_value().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_new_value());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void NewMessage::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<NewMessage*>(&to_msg);
  auto& from = static_cast<const NewMessage&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:test.NewMessage)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_old_value().empty()) {
    _this->_internal_set_old_value(from._internal_old_value());
  }
  if (!from._internal_new_value().empty()) {
    _this->_internal_set_new_value(from._internal_new_value());
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void NewMessage::CopyFrom(const NewMessage& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:test.NewMessage)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void NewMessage::InternalSwap(NewMessage* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.old_value_, &other->_impl_.old_value_, arena);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.new_value_, &other->_impl_.new_value_, arena);
}

::google::protobuf::Metadata NewMessage::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

OldEnumMessage_MappedValueEntry_DoNotUse::OldEnumMessage_MappedValueEntry_DoNotUse() {}
OldEnumMessage_MappedValueEntry_DoNotUse::OldEnumMessage_MappedValueEntry_DoNotUse(::google::protobuf::Arena* arena) : SuperType(arena) {}
const ::google::protobuf::MessageLite::ClassData*
OldEnumMessage_MappedValueEntry_DoNotUse::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // tc_table
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(OldEnumMessage_MappedValueEntry_DoNotUse, _cached_size_),
              false,
          },
          &OldEnumMessage_MappedValueEntry_DoNotUse::MergeImpl,
          &OldEnumMessage_MappedValueEntry_DoNotUse::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
// ===================================================================

class OldEnumMessage::_Internal {
 public:
};

OldEnumMessage::OldEnumMessage(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:test.OldEnumMessage)
}
inline PROTOBUF_NDEBUG_INLINE OldEnumMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::test::OldEnumMessage& from_msg)
      : repeated_value_{visibility, arena, from.repeated_value_},
        _repeated_value_cached_byte_size_{0},
        mapped_value_{visibility, arena, from.mapped_value_},
        _cached_size_{0} {}

OldEnumMessage::OldEnumMessage(
    ::google::protobuf::Arena* arena,
    const OldEnumMessage& from)
    : ::google::protobuf::Message(arena) {
  OldEnumMessage* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  _impl_.value_ = from._impl_.value_;

  // @@protoc_insertion_point(copy_constructor:test.OldEnumMessage)
}
inline PROTOBUF_NDEBUG_INLINE OldEnumMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : repeated_value_{visibility, arena},
        _repeated_value_cached_byte_size_{0},
        mapped_value_{visibility, arena},
        _cached_size_{0} {}

inline void OldEnumMessage::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.value_ = {};
}
OldEnumMessage::~OldEnumMessage() {
  // @@protoc_insertion_point(destructor:test.OldEnumMessage)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void OldEnumMessage::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
OldEnumMessage::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(OldEnumMessage, _impl_._cached_size_),
              false,
          },
          &OldEnumMessage::MergeImpl,
          &OldEnumMessage::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 3, 1, 40, 2> OldEnumMessage::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    3, 8,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967288,  // skipmap
    offsetof(decltype(_table_), field_entries),
    3,  // num_field_entries
    1,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_OldEnumMessage_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::test::OldEnumMessage>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // repeated .test.OldEnumMessage.OldEnum repeated_value = 2;
    {::_pbi::TcParser::FastV32P1,
     {18, 63, 0, PROTOBUF_FIELD_OFFSET(OldEnumMessage, _impl_.repeated_value_)}},
    // .test.OldEnumMessage.OldEnum value = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(OldEnumMessage, _impl_.value_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(OldEnumMessage, _impl_.value_)}},
  }}, {{
    65535, 65535
  }}, {{
    // .test.OldEnumMessage.OldEnum value = 1;
    {PROTOBUF_FIELD_OFFSET(OldEnumMessage, _impl_.value_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kOpenEnum)},
    // repeated .test.OldEnumMessage.OldEnum repeated_value = 2;
    {PROTOBUF_FIELD_OFFSET(OldEnumMessage, _impl_.repeated_value_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedOpenEnum)},
    // map<string, .test.OldEnumMessage.OldEnum> mapped_value = 3;
    {PROTOBUF_FIELD_OFFSET(OldEnumMessage, _impl_.mapped_value_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMap)},
  }}, {{
    {::_pbi::TcParser::GetMapAuxInfo<
        decltype(OldEnumMessage()._impl_.mapped_value_)>(
        1, 0, 0, 9,
        14)},
  }}, {{
    "\23\0\0\14\0\0\0\0"
    "test.OldEnumMessage"
    "mapped_value"
  }},
};

PROTOBUF_NOINLINE void OldEnumMessage::Clear() {
// @@protoc_insertion_point(message_clear_start:test.OldEnumMessage)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.repeated_value_.Clear();
  _impl_.mapped_value_.Clear();
  _impl_.value_ = 0;
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

::uint8_t* OldEnumMessage::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:test.OldEnumMessage)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // .test.OldEnumMessage.OldEnum value = 1;
  if (this->_internal_value() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteEnumToArray(
        1, this->_internal_value(), target);
  }

  // repeated .test.OldEnumMessage.OldEnum repeated_value = 2;
  {
    std::size_t byte_size = _impl_._repeated_value_cached_byte_size_.Get();
    if (byte_size > 0) {
      target = stream->WriteEnumPacked(2, _internal_repeated_value(),
                                       byte_size, target);
    }
  }

  // map<string, .test.OldEnumMessage.OldEnum> mapped_value = 3;
  if (!_internal_mapped_value().empty()) {
    using MapType = ::google::protobuf::Map<std::string, ::test::OldEnumMessage_OldEnum>;
    using WireHelper = _pbi::MapEntryFuncs<std::string, ::test::OldEnumMessage_OldEnum,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_ENUM>;
    const auto& field = _internal_mapped_value();

    if (stream->IsSerializationDeterministic() && field.size() > 1) {
      for (const auto& entry : ::google::protobuf::internal::MapSorterPtr<MapType>(field)) {
        target = WireHelper::InternalSerialize(
            3, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.OldEnumMessage.mapped_value");
      }
    } else {
      for (const auto& entry : field) {
        target = WireHelper::InternalSerialize(
            3, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.OldEnumMessage.mapped_value");
      }
    }
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:test.OldEnumMessage)
  return target;
}

::size_t OldEnumMessage::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:test.OldEnumMessage)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // repeated .test.OldEnumMessage.OldEnum repeated_value = 2;
  {
    std::size_t data_size = 0;
    auto count = static_cast<std::size_t>(this->_internal_repeated_value_size());

    for (std::size_t i = 0; i < count; ++i) {
      data_size += ::_pbi::WireFormatLite::EnumSize(
          this->_internal_repeated_value().Get(static_cast<int>(i)));
    }
    total_size += data_size;
    if (data_size > 0) {
      total_size += 1;
      total_size += ::_pbi::WireFormatLite::Int32Size(
          static_cast<int32_t>(data_size));
    }
    _impl_._repeated_value_cached_byte_size_.Set(::_pbi::ToCachedSize(data_size));
  }
  // map<string, .test.OldEnumMessage.OldEnum> mapped_value = 3;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_mapped_value_size());
  for (const auto& entry : _internal_mapped_value()) {
    total_size += _pbi::MapEntryFuncs<std::string, ::test::OldEnumMessage_OldEnum,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_ENUM>::ByteSizeLong(entry.first, entry.second);
  }
  // .test.OldEnumMessage.OldEnum value = 1;
  if (this->_internal_value() != 0) {
    total_size += 1 +
                  ::_pbi::WireFormatLite::EnumSize(this->_internal_value());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void OldEnumMessage::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<OldEnumMessage*>(&to_msg);
  auto& from = static_cast<const OldEnumMessage&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:test.OldEnumMessage)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_repeated_value()->MergeFrom(from._internal_repeated_value());
  _this->_impl_.mapped_value_.MergeFrom(from._impl_.mapped_value_);
  if (from._internal_value() != 0) {
    _this->_impl_.value_ = from._impl_.value_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void OldEnumMessage::CopyFrom(const OldEnumMessage& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:test.OldEnumMessage)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void OldEnumMessage::InternalSwap(OldEnumMessage* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.repeated_value_.InternalSwap(&other->_impl_.repeated_value_);
  _impl_.mapped_value_.InternalSwap(&other->_impl_.mapped_value_);
  swap(_impl_.value_, other->_impl_.value_);
}

::google::protobuf::Metadata OldEnumMessage::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

NewEnumMessage_MappedValueEntry_DoNotUse::NewEnumMessage_MappedValueEntry_DoNotUse() {}
NewEnumMessage_MappedValueEntry_DoNotUse::NewEnumMessage_MappedValueEntry_DoNotUse(::google::protobuf::Arena* arena) : SuperType(arena) {}
const ::google::protobuf::MessageLite::ClassData*
NewEnumMessage_MappedValueEntry_DoNotUse::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // tc_table
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(NewEnumMessage_MappedValueEntry_DoNotUse, _cached_size_),
              false,
          },
          &NewEnumMessage_MappedValueEntry_DoNotUse::MergeImpl,
          &NewEnumMessage_MappedValueEntry_DoNotUse::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
// ===================================================================

class NewEnumMessage::_Internal {
 public:
};

NewEnumMessage::NewEnumMessage(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:test.NewEnumMessage)
}
inline PROTOBUF_NDEBUG_INLINE NewEnumMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::test::NewEnumMessage& from_msg)
      : repeated_value_{visibility, arena, from.repeated_value_},
        _repeated_value_cached_byte_size_{0},
        mapped_value_{visibility, arena, from.mapped_value_},
        _cached_size_{0} {}

NewEnumMessage::NewEnumMessage(
    ::google::protobuf::Arena* arena,
    const NewEnumMessage& from)
    : ::google::protobuf::Message(arena) {
  NewEnumMessage* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);
  _impl_.value_ = from._impl_.value_;

  // @@protoc_insertion_point(copy_constructor:test.NewEnumMessage)
}
inline PROTOBUF_NDEBUG_INLINE NewEnumMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : repeated_value_{visibility, arena},
        _repeated_value_cached_byte_size_{0},
        mapped_value_{visibility, arena},
        _cached_size_{0} {}

inline void NewEnumMessage::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.value_ = {};
}
NewEnumMessage::~NewEnumMessage() {
  // @@protoc_insertion_point(destructor:test.NewEnumMessage)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void NewEnumMessage::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
NewEnumMessage::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(NewEnumMessage, _impl_._cached_size_),
              false,
          },
          &NewEnumMessage::MergeImpl,
          &NewEnumMessage::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 3, 1, 40, 2> NewEnumMessage::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    3, 8,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967288,  // skipmap
    offsetof(decltype(_table_), field_entries),
    3,  // num_field_entries
    1,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_NewEnumMessage_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::test::NewEnumMessage>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // repeated .test.NewEnumMessage.NewEnum repeated_value = 2;
    {::_pbi::TcParser::FastV32P1,
     {18, 63, 0, PROTOBUF_FIELD_OFFSET(NewEnumMessage, _impl_.repeated_value_)}},
    // .test.NewEnumMessage.NewEnum value = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(NewEnumMessage, _impl_.value_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(NewEnumMessage, _impl_.value_)}},
  }}, {{
    65535, 65535
  }}, {{
    // .test.NewEnumMessage.NewEnum value = 1;
    {PROTOBUF_FIELD_OFFSET(NewEnumMessage, _impl_.value_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kOpenEnum)},
    // repeated .test.NewEnumMessage.NewEnum repeated_value = 2;
    {PROTOBUF_FIELD_OFFSET(NewEnumMessage, _impl_.repeated_value_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedOpenEnum)},
    // map<string, .test.NewEnumMessage.NewEnum> mapped_value = 3;
    {PROTOBUF_FIELD_OFFSET(NewEnumMessage, _impl_.mapped_value_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMap)},
  }}, {{
    {::_pbi::TcParser::GetMapAuxInfo<
        decltype(NewEnumMessage()._impl_.mapped_value_)>(
        1, 0, 0, 9,
        14)},
  }}, {{
    "\23\0\0\14\0\0\0\0"
    "test.NewEnumMessage"
    "mapped_value"
  }},
};

PROTOBUF_NOINLINE void NewEnumMessage::Clear() {
// @@protoc_insertion_point(message_clear_start:test.NewEnumMessage)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.repeated_value_.Clear();
  _impl_.mapped_value_.Clear();
  _impl_.value_ = 0;
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

::uint8_t* NewEnumMessage::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:test.NewEnumMessage)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // .test.NewEnumMessage.NewEnum value = 1;
  if (this->_internal_value() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteEnumToArray(
        1, this->_internal_value(), target);
  }

  // repeated .test.NewEnumMessage.NewEnum repeated_value = 2;
  {
    std::size_t byte_size = _impl_._repeated_value_cached_byte_size_.Get();
    if (byte_size > 0) {
      target = stream->WriteEnumPacked(2, _internal_repeated_value(),
                                       byte_size, target);
    }
  }

  // map<string, .test.NewEnumMessage.NewEnum> mapped_value = 3;
  if (!_internal_mapped_value().empty()) {
    using MapType = ::google::protobuf::Map<std::string, ::test::NewEnumMessage_NewEnum>;
    using WireHelper = _pbi::MapEntryFuncs<std::string, ::test::NewEnumMessage_NewEnum,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_ENUM>;
    const auto& field = _internal_mapped_value();

    if (stream->IsSerializationDeterministic() && field.size() > 1) {
      for (const auto& entry : ::google::protobuf::internal::MapSorterPtr<MapType>(field)) {
        target = WireHelper::InternalSerialize(
            3, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.NewEnumMessage.mapped_value");
      }
    } else {
      for (const auto& entry : field) {
        target = WireHelper::InternalSerialize(
            3, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.NewEnumMessage.mapped_value");
      }
    }
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:test.NewEnumMessage)
  return target;
}

::size_t NewEnumMessage::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:test.NewEnumMessage)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // repeated .test.NewEnumMessage.NewEnum repeated_value = 2;
  {
    std::size_t data_size = 0;
    auto count = static_cast<std::size_t>(this->_internal_repeated_value_size());

    for (std::size_t i = 0; i < count; ++i) {
      data_size += ::_pbi::WireFormatLite::EnumSize(
          this->_internal_repeated_value().Get(static_cast<int>(i)));
    }
    total_size += data_size;
    if (data_size > 0) {
      total_size += 1;
      total_size += ::_pbi::WireFormatLite::Int32Size(
          static_cast<int32_t>(data_size));
    }
    _impl_._repeated_value_cached_byte_size_.Set(::_pbi::ToCachedSize(data_size));
  }
  // map<string, .test.NewEnumMessage.NewEnum> mapped_value = 3;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_mapped_value_size());
  for (const auto& entry : _internal_mapped_value()) {
    total_size += _pbi::MapEntryFuncs<std::string, ::test::NewEnumMessage_NewEnum,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_ENUM>::ByteSizeLong(entry.first, entry.second);
  }
  // .test.NewEnumMessage.NewEnum value = 1;
  if (this->_internal_value() != 0) {
    total_size += 1 +
                  ::_pbi::WireFormatLite::EnumSize(this->_internal_value());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void NewEnumMessage::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<NewEnumMessage*>(&to_msg);
  auto& from = static_cast<const NewEnumMessage&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:test.NewEnumMessage)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_repeated_value()->MergeFrom(from._internal_repeated_value());
  _this->_impl_.mapped_value_.MergeFrom(from._impl_.mapped_value_);
  if (from._internal_value() != 0) {
    _this->_impl_.value_ = from._impl_.value_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void NewEnumMessage::CopyFrom(const NewEnumMessage& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:test.NewEnumMessage)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void NewEnumMessage::InternalSwap(NewEnumMessage* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.repeated_value_.InternalSwap(&other->_impl_.repeated_value_);
  _impl_.mapped_value_.InternalSwap(&other->_impl_.mapped_value_);
  swap(_impl_.value_, other->_impl_.value_);
}

::google::protobuf::Metadata NewEnumMessage::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// ===================================================================

MapMessage_StringToStringEntry_DoNotUse::MapMessage_StringToStringEntry_DoNotUse() {}
MapMessage_StringToStringEntry_DoNotUse::MapMessage_StringToStringEntry_DoNotUse(::google::protobuf::Arena* arena) : SuperType(arena) {}
const ::google::protobuf::MessageLite::ClassData*
MapMessage_StringToStringEntry_DoNotUse::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // tc_table
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(MapMessage_StringToStringEntry_DoNotUse, _cached_size_),
              false,
          },
          &MapMessage_StringToStringEntry_DoNotUse::MergeImpl,
          &MapMessage_StringToStringEntry_DoNotUse::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
// ===================================================================

MapMessage_StringToNumberEntry_DoNotUse::MapMessage_StringToNumberEntry_DoNotUse() {}
MapMessage_StringToNumberEntry_DoNotUse::MapMessage_StringToNumberEntry_DoNotUse(::google::protobuf::Arena* arena) : SuperType(arena) {}
const ::google::protobuf::MessageLite::ClassData*
MapMessage_StringToNumberEntry_DoNotUse::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // tc_table
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(MapMessage_StringToNumberEntry_DoNotUse, _cached_size_),
              false,
          },
          &MapMessage_StringToNumberEntry_DoNotUse::MergeImpl,
          &MapMessage_StringToNumberEntry_DoNotUse::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
// ===================================================================

MapMessage_StringToSignedLongEntry_DoNotUse::MapMessage_StringToSignedLongEntry_DoNotUse() {}
MapMessage_StringToSignedLongEntry_DoNotUse::MapMessage_StringToSignedLongEntry_DoNotUse(::google::protobuf::Arena* arena) : SuperType(arena) {}
const ::google::protobuf::MessageLite::ClassData*
MapMessage_StringToSignedLongEntry_DoNotUse::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // tc_table
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(MapMessage_StringToSignedLongEntry_DoNotUse, _cached_size_),
              false,
          },
          &MapMessage_StringToSignedLongEntry_DoNotUse::MergeImpl,
          &MapMessage_StringToSignedLongEntry_DoNotUse::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
// ===================================================================

MapMessage_StringToUnsignedLongEntry_DoNotUse::MapMessage_StringToUnsignedLongEntry_DoNotUse() {}
MapMessage_StringToUnsignedLongEntry_DoNotUse::MapMessage_StringToUnsignedLongEntry_DoNotUse(::google::protobuf::Arena* arena) : SuperType(arena) {}
const ::google::protobuf::MessageLite::ClassData*
MapMessage_StringToUnsignedLongEntry_DoNotUse::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // tc_table
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(MapMessage_StringToUnsignedLongEntry_DoNotUse, _cached_size_),
              false,
          },
          &MapMessage_StringToUnsignedLongEntry_DoNotUse::MergeImpl,
          &MapMessage_StringToUnsignedLongEntry_DoNotUse::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
// ===================================================================

MapMessage_StringToDoubleEntry_DoNotUse::MapMessage_StringToDoubleEntry_DoNotUse() {}
MapMessage_StringToDoubleEntry_DoNotUse::MapMessage_StringToDoubleEntry_DoNotUse(::google::protobuf::Arena* arena) : SuperType(arena) {}
const ::google::protobuf::MessageLite::ClassData*
MapMessage_StringToDoubleEntry_DoNotUse::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // tc_table
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(MapMessage_StringToDoubleEntry_DoNotUse, _cached_size_),
              false,
          },
          &MapMessage_StringToDoubleEntry_DoNotUse::MergeImpl,
          &MapMessage_StringToDoubleEntry_DoNotUse::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
// ===================================================================

MapMessage_StringToMessageEntry_DoNotUse::MapMessage_StringToMessageEntry_DoNotUse() {}
MapMessage_StringToMessageEntry_DoNotUse::MapMessage_StringToMessageEntry_DoNotUse(::google::protobuf::Arena* arena) : SuperType(arena) {}
const ::google::protobuf::MessageLite::ClassData*
MapMessage_StringToMessageEntry_DoNotUse::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // tc_table
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(MapMessage_StringToMessageEntry_DoNotUse, _cached_size_),
              false,
          },
          &MapMessage_StringToMessageEntry_DoNotUse::MergeImpl,
          &MapMessage_StringToMessageEntry_DoNotUse::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
// ===================================================================

MapMessage_IntToStringEntry_DoNotUse::MapMessage_IntToStringEntry_DoNotUse() {}
MapMessage_IntToStringEntry_DoNotUse::MapMessage_IntToStringEntry_DoNotUse(::google::protobuf::Arena* arena) : SuperType(arena) {}
const ::google::protobuf::MessageLite::ClassData*
MapMessage_IntToStringEntry_DoNotUse::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // tc_table
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(MapMessage_IntToStringEntry_DoNotUse, _cached_size_),
              false,
          },
          &MapMessage_IntToStringEntry_DoNotUse::MergeImpl,
          &MapMessage_IntToStringEntry_DoNotUse::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
// ===================================================================

MapMessage_LongToStringEntry_DoNotUse::MapMessage_LongToStringEntry_DoNotUse() {}
MapMessage_LongToStringEntry_DoNotUse::MapMessage_LongToStringEntry_DoNotUse(::google::protobuf::Arena* arena) : SuperType(arena) {}
const ::google::protobuf::MessageLite::ClassData*
MapMessage_LongToStringEntry_DoNotUse::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // tc_table
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(MapMessage_LongToStringEntry_DoNotUse, _cached_size_),
              false,
          },
          &MapMessage_LongToStringEntry_DoNotUse::MergeImpl,
          &MapMessage_LongToStringEntry_DoNotUse::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
// ===================================================================

class MapMessage::_Internal {
 public:
};

MapMessage::MapMessage(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:test.MapMessage)
}
inline PROTOBUF_NDEBUG_INLINE MapMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from, const ::test::MapMessage& from_msg)
      : stringtostring_{visibility, arena, from.stringtostring_},
        stringtonumber_{visibility, arena, from.stringtonumber_},
        stringtosignedlong_{visibility, arena, from.stringtosignedlong_},
        stringtounsignedlong_{visibility, arena, from.stringtounsignedlong_},
        stringtodouble_{visibility, arena, from.stringtodouble_},
        stringtomessage_{visibility, arena, from.stringtomessage_},
        inttostring_{visibility, arena, from.inttostring_},
        longtostring_{visibility, arena, from.longtostring_},
        _cached_size_{0} {}

MapMessage::MapMessage(
    ::google::protobuf::Arena* arena,
    const MapMessage& from)
    : ::google::protobuf::Message(arena) {
  MapMessage* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_, from);

  // @@protoc_insertion_point(copy_constructor:test.MapMessage)
}
inline PROTOBUF_NDEBUG_INLINE MapMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : stringtostring_{visibility, arena},
        stringtonumber_{visibility, arena},
        stringtosignedlong_{visibility, arena},
        stringtounsignedlong_{visibility, arena},
        stringtodouble_{visibility, arena},
        stringtomessage_{visibility, arena},
        inttostring_{visibility, arena},
        longtostring_{visibility, arena},
        _cached_size_{0} {}

inline void MapMessage::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
MapMessage::~MapMessage() {
  // @@protoc_insertion_point(destructor:test.MapMessage)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void MapMessage::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
MapMessage::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              &_table_.header,
              nullptr,  // OnDemandRegisterArenaDtor
              nullptr,  // IsInitialized
              PROTOBUF_FIELD_OFFSET(MapMessage, _impl_._cached_size_),
              false,
          },
          &MapMessage::MergeImpl,
          &MapMessage::kDescriptorMethods,
          &descriptor_table_test_2eproto,
          nullptr,  // tracker
      };
  ::google::protobuf::internal::PrefetchToLocalCache(&_data_);
  ::google::protobuf::internal::PrefetchToLocalCache(_data_.tc_table);
  return _data_.base();
}
PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 8, 9, 150, 2> MapMessage::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    8, 0,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967040,  // skipmap
    offsetof(decltype(_table_), field_entries),
    8,  // num_field_entries
    9,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_MapMessage_default_instance_._instance,
    nullptr,  // post_loop_handler
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::test::MapMessage>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
  }}, {{
    65535, 65535
  }}, {{
    // map<string, string> stringToString = 1;
    {PROTOBUF_FIELD_OFFSET(MapMessage, _impl_.stringtostring_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMap)},
    // map<string, int32> stringToNumber = 2;
    {PROTOBUF_FIELD_OFFSET(MapMessage, _impl_.stringtonumber_), 0, 1,
    (0 | ::_fl::kFcRepeated | ::_fl::kMap)},
    // map<string, int64> stringToSignedLong = 3;
    {PROTOBUF_FIELD_OFFSET(MapMessage, _impl_.stringtosignedlong_), 0, 2,
    (0 | ::_fl::kFcRepeated | ::_fl::kMap)},
    // map<string, uint64> stringToUnsignedLong = 4;
    {PROTOBUF_FIELD_OFFSET(MapMessage, _impl_.stringtounsignedlong_), 0, 3,
    (0 | ::_fl::kFcRepeated | ::_fl::kMap)},
    // map<string, double> stringToDouble = 5;
    {PROTOBUF_FIELD_OFFSET(MapMessage, _impl_.stringtodouble_), 0, 4,
    (0 | ::_fl::kFcRepeated | ::_fl::kMap)},
    // map<string, .test.OtherMessage> stringToMessage = 6;
    {PROTOBUF_FIELD_OFFSET(MapMessage, _impl_.stringtomessage_), 0, 5,
    (0 | ::_fl::kFcRepeated | ::_fl::kMap)},
    // map<int32, string> intToString = 7;
    {PROTOBUF_FIELD_OFFSET(MapMessage, _impl_.inttostring_), 0, 7,
    (0 | ::_fl::kFcRepeated | ::_fl::kMap)},
    // map<int64, string> longToString = 8;
    {PROTOBUF_FIELD_OFFSET(MapMessage, _impl_.longtostring_), 0, 8,
    (0 | ::_fl::kFcRepeated | ::_fl::kMap)},
  }}, {{
    {::_pbi::TcParser::GetMapAuxInfo<
        decltype(MapMessage()._impl_.stringtostring_)>(
        1, 0, 0, 9,
        9)},
    {::_pbi::TcParser::GetMapAuxInfo<
        decltype(MapMessage()._impl_.stringtonumber_)>(
        1, 0, 0, 9,
        5)},
    {::_pbi::TcParser::GetMapAuxInfo<
        decltype(MapMessage()._impl_.stringtosignedlong_)>(
        1, 0, 0, 9,
        3)},
    {::_pbi::TcParser::GetMapAuxInfo<
        decltype(MapMessage()._impl_.stringtounsignedlong_)>(
        1, 0, 0, 9,
        4)},
    {::_pbi::TcParser::GetMapAuxInfo<
        decltype(MapMessage()._impl_.stringtodouble_)>(
        1, 0, 0, 9,
        1)},
    {::_pbi::TcParser::GetMapAuxInfo<
        decltype(MapMessage()._impl_.stringtomessage_)>(
        1, 0, 0, 9,
        11)},
    {::_pbi::TcParser::CreateInArenaStorageCb<::test::OtherMessage>},
    {::_pbi::TcParser::GetMapAuxInfo<
        decltype(MapMessage()._impl_.inttostring_)>(
        1, 0, 0, 5,
        9)},
    {::_pbi::TcParser::GetMapAuxInfo<
        decltype(MapMessage()._impl_.longtostring_)>(
        1, 0, 0, 3,
        9)},
  }}, {{
    "\17\16\16\22\24\16\17\13\14\0\0\0\0\0\0\0"
    "test.MapMessage"
    "stringToString"
    "stringToNumber"
    "stringToSignedLong"
    "stringToUnsignedLong"
    "stringToDouble"
    "stringToMessage"
    "intToString"
    "longToString"
  }},
};

PROTOBUF_NOINLINE void MapMessage::Clear() {
// @@protoc_insertion_point(message_clear_start:test.MapMessage)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.stringtostring_.Clear();
  _impl_.stringtonumber_.Clear();
  _impl_.stringtosignedlong_.Clear();
  _impl_.stringtounsignedlong_.Clear();
  _impl_.stringtodouble_.Clear();
  _impl_.stringtomessage_.Clear();
  _impl_.inttostring_.Clear();
  _impl_.longtostring_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

::uint8_t* MapMessage::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:test.MapMessage)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // map<string, string> stringToString = 1;
  if (!_internal_stringtostring().empty()) {
    using MapType = ::google::protobuf::Map<std::string, std::string>;
    using WireHelper = _pbi::MapEntryFuncs<std::string, std::string,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_STRING>;
    const auto& field = _internal_stringtostring();

    if (stream->IsSerializationDeterministic() && field.size() > 1) {
      for (const auto& entry : ::google::protobuf::internal::MapSorterPtr<MapType>(field)) {
        target = WireHelper::InternalSerialize(
            1, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.stringToString");
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.second.data(), static_cast<int>(entry.second.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.stringToString");
      }
    } else {
      for (const auto& entry : field) {
        target = WireHelper::InternalSerialize(
            1, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.stringToString");
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.second.data(), static_cast<int>(entry.second.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.stringToString");
      }
    }
  }

  // map<string, int32> stringToNumber = 2;
  if (!_internal_stringtonumber().empty()) {
    using MapType = ::google::protobuf::Map<std::string, ::int32_t>;
    using WireHelper = _pbi::MapEntryFuncs<std::string, ::int32_t,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_INT32>;
    const auto& field = _internal_stringtonumber();

    if (stream->IsSerializationDeterministic() && field.size() > 1) {
      for (const auto& entry : ::google::protobuf::internal::MapSorterPtr<MapType>(field)) {
        target = WireHelper::InternalSerialize(
            2, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.stringToNumber");
      }
    } else {
      for (const auto& entry : field) {
        target = WireHelper::InternalSerialize(
            2, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.stringToNumber");
      }
    }
  }

  // map<string, int64> stringToSignedLong = 3;
  if (!_internal_stringtosignedlong().empty()) {
    using MapType = ::google::protobuf::Map<std::string, ::int64_t>;
    using WireHelper = _pbi::MapEntryFuncs<std::string, ::int64_t,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_INT64>;
    const auto& field = _internal_stringtosignedlong();

    if (stream->IsSerializationDeterministic() && field.size() > 1) {
      for (const auto& entry : ::google::protobuf::internal::MapSorterPtr<MapType>(field)) {
        target = WireHelper::InternalSerialize(
            3, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.stringToSignedLong");
      }
    } else {
      for (const auto& entry : field) {
        target = WireHelper::InternalSerialize(
            3, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.stringToSignedLong");
      }
    }
  }

  // map<string, uint64> stringToUnsignedLong = 4;
  if (!_internal_stringtounsignedlong().empty()) {
    using MapType = ::google::protobuf::Map<std::string, ::uint64_t>;
    using WireHelper = _pbi::MapEntryFuncs<std::string, ::uint64_t,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_UINT64>;
    const auto& field = _internal_stringtounsignedlong();

    if (stream->IsSerializationDeterministic() && field.size() > 1) {
      for (const auto& entry : ::google::protobuf::internal::MapSorterPtr<MapType>(field)) {
        target = WireHelper::InternalSerialize(
            4, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.stringToUnsignedLong");
      }
    } else {
      for (const auto& entry : field) {
        target = WireHelper::InternalSerialize(
            4, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.stringToUnsignedLong");
      }
    }
  }

  // map<string, double> stringToDouble = 5;
  if (!_internal_stringtodouble().empty()) {
    using MapType = ::google::protobuf::Map<std::string, double>;
    using WireHelper = _pbi::MapEntryFuncs<std::string, double,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_DOUBLE>;
    const auto& field = _internal_stringtodouble();

    if (stream->IsSerializationDeterministic() && field.size() > 1) {
      for (const auto& entry : ::google::protobuf::internal::MapSorterPtr<MapType>(field)) {
        target = WireHelper::InternalSerialize(
            5, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.stringToDouble");
      }
    } else {
      for (const auto& entry : field) {
        target = WireHelper::InternalSerialize(
            5, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.stringToDouble");
      }
    }
  }

  // map<string, .test.OtherMessage> stringToMessage = 6;
  if (!_internal_stringtomessage().empty()) {
    using MapType = ::google::protobuf::Map<std::string, ::test::OtherMessage>;
    using WireHelper = _pbi::MapEntryFuncs<std::string, ::test::OtherMessage,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_MESSAGE>;
    const auto& field = _internal_stringtomessage();

    if (stream->IsSerializationDeterministic() && field.size() > 1) {
      for (const auto& entry : ::google::protobuf::internal::MapSorterPtr<MapType>(field)) {
        target = WireHelper::InternalSerialize(
            6, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.stringToMessage");
      }
    } else {
      for (const auto& entry : field) {
        target = WireHelper::InternalSerialize(
            6, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.stringToMessage");
      }
    }
  }

  // map<int32, string> intToString = 7;
  if (!_internal_inttostring().empty()) {
    using MapType = ::google::protobuf::Map<::int32_t, std::string>;
    using WireHelper = _pbi::MapEntryFuncs<::int32_t, std::string,
                                   _pbi::WireFormatLite::TYPE_INT32,
                                   _pbi::WireFormatLite::TYPE_STRING>;
    const auto& field = _internal_inttostring();

    if (stream->IsSerializationDeterministic() && field.size() > 1) {
      for (const auto& entry : ::google::protobuf::internal::MapSorterFlat<MapType>(field)) {
        target = WireHelper::InternalSerialize(
            7, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.second.data(), static_cast<int>(entry.second.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.intToString");
      }
    } else {
      for (const auto& entry : field) {
        target = WireHelper::InternalSerialize(
            7, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.second.data(), static_cast<int>(entry.second.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.intToString");
      }
    }
  }

  // map<int64, string> longToString = 8;
  if (!_internal_longtostring().empty()) {
    using MapType = ::google::protobuf::Map<::int64_t, std::string>;
    using WireHelper = _pbi::MapEntryFuncs<::int64_t, std::string,
                                   _pbi::WireFormatLite::TYPE_INT64,
                                   _pbi::WireFormatLite::TYPE_STRING>;
    const auto& field = _internal_longtostring();

    if (stream->IsSerializationDeterministic() && field.size() > 1) {
      for (const auto& entry : ::google::protobuf::internal::MapSorterFlat<MapType>(field)) {
        target = WireHelper::InternalSerialize(
            8, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.second.data(), static_cast<int>(entry.second.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.longToString");
      }
    } else {
      for (const auto& entry : field) {
        target = WireHelper::InternalSerialize(
            8, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.second.data(), static_cast<int>(entry.second.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "test.MapMessage.longToString");
      }
    }
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:test.MapMessage)
  return target;
}

::size_t MapMessage::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:test.MapMessage)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::_pbi::Prefetch5LinesFrom7Lines(reinterpret_cast<const void*>(this));
  // map<string, string> stringToString = 1;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_stringtostring_size());
  for (const auto& entry : _internal_stringtostring()) {
    total_size += _pbi::MapEntryFuncs<std::string, std::string,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_STRING>::ByteSizeLong(entry.first, entry.second);
  }
  // map<string, int32> stringToNumber = 2;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_stringtonumber_size());
  for (const auto& entry : _internal_stringtonumber()) {
    total_size += _pbi::MapEntryFuncs<std::string, ::int32_t,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_INT32>::ByteSizeLong(entry.first, entry.second);
  }
  // map<string, int64> stringToSignedLong = 3;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_stringtosignedlong_size());
  for (const auto& entry : _internal_stringtosignedlong()) {
    total_size += _pbi::MapEntryFuncs<std::string, ::int64_t,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_INT64>::ByteSizeLong(entry.first, entry.second);
  }
  // map<string, uint64> stringToUnsignedLong = 4;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_stringtounsignedlong_size());
  for (const auto& entry : _internal_stringtounsignedlong()) {
    total_size += _pbi::MapEntryFuncs<std::string, ::uint64_t,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_UINT64>::ByteSizeLong(entry.first, entry.second);
  }
  // map<string, double> stringToDouble = 5;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_stringtodouble_size());
  for (const auto& entry : _internal_stringtodouble()) {
    total_size += _pbi::MapEntryFuncs<std::string, double,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_DOUBLE>::ByteSizeLong(entry.first, entry.second);
  }
  // map<string, .test.OtherMessage> stringToMessage = 6;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_stringtomessage_size());
  for (const auto& entry : _internal_stringtomessage()) {
    total_size += _pbi::MapEntryFuncs<std::string, ::test::OtherMessage,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_MESSAGE>::ByteSizeLong(entry.first, entry.second);
  }
  // map<int32, string> intToString = 7;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_inttostring_size());
  for (const auto& entry : _internal_inttostring()) {
    total_size += _pbi::MapEntryFuncs<::int32_t, std::string,
                                   _pbi::WireFormatLite::TYPE_INT32,
                                   _pbi::WireFormatLite::TYPE_STRING>::ByteSizeLong(entry.first, entry.second);
  }
  // map<int64, string> longToString = 8;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_longtostring_size());
  for (const auto& entry : _internal_longtostring()) {
    total_size += _pbi::MapEntryFuncs<::int64_t, std::string,
                                   _pbi::WireFormatLite::TYPE_INT64,
                                   _pbi::WireFormatLite::TYPE_STRING>::ByteSizeLong(entry.first, entry.second);
  }
  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void MapMessage::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<MapMessage*>(&to_msg);
  auto& from = static_cast<const MapMessage&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:test.MapMessage)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_impl_.stringtostring_.MergeFrom(from._impl_.stringtostring_);
  _this->_impl_.stringtonumber_.MergeFrom(from._impl_.stringtonumber_);
  _this->_impl_.stringtosignedlong_.MergeFrom(from._impl_.stringtosignedlong_);
  _this->_impl_.stringtounsignedlong_.MergeFrom(from._impl_.stringtounsignedlong_);
  _this->_impl_.stringtodouble_.MergeFrom(from._impl_.stringtodouble_);
  _this->_impl_.stringtomessage_.MergeFrom(from._impl_.stringtomessage_);
  _this->_impl_.inttostring_.MergeFrom(from._impl_.inttostring_);
  _this->_impl_.longtostring_.MergeFrom(from._impl_.longtostring_);
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void MapMessage::CopyFrom(const MapMessage& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:test.MapMessage)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}


void MapMessage::InternalSwap(MapMessage* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.stringtostring_.InternalSwap(&other->_impl_.stringtostring_);
  _impl_.stringtonumber_.InternalSwap(&other->_impl_.stringtonumber_);
  _impl_.stringtosignedlong_.InternalSwap(&other->_impl_.stringtosignedlong_);
  _impl_.stringtounsignedlong_.InternalSwap(&other->_impl_.stringtounsignedlong_);
  _impl_.stringtodouble_.InternalSwap(&other->_impl_.stringtodouble_);
  _impl_.stringtomessage_.InternalSwap(&other->_impl_.stringtomessage_);
  _impl_.inttostring_.InternalSwap(&other->_impl_.inttostring_);
  _impl_.longtostring_.InternalSwap(&other->_impl_.longtostring_);
}

::google::protobuf::Metadata MapMessage::GetMetadata() const {
  return ::google::protobuf::Message::GetMetadataImpl(GetClassData()->full());
}
// @@protoc_insertion_point(namespace_scope)
}  // namespace test
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2 static ::std::false_type
    _static_init2_ PROTOBUF_UNUSED =
        (::_pbi::AddDescriptors(&descriptor_table_test_2eproto),
         ::std::false_type{});
#include "google/protobuf/port_undef.inc"
