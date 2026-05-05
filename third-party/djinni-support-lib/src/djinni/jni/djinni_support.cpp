//
// Copyright 2014 Dropbox, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This file has been modified by Snap, Inc.
//

#include "../djinni_common.hpp"
#include "djinni_support.hpp"
#include "../proxy_cache_impl.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <locale>
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#include <codecvt>
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static_assert(sizeof(jlong) >= sizeof(void*), "must be able to fit a void* into a jlong");
 
#if defined(ANDROID) || defined(__ANDROID__)
#define DJINNI_SYSTEM_ASSERT(path, line, message) __assert(path, line, message)
#elif defined(__APPLE__)
// Declaring a function from assert.h. It's hidden if NDEBUG is defined
#if defined(NDEBUG)
extern "C" void __assert_rtn(const char*, const char*, int, const char*);
#endif
// that -1 cast was copied from assert.h to suppress function name:
#define DJINNI_SYSTEM_ASSERT(path, line, message) __assert_rtn(reinterpret_cast<const char*>(-1L), path, line, message)
#elif defined(__GLIBC__)
// Declaring a function from assert.h. It's hidden if NDEBUG is defined
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" void __assert(const char* assertion, const char* file, int line);
#define DJINNI_SYSTEM_ASSERT(path, line, message) __assert(message, path, line)
#else
#define DJINNI_SYSTEM_ASSERT(...) [&] { ::abort(); }()
#endif

/*
 * Assert macro for expressions that don't contain JVM calls. Here we don't check for exceptions
 * thus saving some cycles.
 */
#define DJINNI_ASSERT_NO_EXC_CHECK_MSG(check, message) \
    do { \
        const bool check__res = bool(check); \
        if (!check__res) { \
            DJINNI_SYSTEM_ASSERT(__FILE__, __LINE__, message); \
        } \
    } while(false)
#define DJINNI_ASSERT_NO_EXC_CHECK(check) DJINNI_ASSERT_NO_EXC_CHECK_MSG(check, #check)

namespace djinni {

// Set only once from JNI_OnLoad before any other JNI calls, so no lock needed.
static JavaVM * g_cachedJVM;

static pthread_key_t threadExitCallbackKey;

void onThreadExit(void*)
{
    g_cachedJVM->DetachCurrentThread();
}

void createThreadDetachCallbackKey()
{
    pthread_key_create(&threadExitCallbackKey, onThreadExit);
}

/*static*/
JniClassInitializer::registration_vec & JniClassInitializer::get_vec() {
    static JniClassInitializer::registration_vec m;
    return m;
}

/*static*/
std::mutex & JniClassInitializer::get_mutex() {
    static std::mutex mtx;
    return mtx;
}

/*static*/
JniClassInitializer::registration_vec JniClassInitializer::get_all() {
    const std::lock_guard<std::mutex> lock(get_mutex());
    return get_vec();
}

JniClassInitializer::JniClassInitializer(std::function<void()> init) {
    const std::lock_guard<std::mutex> lock(get_mutex());
    get_vec().push_back(std::move(init));
}

static jobject g_ourClassLoader;
static jmethodID g_loadClassMethodID;

static auto& getMethodRecords() {
    static std::vector<std::tuple<const char*, const JNINativeMethod*, size_t>> methods;
    return methods;
}

void jniInit(JavaVM * jvm) {
    g_cachedJVM = jvm;

    auto env = jniGetThreadEnv();

        // References:
    // https://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/functions.html#wp16027
    // https://developer.android.com/training/articles/perf-jni
    // - FAQ: Why didn't FindClass find my class?

    // Cache class loader for our classes for use in non-Java threads
    jclass ourClass = env->FindClass("com/snapchat/djinni/NativeObjectManager");
    jclass classClass = env->GetObjectClass(ourClass);
    jmethodID getClassLoaderMethod = env->GetMethodID(classClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject tmp = env->CallObjectMethod(ourClass, getClassLoaderMethod);
    jniExceptionCheck(env);
    g_ourClassLoader = (jobject)env->NewGlobalRef(tmp);

    jclass classLoaderClass = env->FindClass("java/lang/ClassLoader");
    g_loadClassMethodID = env->GetMethodID(classLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

    for (const auto& record : getMethodRecords()) {
        auto [classString, nativeMethods, nativeMethodsSize] = record;
        try {
            auto clazz = jniFindClass(classString);
            if (env->RegisterNatives(clazz.get(), nativeMethods, nativeMethodsSize)) {
                return;
            }
        } catch (jni_exception& e) {
            // Experimental java classes are not included in the build
        }
    }

    createThreadDetachCallbackKey();

    try {
        for (const auto & initializer : JniClassInitializer::get_all()) {
            initializer();
        }
    } catch (const std::exception &) {
        // Default exception handling only, since non-default might not be safe if init
        // is incomplete.
        jniDefaultSetPendingFromCurrent(jniGetThreadEnv(), __func__);
    }
}

void jniRegisterMethodRecords(const char* className, const JNINativeMethod* records, size_t size) {
    getMethodRecords().emplace_back(className, records, size);
}

void jniRegisterNatives(JNIEnv* env, const char* className, const JNINativeMethod* records, size_t size) {
        auto clazz = jniFindClass(className);
        env->RegisterNatives(clazz.get(), records, size);
}

void jniShutdown() {
    g_cachedJVM = nullptr;
}

JNIEnv * jniGetThreadEnv() {
    assert(g_cachedJVM);
    JNIEnv * env = nullptr;
    jint get_res = g_cachedJVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (get_res == JNI_EDETACHED) {
#if defined(ANDROID) || defined(__ANDROID__)
        get_res = g_cachedJVM->AttachCurrentThread(&env, nullptr);
#else
        get_res = g_cachedJVM->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr);
#endif
        pthread_setspecific(threadExitCallbackKey, env);
    }
    if (get_res != 0 || !env) {
        // :(
        std::abort();
    }

    return env;
}

static JNIEnv * getOptThreadEnv() {
    if (!g_cachedJVM) {
        return nullptr;
    }

    // Special case: this allows us to ignore GlobalRef deletions that happen after this
    // thread has been detached. (This is known to happen during process shutdown, when
    // there's no need to release the ref anyway.)
    JNIEnv * env = nullptr;
    const jint get_res = g_cachedJVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);

    if (get_res == JNI_EDETACHED) {
        return nullptr;
    }

    // Still bail on any other error.
    if (get_res != 0 || !env) {
        // :(
        std::abort();
    }

    return env;
}

void GlobalRefDeleter::operator() (jobject globalRef) noexcept {
    if (globalRef) {
        if (JNIEnv * env = getOptThreadEnv()) {
            env->DeleteGlobalRef(globalRef);
        }
    }
}

void LocalRefDeleter::operator() (jobject localRef) noexcept {
    if (localRef) {
        jniGetThreadEnv()->DeleteLocalRef(localRef);
    }
}

void jni_exception::set_as_pending(JNIEnv * env) const noexcept {
    assert(env);
    env->Throw(java_exception());
}


void jniExceptionCheck(JNIEnv * env) {
    if (!env) {
        abort();
    }
    if (env->ExceptionCheck()) {
        const LocalRef<jthrowable> e(env->ExceptionOccurred());
        env->ExceptionClear();
        jniThrowCppFromJavaException(env, e.get());
    }
}

DJINNI_WEAK_DEFINITION
DJINNI_NORETURN_DEFINITION
void jniThrowCppFromJavaException(JNIEnv * env, jthrowable java_exception) {
    throw jni_exception { env, java_exception };
}

namespace { // anonymous namespace to guard the struct below
struct SystemClassInfo {
    // This is a singleton class - an instance will be constructed by
    // JniClassInitializer::init_all() at library init time.
    const GlobalRef<jclass> clazz { jniFindClass("java/lang/System") };
    const jmethodID staticmethIdentityHashCode { jniGetStaticMethodID(clazz.get(),
            "identityHashCode", "(Ljava/lang/Object;)I") };
};
} // namespace

/*
 * Hasher and comparator based on Java object identity.
 */
struct JavaIdentityHash { size_t operator() (jobject obj) const; };
struct JavaIdentityEquals { bool operator() (jobject obj1, jobject obj2) const; };

size_t JavaIdentityHash::operator() (jobject obj) const {
    JNIEnv * const env = jniGetThreadEnv();
    const SystemClassInfo & sys = JniClass<SystemClassInfo>::get();
    jint res = env->CallStaticIntMethod(sys.clazz.get(), sys.staticmethIdentityHashCode, obj);
    jniExceptionCheck(env);
    return res;
}
bool JavaIdentityEquals::operator() (jobject obj1, jobject obj2) const {
    JNIEnv * const env = jniGetThreadEnv();
    const bool res = env->IsSameObject(obj1, obj2);
    jniExceptionCheck(env);
    return res;
}

void jniThrowAssertionError(JNIEnv * env, const char * file, int line, const char * check) {
    // basename() exists, but is bad (it's allowed to modify its input).
    const char * slash = strrchr(file, '/');
    const char * file_basename = slash ? slash + 1 : file;

    char buf[256];
    DJINNI_SNPRINTF(buf, sizeof buf, "djinni (%s:%d): %s", file_basename, line, check);

    const jclass cassert = env->FindClass("java/lang/Error");
    assert(cassert);
    env->ThrowNew(cassert, buf);
    assert(env->ExceptionCheck());
    const jthrowable e = env->ExceptionOccurred();
    assert(e);
    env->ExceptionClear();

    env->DeleteLocalRef(cassert);

    jniThrowCppFromJavaException(env, e);
}

GlobalRef<jclass> jniFindClass(const char * name) {
    JNIEnv * env = jniGetThreadEnv();
    DJINNI_ASSERT_NO_EXC_CHECK(name);
    jclass clazz = env->FindClass(name);
    if (!clazz) {
        env->ExceptionClear();
        // Use cached class loader, needed for our classes on non-Java thread
        LocalRef<jstring> jname(env->NewStringUTF(name));
        clazz = static_cast<jclass>(env->CallObjectMethod(g_ourClassLoader, g_loadClassMethodID, jname.get()));
        jniExceptionCheck(env);
    }
    GlobalRef<jclass> guard(env, LocalRef<jclass>(env, clazz).get());
    if (!guard) {
        jniThrowAssertionError(env, __FILE__, __LINE__, "FindClass returned null");
    }
    return guard;
}

jmethodID jniGetStaticMethodID(jclass clazz, const char * name, const char * sig) {
    JNIEnv * env = jniGetThreadEnv();
    DJINNI_ASSERT_NO_EXC_CHECK(clazz);
    DJINNI_ASSERT_NO_EXC_CHECK(name);
    DJINNI_ASSERT_NO_EXC_CHECK(sig);
    jmethodID id = env->GetStaticMethodID(clazz, name, sig);
    jniExceptionCheck(env);
    if (!id) {
        jniThrowAssertionError(env, __FILE__, __LINE__, "GetStaticMethodID returned null");
    }
    return id;
}

jmethodID jniGetMethodID(jclass clazz, const char * name, const char * sig) {
    JNIEnv * env = jniGetThreadEnv();
    DJINNI_ASSERT_NO_EXC_CHECK(clazz);
    DJINNI_ASSERT_NO_EXC_CHECK(name);
    DJINNI_ASSERT_NO_EXC_CHECK(sig);
    jmethodID id = env->GetMethodID(clazz, name, sig);
    jniExceptionCheck(env);
    if (!id) {
        jniThrowAssertionError(env, __FILE__, __LINE__, "GetMethodID returned null");
    }
    return id;
}

jfieldID jniGetFieldID(jclass clazz, const char * name, const char * sig) {
    JNIEnv * env = jniGetThreadEnv();
    DJINNI_ASSERT_NO_EXC_CHECK(clazz);
    DJINNI_ASSERT_NO_EXC_CHECK(name);
    DJINNI_ASSERT_NO_EXC_CHECK(sig);
    jfieldID id = env->GetFieldID(clazz, name, sig);
    jniExceptionCheck(env);
    if (!id) {
        jniThrowAssertionError(env, __FILE__, __LINE__, "GetFieldID returned null");
    }
    return id;
}

JniEnum::JniEnum(const std::string & name)
    : m_clazz { jniFindClass(name.c_str()) },
      m_staticmethValues { jniGetStaticMethodID(m_clazz.get(), "values", ("()[L" + name + ";").c_str()) },
      m_methOrdinal { jniGetMethodID(m_clazz.get(), "ordinal", "()I") }
    {}

jint JniEnum::ordinal(JNIEnv * env, jobject obj) const {
    DJINNI_ASSERT_NO_EXC_CHECK(obj);
    const jint res = env->CallIntMethod(obj, m_methOrdinal);
    jniExceptionCheck(env);
    return res;
}

LocalRef<jobject> JniEnum::create(JNIEnv * env, jint value) const {
    LocalRef<jobject> values(env, env->CallStaticObjectMethod(m_clazz.get(), m_staticmethValues));
    jniExceptionCheck(env);
    DJINNI_ASSERT_NO_EXC_CHECK(values);
    LocalRef<jobject> result(env,
                             env->GetObjectArrayElement(static_cast<jobjectArray>(values.get()),
                                                        value));
    jniExceptionCheck(env);
    return result;
}

JniFlags::JniFlags(const std::string & name)
    : JniEnum { name }
    {}

unsigned JniFlags::flags(JNIEnv * env, jobject obj) const {
    DJINNI_ASSERT(obj && env->IsInstanceOf(obj, m_clazz.get()), env);
    const auto size = env->CallIntMethod(obj, m_methSize);
    jniExceptionCheck(env);
    unsigned flags = 0;
    auto it = LocalRef<jobject>(env, env->CallObjectMethod(obj, m_methIterator));
    jniExceptionCheck(env);
    for(jint i = 0; i < size; ++i) {
        auto jf = LocalRef<jobject>(env, env->CallObjectMethod(it, m_iterator.methNext));
        jniExceptionCheck(env);
        flags |= (1u << static_cast<unsigned>(ordinal(env, jf)));
    }
    return flags;
}

LocalRef<jobject> JniFlags::create(JNIEnv * env, unsigned flags, int bits) const {
    auto j = LocalRef<jobject>(env, env->CallStaticObjectMethod(m_clazz.get(), m_methNoneOf, enumClass()));
    jniExceptionCheck(env);
    unsigned mask = 1;
    for(int i = 0; i < bits; ++i, mask <<= 1) {
        if((flags & mask) != 0) {
            auto jf = create(env, static_cast<jint>(i));
            jniExceptionCheck(env);
            env->CallBooleanMethod(j, m_methAdd, jf.get());
            jniExceptionCheck(env);
        }
    }
    return j;
}

JniLocalScope::JniLocalScope(JNIEnv* p_env, jint capacity, bool throwOnError)
    : m_env(p_env)
    , m_success(_pushLocalFrame(m_env, capacity)) {
    if (throwOnError) {
        DJINNI_ASSERT(m_success, m_env);
    }
}
JniLocalScope::~JniLocalScope() {
    if (m_success) {
        _popLocalFrame(m_env, nullptr);
    }
}

bool JniLocalScope::_pushLocalFrame(JNIEnv* const env, jint capacity) {
    DJINNI_ASSERT_NO_EXC_CHECK(capacity >= 0);
    const jint push_res = env->PushLocalFrame(capacity);
    return 0 == push_res;
}

void JniLocalScope::_popLocalFrame(JNIEnv* const env, jobject returnRef) {
    env->PopLocalFrame(returnRef);
}

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
// std::codecvt / wstring_convert are deprecated in C++17 with no standard replacement
using WcharConverter = std::wstring_convert<std::codecvt_utf16<wchar_t, 0x10ffff, std::codecvt_mode::little_endian>>;
using Utf8Converter = std::wstring_convert<std::codecvt_utf8_utf16<char16_t, 0x10ffff, std::codecvt_mode::little_endian>, char16_t>;

jstring jniStringFromWString(JNIEnv * env, const std::wstring & str) {
    std::string u16 = WcharConverter{}.to_bytes(str);
    jstring res = env->NewString(reinterpret_cast<const jchar*>(u16.data()), u16.size()/sizeof(jchar));
    DJINNI_ASSERT(res, env);
    return res;
}

std::wstring jniWStringFromString(JNIEnv * env, const jstring jstr) {
    DJINNI_ASSERT(jstr, env);
    auto length = env->GetStringLength(jstr);
    const jchar* u16 = env->GetStringChars(jstr, nullptr);
    const char* bytes = reinterpret_cast<const char*>(u16);
    auto byteLength = length * sizeof(jchar);
    std::wstring out = WcharConverter{}.from_bytes(bytes, bytes + byteLength);
    env->ReleaseStringChars(jstr, u16);
    return out;
}

jstring jniStringFromUTF8(JNIEnv * env, const std::string & str) {
    std::u16string u16 = Utf8Converter{}.from_bytes(str);
    jstring res = env->NewString(reinterpret_cast<const jchar*>(u16.data()), u16.size());
    DJINNI_ASSERT(res, env);
    return res;
}

std::string jniUTF8FromString(JNIEnv* env, const jstring jstr) {
    DJINNI_ASSERT(jstr, env);
    auto length = env->GetStringLength(jstr);
    const jchar* u16 = env->GetStringChars(jstr, nullptr);
    const char16_t* p = reinterpret_cast<const char16_t*>(u16);
    std::string out = Utf8Converter{}.to_bytes(p, p + length);
    env->ReleaseStringChars(jstr, u16);
    return out;
}
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

DJINNI_WEAK_DEFINITION
void jniSetPendingFromCurrent(JNIEnv * env, const char * ctx) noexcept {
    jniDefaultSetPendingFromCurrent(env, ctx);
}

void jniDefaultSetPendingFromCurrent(JNIEnv * env, const char * /*ctx*/) noexcept {
    assert(env);
    try {
        throw;
    } catch (const jni_exception & e) {
        e.set_as_pending(env);
        return;
    } catch (const std::exception & e) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    }

    // noexcept will call terminate() for anything not caught above (i.e.
    // exceptions which aren't std::exception subclasses).
}

template class ProxyCache<JavaProxyCacheTraits>;

CppProxyClassInfo::CppProxyClassInfo(const char * className)
    : clazz(jniFindClass(className)),
      constructor(jniGetMethodID(clazz.get(), "<init>", "(J)V")),
      idField(jniGetFieldID(clazz.get(), "nativeRef", "J")) {
}

CppProxyClassInfo::CppProxyClassInfo() : constructor{}, idField{} {
}

CppProxyClassInfo::~CppProxyClassInfo() {
}

/*
 * Wrapper around Java WeakReference objects. (We can't use JNI NewWeakGlobalRef() because
 * it doesn't have the right semantics - see comment in djinni_support.hpp.)
 */
class JavaWeakRef {
private:
    struct JniInfo {
    public:
        const GlobalRef<jclass> clazz { jniFindClass("java/lang/ref/WeakReference") };
        const jmethodID constructor { jniGetMethodID(clazz.get(), "<init>", "(Ljava/lang/Object;)V") };
        const jmethodID method_get { jniGetMethodID(clazz.get(), "get", "()Ljava/lang/Object;") };
    };

    // Helper used by constructor
    static GlobalRef<jobject> create(JNIEnv * jniEnv, jobject obj) {
        const JniInfo & weakRefClass = JniClass<JniInfo>::get();
        LocalRef<jobject> weakRef(jniEnv, jniEnv->NewObject(weakRefClass.clazz.get(), weakRefClass.constructor, obj));
        // DJINNI_ASSERT performs an exception check before anything else, so we don't need
        // a separate jniExceptionCheck call.
        DJINNI_ASSERT(weakRef, jniEnv);
        return GlobalRef<jobject>(jniEnv, weakRef);
    }

public:
    // Constructor
    JavaWeakRef(jobject obj) : JavaWeakRef(jniGetThreadEnv(), obj) {}
    JavaWeakRef(JNIEnv * jniEnv, jobject obj) : m_weakRef(create(jniEnv, obj)) {}

    // Get the object pointed to if it's still strongly reachable or, return null if not.
    // (Analogous to weak_ptr::lock.) Returns a local reference.
    jobject lock() const {
        const auto & jniEnv = jniGetThreadEnv();
        const JniInfo & weakRefClass = JniClass<JniInfo>::get();
        LocalRef<jobject> javaObj(jniEnv->CallObjectMethod(m_weakRef.get(), weakRefClass.method_get));
        jniExceptionCheck(jniEnv);
        return javaObj.release();
    }

    // Java WeakReference objects don't have a way to check whether they're expired except
    // by upgrading them to a strong ref.
    bool expired() const {
        LocalRef<jobject> javaObj { lock() };
        return !javaObj;
    }

private:
    GlobalRef<jobject> m_weakRef;
};

template class ProxyCache<JniCppProxyCacheTraits>;

} // namespace djinni
