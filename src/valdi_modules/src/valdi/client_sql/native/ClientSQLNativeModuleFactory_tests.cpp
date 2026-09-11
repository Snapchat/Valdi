#include "ClientSQLNativeModuleFactory.hpp"
#include "valdi/runtime/Resources/DiskCacheImpl.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"

#include <gtest/gtest.h>

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdlib.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace Valdi;

namespace ValdiTest {

class TypedClientSQLNativeModuleFactory : public ClientSQLNativeModuleFactory {
public:
    TypedClientSQLNativeModuleFactory(const Ref<IDiskCache>& diskCache,
                                      const Ref<IDispatchQueue>& workerQueue)
        : ClientSQLNativeModuleFactory(diskCache, workerQueue) {}

    Ref<snap::valdi_modules::client_sql::ClientSQLNativeModule> loadTypedModule() {
        return onLoadModule();
    }
};

class FlushableClientSQLResourceReleaseQueue final : public IDispatchQueue {
public:
    void sync(const DispatchFunction& function) final {
        function();
    }

    void async(DispatchFunction function) final {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _tasks.emplace_back(std::move(function));
        }
        _condition.notify_all();
    }

    task_id_t asyncAfter(DispatchFunction function, std::chrono::steady_clock::duration delay) final {
        (void)delay;
        async(std::move(function));
        return 0;
    }

    void cancel(task_id_t taskId) final {
        (void)taskId;
    }

    bool waitForPendingTask(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(_mutex);
        return _condition.wait_for(lock, timeout, [this]() { return !_tasks.empty(); });
    }

    size_t pendingTaskCount() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _tasks.size();
    }

    size_t flushTasks() {
        size_t flushedTaskCount = 0;
        while (true) {
            std::deque<DispatchFunction> tasks;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (_tasks.empty()) {
                    return flushedTaskCount;
                }
                tasks.swap(_tasks);
            }
            while (!tasks.empty()) {
                auto task = std::move(tasks.front());
                tasks.pop_front();
                task();
                ++flushedTaskCount;
            }
        }
    }

private:
    mutable std::mutex _mutex;
    std::condition_variable _condition;
    std::deque<DispatchFunction> _tasks;
};

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
                _callCount++;
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

    size_t callCount() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _callCount;
    }

    std::thread::id getCallbackThreadId() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _callbackThreadId;
    }

private:
    mutable std::mutex _mutex;
    std::condition_variable _condition;
    bool _called = false;
    size_t _callCount = 0;
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

TEST(ClientSQLNativeModuleFactory, usesDedicatedDatabaseQueuesWhenFallbackQueueIsOccupied) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);

    std::mutex blockerMutex;
    std::condition_variable blockerCondition;
    bool blockerStarted = false;
    bool releaseBlocker = false;
    workerQueue->async([&]() {
        std::unique_lock<std::mutex> lock(blockerMutex);
        blockerStarted = true;
        blockerCondition.notify_one();
        blockerCondition.wait(lock, [&]() { return releaseBlocker; });
    });
    bool didStartBlocker = false;
    {
        std::unique_lock<std::mutex> lock(blockerMutex);
        didStartBlocker = blockerCondition.wait_for(
            lock,
            std::chrono::seconds(5),
            [&]() { return blockerStarted; });
    }
    if (!didStartBlocker) {
        {
            std::lock_guard<std::mutex> lock(blockerMutex);
            releaseBlocker = true;
        }
        blockerCondition.notify_one();
        workerQueue->fullTeardown();
        FAIL() << "Timed out waiting for fallback queue blocker";
        return;
    }

    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();
    std::vector<Value> openParameters{
        Value("runtime-dedicated-queue-test.sqlite"),
        Value(1),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY)")})),
        Value(ValueArray::make(0)),
    };
    auto connection = callNativeFunction(module, "openDatabase", openParameters);

    ClientSQLAsyncResult insertResult;
    callNativeFunction(connection,
                       "execute",
                       {Value("INSERT INTO item(id) VALUES (1)"),
                        Value(ValueArray::make(0)),
                        Value(insertResult.makeCallback())});
    insertResult.waitForSuccessWithin(std::chrono::seconds(2));

    ClientSQLAsyncResult queryResult;
    callNativeFunction(connection,
                       "query",
                       {Value("SELECT id FROM item"),
                        Value(ValueArray::make(0)),
                        Value(queryResult.makeCallback())});
    auto queryValue = queryResult.waitForSuccessWithin(std::chrono::seconds(2));

    ClientSQLAsyncResult closeResult;
    callNativeFunction(connection, "close", {Value(closeResult.makeCallback())});
    closeResult.waitForSuccessWithin(std::chrono::seconds(2));

    {
        std::lock_guard<std::mutex> lock(blockerMutex);
        releaseBlocker = true;
    }
    blockerCondition.notify_one();
    workerQueue->fullTeardown();

    auto rows = queryValue.getArrayRef();
    ASSERT_NE(nullptr, rows);
    ASSERT_EQ(1ul, rows->size());
    EXPECT_EQ(1, (*rows)[0].getMapValue("id").toInt());
}

TEST(ClientSQLNativeModuleFactory, rejectsMultipleStatementsWithoutExecutingTheFirst) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();
    std::vector<Value> openParameters{
        Value("runtime-single-statement-test.sqlite"),
        Value(1),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY)")})),
        Value(ValueArray::make(0)),
    };
    auto connection = callNativeFunction(module, "openDatabase", openParameters);

    ClientSQLAsyncResult executeResult;
    callNativeFunction(connection,
                       "execute",
                       {Value("INSERT INTO item(id) VALUES (1); INSERT INTO item(id) VALUES (2)"),
                        Value(ValueArray::make(0)),
                        Value(executeResult.makeCallback())});
    const auto executeError = executeResult.waitForError().toString();
    EXPECT_NE(std::string::npos, executeError.find("exactly one statement")) << executeError;

    auto countRows = queryClientSQL(
                         connection,
                         "query",
                         STRING_LITERAL("SELECT COUNT(*) AS count FROM item"),
                         Value(ValueArray::make(0)))
                         .getArrayRef();
    ASSERT_NE(nullptr, countRows);
    ASSERT_EQ(1ul, countRows->size());
    EXPECT_EQ(0, (*countRows)[0].getMapValue("count").toInt());

    ClientSQLAsyncResult queryResult;
    callNativeFunction(connection,
                       "query",
                       {Value("SELECT 1 AS value; SELECT 2 AS value"),
                        Value(ValueArray::make(0)),
                        Value(queryResult.makeCallback())});
    const auto queryError = queryResult.waitForError().toString();
    EXPECT_NE(std::string::npos, queryError.find("exactly one statement")) << queryError;

    executeClientSQL(
        connection,
        STRING_LITERAL("INSERT INTO item(id) VALUES (3); -- trailing comment\n /* still trailing */"),
        Value(ValueArray::make(0)));
    auto rows = queryClientSQL(
                    connection,
                    "query",
                    STRING_LITERAL("SELECT id FROM item; /* trailing comment */"),
                    Value(ValueArray::make(0)))
                    .getArrayRef();
    ASSERT_NE(nullptr, rows);
    ASSERT_EQ(1ul, rows->size());
    EXPECT_EQ(3, (*rows)[0].getMapValue("id").toInt());

    ClientSQLAsyncResult closeResult;
    callNativeFunction(connection, "close", {Value(closeResult.makeCallback())});
    closeResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, boundsQueryResultRowMaterialization) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();
    auto connection = callNativeFunction(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(STRING_LITERAL("runtime-query-row-limit-test.sqlite")));

    ClientSQLAsyncResult queryResult;
    callNativeFunction(
        connection,
        "query",
        {Value(
             "WITH RECURSIVE generated(value) AS ("
             "VALUES(1) UNION ALL SELECT value + 1 FROM generated WHERE value < 10001"
             ") SELECT value FROM generated"),
         Value::undefined(),
         Value(queryResult.makeCallback())});
    const auto error = queryResult.waitForError().toString();
    EXPECT_NE(std::string::npos, error.find("10000 row materialization limit")) << error;

    ClientSQLAsyncResult closeResult;
    callNativeFunction(connection, "close", {Value(closeResult.makeCallback())});
    closeResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, boundsQueryOnWriterResultByteMaterialization) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();
    auto connection = callNativeFunction(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(STRING_LITERAL("runtime-query-byte-limit-test.sqlite")));

    ClientSQLAsyncResult queryResult;
    callNativeFunction(
        connection,
        "queryOnWriter",
        {Value("SELECT zeroblob(8388609) AS payload"),
         Value::undefined(),
         Value(queryResult.makeCallback())});
    const auto error = queryResult.waitForError().toString();
    EXPECT_NE(std::string::npos, error.find("8388608 byte materialization limit")) << error;

    ClientSQLAsyncResult closeResult;
    callNativeFunction(connection, "close", {Value(closeResult.makeCallback())});
    closeResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, countsNullCellAndColumnOverheadTowardQueryLimit) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();
    auto connection = callNativeFunction(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(STRING_LITERAL("runtime-query-null-overhead-test.sqlite")));

    std::string sql =
        "WITH RECURSIVE generated(value) AS ("
        "VALUES(1) UNION ALL SELECT value + 1 FROM generated WHERE value < 300"
        ") SELECT ";
    for (int columnIndex = 0; columnIndex < 512; ++columnIndex) {
        if (columnIndex != 0) {
            sql.append(", ");
        }
        sql.append("NULL AS column_");
        sql.append(std::to_string(columnIndex));
    }
    sql.append(" FROM generated");

    ClientSQLAsyncResult queryResult;
    callNativeFunction(
        connection,
        "query",
        {Value(StringCache::getGlobal().makeString(sql)),
         Value::undefined(),
         Value(queryResult.makeCallback())});
    const auto error = queryResult.waitForError().toString();
    EXPECT_NE(std::string::npos, error.find("8388608 byte materialization limit")) << error;

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

TEST(ClientSQLNativeModuleFactory, timesOutAndRollsBackTransactionThatNeverCompletes) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(
        diskCache, workerQueue, std::chrono::milliseconds(50));
    auto module = factory->loadModule();

    std::vector<Value> openParameters{
        Value("runtime-transaction-timeout-test.sqlite"),
        Value(1),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY)")})),
        Value(ValueArray::make(0)),
    };
    auto connection = callNativeFunction(module, "openDatabase", openParameters);

    std::mutex doneCallbackMutex;
    Ref<ValueFunction> savedDoneCallback;
    auto body = makeShared<ValueFunctionWithCallable>(
        [&doneCallbackMutex, &savedDoneCallback](const ValueFunctionCallContext& callContext) -> Value {
            auto transaction = callContext.getParameter(0);
            auto doneCallback = callContext.getParameterAsFunction(1);
            if (!callContext.getExceptionTracker()) {
                return Value::undefined();
            }
            {
                std::lock_guard<std::mutex> lock(doneCallbackMutex);
                savedDoneCallback = doneCallback;
            }

            auto insertCallback = makeShared<ValueFunctionWithCallable>(
                [](const ValueFunctionCallContext& /*insertContext*/) -> Value {
                    return Value::undefined();
                });
            callNativeFunction(
                transaction,
                "execute",
                {Value("INSERT INTO item(id) VALUES (1)"),
                 Value::undefined(),
                 Value(insertCallback)});
            return Value::undefined();
        });

    ClientSQLAsyncResult transactionResult;
    callNativeFunction(connection, "transaction", {Value(body), Value(transactionResult.makeCallback())});
    const auto error = transactionResult.waitForError().toString();
    EXPECT_NE(std::string::npos, error.find("timed out after 50 ms")) << error;

    auto rows = queryClientSQL(
                    connection,
                    "queryOnWriter",
                    STRING_LITERAL("SELECT COUNT(*) AS count FROM item"),
                    Value::undefined())
                    .getArrayRef();
    ASSERT_NE(rows, nullptr);
    ASSERT_EQ(1ul, rows->size());
    EXPECT_EQ(0, (*rows)[0].getMapValue("count").toInt());

    Ref<ValueFunction> lateDoneCallback;
    {
        std::lock_guard<std::mutex> lock(doneCallbackMutex);
        lateDoneCallback = std::move(savedDoneCallback);
    }
    ASSERT_NE(lateDoneCallback, nullptr);
    (*lateDoneCallback)({Value("too late"), Value::undefined()});
    lateDoneCallback = nullptr;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(1ul, transactionResult.callCount());

    ClientSQLAsyncResult closeResult;
    callNativeFunction(connection, "close", {Value(closeResult.makeCallback())});
    closeResult.waitForSuccess();
    workerQueue->fullTeardown();
}

TEST(ClientSQLNativeModuleFactory, rollsBackWhenTransactionBodyThrowsStandardException) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<TypedClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadTypedModule();
    auto connection = module->openDatabase(
        STRING_LITERAL("runtime-transaction-standard-exception-test.sqlite"),
        1,
        {STRING_LITERAL("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY)")},
        {});

    std::mutex transactionMutex;
    std::condition_variable transactionCondition;
    bool transactionCallbackCalled = false;
    std::optional<StringBox> transactionError;
    connection->transaction(
        [](auto /*transaction*/, auto /*doneCallback*/) {
            throw std::runtime_error("standard transaction failure");
        },
        [&transactionMutex, &transactionCondition, &transactionCallbackCalled, &transactionError](
            Value /*value*/, std::optional<StringBox> error) {
            {
                std::lock_guard<std::mutex> lock(transactionMutex);
                transactionError = std::move(error);
                transactionCallbackCalled = true;
            }
            transactionCondition.notify_one();
        });

    {
        std::unique_lock<std::mutex> lock(transactionMutex);
        ASSERT_TRUE(transactionCondition.wait_for(
            lock, std::chrono::seconds(5), [&transactionCallbackCalled]() {
                return transactionCallbackCalled;
            }));
    }
    ASSERT_TRUE(transactionError.has_value());
    EXPECT_NE(std::string_view::npos, transactionError->toStringView().find("standard transaction failure"));

    std::mutex queryMutex;
    std::condition_variable queryCondition;
    bool queryCallbackCalled = false;
    std::optional<StringBox> queryError;
    connection->query(
        STRING_LITERAL("SELECT COUNT(*) AS count FROM item"),
        std::nullopt,
        [&queryMutex, &queryCondition, &queryCallbackCalled, &queryError](
            std::optional<std::vector<Value>> /*rows*/, std::optional<StringBox> error) {
            {
                std::lock_guard<std::mutex> lock(queryMutex);
                queryError = std::move(error);
                queryCallbackCalled = true;
            }
            queryCondition.notify_one();
        });
    {
        std::unique_lock<std::mutex> lock(queryMutex);
        ASSERT_TRUE(queryCondition.wait_for(
            lock, std::chrono::seconds(5), [&queryCallbackCalled]() {
                return queryCallbackCalled;
            }));
    }
    EXPECT_FALSE(queryError.has_value());

    std::mutex closeMutex;
    std::condition_variable closeCondition;
    bool closeCallbackCalled = false;
    connection->close([&closeMutex, &closeCondition, &closeCallbackCalled](
                          Value /*value*/, std::optional<StringBox> /*error*/) {
        {
            std::lock_guard<std::mutex> lock(closeMutex);
            closeCallbackCalled = true;
        }
        closeCondition.notify_one();
    });
    {
        std::unique_lock<std::mutex> lock(closeMutex);
        ASSERT_TRUE(closeCondition.wait_for(
            lock, std::chrono::seconds(5), [&closeCallbackCalled]() {
                return closeCallbackCalled;
            }));
    }
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

TEST(ClientSQLNativeModuleFactory, releasesFinalDroppedHandleWithoutClose) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto resourceReleaseQueue = makeShared<FlushableClientSQLResourceReleaseQueue>();
    auto factory = makeShared<ClientSQLNativeModuleFactory>(
        diskCache, workerQueue, std::chrono::seconds(30), resourceReleaseQueue);
    auto module = factory->loadModule();

    auto baselineConnection = callNativeFunction(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(STRING_LITERAL("lifetime-drop-baseline.sqlite")));
    queryClientSQL(
        baselineConnection,
        "query",
        STRING_LITERAL("SELECT COUNT(*) AS count FROM item"),
        Value::undefined());
    ClientSQLAsyncResult baselineDebugInfoResult;
    callNativeFunction(baselineConnection, "debugInfo", {Value(baselineDebugInfoResult.makeCallback())});
    const auto baselineCoordinatorCount =
        baselineDebugInfoResult.waitForSuccess().getMapValue("liveCoordinators").toInt();

    auto droppedConnection = callNativeFunction(
        module,
        "openDatabase",
        makeClientSQLOpenParameters(STRING_LITERAL("lifetime-dropped-handle.sqlite")));
    queryClientSQL(
        droppedConnection,
        "query",
        STRING_LITERAL("SELECT COUNT(*) AS count FROM item"),
        Value::undefined());

    droppedConnection = Value::undefined();
    // The coordinator destructor only submits this task when its final ref is
    // released on one of its own database queues. Keeping the injected queue
    // unflushed proves the queue-owning refs were handed off rather than being
    // destroyed on that database queue.
    ASSERT_TRUE(resourceReleaseQueue->waitForPendingTask(std::chrono::seconds(5)));
    EXPECT_EQ(1u, resourceReleaseQueue->pendingTaskCount());

    ClientSQLAsyncResult debugInfoResult;
    callNativeFunction(baselineConnection, "debugInfo", {Value(debugInfoResult.makeCallback())});
    EXPECT_EQ(
        baselineCoordinatorCount,
        debugInfoResult.waitForSuccess().getMapValue("liveCoordinators").toInt());

    EXPECT_EQ(1u, resourceReleaseQueue->flushTasks());
    EXPECT_EQ(0u, resourceReleaseQueue->pendingTaskCount());

    ClientSQLAsyncResult closeResult;
    callNativeFunction(baselineConnection, "close", {Value(closeResult.makeCallback())});
    closeResult.waitForSuccess();
    ASSERT_TRUE(resourceReleaseQueue->waitForPendingTask(std::chrono::seconds(5)));
    EXPECT_EQ(1u, resourceReleaseQueue->flushTasks());
    EXPECT_EQ(0u, resourceReleaseQueue->pendingTaskCount());
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

TEST(ClientSQLNativeModuleFactory, rejectsEmbeddedNullInSchemaAndMigrationSQL) {
    ClientSQLTemporaryDirectory directory;
    auto diskCache = makeShared<DiskCacheImpl>(directory.get());
    auto workerQueue = DispatchQueue::create(STRING_LITERAL("ClientSQL Test Worker"), ThreadQoSClassNormal);
    auto factory = makeShared<ClientSQLNativeModuleFactory>(diskCache, workerQueue);
    auto module = factory->loadModule();

    const std::string createSQLWithNull =
        std::string("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY)") + '\0' +
        "DROP TABLE item";
    auto createSQL = StringCache::getGlobal().makeString(
        std::string_view(createSQLWithNull.data(), createSQLWithNull.size()));
    std::vector<Value> createParameters{
        Value("schema-null-test.sqlite"),
        Value(1),
        Value(ValueArray::make({Value(createSQL)})),
        Value(ValueArray::make(0)),
    };
    const auto createError = callNativeFunctionExpectingError(module, "openDatabase", createParameters);
    EXPECT_NE(std::string::npos, createError.find("create statement 0 contains an embedded NUL byte"))
        << createError;

    const std::string migrationSQLWithNull =
        std::string("ALTER TABLE item ADD COLUMN name TEXT") + '\0' +
        "DROP TABLE item";
    auto migrationSQL = StringCache::getGlobal().makeString(
        std::string_view(migrationSQLWithNull.data(), migrationSQLWithNull.size()));
    Value migration;
    migration.setMapValue("version", Value(2));
    migration.setMapValue("statements", Value(ValueArray::make({Value(migrationSQL)})));
    std::vector<Value> migrationParameters{
        Value("migration-null-test.sqlite"),
        Value(2),
        Value(ValueArray::make({Value("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY)")})),
        Value(ValueArray::make({migration})),
    };
    const auto migrationError = callNativeFunctionExpectingError(module, "openDatabase", migrationParameters);
    EXPECT_NE(std::string::npos, migrationError.find("migration 2 statement 0 contains an embedded NUL byte"))
        << migrationError;
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
