#include <gtest/gtest.h>

#import "valdi_core/SCValdiFieldValue.h"
#import "valdi_core/SCValdiMarshallableObjectUtils.h"

#include <atomic>

/// Records its own deallocation so a test can tell "still alive" from "freed underneath us" without
/// dereferencing a pointer that may already be dangling.
@interface SCValdiDeallocCounter: NSObject
@end

@implementation SCValdiDeallocCounter {
    std::atomic<int> *_deallocCount;
}

- (instancetype)initWithDeallocCount:(std::atomic<int> *)deallocCount
{
    self = [super init];

    if (self) {
        _deallocCount = deallocCount;
    }

    return self;
}

- (void)dealloc
{
    _deallocCount->fetch_add(1);
}

@end

namespace {

SCValdiFieldValueDescriptor objectFieldDescriptor() {
    SCValdiFieldValueDescriptor descriptor = SCValdiFieldValueDescriptorMakeEmpty();
    descriptor.type = SCValdiFieldValueTypeObject;
    return descriptor;
}

TEST(SCValdiMarshallableObjectUtils, replacingAFieldReleasesTheOutgoingValueExactlyOnce) {
    std::atomic<int> firstDeallocCount{0};
    std::atomic<int> secondDeallocCount{0};
    SCValdiFieldValue *storage = SCValdiAllocateFieldsStorage(1);
    const SCValdiFieldValueDescriptor descriptor = objectFieldDescriptor();

    @autoreleasepool {
        id first = [[SCValdiDeallocCounter alloc] initWithDeallocCount:&firstDeallocCount];
        SCValdiSetFieldsStorageObjectValue(storage, 0, NO, first);
    }
    ASSERT_EQ(0, firstDeallocCount.load()) << "storage must own the value it was given";

    @autoreleasepool {
        id second = [[SCValdiDeallocCounter alloc] initWithDeallocCount:&secondDeallocCount];
        SCValdiSetFieldsStorageObjectValue(storage, 0, NO, second);
        ASSERT_TRUE(SCValdiFieldValueGetObject(storage[0]) == second);
    }
    ASSERT_EQ(1, firstDeallocCount.load()) << "replaced value must be released once";
    ASSERT_EQ(0, secondDeallocCount.load());

    SCValdiDeallocateFieldsStorage(storage, 1, &descriptor);
    ASSERT_EQ(1, firstDeallocCount.load());
    ASSERT_EQ(1, secondDeallocCount.load()) << "teardown must release the value still in the slot";
}

// Regression test for the crash in PREVIEW-32646. Assigning a field the value it already holds used
// to release the outgoing value before retaining the incoming one, so when the field was the only
// owner the object was freed and then "re-retained" through a dangling pointer.
TEST(SCValdiMarshallableObjectUtils, selfAssignmentKeepsTheValueAlive) {
    std::atomic<int> deallocCount{0};
    SCValdiFieldValue *storage = SCValdiAllocateFieldsStorage(1);
    const SCValdiFieldValueDescriptor descriptor = objectFieldDescriptor();

    @autoreleasepool {
        id value = [[SCValdiDeallocCounter alloc] initWithDeallocCount:&deallocCount];
        SCValdiSetFieldsStorageObjectValue(storage, 0, NO, value);
    }
    ASSERT_EQ(0, deallocCount.load());

    // Read the slot without taking a reference, the way a generated trampoline forwards an
    // unretained field value, so that storage is genuinely the only owner across the assignment.
    __unsafe_unretained id currentValue = SCValdiFieldValueGetObject(storage[0]);
    SCValdiSetFieldsStorageObjectValue(storage, 0, NO, currentValue);

    ASSERT_EQ(0, deallocCount.load()) << "self-assignment must not release the only reference";
    ASSERT_TRUE(SCValdiFieldValueGetObject(storage[0]) == currentValue);

    SCValdiDeallocateFieldsStorage(storage, 1, &descriptor);
    ASSERT_EQ(1, deallocCount.load());
}

// Two threads setting one field used to both read the same outgoing pointer and both release it,
// driving the refcount negative. Every value ever stored must be released exactly once, so the
// dealloc count has to match the number of objects created.
TEST(SCValdiMarshallableObjectUtils, concurrentWritesToOneFieldReleaseEachValueExactlyOnce) {
    static constexpr int kWritesPerQueue = 2000;
    std::atomic<int> deallocCount{0};
    SCValdiFieldValue *storage = SCValdiAllocateFieldsStorage(1);
    const SCValdiFieldValueDescriptor descriptor = objectFieldDescriptor();

    dispatch_queue_t first = dispatch_queue_create("com.snap.valdi.fieldstorage.first", DISPATCH_QUEUE_SERIAL);
    dispatch_queue_t second = dispatch_queue_create("com.snap.valdi.fieldstorage.second", DISPATCH_QUEUE_SERIAL);
    dispatch_group_t group = dispatch_group_create();

    // A block captures by const value, and std::atomic is non-copyable, so hand the block a pointer.
    std::atomic<int> *deallocCountRef = &deallocCount;

    for (dispatch_queue_t queue in @[ first, second ]) {
        dispatch_group_async(group, queue, ^{
            for (int i = 0; i < kWritesPerQueue; i++) {
                @autoreleasepool {
                    id value = [[SCValdiDeallocCounter alloc] initWithDeallocCount:deallocCountRef];
                    SCValdiSetFieldsStorageObjectValue(storage, 0, NO, value);
                }
            }
        });
    }
    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

    SCValdiDeallocateFieldsStorage(storage, 1, &descriptor);

    ASSERT_EQ(2 * kWritesPerQueue, deallocCount.load())
        << "every stored value must be released once and only once";
}

TEST(SCValdiMarshallableObjectUtils, clearingAFieldReleasesTheOutgoingValue) {
    std::atomic<int> deallocCount{0};
    SCValdiFieldValue *storage = SCValdiAllocateFieldsStorage(1);
    const SCValdiFieldValueDescriptor descriptor = objectFieldDescriptor();

    @autoreleasepool {
        id value = [[SCValdiDeallocCounter alloc] initWithDeallocCount:&deallocCount];
        SCValdiSetFieldsStorageObjectValue(storage, 0, NO, value);
    }
    ASSERT_EQ(0, deallocCount.load());

    SCValdiSetFieldsStorageObjectValue(storage, 0, NO, nil);
    ASSERT_EQ(1, deallocCount.load());
    ASSERT_TRUE(SCValdiFieldValueGetObject(storage[0]) == nil);

    SCValdiDeallocateFieldsStorage(storage, 1, &descriptor);
    ASSERT_EQ(1, deallocCount.load()) << "an already-cleared slot must not be released again";
}

}  // namespace
