//
//  SCValdiBlockHelper.m
//  valdi_core-ios
//
//  Created by Simon Corsin on 1/31/23.
//

#import "valdi_core/SCValdiBlockHelper.h"
#import "valdi_core/SCValdiMarshallableObjectUtils.h"

#if __has_feature(ptrauth_calls)
#import <ptrauth.h>
#endif

namespace ValdiIOS {

/**
 This struct represent the ABI of an Objective-C block inside the Objective-C runtime.
 */
typedef struct ObjectiveCBlock {
    void *isa;
    int flags;
    int reserved;
    /**
     Holds the actual function pointer to the block. On arm64e a block's
     invoke pointer is signed with the block ABI (IA key, address-diversified
     by the field's address); declaring it as a plain function pointer
     authenticates it with the wrong (zero) discriminator and PAC-faults when
     the block is called. Match the block ABI so objcBlock->invoke authenticates.
     */
#if __has_feature(ptrauth_calls)
    void (*__ptrauth(ptrauth_key_function_pointer, 1, 0) invoke)(struct ObjectiveCBlock *);
#else
    void (*invoke)(struct ObjectiveCBlock *);
#endif
} ObjectiveCBlock;

ObjCBlockHelper::ObjCBlockHelper(SCValdiFunctionInvoker invoker,
                     SCValdiBlockFactory factory,
                     SCValdiFieldValueType returnType,
                     size_t parametersSize,
                     const SCValdiFieldValueType *parameterTypes): _invoker(invoker), _factory(factory), _returnType(returnType),_parametersSize(parametersSize) {
        Valdi::InlineContainerAllocator<ObjCBlockHelper, SCValdiFieldValueType> allocator;
        for (size_t i = 0; i < parametersSize; i++) {
            allocator.constructContainerEntry(this, i, parameterTypes[i]);
        }
    }

ObjCBlockHelper::~ObjCBlockHelper() {
    Valdi::InlineContainerAllocator<ObjCBlockHelper, SCValdiFieldValueType> allocator;
    allocator.deallocate(this, _parametersSize);
}

size_t ObjCBlockHelper::getParametersSize() const {
    return _parametersSize;
}

const SCValdiFieldValueType &ObjCBlockHelper::getReturnType() const {
    return _returnType;
}

const SCValdiFieldValueType *ObjCBlockHelper::getParameterTypes() const {
    Valdi::InlineContainerAllocator<ObjCBlockHelper, SCValdiFieldValueType> allocator;
    return allocator.getContainerStartPtr(this);
}

ObjCValue ObjCBlockHelper::invokeWithBlock(__unsafe_unretained id block, const ObjCValue* parameters) {
    const ObjectiveCBlock *objcBlock = (__bridge const ObjectiveCBlock *)(block);
    return invokeWithImp((IMP)objcBlock->invoke, parameters);
}

ObjCValue ObjCBlockHelper::invokeWithImp(IMP imp, const ObjCValue* parameters) {
    auto result = ObjCValue::toFieldValues(_parametersSize, parameters, getParameterTypes(), [&](auto *fields) {
        return _invoker((const void *)imp, fields);
    });

    return ObjCValue(result, _returnType);
}

ObjCValue ObjCBlockHelper::makeBlock(SCValdiBlockTrampoline blockTrampoline) {
    return ObjCValue::makeObject(_factory(blockTrampoline));
}

ObjCBlockProxy ObjCBlockHelper::makeSelectorImpProxy(size_t fieldIndex) {
    // First parameter should be object, second selector
    SC_ASSERT(_parametersSize >= 2);

    ObjCBlockProxy proxy;
    proxy.objcTypeEncoding.append(std::string_view(SCValdiObjCTypeForFieldType(_returnType)));
    proxy.objcTypeEncoding.append(@encode(id));
    proxy.objcTypeEncoding.append(@encode(SEL));

    for (size_t i = 2; i < _parametersSize; i++) {
        proxy.objcTypeEncoding.append(std::string_view(SCValdiObjCTypeForFieldType(getParameterTypes()[i])));
    }

    auto self = Valdi::strongSmallRef(this);
    proxy.block = makeBlock(^SCValdiFieldValue(const void *receiver, ...) {
        auto parametersSize = self->getParametersSize();

        SCValdiFieldValue parameters[parametersSize];

        va_list v;
        va_start(v, receiver);
        SCValdiMarshallableObject *marshallableObject = SCValdiFieldValueGetObject(va_arg(v, SCValdiFieldValue));
        SC_ASSERT(marshallableObject != nil);
        SCValdiFieldValue *fieldsStorage = SCValdiGetMarshallableObjectFieldsStorage(marshallableObject);
        SC_ASSERT(fieldsStorage != nil);

        SCValdiFieldValue block = fieldsStorage[fieldIndex];

        const ObjectiveCBlock *objcBlock = (const ObjectiveCBlock *)(SCValdiFieldValueGetPtr(block));

        // Replace the receiver by the block
        parameters[0] = block;
        // Set null CMD, the parameter will be ignored
        parameters[1] = SCValdiFieldValueMakeNull();

        for (size_t i = 2; i < parametersSize; i++) {
            parameters[i] = va_arg(v, SCValdiFieldValue);
        }
        va_end(v);

        return self->_invoker((const void *)objcBlock->invoke, &parameters[0]);
    });

    return proxy;
}

Valdi::Ref<ObjCBlockHelper> ObjCBlockHelper::make(SCValdiFunctionInvoker invoker,
                                                     SCValdiBlockFactory factory,
                                                     SCValdiFieldValueType returnType,
                                                     size_t parametersSize,
                                                     const SCValdiFieldValueType *parameterTypes) {
    Valdi::InlineContainerAllocator<ObjCBlockHelper, SCValdiFieldValueType> allocator;
    return allocator.allocate(parametersSize, invoker, factory, returnType, parametersSize, parameterTypes);
}

Valdi::Ref<ObjCBlockHelper> ObjCBlockHelper::make(const Valdi::StringBox& typeEncoding,
                                                     SCValdiFunctionInvoker invoker,
                                                     SCValdiBlockFactory factory) {
    Valdi::SmallVector<SCValdiFieldValueType, 16> parameterTypes;
    auto returnType = SCValdiFieldValueTypeVoid;

    bool inReturnType = false;
    auto typeEncodingStr = typeEncoding.toStringView();
    for (size_t i = 0; i < typeEncodingStr.length(); i++) {
        if (typeEncodingStr[i] == ':') {
            inReturnType = true;
        } else {
            auto type = SCValdiFieldTypeFromCTypeChar(typeEncodingStr[i]);
            if (inReturnType) {
                returnType = type;
            } else {
                parameterTypes.emplace_back(type);
            }
        }
    }

    return make(invoker, factory, returnType, parameterTypes.size(), parameterTypes.data());
}

}
