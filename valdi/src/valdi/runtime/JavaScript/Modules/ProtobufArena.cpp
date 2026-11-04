//
//  ProtobufArena.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 6/21/23.
//

#include "valdi/runtime/JavaScript/Modules/ProtobufArena.hpp"
#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

#include <algorithm>
#include <functional>

namespace Valdi {

JSProtobufMessage::JSProtobufMessage(size_t messageIndex,
                                     const google::protobuf::Descriptor* descriptor,
                                     const Ref<RefCountable>& dataSource)
    : Protobuf::Message(descriptor, dataSource), _messageIndex(messageIndex) {}

JSProtobufMessage::~JSProtobufMessage() = default;

size_t JSProtobufMessage::getMessageIndex() const {
    return _messageIndex;
}

ProtobufArena::ProtobufArena(bool eagerDecoding, bool includeAllFieldsDuringEncoding)
    : _eagerDecoding(eagerDecoding), _includeAllFieldsDuringEncoding(includeAllFieldsDuringEncoding) {}
ProtobufArena::~ProtobufArena() = default;

std::unique_lock<std::recursive_mutex> ProtobufArena::lock() const {
    return std::unique_lock<std::recursive_mutex>(_mutex);
}

std::vector<std::unique_lock<std::recursive_mutex>> ProtobufArena::lockRetainedMessageFactories() const {
    // Lock factories in a stable address order so that concurrently locking more than
    // one factory (only possible when an arena retains multiple, e.g. via copyMessage)
    // cannot deadlock against another thread acquiring them in the opposite order.
    std::vector<ProtobufMessageFactory*> ordered;
    ordered.reserve(_retainedMessageFactories.size());
    for (const auto& factory : _retainedMessageFactories) {
        ordered.emplace_back(factory.get());
    }
    // Use std::less rather than the default operator< : the built-in operator< on
    // pointers to unrelated objects is unspecified, whereas std::less is guaranteed to
    // yield a strict total order over pointers — which is what makes the ordering safe.
    std::sort(ordered.begin(), ordered.end(), std::less<>{});

    std::vector<std::unique_lock<std::recursive_mutex>> locks;
    locks.reserve(ordered.size());
    for (auto* factory : ordered) {
        locks.emplace_back(factory->lock());
    }
    return locks;
}

size_t ProtobufArena::createMessage(const Ref<ProtobufMessageFactory>& messageFactory,
                                    size_t descriptorIndex,
                                    ExceptionTracker& exceptionTracker) {
    // Serialize access to the factory's shared, non-thread-safe DescriptorPool for the
    // whole operation: descriptor lookup may lazily build dependencies, and later field
    // access (e.g. during encoding) reads metadata that resolves message types on demand.
    auto factoryLock = messageFactory->lock();

    const auto* descriptor = messageFactory->getDescriptorAtIndex(descriptorIndex, exceptionTracker);
    if (descriptor == nullptr) {
        return 0;
    }

    retainMessageFactory(messageFactory);
    return createMessageForDescriptor(descriptor, nullptr)->getMessageIndex();
}

size_t ProtobufArena::decodeMessage(const Ref<ProtobufMessageFactory>& messageFactory,
                                    size_t descriptorIndex,
                                    const BytesView& bytes,
                                    bool isFromAsyncCall,
                                    ExceptionTracker& exceptionTracker) {
    // Hold the factory lock across the whole decode: postprocess() reads descriptor field
    // metadata (e.g. FieldDescriptor::message_type()) which lazily mutates the shared pool.
    auto factoryLock = messageFactory->lock();

    const auto* descriptor = messageFactory->getDescriptorAtIndex(descriptorIndex, exceptionTracker);
    if (descriptor == nullptr) {
        return 0;
    }

    VALDI_TRACE_META("Protobuf.decodeMessage", descriptor->name());

    auto message = createMessageForDescriptor(descriptor, bytes.getSource());

    if (!message->decode(bytes.data(), bytes.size(), exceptionTracker)) {
        return 0;
    }

    return postProcessDecodedMessage(messageFactory, message, isFromAsyncCall, exceptionTracker);
}

size_t ProtobufArena::decodeMessageFromJSON(const Ref<ProtobufMessageFactory>& messageFactory,
                                            size_t descriptorIndex,
                                            std::string_view json,
                                            bool isFromAsyncCall,
                                            ExceptionTracker& exceptionTracker) {
    // Hold the factory lock across the whole decode: JSON parsing and postprocess() read
    // descriptor field metadata that lazily mutates the shared pool.
    auto factoryLock = messageFactory->lock();

    const auto* descriptor = messageFactory->getDescriptorAtIndex(descriptorIndex, exceptionTracker);
    if (descriptor == nullptr) {
        return 0;
    }

    VALDI_TRACE_META("Protobuf.decodeMessageFromJSON", descriptor->name());

    auto message = createMessageForDescriptor(descriptor, nullptr);

    if (!message->decodeFromJSON(json, exceptionTracker)) {
        return 0;
    }

    return postProcessDecodedMessage(messageFactory, message, isFromAsyncCall, exceptionTracker);
}

size_t ProtobufArena::postProcessDecodedMessage(const Ref<ProtobufMessageFactory>& messageFactory,
                                                const Ref<JSProtobufMessage>& message,
                                                bool isFromAsyncCall,
                                                ExceptionTracker& exceptionTracker) {
    retainMessageFactory(messageFactory);

    if (isFromAsyncCall || _eagerDecoding) {
        if (!message->postprocess(true, *this, exceptionTracker)) {
            return 0;
        }
    }

    return message->getMessageIndex();
}

Ref<Protobuf::Message> ProtobufArena::newMessage(const google::protobuf::Descriptor* descriptor,
                                                 const Ref<RefCountable>& dataSource) {
    return createMessageForDescriptor(descriptor, dataSource);
}

JSProtobufMessage* ProtobufArena::getMessage(size_t messageIndex, ExceptionTracker& exceptionTracker) const {
    if (messageIndex >= _messages.size()) {
        exceptionTracker.onError(Error("Invalid message"));
        return nullptr;
    }

    return _messages[messageIndex].get();
}

BytesView ProtobufArena::encodeMessage(size_t messageIndex, ExceptionTracker& exceptionTracker) const {
    auto* message = getMessage(messageIndex, exceptionTracker);
    if (message == nullptr) {
        return BytesView();
    }

    VALDI_TRACE_META("Protobuf.encodeMessage", message->getDescriptor()->name());

    // Encoding walks the message's descriptor and may trigger first-time lazy resolution
    // of message types (e.g. for messages created/decoded without postprocess) from the
    // shared pool, so it must be serialized against concurrent pool access.
    auto factoryLocks = lockRetainedMessageFactories();

    return message->encode(_includeAllFieldsDuringEncoding);
}

size_t ProtobufArena::fieldToMessageIndex(JSProtobufMessage& message,
                                          Protobuf::Field& field,
                                          const google::protobuf::FieldDescriptor* fieldDescriptor,
                                          ExceptionTracker& exceptionTracker) {
    // Lazily decoding a nested message reads the nested descriptor's metadata, which can
    // lazily mutate the shared pool; serialize against concurrent pool access.
    auto factoryLocks = lockRetainedMessageFactories();

    auto raw = field.getRaw();
    if (raw.data != nullptr) {
        // Parse the bytes of the message into a concrete message instance
        auto outputMessage = createMessageForDescriptor(fieldDescriptor->message_type(), message.getDataSource());

        SimpleExceptionTracker decodeExceptionTracker;
        if (!outputMessage->decode(raw.data, static_cast<size_t>(raw.length), decodeExceptionTracker)) {
            // A lazily decoded child fails at its first read, far from the decode call that took
            // the bytes, so name the field it hangs off to point back at that payload.
            exceptionTracker.onError(Error(fmt::format("{} (reached through field '{}')",
                                                       decodeExceptionTracker.extractError().getMessage(),
                                                       fieldDescriptor->full_name())));
            return 0;
        }

        field.setMessage(outputMessage.get());
        return outputMessage->getMessageIndex();
    }

    auto* nestedMessage = dynamic_cast<JSProtobufMessage*>(field.getMessage());
    if (nestedMessage == nullptr) {
        exceptionTracker.onError(Error("Nested message is not a message"));
        return 0;
    }

    return nestedMessage->getMessageIndex();
}

size_t ProtobufArena::copyMessage(const ProtobufArena& fromArena,
                                  size_t messageIndex,
                                  ExceptionTracker& exceptionTracker) {
    auto* message = fromArena.getMessage(messageIndex, exceptionTracker);
    if (message == nullptr) {
        return 0;
    }

    // We just encode/decode to simplify the implementation
    // In practice there should be almost never any cases where messages are transferred between arenas.

    VALDI_TRACE_META("Protobuf.copyMessage", message->getDescriptor()->name());

    // The descriptors driving the encode/decode below belong to the source arena's
    // factories' pool; serialize access to it against concurrent pool mutation.
    auto factoryLocks = fromArena.lockRetainedMessageFactories();

    auto encoded = message->encode(_includeAllFieldsDuringEncoding);

    // Also retain the message factories
    for (const auto& messageFactory : fromArena._retainedMessageFactories) {
        retainMessageFactory(messageFactory);
    }

    auto outputMessage = createMessageForDescriptor(message->getDescriptor(), encoded.getSource());

    if (!outputMessage->decode(encoded.data(), encoded.size(), exceptionTracker)) {
        return 0;
    }

    return outputMessage->getMessageIndex();
}

std::string ProtobufArena::messageToJSON(size_t messageIndex,
                                         const Protobuf::JSONPrintOptions& printOptions,
                                         ExceptionTracker& exceptionTracker) const {
    auto message = getMessage(messageIndex, exceptionTracker);
    if (!exceptionTracker) {
        return "";
    }

    // toJSON() builds a TypeResolver over the descriptor's pool and may call
    // FindMessageTypeByName (a pool mutation); serialize against concurrent pool access.
    auto factoryLocks = lockRetainedMessageFactories();

    return message->toJSON(printOptions, exceptionTracker);
}

void ProtobufArena::retainMessageFactory(const Ref<ProtobufMessageFactory>& messageFactory) {
    for (const auto& existingMessageFactory : _retainedMessageFactories) {
        if (existingMessageFactory == messageFactory) {
            return;
        }
    }

    _retainedMessageFactories.emplace_back(messageFactory);
}

Ref<JSProtobufMessage> ProtobufArena::createMessageForDescriptor(const google::protobuf::Descriptor* descriptor,
                                                                 const Ref<RefCountable>& dataSource) {
    auto messageIndex = _messages.size();
    auto message = makeShared<JSProtobufMessage>(messageIndex, descriptor, dataSource);
    _messages.emplace_back(message);
    return message;
}

VALDI_CLASS_IMPL(ProtobufArena)

} // namespace Valdi
