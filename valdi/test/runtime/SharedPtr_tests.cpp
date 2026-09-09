//
//  SharedPtr_tests.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 10/23/19.
//

#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include <gtest/gtest.h>

using namespace Valdi;

namespace ValdiTest {

struct Int : public SharedPtrRefCountable {
    int value;

    Int(int value) : value(value) {}
};

struct Container : public SimpleRefCountable {
    Shared<Int> value;
};

struct Outer : public SharedPtrRefCountable {
    Shared<Int> inner;

    Outer() {
        inner = Valdi::makeShared<Int>(0).toShared();
    }
};

struct DestroyCounted : public SharedPtrRefCountable {
    int* destroyCount;

    explicit DestroyCounted(int* destroyCount) : destroyCount(destroyCount) {}

    ~DestroyCounted() override {
        ++*destroyCount;
    }
};

// Owns the only references to a single DestroyCounted, through a Ref, an upcast Ref and a Shared,
// so releasing the parent destroys the child. Assigning one of those members into the Ref that
// holds the last parent reference is the tied-lifetime shape: the argument stays alive only as
// long as the parent it lives in.
struct Parent : public SharedPtrRefCountable {
    int* parentDestroyCount;
    Ref<DestroyCounted> child;
    Ref<SharedPtrRefCountable> childBase;
    Shared<SharedPtrRefCountable> childShared;

    Parent(int* parentDestroyCount, int* childDestroyCount)
        : parentDestroyCount(parentDestroyCount),
          child(Valdi::makeShared<DestroyCounted>(childDestroyCount)),
          childBase(child),
          childShared(child.toShared()) {}

    ~Parent() override {
        ++*parentDestroyCount;
    }
};

// Releases the last strong reference to a DestroyCounted while keeping its storage alive through
// a weak reference, then hands back the raw pointer to it. This is the shape a use-after-destroy
// takes in production: the destructor has run, but the control block, and therefore the storage,
// is still around because something else holds a weak reference to it.
struct DestroyedInstance {
    int destroyCount = 0;
    Weak<DestroyCounted> weak;
    DestroyCounted* rawPtr = nullptr;

    DestroyedInstance() {
        auto object = Valdi::makeShared<DestroyCounted>(&destroyCount);
        rawPtr = object.get();
        weak = object.toWeak();
    }
};

TEST(SharedPtr, canBridgeRetainAndRelease) {
    auto object = Valdi::makeShared<Int>(0).toShared();

    ASSERT_EQ(1, object.use_count());
    auto ptr = unsafeRetain(object);
    ASSERT_EQ(2, object.use_count());
    unsafeRelease(ptr);
    ASSERT_EQ(1, object.use_count());
}

TEST(SharedPtr, canAllocateSharedPtrRefCountable) {
    auto outer = Valdi::makeShared<Outer>();

    ASSERT_EQ(1, outer.use_count());

    auto outerShared = outer.toShared();

    ASSERT_EQ(2, outer.use_count());
    ASSERT_EQ(2, outerShared.use_count());

    outer = nullptr;

    ASSERT_EQ(1, outerShared.use_count());
}

TEST(SharedPtr, canDeallocateOnBridgeRelease) {
    auto outer = Valdi::makeShared<Outer>().toShared();
    Weak<Int> inner = outer->inner;

    ASSERT_EQ(1, inner.use_count());
    auto ptr = unsafeRetain(outer);
    outer = nullptr;
    ASSERT_EQ(1, inner.use_count());
    unsafeRelease(ptr);
    ASSERT_EQ(0, inner.use_count());
}

TEST(SharedPtr, canBridge) {
    auto object = Valdi::makeShared<Int>(0).toShared();

    ASSERT_EQ(1, object.use_count());
    auto ptr = unsafeRetain(object);
    ASSERT_EQ(2, object.use_count());

    auto object2 = strongRef(ptr);
    ASSERT_EQ(3, object.use_count());
    ASSERT_EQ(object, object2);

    unsafeRelease(ptr);
    ASSERT_EQ(2, object.use_count());

    object2 = nullptr;

    ASSERT_EQ(1, object.use_count());
}

TEST(SharedPtr, canRetainFromRawPtr) {
    auto object = Valdi::makeShared<Int>(0).toShared();

    ASSERT_EQ(1, object.use_count());
    auto ptr = unsafeRetain(object);
    ASSERT_EQ(2, object.use_count());
    unsafeRetain(ptr);
    ASSERT_EQ(3, object.use_count());
    unsafeRelease(ptr);
    ASSERT_EQ(2, object.use_count());
    unsafeRelease(ptr);
    ASSERT_EQ(1, object.use_count());
}

TEST(SharedPtr, canMove) {
    auto object = Valdi::makeShared<Int>(0).toShared();
    auto copy = object;

    ASSERT_EQ(2, object.use_count());

    auto rawPtr = unsafeSharedMove(std::move(copy));

    ASSERT_EQ(2, object.use_count());
    ASSERT_TRUE(copy == nullptr);
    ASSERT_EQ(object.get(), rawPtr);

    copy = nullptr;
    ASSERT_EQ(2, object.use_count());

    unsafeRelease(rawPtr);
    ASSERT_EQ(1, object.use_count());
}

TEST(Ref, canRetainAndRelease) {
    auto object = Valdi::makeShared<Int>(0).toShared();

    Ref<Int> ptr(object.get());

    ASSERT_EQ(object.get(), ptr.get());
    ASSERT_EQ(2, object.use_count());

    ptr = nullptr;

    ASSERT_EQ(nullptr, ptr.get());
    ASSERT_EQ(1, object.use_count());
}

TEST(Ref, retainAndReleaseOnCopy) {
    auto object = Valdi::makeShared<Int>(0).toShared();

    Ref<Int> ptr(object.get());

    ASSERT_EQ(2, object.use_count());

    Ref<Int> copy1(ptr);
    ASSERT_EQ(object.get(), copy1.get());
    ASSERT_EQ(3, object.use_count());

    auto copy2 = copy1;
    ASSERT_EQ(object.get(), copy2.get());
    ASSERT_EQ(4, object.use_count());

    copy2 = nullptr;
    ASSERT_EQ(nullptr, copy2.get());
    ASSERT_EQ(3, object.use_count());

    copy1 = nullptr;
    ASSERT_EQ(nullptr, copy1.get());
    ASSERT_EQ(2, object.use_count());

    ptr = nullptr;
    ASSERT_EQ(nullptr, ptr.get());
    ASSERT_EQ(1, object.use_count());
}

TEST(Ref, canMove) {
    auto object = Valdi::makeShared<Int>(0).toShared();

    Ref<Int> ptr(object.get());

    ASSERT_EQ(2, object.use_count());

    Ref<Int> move1(std::move(ptr));
    ASSERT_EQ(object.get(), move1.get());
    ASSERT_EQ(nullptr, ptr.get());
    ASSERT_EQ(2, object.use_count());

    Ref<Int> move2 = std::move(move1);
    ASSERT_EQ(object.get(), move2.get());
    ASSERT_EQ(nullptr, move1.get());
    ASSERT_EQ(2, object.use_count());

    move1 = nullptr;
    ASSERT_EQ(2, object.use_count());
    ptr = nullptr;
    ASSERT_EQ(2, object.use_count());
    move2 = nullptr;
    ASSERT_EQ(1, object.use_count());
}

TEST(Ref, canCreateWeakRef) {
    auto object = Valdi::makeShared<Int>(0).toShared();

    Ref<Int> ptr(object.get());

    ASSERT_EQ(2, object.use_count());

    auto weak = weakRef(ptr.get());

    ASSERT_EQ(2, object.use_count());
    ASSERT_FALSE(weak.expired());
    ASSERT_EQ(weak.lock(), object);
}

TEST(Ref, weakRefDoesNotRetain) {
    auto object = Valdi::makeShared<Int>(0).toShared();

    ASSERT_EQ(1, object->retainCount());

    auto weak = weakRef(object.get());

    ASSERT_EQ(1, object->retainCount());
    ASSERT_FALSE(weak.expired());
    ASSERT_EQ(object, weak.lock());

    // Locking is what takes the strong reference, and dropping it gives it back.
    {
        auto locked = weak.lock();
        ASSERT_EQ(2, object->retainCount());
    }

    ASSERT_EQ(1, object->retainCount());
}

TEST(Ref, rawSelfAssignmentKeepsTheLastReference) {
    int destroyCount = 0;
    Ref<DestroyCounted> ref(Valdi::makeShared<DestroyCounted>(&destroyCount));
    auto* rawPtr = ref.get();

    ASSERT_EQ(1, ref->retainCount());

    // Releasing before retaining would drop the only reference here, destroying the instance and
    // leaving the retain to abort on a dead one.
    ref = ref.get();

    ASSERT_EQ(0, destroyCount);
    ASSERT_EQ(1, ref->retainCount());
    ASSERT_EQ(rawPtr, ref.get());
}

TEST(Ref, rawTiedLifetimeAssignmentRetainsBeforeReleasing) {
    int parentDestroyCount = 0;
    int childDestroyCount = 0;
    Ref<SharedPtrRefCountable> ref = Valdi::makeShared<Parent>(&parentDestroyCount, &childDestroyCount);

    ASSERT_EQ(1, ref->retainCount());

    // The raw pointer is owned by the instance ref is about to let go of. Releasing first destroys
    // the parent, and with it the child, so the retain would land on a destroyed instance.
    ref = static_cast<Parent*>(ref.get())->child.get();

    ASSERT_EQ(1, parentDestroyCount);
    ASSERT_EQ(0, childDestroyCount);
    ASSERT_EQ(1, ref->retainCount());

    ref = nullptr;

    ASSERT_EQ(1, childDestroyCount);
}

TEST(Ref, refTiedLifetimeAssignmentRetainsBeforeReleasing) {
    int parentDestroyCount = 0;
    int childDestroyCount = 0;
    Ref<SharedPtrRefCountable> ref = Valdi::makeShared<Parent>(&parentDestroyCount, &childDestroyCount);

    ASSERT_EQ(1, ref->retainCount());

    // childBase is a Ref<SharedPtrRefCountable>, so this binds to operator=(const Ref<T>&) without
    // a converting temporary: other itself lives inside the parent ref is releasing.
    ref = static_cast<Parent*>(ref.get())->childBase;

    ASSERT_EQ(1, parentDestroyCount);
    ASSERT_EQ(0, childDestroyCount);
    ASSERT_EQ(1, ref->retainCount());

    ref = nullptr;

    ASSERT_EQ(1, childDestroyCount);
}

TEST(Ref, sharedMoveTiedLifetimeAssignmentRetainsBeforeReleasing) {
    int parentDestroyCount = 0;
    int childDestroyCount = 0;
    Ref<SharedPtrRefCountable> ref = Valdi::makeShared<Parent>(&parentDestroyCount, &childDestroyCount);

    ASSERT_EQ(1, ref->retainCount());

    // Same shape through operator=(Shared<T>&&): the shared_ptr being moved from is a member of the
    // parent, so releasing first would move out of freed storage.
    ref = std::move(static_cast<Parent*>(ref.get())->childShared);

    ASSERT_EQ(1, parentDestroyCount);
    ASSERT_EQ(0, childDestroyCount);
    ASSERT_EQ(1, ref->retainCount());

    ref = nullptr;

    ASSERT_EQ(1, childDestroyCount);
}

TEST(SharedPtr, strongRefReturnsNullForDestroyedInstance) {
    DestroyedInstance destroyed;

    ASSERT_EQ(1, destroyed.destroyCount);
    ASSERT_TRUE(destroyed.weak.expired());

    // Taking a strong reference here would revive the instance and destroy it a second time when
    // that reference dropped.
    ASSERT_TRUE(strongRef(destroyed.rawPtr) == nullptr);
    ASSERT_EQ(1, destroyed.destroyCount);
}

TEST(SharedPtr, weakRefIsExpiredForDestroyedInstance) {
    DestroyedInstance destroyed;

    ASSERT_EQ(1, destroyed.destroyCount);

    // weakRef() goes through strongRef(), whose lock comes back null here, so the transient strong
    // reference cannot revive the instance and destroy it again on release.
    auto weak = weakRef(destroyed.rawPtr);

    ASSERT_TRUE(weak.expired());
    ASSERT_TRUE(weak.lock() == nullptr);
    ASSERT_EQ(1, destroyed.destroyCount);
}

TEST(Ref, refusesToRetainDestroyedInstanceFromRawPointer) {
    DestroyedInstance destroyed;

    ASSERT_EQ(1, destroyed.destroyCount);

    // Don't pin the signal or the abort message: the SC_ABORT fires on both platforms, but macOS
    // aborts with the message on stderr while on Linux/glibc the assert reporter itself faults
    // while formatting, so nothing reaches the child's stderr. Matching ".*" keeps the real
    // guarantee (a retain of a destroyed instance must not silently succeed) portable.
    EXPECT_DEATH(
        {
            Ref<DestroyCounted> revived(destroyed.rawPtr);
            (void)revived;
        },
        ".*");
    EXPECT_DEATH({ (void)strongSmallRef(destroyed.rawPtr); }, ".*");

    // Neither attempt ran the destructor again in this process.
    ASSERT_EQ(1, destroyed.destroyCount);
}

TEST(SimpleRefCountable, canAllocAndDealloc) {
    auto value = makeShared<Int>(42).toShared();
    auto container = makeShared<Container>();

    ASSERT_EQ(1, value.use_count());
    ASSERT_EQ(1, container.use_count());

    container->value = value;

    ASSERT_EQ(2, value.use_count());
    ASSERT_EQ(1, container.use_count());

    container = nullptr;

    ASSERT_EQ(1, value.use_count());
    ASSERT_EQ(0, container.use_count());
}

} // namespace ValdiTest
