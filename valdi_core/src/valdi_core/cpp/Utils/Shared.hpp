//
//  Shared.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 2/4/19.
//

#pragma once

#include "utils/debugging/Assert.hpp"
#include <atomic>
#include <memory>
#include <type_traits>

namespace Valdi {

template<typename T>
using Shared = std::shared_ptr<T>;

template<typename T>
using Weak = std::weak_ptr<T>;

class SharedPtrRefCountable;

template<typename T>
class Ref;

class RefCountable {
public:
    virtual ~RefCountable() = default;

    virtual void unsafeRetainInner() = 0;
    virtual void unsafeReleaseInner() = 0;
};

/**
 * A RefCountable implementation that uses an atomic long
 * for ref counting. Does not support weak pointers.
 * If you need weak pointers support, please use SharedPtrRefCountable.
 */
class SimpleRefCountable : public RefCountable {
public:
    SimpleRefCountable();
    ~SimpleRefCountable() override;

    void unsafeRetainInner() final;
    void unsafeReleaseInner() final;

    long retainCount() const;

private:
    std::atomic_long _retainCount;
};

/**
 * A RefCountable implementation that uses a non atomic
 * long for ref counting and does not support weak pointers.
 * Please DO NOT USE THIS unless you are absolutely certain
 * that the instances from this class won't be shared between
 * threads. Please only consider this class if you've measured
 * that atomic ref counting is a bottleneck for your use case.
 */
class NonAtomicRefCountable : public RefCountable {
public:
    NonAtomicRefCountable();
    ~NonAtomicRefCountable() override;

    void unsafeRetainInner() final;
    void unsafeReleaseInner() final;

    long retainCount() const;

private:
    long _retainCount;
};

/**
 * A RefCountable implementation that uses C++'s builtin std::shared_ptr
 * for ref counting, and support weak pointers. Instances of this class
 * can be converted into std::shared_ptr at no cost.
 * It uses slightly more memory than SimpleRefCountable, and increment/decrement
 * are slightly slower. Prefer using SimpleRefCountable unless you need
 * std::shared_ptr compatibility or weak pointers support.
 */
class SharedPtrRefCountable : public std::enable_shared_from_this<SharedPtrRefCountable>, public RefCountable {
public:
    long retainCount() const;

    void unsafeRetainInner() final;

    void unsafeReleaseInner() final;

    /**
     * Takes an owning reference the way unsafeRetainInner() does, except that it refuses to do so
     * when the strong reference count has already reached zero, in which case it takes no
     * reference and returns false. Use this whenever the pointer came in as a raw pointer, since
     * unsafeRetainInner() would revive an instance that is being (or has been) destroyed.
     */
    bool tryRetainInnerFromRaw();

    const Shared<SharedPtrRefCountable>* getInnerSharedPtr() const;

    /**
     * The weak reference std::enable_shared_from_this keeps to this instance. Locking it is the
     * only way to obtain a strong reference from a raw pointer without reviving a dead instance,
     * and copying it is the only way to obtain a weak reference without touching the strong count.
     */
    const Weak<SharedPtrRefCountable>* getInnerWeakPtr() const;
};

/**
 Aborts the process. Called when an owning reference is requested from a raw pointer to a
 SharedPtrRefCountable whose strong reference count already reached zero. The abort names the
 failure at the call site that caused it, instead of letting the revived instance be destroyed a
 second time and double free its members later, on another thread.
*/
[[noreturn]] void reportRetainOfDestroyedInstance();

/**
 Increment the reference count of a SharedPtrRefCountable that came in as a raw pointer.

 Unlike unsafeRetain(), which is always handed a pointer some other reference already owns, the
 argument here may be a pointer to an instance whose strong count already reached zero: its
 destructor is running or has run, while its storage is kept alive by the weak references holding
 the control block. Incrementing the count of such an instance revives it, and destroys it a
 second time when the new reference drops. Abort instead.
*/
template<typename T>
T* unsafeRetainFromRaw(T* instance) {
    if (instance != nullptr) {
        if constexpr (std::is_convertible_v<T*, SharedPtrRefCountable*>) {
            if (!static_cast<SharedPtrRefCountable*>(instance)->tryRetainInnerFromRaw()) {
                reportRetainOfDestroyedInstance();
            }
        } else {
            // SimpleRefCountable and NonAtomicRefCountable free their storage as the count reaches
            // zero, so there is nothing left to inspect and no safe check to make.
            instance->unsafeRetainInner();
        }
    }
    return instance;
}

template<typename T>
constexpr void assertSharedTypeCompatible() {
    static_assert(std::is_convertible_v<T*, SharedPtrRefCountable*>,
                  "Only types that inherits from SharedPtrRefCountable are compatible");
}

struct AdoptRef {};

/**
 A Shared pointer which is 1 word long.
 It is only compatible with pointers returned from Valdi::makeShared<>() and which inherits SharedPtrRefCountable.
 */
template<typename T>
class Ref {
public:
    Ref() : _ptr(nullptr) {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    Ref(const std::nullptr_t& /*ptr*/) : Ref() {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    Ref(Ref<T>&& other) noexcept : _ptr(other._ptr) {
        other._ptr = nullptr;
    }

    explicit Ref(T* ptr) : _ptr(unsafeRetainFromRaw(ptr)) {}

    Ref(T* ptr, AdoptRef /*unused*/) : _ptr(ptr) {}

    Ref(const Ref<T>& other) : _ptr(unsafeRetain(other._ptr)) {}

    template<typename From>
    // NOLINTNEXTLINE(google-explicit-constructor)
    Ref(const Ref<From>& other, typename std::enable_if_t<std::is_convertible<From*, T*>::value, int> /*unused*/ = 0)
        : _ptr(unsafeRetain(other.get())) {}

    template<typename From>
    // NOLINTNEXTLINE(google-explicit-constructor)
    Ref(const Shared<From>& other, typename std::enable_if_t<std::is_convertible<From*, T*>::value, int> /*unused*/ = 0)
        : _ptr(unsafeRetain(other.get())) {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    Ref(Shared<T>&& other) : _ptr(unsafeSharedMove(std::move(other))) {}

    ~Ref() {
        unsafeRelease(_ptr);
    }

    /*
     Every assignment below takes the new reference before releasing the old one, because the
     argument may be owned by the instance currently held: in `ref = ref->child`, releasing first
     destroys the parent and with it the child, leaving the assignment to retain, or even to read,
     an already destroyed argument.
    */
    Ref<T>& operator=(T* ptr) {
        if (_ptr != ptr) {
            T* old = _ptr;
            _ptr = unsafeRetainFromRaw(ptr);
            unsafeRelease(old);
        }
        return *this;
    }

    Ref<T>& operator=(const Ref<T>& other) {
        if (this != &other) {
            T* old = _ptr;
            _ptr = unsafeRetain(other._ptr);
            unsafeRelease(old);
        }
        return *this;
    }

    Ref<T>& operator=(const Shared<T>& other) {
        T* old = _ptr;
        _ptr = unsafeRetain(other.get());
        unsafeRelease(old);

        return *this;
    }

    Ref<T>& operator=(Shared<T>&& other) {
        T* old = _ptr;
        _ptr = unsafeSharedMove(std::move(other));
        unsafeRelease(old);

        return *this;
    }

    Ref<T>& operator=(Ref<T>&& other) noexcept {
        if (this != &other) {
            T* newPtr = other._ptr;
            other._ptr = nullptr;

            T* old = _ptr;
            _ptr = newPtr;
            unsafeRelease(old);
        }
        return *this;
    }

    constexpr T* operator->() const {
        return _ptr;
    }

    constexpr T& operator*() const {
        return *_ptr;
    }

    constexpr T* get() const {
        return _ptr;
    }

    Shared<T> toShared() const {
        assertSharedTypeCompatible<T>();
        return strongRef(_ptr);
    }

    Weak<T> toWeak() const {
        assertSharedTypeCompatible<T>();
        return weakRef(_ptr);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    long use_count() const {
        return _ptr != nullptr ? _ptr->retainCount() : 0;
    }

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator Weak<T>() const& {
        assertSharedTypeCompatible<T>();
        return toWeak();
    }

private:
    T* _ptr;
};

template<typename T>
inline bool operator==(const Ref<T>& left, const std::nullptr_t& right) {
    return left.get() == right;
}

template<typename T>
inline bool operator==(const std::nullptr_t& left, const Ref<T>& right) {
    return left == right.get();
}

template<typename T>
inline bool operator!=(const Ref<T>& left, const std::nullptr_t& right) {
    return !(left == right);
}

template<typename T>
inline bool operator!=(const std::nullptr_t& left, const Ref<T>& right) {
    return !(left == right);
}

template<typename T, typename T2>
inline bool operator==(const Ref<T>& left, const Ref<T2>& right) {
    return left.get() == right.get();
}

template<typename T, typename T2>
inline bool operator!=(const Ref<T>& left, const Ref<T2>& right) {
    return !(left == right);
}

template<typename T, typename T2>
inline bool operator==(const Shared<T>& left, const Ref<T2>& right) {
    return left.get() == right.get();
}

template<typename T, typename T2>
inline bool operator!=(const Shared<T>& left, const Ref<T2>& right) {
    return !(left == right);
}

template<typename T, typename T2>
inline bool operator==(const Ref<T>& left, const Shared<T2>& right) {
    return left.get() == right.get();
}

template<typename T, typename T2>
inline bool operator!=(const Ref<T>& left, const Shared<T2>& right) {
    return !(left == right);
}

template<typename T,
         typename std::enable_if<std::is_convertible<T*, SimpleRefCountable*>::value, int>::type = 0,
         typename... Args>
Ref<T> makeShared(Args&&... args) {
    return Ref<T>(new T(std::forward<Args>(args)...), AdoptRef());
}

template<typename T,
         typename std::enable_if<std::is_convertible<T*, NonAtomicRefCountable*>::value, int>::type = 0,
         typename... Args>
Ref<T> makeShared(Args&&... args) {
    return Ref<T>(new T(std::forward<Args>(args)...), AdoptRef());
}

template<typename T,
         typename std::enable_if<std::is_convertible<T*, SharedPtrRefCountable*>::value, int>::type = 0,
         typename... Args>
inline Ref<T> makeShared(Args&&... args) {
    typename std::aligned_storage<sizeof(std::shared_ptr<T>), alignof(std::shared_ptr<T>)>::type storage;
    static_assert(sizeof(storage) == sizeof(std::shared_ptr<T>));
    new (&storage) std::shared_ptr<T>(std::make_shared<T>(std::forward<Args>(args)...));

    return Ref<T>(reinterpret_cast<std::shared_ptr<T>&>(storage).get(), AdoptRef());
}

template<typename T,
         typename std::enable_if<!std::is_convertible<T*, RefCountable*>::value, int>::type = 0,
         typename... Args>
inline Shared<T> makeShared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

/**
Increment the reference count of a SharedPtrRefCountable allocated from Valdi::makeShared<T> by 1.
*/
template<typename T>
T* unsafeRetain(const std::shared_ptr<T>& instance) {
    return unsafeRetain(instance.get());
}

/**
Increment the reference count of a SharedPtrRefCountable allocated from Valdi::makeShared<T> by 1.
*/
template<typename T>
T* unsafeRetain(const Ref<T>& instance) {
    return unsafeRetain(instance.get());
}

/**
Increment the reference count of a SharedPtrRefCountable allocated from Valdi::makeShared<T> by 1.
*/
template<typename T>
T* unsafeRetain(T* instance) {
    if (instance != nullptr) {
        instance->unsafeRetainInner();
    }
    return instance;
}

template<typename T>
void* unsafeBridgeRetain(T* instance) {
    return reinterpret_cast<void*>(unsafeRetain(static_cast<RefCountable*>(instance)));
}

void unsafeBridgeRelease(void* bridgedInstance);

template<typename T>
Ref<T> unsafeBridge(void* bridgedInstance) {
    return Ref<T>(static_cast<T*>(reinterpret_cast<RefCountable*>(bridgedInstance)));
}

template<typename T>
T* unsafeBridgeUnretained(void* bridgedInstance) {
    return static_cast<T*>(reinterpret_cast<RefCountable*>(bridgedInstance));
}

template<typename T>
Ref<T> unsafeBridgeTransfer(void* bridgedInstance) {
    return Ref<T>(static_cast<T*>(reinterpret_cast<RefCountable*>(bridgedInstance)), AdoptRef());
}

template<typename T>
void* unsafeBridgeCast(T* instance) {
    return reinterpret_cast<void*>(static_cast<RefCountable*>(instance));
}

/**
 Decrement the reference count of a SharedPtrRefCountable allocated from Valdi::makeShared<T> by 1
*/
template<typename T>
void unsafeRelease(T* instance) {
    if (instance != nullptr) {
        instance->unsafeReleaseInner();
    }
}

template<typename T, typename std::enable_if<std::is_convertible<T*, SharedPtrRefCountable*>::value, int>::type = 0>
Shared<T> strongRef(T* instance) {
    if (instance == nullptr) {
        return nullptr;
    }

    const auto* innerSharedPtr = instance->getInnerSharedPtr();
    if (innerSharedPtr->get() == nullptr) {
        // Never owned by a shared_ptr: a stack instance, or one referenced from inside its own
        // constructor, before makeShared() wired up the weak reference. There is no count to take
        // and nothing to revive, so keep the historical non-owning aliasing behaviour.
        return Shared<T>(*innerSharedPtr, instance);
    }

    // Copying the inner shared_ptr would increment the count unconditionally, reviving an instance
    // that already reached zero. Locking the weak reference refuses to do that.
    auto owner = instance->getInnerWeakPtr()->lock();
    if (owner == nullptr) {
        return nullptr;
    }

    return Shared<T>(owner, instance);
}

template<typename T, typename std::enable_if<std::is_convertible<T*, SharedPtrRefCountable*>::value, int>::type = 0>
Ref<T> strongRef(const Weak<T>& weakRef) {
    return Ref<T>(weakRef.lock());
}

template<typename T>
Ref<T> strongSmallRef(T* instance) {
    return Ref<T>(instance);
}

template<typename T, typename std::enable_if<std::is_convertible<T*, SharedPtrRefCountable*>::value, int>::type = 0>
Weak<T> weakRef(T* instance) {
    // strongRef() locks the inner weak reference, so the transient strong reference below cannot
    // revive an instance whose count already reached zero: it comes back null and the result is an
    // expired Weak. std::weak_ptr has no aliasing constructor, so this is the only cast-free path
    // from a raw pointer to a Weak<T>.
    return Weak<T>(strongRef(instance));
}

/**
 Move a std::shared_ptr<T> into a T* while maintaining the current reference count.
*/
template<typename T, typename std::enable_if<std::is_convertible<T*, SharedPtrRefCountable*>::value, int>::type = 0>
T* unsafeSharedMove(std::shared_ptr<T>&& ptr) {
    auto* innerPtr = ptr.get();

    typename std::aligned_storage<sizeof(std::shared_ptr<T>), alignof(std::shared_ptr<T>)>::type storage;
    static_assert(sizeof(storage) == sizeof(std::shared_ptr<T>));
    new (&storage) std::shared_ptr<T>(std::move(ptr));

    return innerPtr;
}

template<typename T, typename From>
Ref<T> unsafeCast(const Ref<From>& ptr) {
    return Ref<T>(static_cast<T*>(ptr.get()));
}

template<typename T, typename From>
Shared<T> unsafeCast(const Shared<From>& ptr) {
    return std::static_pointer_cast<T>(ptr);
}

template<typename T, typename From>
Ref<T> castOrNull(const Ref<From>& ptr) {
    return Ref<T>(dynamic_cast<T*>(ptr.get()));
}

template<typename T, typename From>
Shared<T> castOrNull(const Shared<From>& ptr) {
    return std::dynamic_pointer_cast<T>(ptr);
}

} // namespace Valdi
