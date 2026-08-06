#include "valdi/runtime/JavaScript/Modules/FileSystemFactory.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"

namespace Valdi {
FileSystemFactory::FileSystemFactory() = default;
FileSystemFactory::~FileSystemFactory() = default;

StringBox FileSystemFactory::getModulePath() {
    return STRING_LITERAL("FileSystem");
}

Value FileSystemFactory::loadModule() {
    Value out;

    auto strongThis = shared_from_this();

    out.setMapValue(
        "existsSync",
        Value(makeShared<ValueFunctionWithCallable>([strongThis](const ValueFunctionCallContext& callContext) -> Value {
            auto pathNameResult = callContext.getParameterAsString(0);

            if (!callContext.getExceptionTracker()) {
                return Value::undefined();
            }

            Path const path = Path(pathNameResult.toStringView());
            return Value(DiskUtils::stat(path).exists());
        })));

    out.setMapValue(
        "removeSync",
        Value(makeShared<ValueFunctionWithCallable>([strongThis](const ValueFunctionCallContext& callContext) -> Value {
            auto pathNameResult = callContext.getParameterAsString(0);

            if (!callContext.getExceptionTracker()) {
                return Value::undefined();
            }

            Path const path = Path(pathNameResult.toStringView());

            bool const result = DiskUtils::remove(path);
            if (!result) {
                callContext.getExceptionTracker().onError(fmt::format("Could not remove path: '{}'", pathNameResult));
                return Value::undefined();
            }

            return Value(result);
        })));

    out.setMapValue(
        "createDirectorySync",
        Value(makeShared<ValueFunctionWithCallable>([strongThis](const ValueFunctionCallContext& callContext) -> Value {
            auto pathName = callContext.getParameterAsString(0);

            if (!callContext.getExceptionTracker()) {
                return Value::undefined();
            }

            auto createIntermediates = callContext.getParameterAsBool(1);
            if (!callContext.getExceptionTracker()) {
                callContext.getExceptionTracker().onError(
                    fmt::format("Please set createIntermediates for: '{}'", pathName));
                return Value::undefined();
            }

            Path const path = Path(pathName.toStringView());
            if (DiskUtils::isDirectory(path)) {
                return Value(true);
            }

            bool const result = DiskUtils::makeDirectory(path, createIntermediates);
            if (!result) {
                callContext.getExceptionTracker().onError(
                    fmt::format("Could not create directory at path: '{}'", pathName));
                return Value::undefined();
            }

            return Value(result);
        })));

    out.setMapValue(
        "readFileSync",
        Value(makeShared<ValueFunctionWithCallable>([strongThis](const ValueFunctionCallContext& callContext) -> Value {
            auto pathToFile = callContext.getParameterAsString(0);

            if (!callContext.getExceptionTracker()) {
                return Value::undefined();
            }

            auto options = callContext.getParameter(1);

            Path const path = Path(pathToFile.toStringView());

            auto result = DiskUtils::load(path);
            if (!result) {
                callContext.getExceptionTracker().onError(
                    fmt::format("Could not read the file at path: '{}'", pathToFile));
                return Value::undefined();
            }

            auto fileContent = result.moveValue();

            auto encoding = options.getMapValue("encoding");

            if (encoding == Value("utf8")) {
                return Value(StaticString::makeUTF8(fileContent.asStringView()));
            } else if (encoding == Value("utf16")) {
                return Value(StaticString::makeUTF16(reinterpret_cast<const char16_t*>(fileContent.data()),
                                                     fileContent.size() / sizeof(char16_t)));
            } else {
                return Value(makeShared<ValueTypedArray>(ArrayBuffer, fileContent));
            }
        })));

    out.setMapValue(
        "writeFileSync",
        Value(makeShared<ValueFunctionWithCallable>([strongThis](const ValueFunctionCallContext& callContext) -> Value {
            auto pathToFile = callContext.getParameterAsString(0);

            if (!callContext.getExceptionTracker()) {
                return Value::undefined();
            }

            auto fileContent = callContext.getParameter(1);
            if (!callContext.getExceptionTracker()) {
                callContext.getExceptionTracker().onError(
                    fmt::format("Please set file content for file: '{}'", pathToFile));
                return Value::undefined();
            }

            BytesView bytes;

            if (fileContent.isTypedArray()) {
                bytes = fileContent.getTypedArray()->getBuffer();
            } else if (fileContent.isString()) {
                auto string = fileContent.toStringBox();
                bytes = BytesView(
                    string.getInternedString(), reinterpret_cast<const Byte*>(string.getCStr()), string.length());
            }

            Path const path = Path(pathToFile.toStringView());

            if (DiskUtils::isFile(path)) {
                DiskUtils::remove(path);
            }

            if (path.getComponents().size() > 1) {
                auto directory = path.removingLastComponent();
                if (!DiskUtils::isDirectory(directory)) {
                    DiskUtils::makeDirectory(directory, true);
                }
            }

            auto result = DiskUtils::store(path, bytes);
            if (!result) {
                callContext.getExceptionTracker().onError(
                    fmt::format("Could not store the file at path: '{}'", pathToFile));
                return Value::undefined();
            }

            return Value::undefined();
        })));

    out.setMapValue(
        "currentWorkingDirectory",
        Value(makeShared<ValueFunctionWithCallable>([strongThis](const ValueFunctionCallContext& callContext) -> Value {
            auto const currentWorkingDirectory = DiskUtils::currentWorkingDirectory();
            return Value(currentWorkingDirectory.toString());
        })));

    return out;
}
} // namespace Valdi
