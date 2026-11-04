//
//  ProtobufArena.hpp
//  valdi-pc
//
//  Created by Simon Corsin on 6/21/23.
//

#pragma once

#include "valdi/runtime/JavaScript/Modules/ProtobufMessageFactory.hpp"
#include "valdi_protobuf/Message.hpp"

#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

#include <google/protobuf/dynamic_message.h>

#include <mutex>
#include <vector>

namespace Valdi {

class JSProtobufMessage : public Protobuf::Message {
public:
    JSProtobufMessage(size_t messageIndex,
                      const google::protobuf::Descriptor* descriptor,
                      const Ref<RefCountable>& dataSource);
    ~JSProtobufMessage() override;

    size_t getMessageIndex() const;

private:
    size_t _messageIndex;
};

class ProtobufArena : public ValdiObject, protected Protobuf::IMessageFactory {
public:
    ProtobufArena(bool eagerDecoding, bool includeAllFieldsDuringEncoding);
    ~ProtobufArena() override;

    std::unique_lock<std::recursive_mutex> lock() const;

    // Locks every retained message factory so that any access to descriptors from their
    // shared, non-thread-safe DescriptorPool — including lazy dependency resolution
    // triggered by accessors such as FieldDescriptor::message_type() during field
    // get/set, encoding, postprocessing, or JSON serialization — is serialized against
    // concurrent pool mutation from other JS runtimes. Used by callers operating on an
    // existing message that do not receive a ProtobufMessageFactory directly. Factories
    // are locked in a stable address order so locking more than one (rare) cannot
    // deadlock against another thread doing the same.
    std::vector<std::unique_lock<std::recursive_mutex>> lockRetainedMessageFactories() const;

    size_t createMessage(const Ref<ProtobufMessageFactory>& messageFactory,
                         size_t descriptorIndex,
                         ExceptionTracker& exceptionTracker);
    size_t decodeMessage(const Ref<ProtobufMessageFactory>& messageFactory,
                         size_t descriptorIndex,
                         const BytesView& bytes,
                         bool isFromAsyncCall,
                         ExceptionTracker& exceptionTracker);
    size_t decodeMessageFromJSON(const Ref<ProtobufMessageFactory>& messageFactory,
                                 size_t descriptorIndex,
                                 std::string_view json,
                                 bool isFromAsyncCall,
                                 ExceptionTracker& exceptionTracker);

    size_t fieldToMessageIndex(JSProtobufMessage& message,
                               Protobuf::Field& field,
                               const google::protobuf::FieldDescriptor* fieldDescriptor,
                               ExceptionTracker& exceptionTracker);

    BytesView encodeMessage(size_t messageIndex, ExceptionTracker& exceptionTracker) const;

    JSProtobufMessage* getMessage(size_t messageIndex, ExceptionTracker& exceptionTracker) const;

    size_t copyMessage(const ProtobufArena& fromArena, size_t messageIndex, ExceptionTracker& exceptionTracker);

    std::string messageToJSON(size_t messageIndex,
                              const Protobuf::JSONPrintOptions& printOptions,
                              ExceptionTracker& exceptionTracker) const;

    VALDI_CLASS_HEADER(ProtobufArena)

protected:
    Ref<Protobuf::Message> newMessage(const google::protobuf::Descriptor* descriptor,
                                      const Ref<RefCountable>& dataSource) final;

private:
    mutable std::recursive_mutex _mutex;
    std::vector<Ref<ProtobufMessageFactory>> _retainedMessageFactories;
    std::vector<Ref<JSProtobufMessage>> _messages;
    bool _eagerDecoding = false;
    bool _includeAllFieldsDuringEncoding = false;

    void retainMessageFactory(const Ref<ProtobufMessageFactory>& messageFactory);

    Ref<JSProtobufMessage> createMessageForDescriptor(const google::protobuf::Descriptor* descriptor,
                                                      const Ref<RefCountable>& dataSource);

    size_t postProcessDecodedMessage(const Ref<ProtobufMessageFactory>& messageFactory,
                                     const Ref<JSProtobufMessage>& message,
                                     bool isFromAsyncCall,
                                     ExceptionTracker& exceptionTracker);
};

} // namespace Valdi
