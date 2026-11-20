//
//  JNISignatures.cpp
//  valdi_core
//

#include "valdi_core/cpp/Utils/JNISignatures.hpp"

namespace ValdiAndroid {

const char* const kJniSigObject = JAVA_CLASS("java/lang/Object");
const char* const kJniSigClass = JAVA_CLASS("java/lang/Class");

const char* const kJniSigString = JAVA_CLASS("java/lang/String");
const char* const kJniSigView = JAVA_CLASS("android.view.View");
const char* const kJniSigObjectArray = JAVA_ARRAY(JAVA_CLASS("java/lang/Object"));
const char* const kJniSigByteArray = JAVA_ARRAY("B");
const char* const kJniSigVoid = "V";
const char* const kJniSigInt = "I";
const char* const kJniSigLong = "J";
const char* const kJniSigDouble = "D";
const char* const kJniSigFloat = "F";
const char* const kJniSigBool = "Z";
const char* const kJniSigPromise = JAVA_CLASS("com/snap/valdi/promise/Promise");
const char* const kJniSigConstructor = "<init>";
const char* const kJniSigConstructorRetType = "V";

} // namespace ValdiAndroid
