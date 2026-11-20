//
//  JavaTypeSignature.hpp
//  valdi_core
//

#pragma once

#include "valdi_core/cpp/Schema/ValueSchema.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

#include <functional>
#include <string>
#include <string_view>

namespace ValdiAndroid {

/**
 * Resolves the name of the Java class that represents a Valdi function with the given number of parameters.
 */
using FunctionClassNameResolver = std::function<Valdi::StringBox(size_t parametersSize)>;

/**
 * Returns the JNI field descriptor of a class given its slash-separated name: "a/b/C" becomes "La/b/C;".
 */
std::string getJavaTypeSignatureFromTypeName(const std::string_view& typeName);

/**
 * Same as getJavaTypeSignatureFromTypeName, but accepts a dot-separated name: "a.b.C" becomes "La/b/C;".
 */
std::string getJavaTypeSignatureFromClassName(const Valdi::StringBox& typeName);

/**
 * Returns the JNI descriptor of the Java type that a Valdi schema is marshalled to.
 */
std::string getJavaTypeSignatureFromValueSchema(const Valdi::ValueSchema& schema,
                                                bool treatArrayAsList,
                                                const FunctionClassNameResolver& functionClassNameResolver);

} // namespace ValdiAndroid
