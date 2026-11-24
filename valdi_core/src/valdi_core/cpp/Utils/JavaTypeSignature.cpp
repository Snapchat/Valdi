//
//  JavaTypeSignature.cpp
//  valdi_core
//

#include "valdi_core/cpp/Utils/JavaTypeSignature.hpp"
#include "utils/debugging/Assert.hpp"
#include "valdi_core/cpp/Utils/JNISignatures.hpp"

#include <cstdlib>

namespace ValdiAndroid {

std::string getJavaTypeSignatureFromTypeName(const std::string_view& typeName) {
    std::string out = "L";
    out += typeName;
    out += ";";

    return out;
}

std::string getJavaTypeSignatureFromClassName(const Valdi::StringBox& typeName) {
    return getJavaTypeSignatureFromTypeName(typeName.replacing('.', '/').toStringView());
}

std::string getJavaTypeSignatureFromValueSchema(const Valdi::ValueSchema& schema,
                                                bool treatArrayAsList,
                                                const FunctionClassNameResolver& functionClassNameResolver) {
    if (schema.isUntyped()) {
        return kJniSigObject;
    } else if (schema.isVoid()) {
        return kJniSigVoid;
    } else if (schema.isString()) {
        return kJniSigString;
    } else if (schema.isInteger()) {
        if (schema.isBoxed()) {
            return JAVA_CLASS("java/lang/Integer");
        } else {
            return kJniSigInt;
        }
    } else if (schema.isBigInt()) {
        return JAVA_CLASS("com/snap/valdi/BigInt");
    } else if (schema.isLongInteger()) {
        if (schema.isBoxed()) {
            return JAVA_CLASS("java/lang/Long");
        } else {
            return kJniSigLong;
        }
    } else if (schema.isDouble()) {
        if (schema.isBoxed()) {
            return JAVA_CLASS("java/lang/Double");
        } else {
            return kJniSigDouble;
        }
    } else if (schema.isBoolean()) {
        if (schema.isBoxed()) {
            return JAVA_CLASS("java/lang/Boolean");
        } else {
            return kJniSigBool;
        }
    } else if (schema.isFunction()) {
        return getJavaTypeSignatureFromClassName(functionClassNameResolver(schema.getFunction()->getParametersSize()));
    } else if (schema.isClass()) {
        return getJavaTypeSignatureFromClassName(schema.getClass()->getClassName());
    } else if (schema.isEnum()) {
        return getJavaTypeSignatureFromClassName(schema.getEnum()->getName());
    } else if (schema.isValueTypedArray()) {
        return kJniSigByteArray;
    } else if (schema.isArray()) {
        if (treatArrayAsList) {
            return JAVA_CLASS("java/util/List");
        } else {
            std::string out = "[";
            out += getJavaTypeSignatureFromValueSchema(
                schema.getArrayItemSchema(), treatArrayAsList, functionClassNameResolver);

            return out;
        }
    } else if (schema.isMap()) {
        return JAVA_CLASS("java/util/Map");
    } else if (schema.isTypeReference()) {
        auto typeReference = schema.getTypeReference();
        if (typeReference.isPositional()) {
            // A generic parameter of an exported class erases to Object in the generated Java/Kotlin class.
            return kJniSigObject;
        }
        SC_ASSERT(typeReference.isNamed());
        return getJavaTypeSignatureFromClassName(typeReference.getName());
    } else if (schema.isGenericTypeReference()) {
        const auto& typeReference = schema.getGenericTypeReference()->getType();
        SC_ASSERT(typeReference.isNamed());
        return getJavaTypeSignatureFromClassName(typeReference.getName());
    } else if (schema.isSchemaReference()) {
        return getJavaTypeSignatureFromValueSchema(
            schema.getReferencedSchema(), treatArrayAsList, functionClassNameResolver);
    } else if (schema.isPromise()) {
        return kJniSigPromise;
    } else {
        std::abort();
    }
}

} // namespace ValdiAndroid
