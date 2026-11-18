#include "valdi_core/cpp/Schema/ValueSchema.hpp"
#include "valdi_core/cpp/Utils/JavaTypeSignature.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include "gtest/gtest.h"

using namespace Valdi;
using namespace ValdiAndroid;

namespace {

std::string signatureOf(const ValueSchema& schema, bool treatArrayAsList = false) {
    return getJavaTypeSignatureFromValueSchema(schema, treatArrayAsList, [](size_t parametersSize) {
        return StringCache::getGlobal().makeString("com.snap.valdi.callable.ValdiFunction" +
                                                   std::to_string(parametersSize));
    });
}

ValueSchema namedReference(const char* name) {
    return ValueSchema::typeReference(ValueSchemaTypeReference::named(STRING_LITERAL(name)));
}

} // namespace

TEST(JavaTypeSignatureTest, typeNameIsWrappedInClassDescriptor) {
    EXPECT_EQ("Ljava/lang/Object;", getJavaTypeSignatureFromTypeName("java/lang/Object"));
}

TEST(JavaTypeSignatureTest, classNameReplacesDotsWithSlashes) {
    EXPECT_EQ("Lcom/snap/modules/Foo;", getJavaTypeSignatureFromClassName(STRING_LITERAL("com.snap.modules.Foo")));
}

TEST(JavaTypeSignatureTest, classNameKeepsNestedClassSeparator) {
    EXPECT_EQ("Lcom/snap/Outer$Inner;", getJavaTypeSignatureFromClassName(STRING_LITERAL("com.snap.Outer$Inner")));
}

TEST(JavaTypeSignatureTest, primitives) {
    EXPECT_EQ("I", signatureOf(ValueSchema::integer()));
    EXPECT_EQ("J", signatureOf(ValueSchema::longInteger()));
    EXPECT_EQ("D", signatureOf(ValueSchema::doublePrecision()));
    EXPECT_EQ("Z", signatureOf(ValueSchema::boolean()));
    EXPECT_EQ("V", signatureOf(ValueSchema::voidType()));
}

TEST(JavaTypeSignatureTest, boxedPrimitives) {
    EXPECT_EQ("Ljava/lang/Integer;", signatureOf(ValueSchema::boxed(ValueSchema::integer())));
    EXPECT_EQ("Ljava/lang/Long;", signatureOf(ValueSchema::boxed(ValueSchema::longInteger())));
    EXPECT_EQ("Ljava/lang/Double;", signatureOf(ValueSchema::boxed(ValueSchema::doublePrecision())));
    EXPECT_EQ("Ljava/lang/Boolean;", signatureOf(ValueSchema::boxed(ValueSchema::boolean())));
}

TEST(JavaTypeSignatureTest, stringAndUntyped) {
    EXPECT_EQ("Ljava/lang/String;", signatureOf(ValueSchema::string()));
    EXPECT_EQ("Ljava/lang/Object;", signatureOf(ValueSchema::untyped()));
}

TEST(JavaTypeSignatureTest, byteArray) {
    EXPECT_EQ("[B", signatureOf(ValueSchema::valueTypedArray()));
}

TEST(JavaTypeSignatureTest, arrayIsAJavaArrayOfItsItemSignature) {
    EXPECT_EQ("[Ljava/lang/String;", signatureOf(ValueSchema::array(ValueSchema::string())));
    EXPECT_EQ("[[I", signatureOf(ValueSchema::array(ValueSchema::array(ValueSchema::integer()))));
}

TEST(JavaTypeSignatureTest, arrayIsAListWhenTreatedAsList) {
    EXPECT_EQ("Ljava/util/List;", signatureOf(ValueSchema::array(ValueSchema::string()), true));
}

TEST(JavaTypeSignatureTest, map) {
    EXPECT_EQ("Ljava/util/Map;", signatureOf(ValueSchema::map(ValueSchema::string(), ValueSchema::integer())));
}

TEST(JavaTypeSignatureTest, promise) {
    EXPECT_EQ("Lcom/snap/valdi/promise/Promise;", signatureOf(ValueSchema::promise(ValueSchema::string())));
}

TEST(JavaTypeSignatureTest, functionUsesTheClassOfItsArity) {
    EXPECT_EQ("Lcom/snap/valdi/callable/ValdiFunction0;",
              signatureOf(ValueSchema::function(ValueSchema::voidType(), {})));
    EXPECT_EQ(
        "Lcom/snap/valdi/callable/ValdiFunction2;",
        signatureOf(ValueSchema::function(ValueSchema::voidType(), {ValueSchema::string(), ValueSchema::integer()})));
}

TEST(JavaTypeSignatureTest, classUsesItsClassName) {
    EXPECT_EQ("Lcom/snap/modules/Foo;",
              signatureOf(ValueSchema::cls(STRING_LITERAL("com.snap.modules.Foo"), false, {})));
}

TEST(JavaTypeSignatureTest, namedTypeReferenceUsesItsName) {
    EXPECT_EQ("Lcom/snap/modules/Foo;", signatureOf(namedReference("com.snap.modules.Foo")));
}

TEST(JavaTypeSignatureTest, arrayOfNamedTypeReference) {
    EXPECT_EQ("[Lcom/snap/modules/Foo;", signatureOf(ValueSchema::array(namedReference("com.snap.modules.Foo"))));
}

TEST(JavaTypeSignatureTest, genericTypeReferenceUsesTheNameOfItsType) {
    auto schema = ValueSchema::genericTypeReference(
        ValueSchemaTypeReference::named(STRING_LITERAL("com.snap.modules.GenAIGenerationRequest")),
        {namedReference("com.snap.modules.GenAIMediaReference")});

    EXPECT_EQ("Lcom/snap/modules/GenAIGenerationRequest;", signatureOf(schema));
}

// A positional reference is a generic parameter of an exported class, like Input on a request model. The generated
// class erases a generic parameter to Object.
TEST(JavaTypeSignatureTest, positionalTypeReferenceIsObject) {
    EXPECT_EQ("Ljava/lang/Object;", signatureOf(ValueSchema::typeReference(ValueSchemaTypeReference::positional(0))));
}

TEST(JavaTypeSignatureTest, arrayOfPositionalTypeReferenceIsObjectArray) {
    EXPECT_EQ("[Ljava/lang/Object;",
              signatureOf(ValueSchema::array(ValueSchema::typeReference(ValueSchemaTypeReference::positional(0)))));
}
