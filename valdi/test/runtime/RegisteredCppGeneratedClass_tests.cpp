#include "valdi_core/cpp/Marshalling/CppGeneratedClass.hpp"
#include "valdi_core/cpp/Marshalling/RegisteredCppGeneratedClass.hpp"
#include "valdi_core/cpp/Schema/ValueSchema.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaRegistry.hpp"
#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include <gtest/gtest.h>

using namespace Valdi;

namespace ValdiTest {

class CppGeneratedClassTests : public ::testing::Test {
protected:
    void SetUp() override {
        _registry = makeShared<ValueSchemaRegistry>();
    }

    void TearDown() override {
        _registry = nullptr;
    }

    RegisteredCppGeneratedClass registerSchema(const char* schemaString,
                                               GetTypeReferencesFunction getTypeReferencesFunction) {
        return RegisteredCppGeneratedClass(
            _registry.get(), schemaString, std::move(getTypeReferencesFunction), GetTypeArgumentsFunction());
    }

    RegisteredCppGeneratedClass registerSchema(const char* schemaString) {
        return registerSchema(schemaString, []() -> std::vector<RegisteredCppGeneratedClass*> { return {}; });
    }

    Ref<ValueSchemaRegistry> _registry;
};

TEST_F(CppGeneratedClassTests, registersSchema) {
    auto registeredClass = registerSchema("c 'MyObject' {'prop': b}");

    ASSERT_EQ(std::vector<ValueSchema>(), _registry->getAllSchemas());

    SimpleExceptionTracker exceptionTracker;
    registeredClass.ensureSchemaRegistered(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    auto expectedSchema = ValueSchema::cls(
        STRING_LITERAL("MyObject"), false, {ClassPropertySchema(STRING_LITERAL("prop"), ValueSchema::boolean())});

    ASSERT_EQ(std::vector<ValueSchema>({expectedSchema}), _registry->getAllSchemas());

    // Schema should be registered only once
    registeredClass.ensureSchemaRegistered(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    ASSERT_EQ(std::vector<ValueSchema>({expectedSchema}), _registry->getAllSchemas());
}

TEST_F(CppGeneratedClassTests, resolvesSchema) {
    auto registeredClass1 = registerSchema("c 'MyObject' {'prop': b}");

    auto registeredClass2 = registerSchema("c 'MyObjectList' {'array': a<r:'MyObject'>}",
                                           [&]() -> TypeReferencesVec { return {&registeredClass1}; });

    SimpleExceptionTracker exceptionTracker;
    auto classSchema = registeredClass2.getResolvedClassSchema(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    ASSERT_EQ("class 'MyObjectList'{'array': array<link:ref:'MyObject'>}", ValueSchema::cls(classSchema).toString());
}

TEST_F(CppGeneratedClassTests, registersSchemaDependenciesWhenResolvingSchema) {
    auto registeredClass1 = registerSchema("c 'MyObject' {'prop': b}");

    auto registeredClass2 = registerSchema("c 'MyObjectList' {'array': a<r:'MyObject'>}",
                                           [&]() -> TypeReferencesVec { return {&registeredClass1}; });

    ASSERT_EQ(std::vector<ValueSchema>(), _registry->getAllSchemas());

    SimpleExceptionTracker exceptionTracker;
    registeredClass2.getResolvedClassSchema(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    auto schemas = _registry->getAllSchemas();

    ASSERT_EQ(static_cast<size_t>(2), schemas.size());

    ASSERT_EQ("class 'MyObjectList'{'array': array<link:ref:'MyObject'>}", schemas[0].toString());
    ASSERT_EQ("class 'MyObject'{'prop': bool}", schemas[1].toString());

    // Calling getResolvedClassSchema again should be a no-op
    registeredClass2.getResolvedClassSchema(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    schemas = _registry->getAllSchemas();

    ASSERT_EQ(static_cast<size_t>(2), schemas.size());

    ASSERT_EQ("class 'MyObjectList'{'array': array<link:ref:'MyObject'>}", schemas[0].toString());
    ASSERT_EQ("class 'MyObject'{'prop': bool}", schemas[1].toString());
}

TEST_F(CppGeneratedClassTests, canGetClassName) {
    auto registeredClass1 = registerSchema("c 'MyObject' {'prop': b}");
    ASSERT_EQ(STRING_LITERAL("MyObject"), registeredClass1.getClassName());
}

} // namespace ValdiTest
