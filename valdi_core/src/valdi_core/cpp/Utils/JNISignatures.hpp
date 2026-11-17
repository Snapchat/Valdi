//
//  JNISignatures.hpp
//  valdi_core
//

#pragma once

#define JAVA_CLASS(x) "L" x ";"
#define JAVA_ARRAY(x) "[" x

namespace ValdiAndroid {

extern const char* const kJniSigObject;
extern const char* const kJniSigClass;
extern const char* const kJniSigString;
extern const char* const kJniSigObjectArray;
extern const char* const kJniSigByteArray;
extern const char* const kJniSigVoid;
extern const char* const kJniSigInt;
extern const char* const kJniSigLong;
extern const char* const kJniSigDouble;
extern const char* const kJniSigFloat;
extern const char* const kJniSigBool;
extern const char* const kJniSigView;
extern const char* const kJniSigPromise;
extern const char* const kJniSigConstructor;
extern const char* const kJniSigConstructorRetType;

} // namespace ValdiAndroid
