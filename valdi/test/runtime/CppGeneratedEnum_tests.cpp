#include "valdi_core/cpp/Marshalling/CppGeneratedEnum.hpp"
#include <gtest/gtest.h>

using namespace Valdi;

namespace ValdiTest {

namespace {

enum class SparseIntEnum { UNSET = 0, FIRST = 100, SECOND = 105 };

const CppIntEnumMarshaller<SparseIntEnum, 3>& sparseMarshaller() {
    static auto kMarshaller = CppIntEnumMarshaller<SparseIntEnum, 3>({0, 100, 105});
    return kMarshaller;
}

} // namespace

TEST(CppIntEnumMarshaller, marshallsCaseToItsValue) {
    SimpleExceptionTracker exceptionTracker;
    Value out;

    sparseMarshaller().marshall(exceptionTracker, SparseIntEnum::SECOND, out);

    ASSERT_TRUE(exceptionTracker);
    ASSERT_EQ(105, out.toInt());
}

TEST(CppIntEnumMarshaller, roundTripsNonContiguousValues) {
    for (auto value : {SparseIntEnum::UNSET, SparseIntEnum::FIRST, SparseIntEnum::SECOND}) {
        SimpleExceptionTracker exceptionTracker;
        Value marshalled;
        SparseIntEnum unmarshalled = SparseIntEnum::UNSET;

        sparseMarshaller().marshall(exceptionTracker, value, marshalled);
        sparseMarshaller().unmarshall(exceptionTracker, marshalled, unmarshalled);

        ASSERT_TRUE(exceptionTracker);
        ASSERT_EQ(value, unmarshalled);
    }
}

TEST(CppIntEnumMarshaller, rejectsUnknownValues) {
    SimpleExceptionTracker marshallTracker;
    Value out;
    sparseMarshaller().marshall(marshallTracker, static_cast<SparseIntEnum>(1), out);
    ASSERT_FALSE(marshallTracker);
    marshallTracker.clearError();

    SimpleExceptionTracker unmarshallTracker;
    SparseIntEnum unmarshalled = SparseIntEnum::UNSET;
    sparseMarshaller().unmarshall(unmarshallTracker, Value(101), unmarshalled);
    ASSERT_FALSE(unmarshallTracker);
    unmarshallTracker.clearError();
}

} // namespace ValdiTest
