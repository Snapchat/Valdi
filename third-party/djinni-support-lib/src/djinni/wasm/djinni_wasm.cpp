/**
  * Copyright 2021 Snap, Inc.
  *
  * Licensed under the Apache License, Version 2.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  *    http://www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */

#include "djinni_wasm.hpp"

namespace djinni {

Binary::CppType Binary::toCpp(const JsType& j) {
    return PrimitiveArray<Primitive<uint8_t>, Binary>::toCpp(j);
}
Binary::JsType Binary::fromCpp(const CppType& c) {
    static em::val arrayClass = em::val::global("Uint8Array");
    return PrimitiveArray<Primitive<uint8_t>, Binary>::fromCpp(c);
}
em::val Binary::getArrayClass() {
    static em::val arrayClass = em::val::global("Uint8Array");
    return arrayClass;
}

Date::CppType Date::toCpp(const JsType& j) {
    auto nanosSinceEpoch = std::chrono::nanoseconds(static_cast<int64_t>(j.call<em::val>("getTime").as<double>() * 1'000'000));
    return CppType(std::chrono::duration_cast<std::chrono::system_clock::duration>(nanosSinceEpoch));
}
Date::JsType Date::fromCpp(const CppType& c) {
    auto nanosSinceEpoch = std::chrono::duration_cast<std::chrono::nanoseconds>(c.time_since_epoch());
    static em::val dateType = em::val::global("Date");
    return dateType.new_(static_cast<double>(nanosSinceEpoch.count()) / 1'000'000.0);
}

JsProxyId nextId = 0;
std::unordered_map<JsProxyId, std::weak_ptr<JsProxyBase>> jsProxyCache;
std::unordered_map<void*, CppProxyCacheEntry> cppProxyCache;
std::mutex jsProxyCacheMutex;
std::mutex cppProxyCacheMutex;

JsProxyBase::JsProxyBase(const em::val& v) : _js(v), _id(_js["_djinni_js_proxy_id"].as<JsProxyId>()) {
}

JsProxyBase::~JsProxyBase() {
    std::lock_guard lk(jsProxyCacheMutex);
    jsProxyCache.erase(_id);
}

const em::val& JsProxyBase::_jsRef() const {
    return _js;
}

void JsProxyBase::checkError(const em::val& v) {
    if (v.instanceof(em::val::global("Error"))) {
        auto cppExceptionPtr = v["_djinni_cpp_exception_ptr"];
        if (!cppExceptionPtr.isUndefined()) {
            std::exception_ptr* exptr = reinterpret_cast<std::exception_ptr*>(cppExceptionPtr.as<int>());
            std::rethrow_exception(*exptr);
        } else {
            throw JsException(v);
        }
    }
}

void checkForNull(void* ptr, const char* context) {
    if (!ptr) {
        throw std::invalid_argument(std::string("nullptr is not allowed in ") + context);
    }
}

em::val getCppProxyFinalizerRegistry() {
    static auto inst  = em::val::module_property("cppProxyFinalizerRegistry");
    return inst;
}

em::val getCppProxyClass() {
    static auto inst  = em::val::module_property("DjinniCppProxy");
    return inst;
}

em::val getWasmMemoryBuffer() {
    // When ALLOW_MEMORY_GROWTH is turned on, the WebAssembly.Memory object's underlying buffer changes as it grows,
    // and the HEAP* views in the Emscripten module object get reset to new views over the bigger memory buffer.
    // In this mode, capturing the heap here as a static variable is incorrect, and leads to runtime errors.
    // https://github.com/emscripten-core/emscripten/blob/3.1.7/src/settings.js#L194-L207 is the growth setting
    // https://github.com/emscripten-core/emscripten/blob/3.1.7/src/library.js#L127 is the call to reset the views after grow()
    // https://github.com/emscripten-core/emscripten/blob/3.1.7/src/preamble.js#L269-L286 is where the views get reset
    return em::val::module_property("HEAPU32")["buffer"];
}

em::val DataObject::createJsObject() {
    static auto finalizerRegistry = em::val::module_property("directBufferFinalizerRegistry");
    static auto uint8ArrayClass = em::val::global("Uint8Array");
    em::val jsObj = uint8ArrayClass.new_(getWasmMemoryBuffer(), addr(), size());
    finalizerRegistry.call<void>("register", jsObj, reinterpret_cast<unsigned>(this));
    return jsObj;
}

static em::val allocateWasmBuffer(unsigned size) {
    auto* dbuf = new GenericBuffer<std::vector<uint8_t>>(size);
    return dbuf->createJsObject();
}

extern "C" EMSCRIPTEN_KEEPALIVE
void releaseWasmBuffer(unsigned addr) {
    delete reinterpret_cast<DataObject*>(addr);
}

EM_JS(void, djinni_init_wasm, (), {
        // console.log("djinni_init_wasm");
        Module.cppProxyFinalizerRegistry = new FinalizationRegistry(nativeRef => {
            // console.log("finalizing cpp object @" + nativeRef);
            nativeRef.nativeDestroy();
            nativeRef.delete();
        });

        Module.directBufferFinalizerRegistry = new FinalizationRegistry(addr => {
            Module._releaseWasmBuffer(addr);
        });

        class DjinniCppProxy {
            constructor(nativeRef, methods) {
                // console.log('new cpp proxy');
                this._djinni_native_ref = nativeRef;
                let self = this;
                methods.forEach(function(method) {
                    self[method] = function(...args) {
                        return nativeRef[method](...args);
                    }
                });
            }
        }
        Module.DjinniCppProxy = DjinniCppProxy;

        class DjinniJsPromiseBuilder {
            constructor(cppHandlerPtr) {
                this.promise = new Promise((resolveFunc, rejectFunc) => {
                        Module.initCppResolveHandler(cppHandlerPtr, resolveFunc, rejectFunc);
                    });
            }
        }
        Module.DjinniJsPromiseBuilder = DjinniJsPromiseBuilder;

        Module.makeNativePromiseResolver = function(func, pNativePromise) {
            return function(res) {
                Module.resolveNativePromise(func, pNativePromise, res);
            };
        };
        Module.makeNativePromiseRejecter = function(func, pNativePromise) {
            return function(err) {
                Module.rejectNativePromise(func, pNativePromise, err);
            };
        };

        Module.writeNativeMemory = function(src, nativePtr) {
            var srcByteView = new Uint8Array(src.buffer, src.byteOffset, src.byteLength);
            Module.HEAPU8.set(srcByteView, nativePtr);
        };
        Module.readNativeMemory = function(cls, nativePtr, nativeSize) {
            return new cls(Module.HEAPU8.buffer.slice(nativePtr, nativePtr + nativeSize));
        };

        Module.protobuf = {};
        Module.registerProtobufLib = function(name, proto) {
            Module.protobuf[name] = proto;
        };

        Module.callJsProxyMethod = function(obj, method, ...args) {
            try {
                return obj[method].apply(obj, args);
            } catch (e) {
                return e;
            }
        };
});

EM_JS(void, djinni_register_name_in_ns, (const char* prefixedName, const char* namespacedName), {
        // Compatibility: readLatin1String was removed in newer Emscripten versions
        // Use UTF8ToString as fallback (available in both old and new versions)
        var stringConverter = (typeof readLatin1String !== 'undefined') ? readLatin1String : UTF8ToString;
        prefixedName = stringConverter(prefixedName);
        namespacedName = stringConverter(namespacedName);
        let parts = namespacedName.split('.');
        let name = parts.pop();
        let ns = parts.reduce(function(path, part) {
            if (!path.hasOwnProperty(part)) { path[part] = {}}; 
            return path[part]
        }, Module);
        ns[name] = Module[prefixedName];
});

em::val djinni_native_exception_to_js(const std::exception& e) {
    if (const auto* jsEx = dynamic_cast<const JsException*>(&e)) {
        return jsEx->cause();
    } else {
        static std::exception_ptr exptr;
        static auto ErrorClass = em::val::global("Error");
        auto error = ErrorClass.new_(std::string("C++: ") + e.what());
        exptr = std::current_exception();
        error.set("_djinni_cpp_exception_ptr", em::val(reinterpret_cast<int>(&exptr)));
        return error;
    }
}

void djinni_throw_native_exception(const std::exception& e) {
    djinni_native_exception_to_js(e).throw_();
}

EMSCRIPTEN_BINDINGS(djinni_wasm) {
    djinni_init_wasm();    
    em::function("allocateWasmBuffer", &allocateWasmBuffer);
    em::function("initCppResolveHandler", &CppResolveHandlerBase::initInstance);
    em::function("resolveNativePromise", &CppResolveHandlerBase::resolveNativePromise);
    em::function("rejectNativePromise", &CppResolveHandlerBase::rejectNativePromise);
}

}
