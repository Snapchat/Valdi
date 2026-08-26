#include "ClientSQLNativeModuleFactory.hpp"
#include "valdi/runtime/Resources/DiskCacheImpl.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"

#include <gtest/gtest.h>

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdlib.h>
#include <string>
#include <thread>
#include <vector>

using namespace Valdi;

namespace ValdiTest {

class ClientSQLTemporaryDirectory {
public:
    ClientSQLTemporaryDirectory() {
        char directoryLocation[] = "/tmp/.valdi_clientsql_test.XXXXXX";
        if (mkdtemp(directoryLocation) == nullptr) {
            throw Exception(STRING_FORMAT("Failed to create temporary directory: {}", strerror(errno)));
        }
        _rootDirectory = STRING_LITERAL(directoryLocation);
    }

    ~ClientSQLTemporaryDirectory() {
        if (!DiskUtils::remove(Path(_rootDirectory.toStringView()))) {
            std::cout << "Failed to delete temporary directory: " << strerror(errno) << std::endl;
        }
    }

    const StringBox& get() const {
        return _rootDirectory;
    }

private:
    StringBox _rootDirectory;
};

class ClientSQLAsyncResult {
public:
    Ref<ValueFunction> makeCallback() {
        return makeShared<ValueFunctionWithCallable>([this](const ValueFunctionCallContext& callContext) -> Value {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _value = callContext.getParameter(0);
                _error = callContext.getParameter(1);
                _callbackThreadId = std::this_thread::get_id();
                _called = true;
            }
            _condition.notify_one();
            return Value::undefined();
        });
    }

    Value waitForSuccess() {
        return waitForSuccessWithin(std::chrono::seconds(5));
    }

    Value waitForSuccessWithin(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(_mutex);
        if (!_condition.wait_for(lock, timeout, [this]() { return _called; })) {
            ADD_FAILURE() << "Timed out waiting for ClientSQL callback";
            return Value::undefined();
        }
        EXPECT_TRUE(_error.isNullOrUndefined()) << _error.toString();
        return _value;
    }

    Value waitForError() {
        std::unique_lock<std::mutex> lock(_mutex);
        if (!_condition.wait_for(lock, std::chrono::seconds(5), [this]() { return _called; })) {
            ADD_FAILURE() << "Timed out waiting for ClientSQL callback";
            return Value::undefined();
        }
        EXPECT_TRUE(_value.isNullOrUndefined()) << _value.toString();
        EXPECT_FALSE(_error.isNullOrUndefined());
        return _error;
    }

    bool wasCalled() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _called;
    }

    std::thread::id getCallbackThreadId() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _callbackThreadId;
    }

private:
    mutable std::mutex _mutex;
    std::condition_variable _condition;
    bool _called = false;
    Value _value = Value::undefined();
    Value _error = Value::undefined();
    std::thread::id _callbackThreadId;
};

Value callNativeFunction(const Value& object, std::string_view method, const std::vector<Value>& parameters) {
    auto callableObject = object;
    if (object.isTypedObject()) {
        callableObject = Value(object.getTypedObjectRef()->toValueMap(true));
    } else if (object.isProxyObject()) {
        callableObject = Value(object.getTypedProxyObjectRef()->getTypedObject()->toValueMap(true));
    }

    auto function = callableObject.getMapValue(method).getFunctionRef();
    EXPECT_NE(function, nullptr) << method << ": " << callableObject.toString();
    if (function == nullptr) {
        return Value::undefined();
    }

    auto result = (*function)(parameters.data(), parameters.size());
    EXPECT_TRUE(result.success()) << result.description();
    if (!result) {
        return Value::undefined();
    }
    return result.moveValue();
}

std::string callNativeFunctionExpectingError(const Value& object,
                                             std::string_view method,
                                             const std::vector<Value>& parameters) {
    auto callableObject = object;
    if (object.isTypedObject()) {
        callableObject = Value(object.getTypedObjectRef()->toValueMap(true));
    } else if (object.isProxyObject()) {
        callableObject = Value(object.getTypedProxyObjectRef()->getTypedObject()->toValueMap(true));
    }

    auto function = callableObject.getMapValue(method).getFunctionRef();
    EXPECT_NE(function, nullptr) << method << ": " << callableObject.toString();
    if (function == nullptr) {
        return "Native function is unavailable";
    }

    auto result = (*function)(parameters.data(), parameters.size());
    EXPECT_TRUE(result.failure()) << result.description();
    return result.description();
}

std::vector<Value> makeClientSQLOpenParameters(StringBox name) {
    return {
        Value(std::move(name)),
        Value(1),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY)")})),
        Value(ValueArray::make(0)),
    };
}

Value callClientSQLAsync(const Value& connection, std::string_view method, StringBox sql, Value parameters) {
    ClientSQLAsyncResult asyncResult;
    std::vector<Value> callParameters{
        Value(sql),
        std::move(parameters),
        Value(asyncResult.makeCallback()),
    };

    callNativeFunction(connection, method, callParameters);
    return asyncResult.waitForSuccess();
}

Value queryClientSQL(const Value& connection,
                     std::string_view method,
                     StringBox sql,
                     Value parameters) {
    return callClientSQLAsync(connection, method, sql, std::move(parameters));
}

void executeClientSQL(const Value& connection, StringBox sql, Value parameters) {
    callClientSQLAsync(connection, "execute", sql, std::move(parameters));
}

TEST(ClientSQLNativeModuleFactory, rejectsTraversalDatabaseName) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    const auto error = callNativeFunctionExpectingError(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(STRING_LITERAL("../escape.sqlite")));
    EXPECT_NE(std::string::npos, error.find("Invalid ClientSQLNative database name")) << error;
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, rejectsAbsoluteDatabaseName) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    const auto error = callNativeFunctionExpectingError(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(STRING_LITERAL("/tmp/escape.sqlite")));
    EXPECT_NE(std::string::npos, error.find("Invalid ClientSQLNative database name")) << error;
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, rejectsMissingDatabaseStorageRoot) {
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(nullptr, workerQueue);
    auto module = factory->loadModule();

    const auto error = callNativeFunctionExpectingError(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(STRING_LITERAL("missing-root.sqlite")));
    EXPECT_NE(std::string::npos, error.find("requires an owned database storage root")) << error;
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, rejectsControlCharactersInDatabaseName) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    const auto embeddedNullName = StringBox::fromString(
        std::string_view("nul\0name.sqlite", sizeof("nul\0name.sqlite") - 1));
    const auto nullError = callNativeFunctionExpectingError(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(embeddedNullName));
    EXPECT_NE(std::string::npos, nullError.find("Invalid ClientSQLNative database name")) << nullError;

    const auto controlError = callNativeFunctionExpectingError(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(StringBox::fromString(std::string_view("line\nfeed.sqlite"))));
    EXPECT_NE(std::string::npos, controlError.find("Invalid ClientSQLNative database name")) << controlError;

    const auto c1ControlError = callNativeFunctionExpectingError(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(StringBox::fromString(std::string_view("next\xc2\x85line.sqlite"))));
    EXPECT_NE(std::string::npos, c1ControlError.find("Invalid ClientSQLNative database name")) << c1ControlError;
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, rejectsFilesystemAliasDatabaseNames) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    for (const auto& name : {"Uppercase.sqlite", "trailing-dot.", "double..dot.sqlite", "trailing-space.sqlite "}) {
        const auto error = callNativeFunctionExpectingError(
            module,
            "openDatabase",
            makeClientSQLOpenParameters(StringBox::fromString(std::string_view(name))));
        EXPECT_NE(std::string::npos, error.find("Invalid ClientSQLNative database name")) << name << ": " << error;
    }
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, rejectsEmptyDatabaseStorageRoot) {
    auto diskCache = makeShared<DiskCacheImpl>(StringBox::emptyString());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    const auto error = callNativeFunctionExpectingError(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(STRING_LITERAL("empty-root.sqlite")));
    EXPECT_NE(std::string::npos, error.find("database storage root must be nonempty and absolute")) << error;
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, rejectsRelativeDatabaseStorageRoot) {
    auto diskCache = makeShared<DiskCacheImpl>(STRING_LITERAL("relative-root"));
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    const auto error = callNativeFunctionExpectingError(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(STRING_LITERAL("relative-root.sqlite")));
    EXPECT_NE(std::string::npos, error.find("database storage root must be nonempty and absolute")) << error;
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, opensWALDatabaseAndUsesReadonlyReaders) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    std::vector<Value> openParameters{
        Value("runtime-test.sqlite"),
        Value(1),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, name TEXT NOT NULL)")})),
        Value(ValueArray::make(0)),
    };
    auto connection = callNativeFunction(module, "openDatabase", openParameters);

    executeClientSQL(connection,
                     STRING_LITERAL("INSERT INTO item(id, name) VALUES (?, ?)"),
                     Value(ValueArray::make({Value(1), Value("Ada")})));

    auto rows = queryClientSQL(
                    connection, "query", STRING_LITERAL("SELECT name FROM item ORDER BY id"), Value::undefined())
                    .getArrayRef();
    ASSERT_NE(rows, nullptr);
    ASSERT_EQ(1ul, rows->size());
    EXPECT_EQ(STRING_LITERAL("Ada"), (*rows)[0].getMapValue("name").toStringBox());

    auto journalRows = queryClientSQL(
                           connection, "queryOnWriter", STRING_LITERAL("PRAGMA journal_mode"), Value::undefined())
                           .getArrayRef();
    ASSERT_NE(journalRows, nullptr);
    ASSERT_EQ(1ul, journalRows->size());
    EXPECT_EQ(STRING_LITERAL("wal"), (*journalRows)[0].getMapValue("journal_mode").toStringBox());

    auto readerRows =
        queryClientSQL(connection, "query", STRING_LITERAL("PRAGMA query_only"), Value::undefined()).getArrayRef();
    ASSERT_NE(readerRows, nullptr);
    ASSERT_EQ(1ul, readerRows->size());
    EXPECT_EQ(1, (*readerRows)[0].getMapValue("query_only").toInt());

    ClientSQLAsyncResult debugInfoResult;
    callNativeFunction(connection, "debugInfo", {Value(debugInfoResult.makeCallback())});
    const auto debugInfo = debugInfoResult.waitForSuccess();
    EXPECT_GE(debugInfo.getMapValue("sqliteVersionNumber").toInt(), 3016000);

    ClientSQLAsyncResult closeResult;
    callNativeFunction(connection, "close", {Value(closeResult.makeCallback())});
    closeResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, dispatchesWriterWorkOffCallerThread) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    std::vector<Value> openParameters{
        Value("runtime-thread-test.sqlite"),
        Value(1),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, name TEXT NOT NULL)")})),
        Value(ValueArray::make(0)),
    };
    auto connection = callNativeFunction(module, "openDatabase", openParameters);

    ClientSQLAsyncResult insertResult;
    callNativeFunction(connection,
                       "execute",
                       {Value("INSERT INTO item(id, name) VALUES (?, ?)"),
                        Value(ValueArray::make({Value(1), Value("Ada")})),
                        Value(insertResult.makeCallback())});

    insertResult.waitForSuccess();
    EXPECT_NE(std::this_thread::get_id(), insertResult.getCallbackThreadId());

    ClientSQLAsyncResult closeResult;
    callNativeFunction(connection, "close", {Value(closeResult.makeCallback())});
    closeResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, rejectsIntegersOutsideJavaScriptSafeRange) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    std::vector<Value> openParameters{
        Value("runtime-integer-range-test.sqlite"),
        Value(1),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY)")})),
        Value(ValueArray::make(0)),
    };
    auto connection = callNativeFunction(module, "openDatabase", openParameters);

    ClientSQLAsyncResult queryResult;
    callNativeFunction(connection,
                       "query",
                       {Value("SELECT 9007199254740992 AS value"),
                        Value(ValueArray::make(0)),
                        Value(queryResult.makeCallback())});
    const auto error = queryResult.waitForError().toString();
    EXPECT_NE(std::string::npos, error.find("exceeds JavaScript's exact integer range")) << error;

    ClientSQLAsyncResult closeResult;
    callNativeFunction(connection, "close", {Value(closeResult.makeCallback())});
    closeResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, rejectsNonFiniteDoubleParameters) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();
    auto connection = callNativeFunction(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(STRING_LITERAL("runtime-nonfinite-test.sqlite")));

    ClientSQLAsyncResult infinityResult;
    callNativeFunction(connection,
                       "query",
                       {Value("SELECT ? AS value"),
                        Value(ValueArray::make({Value(std::numeric_limits<double>::infinity())})),
                        Value(infinityResult.makeCallback())});
    const auto infinityError = infinityResult.waitForError().toString();
    EXPECT_NE(std::string::npos, infinityError.find("double parameters must be finite")) << infinityError;

    ClientSQLAsyncResult nanResult;
    callNativeFunction(connection,
                       "query",
                       {Value("SELECT ? AS value"),
                        Value(ValueArray::make({Value(std::numeric_limits<double>::quiet_NaN())})),
                        Value(nanResult.makeCallback())});
    const auto nanError = nanResult.waitForError().toString();
    EXPECT_NE(std::string::npos, nanError.find("double parameters must be finite")) << nanError;

    ClientSQLAsyncResult closeResult;
    callNativeFunction(connection, "close", {Value(closeResult.makeCallback())});
    closeResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, readsEmptyTextAndBlobValuesSafely) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();
    auto connection = callNativeFunction(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(STRING_LITERAL("runtime-empty-values-test.sqlite")));

    auto rows = queryClientSQL(
                    connection,
                    "query",
                    STRING_LITERAL("SELECT '' AS empty_text, CAST(X'' AS BLOB) AS empty_blob"),
                    Value::undefined())
                    .getArrayRef();
    ASSERT_NE(rows, nullptr);
    ASSERT_EQ(1ul, rows->size());
    EXPECT_TRUE((*rows)[0].getMapValue("empty_text").toStringBox().isEmpty());
    const auto& emptyBlob = (*rows)[0].getMapValue("empty_blob");
    ASSERT_EQ(ValueType::TypedArray, emptyBlob.getType());
    ASSERT_NE(nullptr, emptyBlob.getTypedArray());
    EXPECT_EQ(0ul, emptyBlob.getTypedArray()->getBuffer().size());

    ClientSQLAsyncResult closeResult;
    callNativeFunction(connection, "close", {Value(closeResult.makeCallback())});
    closeResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, bindsEmptyArrayBufferAsZeroLengthBlob) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();
    std::vector<Value> openParameters{
        Value("runtime-empty-blob-bind-test.sqlite"),
        Value(1),
        Value(ValueArray::make({Value(
            "CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, payload BLOB NOT NULL)")})),
        Value(ValueArray::make(0)),
    };
    auto connection = callNativeFunction(module, "openDatabase", openParameters);

    auto bytes = makeShared<Bytes>();
    auto emptyBlob = Value(makeShared<ValueTypedArray>(TypedArrayType::ArrayBuffer, bytes));
    executeClientSQL(
        connection,
        STRING_LITERAL("INSERT INTO item(id, payload) VALUES (?, ?)"),
        Value(ValueArray::make({Value(1), std::move(emptyBlob)})));

    auto rows = queryClientSQL(
                    connection,
                    "query",
                    STRING_LITERAL(
                        "SELECT typeof(payload) AS storage_type, length(payload) AS payload_length, payload FROM item"),
                    Value(ValueArray::make(0)))
                    .getArrayRef();
    ASSERT_NE(rows, nullptr);
    ASSERT_EQ(1ul, rows->size());
    EXPECT_EQ(STRING_LITERAL("blob"), (*rows)[0].getMapValue("storage_type").toStringBox());
    EXPECT_EQ(0, (*rows)[0].getMapValue("payload_length").toInt());
    const auto& roundTrippedBlob = (*rows)[0].getMapValue("payload");
    ASSERT_EQ(ValueType::TypedArray, roundTrippedBlob.getType());
    EXPECT_EQ(0ul, roundTrippedBlob.getTypedArray()->getBuffer().size());

    ClientSQLAsyncResult closeResult;
    callNativeFunction(connection, "close", {Value(closeResult.makeCallback())});
    closeResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, rejectsWritesThroughReadonlyReaders) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    std::vector<Value> openParameters{
        Value("runtime-reader-test.sqlite"),
        Value(1),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, name TEXT NOT NULL)")})),
        Value(ValueArray::make(0)),
    };
    auto connection = callNativeFunction(module, "openDatabase", openParameters);

    executeClientSQL(connection,
                     STRING_LITERAL("INSERT INTO item(id, name) VALUES (?, ?)"),
                     Value(ValueArray::make({Value(1), Value("Ada")})));

    auto readerRows =
        queryClientSQL(connection, "query", STRING_LITERAL("PRAGMA query_only"), Value::undefined()).getArrayRef();
    ASSERT_NE(readerRows, nullptr);
    ASSERT_EQ(1ul, readerRows->size());
    EXPECT_EQ(1, (*readerRows)[0].getMapValue("query_only").toInt());

    ClientSQLAsyncResult readerWriteResult;
    callNativeFunction(connection,
                       "query",
                       {Value("INSERT INTO item(id, name) VALUES (?, ?)"),
                        Value(ValueArray::make({Value(2), Value("Grace")})),
                        Value(readerWriteResult.makeCallback())});

    const auto error = readerWriteResult.waitForError().toString();
    EXPECT_NE(std::string::npos, error.find("attempt to write a readonly database")) << error;

    ClientSQLAsyncResult closeResult;
    callNativeFunction(connection, "close", {Value(closeResult.makeCallback())});
    closeResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, keepsSharedDatabaseOpenUntilLastHandleCloses) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    std::vector<Value> openParameters{
        Value("runtime-shared-test.sqlite"),
        Value(1),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, name TEXT NOT NULL)")})),
        Value(ValueArray::make(0)),
    };
    auto firstConnection = callNativeFunction(module, "openDatabase", openParameters);
    auto secondConnection = callNativeFunction(module, "openDatabase", openParameters);

    executeClientSQL(firstConnection,
                     STRING_LITERAL("INSERT INTO item(id, name) VALUES (?, ?)"),
                     Value(ValueArray::make({Value(1), Value("Ada")})));

    ClientSQLAsyncResult firstCloseResult;
    callNativeFunction(firstConnection, "close", {Value(firstCloseResult.makeCallback())});
    firstCloseResult.waitForSuccess();

    executeClientSQL(secondConnection,
                     STRING_LITERAL("INSERT INTO item(id, name) VALUES (?, ?)"),
                     Value(ValueArray::make({Value(2), Value("Grace")})));

    auto rows = queryClientSQL(
                    secondConnection, "query", STRING_LITERAL("SELECT name FROM item ORDER BY id"), Value::undefined())
                    .getArrayRef();
    ASSERT_NE(rows, nullptr);
    ASSERT_EQ(2ul, rows->size());
    EXPECT_EQ(STRING_LITERAL("Ada"), (*rows)[0].getMapValue("name").toStringBox());
    EXPECT_EQ(STRING_LITERAL("Grace"), (*rows)[1].getMapValue("name").toStringBox());

    ClientSQLAsyncResult secondCloseResult;
    callNativeFunction(secondConnection, "close", {Value(secondCloseResult.makeCallback())});
    secondCloseResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, serializesWritesAcrossSharedHandles) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    std::vector<Value> openParameters{
        Value("runtime-shared-writer-test.sqlite"),
        Value(1),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, name TEXT NOT NULL)")})),
        Value(ValueArray::make(0)),
    };
    auto firstConnection = callNativeFunction(module, "openDatabase", openParameters);
    auto secondConnection = callNativeFunction(module, "openDatabase", openParameters);

    constexpr int writeCount = 20;
    std::vector<std::unique_ptr<ClientSQLAsyncResult>> writeResults;
    writeResults.reserve(writeCount);
    for (int index = 0; index < writeCount; ++index) {
        auto result = std::make_unique<ClientSQLAsyncResult>();
        const auto& connection = index % 2 == 0 ? firstConnection : secondConnection;
        callNativeFunction(connection,
                           "execute",
                           {Value("INSERT INTO item(id, name) VALUES (?, ?)"),
                            Value(ValueArray::make({Value(index + 1), Value(STRING_FORMAT("Item {}", index + 1))})),
                            Value(result->makeCallback())});
        writeResults.emplace_back(std::move(result));
    }

    for (const auto& result : writeResults) {
        result->waitForSuccess();
    }

    auto rows = queryClientSQL(
                    firstConnection, "query", STRING_LITERAL("SELECT COUNT(*) AS count FROM item"), Value::undefined())
                    .getArrayRef();
    ASSERT_NE(rows, nullptr);
    ASSERT_EQ(1ul, rows->size());
    EXPECT_EQ(writeCount, (*rows)[0].getMapValue("count").toInt());

    ClientSQLAsyncResult firstCloseResult;
    callNativeFunction(firstConnection, "close", {Value(firstCloseResult.makeCallback())});
    firstCloseResult.waitForSuccess();

    ClientSQLAsyncResult secondCloseResult;
    callNativeFunction(secondConnection, "close", {Value(secondCloseResult.makeCallback())});
    secondCloseResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, commitsNativeTransactionAfterDoneCallback) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    std::vector<Value> openParameters{
        Value("runtime-transaction-commit-test.sqlite"),
        Value(1),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, name TEXT NOT NULL)")})),
        Value(ValueArray::make(0)),
    };
    auto connection = callNativeFunction(module, "openDatabase", openParameters);

    auto body = makeShared<ValueFunctionWithCallable>([](const ValueFunctionCallContext& callContext) -> Value {
        auto transaction = callContext.getParameter(0);
        auto doneCallback = callContext.getParameterAsFunction(1);
        if (!callContext.getExceptionTracker()) {
            return Value::undefined();
        }

        auto insertCallback = makeShared<ValueFunctionWithCallable>(
            [transaction, doneCallback](const ValueFunctionCallContext& insertContext) -> Value {
                const auto& insertError = insertContext.getParameter(1);
                if (!insertError.isNullOrUndefined()) {
                    (*doneCallback)({Value::undefined(), insertError});
                    return Value::undefined();
                }

                auto queryCallback = makeShared<ValueFunctionWithCallable>(
                    [doneCallback](const ValueFunctionCallContext& queryContext) -> Value {
                        const auto& queryError = queryContext.getParameter(1);
                        if (!queryError.isNullOrUndefined()) {
                            (*doneCallback)({Value::undefined(), queryError});
                            return Value::undefined();
                        }

                        auto rows = queryContext.getParameter(0).getArrayRef();
                        if (rows == nullptr || rows->size() != 1 ||
                            (*rows)[0].getMapValue("count").toInt() != 1) {
                            (*doneCallback)({Value::undefined(), Value("transaction query did not observe write")});
                            return Value::undefined();
                        }

                        (*doneCallback)({Value("committed"), Value::undefined()});
                        return Value::undefined();
                    });
                callNativeFunction(
                    transaction,
                    "query",
                    {Value("SELECT COUNT(*) AS count FROM item"), Value(ValueArray::make(0)), Value(queryCallback)});
                return Value::undefined();
            });
        callNativeFunction(
            transaction,
            "execute",
            {Value("INSERT INTO item(id, name) VALUES (?, ?)"),
             Value(ValueArray::make({Value(1), Value("Ada")})),
             Value(insertCallback)});
        return Value::undefined();
    });

    ClientSQLAsyncResult transactionResult;
    callNativeFunction(connection, "transaction", {Value(body), Value(transactionResult.makeCallback())});
    EXPECT_EQ(STRING_LITERAL("committed"), transactionResult.waitForSuccess().toStringBox());

    auto rows = queryClientSQL(
                    connection, "query", STRING_LITERAL("SELECT name FROM item ORDER BY id"), Value::undefined())
                    .getArrayRef();
    ASSERT_NE(rows, nullptr);
    ASSERT_EQ(1ul, rows->size());
    EXPECT_EQ(STRING_LITERAL("Ada"), (*rows)[0].getMapValue("name").toStringBox());

    ClientSQLAsyncResult closeResult;
    callNativeFunction(connection, "close", {Value(closeResult.makeCallback())});
    closeResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, rollsBackNativeTransactionWhenDoneReceivesError) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    std::vector<Value> openParameters{
        Value("runtime-transaction-rollback-test.sqlite"),
        Value(1),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, name TEXT NOT NULL)")})),
        Value(ValueArray::make(0)),
    };
    auto connection = callNativeFunction(module, "openDatabase", openParameters);

    auto body = makeShared<ValueFunctionWithCallable>([](const ValueFunctionCallContext& callContext) -> Value {
        auto transaction = callContext.getParameter(0);
        auto doneCallback = callContext.getParameterAsFunction(1);
        if (!callContext.getExceptionTracker()) {
            return Value::undefined();
        }

        auto insertCallback = makeShared<ValueFunctionWithCallable>(
            [doneCallback](const ValueFunctionCallContext& insertContext) -> Value {
                const auto& insertError = insertContext.getParameter(1);
                if (!insertError.isNullOrUndefined()) {
                    (*doneCallback)({Value::undefined(), insertError});
                    return Value::undefined();
                }

                (*doneCallback)({Value::undefined(), Value("rollback requested")});
                return Value::undefined();
            });
        callNativeFunction(
            transaction,
            "execute",
            {Value("INSERT INTO item(id, name) VALUES (?, ?)"),
             Value(ValueArray::make({Value(1), Value("Ada")})),
             Value(insertCallback)});
        return Value::undefined();
    });

    ClientSQLAsyncResult transactionResult;
    callNativeFunction(connection, "transaction", {Value(body), Value(transactionResult.makeCallback())});
    const auto error = transactionResult.waitForError().toString();
    EXPECT_NE(std::string::npos, error.find("rollback requested")) << error;

    auto rows = queryClientSQL(
                    connection, "query", STRING_LITERAL("SELECT COUNT(*) AS count FROM item"), Value::undefined())
                    .getArrayRef();
    ASSERT_NE(rows, nullptr);
    ASSERT_EQ(1ul, rows->size());
    EXPECT_EQ(0, (*rows)[0].getMapValue("count").toInt());

    ClientSQLAsyncResult closeResult;
    callNativeFunction(connection, "close", {Value(closeResult.makeCallback())});
    closeResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, defersExternalWriterWorkUntilNativeTransactionFinishes) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    std::vector<Value> openParameters{
        Value("runtime-transaction-queue-test.sqlite"),
        Value(1),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, name TEXT NOT NULL)")})),
        Value(ValueArray::make(0)),
    };
    auto firstConnection = callNativeFunction(module, "openDatabase", openParameters);
    auto secondConnection = callNativeFunction(module, "openDatabase", openParameters);

    std::mutex mutex;
    std::condition_variable condition;
    bool transactionReady = false;
    Ref<ValueFunction> savedDoneCallback;

    auto body = makeShared<ValueFunctionWithCallable>(
        [&mutex, &condition, &transactionReady, &savedDoneCallback](const ValueFunctionCallContext& callContext)
            -> Value {
            auto transaction = callContext.getParameter(0);
            auto doneCallback = callContext.getParameterAsFunction(1);
            if (!callContext.getExceptionTracker()) {
                return Value::undefined();
            }

            auto insertCallback = makeShared<ValueFunctionWithCallable>(
                [&mutex, &condition, &transactionReady, &savedDoneCallback, doneCallback](
                    const ValueFunctionCallContext& insertContext) -> Value {
                    const auto& insertError = insertContext.getParameter(1);
                    if (!insertError.isNullOrUndefined()) {
                        (*doneCallback)({Value::undefined(), insertError});
                        return Value::undefined();
                    }

                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        savedDoneCallback = doneCallback;
                        transactionReady = true;
                    }
                    condition.notify_one();
                    return Value::undefined();
                });
            callNativeFunction(
                transaction,
                "execute",
                {Value("INSERT INTO item(id, name) VALUES (?, ?)"),
                 Value(ValueArray::make({Value(1), Value("Inside")})),
                 Value(insertCallback)});
            return Value::undefined();
        });

    ClientSQLAsyncResult transactionResult;
    callNativeFunction(firstConnection, "transaction", {Value(body), Value(transactionResult.makeCallback())});

    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(condition.wait_for(lock, std::chrono::seconds(5), [&transactionReady]() {
            return transactionReady;
        }));
    }

    ClientSQLAsyncResult outsideWriteResult;
    callNativeFunction(secondConnection,
                       "execute",
                       {Value("INSERT INTO item(id, name) VALUES (?, ?)"),
                        Value(ValueArray::make({Value(2), Value("Outside")})),
                        Value(outsideWriteResult.makeCallback())});

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(outsideWriteResult.wasCalled());

    ClientSQLAsyncResult debugInfoResult;
    callNativeFunction(firstConnection, "debugInfo", {Value(debugInfoResult.makeCallback())});
    auto debugInfo = debugInfoResult.waitForSuccess();
    EXPECT_TRUE(debugInfo.getMapValue("activeTransaction").toBool());
    EXPECT_GE(debugInfo.getMapValue("deferredWriterWork").toInt(), 1);
    EXPECT_EQ(2, debugInfo.getMapValue("activeHandles").toInt());

    Ref<ValueFunction> doneCallback;
    {
        std::lock_guard<std::mutex> lock(mutex);
        doneCallback = savedDoneCallback;
    }
    ASSERT_NE(doneCallback, nullptr);
    (*doneCallback)({Value::undefined(), Value::undefined()});

    transactionResult.waitForSuccess();
    outsideWriteResult.waitForSuccess();

    auto rows = queryClientSQL(
                    firstConnection, "query", STRING_LITERAL("SELECT name FROM item ORDER BY id"), Value::undefined())
                    .getArrayRef();
    ASSERT_NE(rows, nullptr);
    ASSERT_EQ(2ul, rows->size());
    EXPECT_EQ(STRING_LITERAL("Inside"), (*rows)[0].getMapValue("name").toStringBox());
    EXPECT_EQ(STRING_LITERAL("Outside"), (*rows)[1].getMapValue("name").toStringBox());

    ClientSQLAsyncResult firstCloseResult;
    callNativeFunction(firstConnection, "close", {Value(firstCloseResult.makeCallback())});
    firstCloseResult.waitForSuccess();

    ClientSQLAsyncResult secondCloseResult;
    callNativeFunction(secondConnection, "close", {Value(secondCloseResult.makeCallback())});
    secondCloseResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, allowsSeparateHandleSnapshotReadersDuringActiveNativeTransaction) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    std::vector<Value> openParameters{
        Value("runtime-transaction-reader-test.sqlite"),
        Value(1),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, name TEXT NOT NULL)")})),
        Value(ValueArray::make(0)),
    };
    auto firstConnection = callNativeFunction(module, "openDatabase", openParameters);
    auto secondConnection = callNativeFunction(module, "openDatabase", openParameters);

    executeClientSQL(firstConnection,
                     STRING_LITERAL("INSERT INTO item(id, name) VALUES (?, ?)"),
                     Value(ValueArray::make({Value(1), Value("Before")})));

    std::mutex mutex;
    std::condition_variable condition;
    bool transactionReady = false;
    Ref<ValueFunction> savedDoneCallback;

    auto body = makeShared<ValueFunctionWithCallable>(
        [&mutex, &condition, &transactionReady, &savedDoneCallback](const ValueFunctionCallContext& callContext)
            -> Value {
            auto transaction = callContext.getParameter(0);
            auto doneCallback = callContext.getParameterAsFunction(1);
            if (!callContext.getExceptionTracker()) {
                return Value::undefined();
            }

            auto insertCallback = makeShared<ValueFunctionWithCallable>(
                [&mutex, &condition, &transactionReady, &savedDoneCallback, doneCallback](
                    const ValueFunctionCallContext& insertContext) -> Value {
                    const auto& insertError = insertContext.getParameter(1);
                    if (!insertError.isNullOrUndefined()) {
                        (*doneCallback)({Value::undefined(), insertError});
                        return Value::undefined();
                    }

                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        savedDoneCallback = doneCallback;
                        transactionReady = true;
                    }
                    condition.notify_one();
                    return Value::undefined();
                });
            callNativeFunction(
                transaction,
                "execute",
                {Value("INSERT INTO item(id, name) VALUES (?, ?)"),
                 Value(ValueArray::make({Value(2), Value("Inside")})),
                 Value(insertCallback)});
            return Value::undefined();
        });

    ClientSQLAsyncResult transactionResult;
    callNativeFunction(firstConnection, "transaction", {Value(body), Value(transactionResult.makeCallback())});

    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(condition.wait_for(lock, std::chrono::seconds(5), [&transactionReady]() {
            return transactionReady;
        }));
    }

    ClientSQLAsyncResult outsideReadResult;
    callNativeFunction(secondConnection,
                       "query",
                       {Value("SELECT name FROM item ORDER BY id"),
                        Value(ValueArray::make(0)),
                        Value(outsideReadResult.makeCallback())});

    auto rowsBeforeCommit = outsideReadResult.waitForSuccessWithin(std::chrono::seconds(1)).getArrayRef();
    ASSERT_NE(rowsBeforeCommit, nullptr);
    ASSERT_EQ(1ul, rowsBeforeCommit->size());
    EXPECT_EQ(STRING_LITERAL("Before"), (*rowsBeforeCommit)[0].getMapValue("name").toStringBox());

    Ref<ValueFunction> doneCallback;
    {
        std::lock_guard<std::mutex> lock(mutex);
        doneCallback = savedDoneCallback;
    }
    ASSERT_NE(doneCallback, nullptr);
    (*doneCallback)({Value::undefined(), Value::undefined()});

    transactionResult.waitForSuccess();

    auto rowsAfterCommit = queryClientSQL(
                               secondConnection,
                               "query",
                               STRING_LITERAL("SELECT name FROM item ORDER BY id"),
                               Value::undefined())
                               .getArrayRef();
    ASSERT_NE(rowsAfterCommit, nullptr);
    ASSERT_EQ(2ul, rowsAfterCommit->size());
    EXPECT_EQ(STRING_LITERAL("Before"), (*rowsAfterCommit)[0].getMapValue("name").toStringBox());
    EXPECT_EQ(STRING_LITERAL("Inside"), (*rowsAfterCommit)[1].getMapValue("name").toStringBox());

    ClientSQLAsyncResult firstCloseResult;
    callNativeFunction(firstConnection, "close", {Value(firstCloseResult.makeCallback())});
    firstCloseResult.waitForSuccess();

    ClientSQLAsyncResult secondCloseResult;
    callNativeFunction(secondConnection, "close", {Value(secondCloseResult.makeCallback())});
    secondCloseResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, rejectsParentConnectionReentrancyInsideTransactionBody) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();
    auto connection = callNativeFunction(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(STRING_LITERAL("runtime-transaction-reentrancy-test.sqlite")));

    std::atomic_bool executeRejected{false};
    std::atomic_bool queryRejected{false};
    std::atomic_bool closeRejected{false};
    auto body = makeShared<ValueFunctionWithCallable>(
        [connection, &executeRejected, &queryRejected, &closeRejected](
            const ValueFunctionCallContext& callContext) -> Value {
            auto doneCallback = callContext.getParameterAsFunction(1);
            if (!callContext.getExceptionTracker()) {
                return Value::undefined();
            }

            auto executeCallback = makeShared<ValueFunctionWithCallable>(
                [&executeRejected](const ValueFunctionCallContext& executeContext) -> Value {
                    executeRejected.store(
                        executeContext.getParameter(1).toString().find("parent connection execute") != std::string::npos);
                    return Value::undefined();
                });
            callNativeFunction(
                connection,
                "execute",
                {Value("INSERT INTO item(id) VALUES (1)"),
                 Value(ValueArray::make(0)),
                 Value(executeCallback)});

            auto queryCallback = makeShared<ValueFunctionWithCallable>(
                [&queryRejected](const ValueFunctionCallContext& queryContext) -> Value {
                    queryRejected.store(
                        queryContext.getParameter(1).toString().find("parent connection query") != std::string::npos);
                    return Value::undefined();
                });
            callNativeFunction(
                connection,
                "query",
                {Value("SELECT COUNT(*) AS count FROM item"),
                 Value(ValueArray::make(0)),
                 Value(queryCallback)});

            auto closeCallback = makeShared<ValueFunctionWithCallable>(
                [&closeRejected, doneCallback](const ValueFunctionCallContext& closeContext) -> Value {
                    closeRejected.store(
                        closeContext.getParameter(1).toString().find("parent connection close") != std::string::npos);
                    (*doneCallback)({Value::undefined(), Value::undefined()});
                    return Value::undefined();
                });
            callNativeFunction(connection, "close", {Value(closeCallback)});
            return Value::undefined();
        });

    ClientSQLAsyncResult transactionResult;
    callNativeFunction(connection, "transaction", {Value(body), Value(transactionResult.makeCallback())});
    transactionResult.waitForSuccess();
    EXPECT_TRUE(executeRejected.load());
    EXPECT_TRUE(queryRejected.load());
    EXPECT_TRUE(closeRejected.load());

    auto rows = queryClientSQL(
                    connection,
                    "query",
                    STRING_LITERAL("SELECT COUNT(*) AS count FROM item"),
                    Value(ValueArray::make(0)))
                    .getArrayRef();
    ASSERT_NE(rows, nullptr);
    ASSERT_EQ(1ul, rows->size());
    EXPECT_EQ(0, (*rows)[0].getMapValue("count").toInt());

    ClientSQLAsyncResult closeResult;
    callNativeFunction(connection, "close", {Value(closeResult.makeCallback())});
    closeResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, rejectsSchemaFingerprintMismatchForSharedIdentity) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    auto firstConnection = callNativeFunction(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(STRING_LITERAL("schema-fingerprint-test.sqlite")));
    queryClientSQL(
        firstConnection,
        "query",
        STRING_LITERAL("SELECT COUNT(*) AS count FROM item"),
        Value(ValueArray::make(0)));

    std::vector<Value> collidingOpenParameters{
        Value("schema-fingerprint-test.sqlite"),
        Value(1),
        Value(ValueArray::make({
            Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY)"),
            Value("SELECT 'schema-boundary-a'"),
        })),
        Value(ValueArray::make(0)),
    };
    auto collidingConnection = callNativeFunction(module, "openDatabase", collidingOpenParameters);
    ClientSQLAsyncResult queryResult;
    callNativeFunction(
        collidingConnection,
        "query",
        {Value("SELECT COUNT(*) AS count FROM item"),
         Value(ValueArray::make(0)),
         Value(queryResult.makeCallback())});
    const auto error = queryResult.waitForError().toString();
    EXPECT_NE(std::string::npos, error.find("different schema fingerprint")) << error;

    ClientSQLAsyncResult firstCloseResult;
    callNativeFunction(firstConnection, "close", {Value(firstCloseResult.makeCallback())});
    firstCloseResult.waitForSuccess();
    ClientSQLAsyncResult collisionCloseResult;
    callNativeFunction(collidingConnection, "close", {Value(collisionCloseResult.makeCallback())});
    collisionCloseResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, releasesManyClosedDatabaseCoordinators) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    for (int index = 0; index < 64; ++index) {
        auto connection = callNativeFunction(
            module,
            "openDatabase",
            makeClientSQLOpenParameters(STRING_FORMAT("lifetime-{}.sqlite", index)));
        queryClientSQL(
            connection,
            "query",
            STRING_LITERAL("SELECT COUNT(*) AS count FROM item"),
            Value(ValueArray::make(0)));
        ClientSQLAsyncResult closeResult;
        callNativeFunction(connection, "close", {Value(closeResult.makeCallback())});
        closeResult.waitForSuccess();
    }

    auto finalConnection = callNativeFunction(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(STRING_LITERAL("lifetime-final.sqlite")));
    queryClientSQL(
        finalConnection,
        "query",
        STRING_LITERAL("SELECT COUNT(*) AS count FROM item"),
        Value(ValueArray::make(0)));
    ClientSQLAsyncResult debugInfoResult;
    callNativeFunction(finalConnection, "debugInfo", {Value(debugInfoResult.makeCallback())});
    EXPECT_EQ(1, debugInfoResult.waitForSuccess().getMapValue("liveCoordinators").toInt());

    ClientSQLAsyncResult closeResult;
    callNativeFunction(finalConnection, "close", {Value(closeResult.makeCallback())});
    closeResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, validatesSchemaAndMigrationVersionsAtBoundary) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    auto invalidSchemaParameters = makeClientSQLOpenParameters(STRING_LITERAL("invalid-schema-version.sqlite"));
    invalidSchemaParameters[1] = Value(0);
    const auto schemaError = callNativeFunctionExpectingError(module, "openDatabase", invalidSchemaParameters);
    EXPECT_NE(std::string::npos, schemaError.find("positive 32-bit integer")) << schemaError;

    Value firstMigration;
    firstMigration.setMapValue("version", Value(2));
    firstMigration.setMapValue("statements", Value(ValueArray::make({Value("SELECT 1")})));
    Value duplicateMigration;
    duplicateMigration.setMapValue("version", Value(2));
    duplicateMigration.setMapValue("statements", Value(ValueArray::make({Value("SELECT 2")})));
    std::vector<Value> duplicateMigrationParameters{
        Value("duplicate-migration-version.sqlite"),
        Value(2),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY)")})),
        Value(ValueArray::make({firstMigration, duplicateMigration})),
    };
    const auto duplicateError =
        callNativeFunctionExpectingError(module, "openDatabase", duplicateMigrationParameters);
    EXPECT_NE(std::string::npos, duplicateError.find("duplicate migration version 2")) << duplicateError;
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, rejectsMissingMigrationVersions) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    std::vector<Value> initialOpenParameters{
        Value("runtime-migration-gap-test.sqlite"),
        Value(1),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY)")})),
        Value(ValueArray::make(0)),
    };
    auto initialConnection = callNativeFunction(module, "openDatabase", initialOpenParameters);
    queryClientSQL(
        initialConnection, "query", STRING_LITERAL("SELECT COUNT(*) AS count FROM item"), Value::undefined());

    ClientSQLAsyncResult initialCloseResult;
    callNativeFunction(initialConnection, "close", {Value(initialCloseResult.makeCallback())});
    initialCloseResult.waitForSuccess();

    Value versionThreeMigration;
    versionThreeMigration.setMapValue("version", Value(3));
    versionThreeMigration.setMapValue(
        "statements",
        Value(ValueArray::make({Value("ALTER TABLE item ADD COLUMN name TEXT")})));
    std::vector<Value> upgradedOpenParameters{
        Value("runtime-migration-gap-test.sqlite"),
        Value(3),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, name TEXT)")})),
        Value(ValueArray::make({versionThreeMigration})),
    };
    auto upgradedConnection = callNativeFunction(module, "openDatabase", upgradedOpenParameters);

    ClientSQLAsyncResult queryResult;
    callNativeFunction(upgradedConnection,
                       "query",
                       {Value("SELECT COUNT(*) AS count FROM item"),
                        Value(ValueArray::make(0)),
                        Value(queryResult.makeCallback())});
    const auto error = queryResult.waitForError().toString();
    EXPECT_NE(std::string::npos, error.find("missing migration for schema version 2")) << error;

    ClientSQLAsyncResult upgradedCloseResult;
    callNativeFunction(upgradedConnection, "close", {Value(upgradedCloseResult.makeCallback())});
    upgradedCloseResult.waitForSuccess();
    workerQueue->fullTeardown();
}

} // namespace ValdiTest
