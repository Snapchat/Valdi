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

#include "Provider_c.h"
#include "djinni_c_types.hpp"

namespace djinni {

class Provider : public Object {
public:
  Provider(djinni_provider_func_ptr func, void* context, djinni_provider_context_releaser context_releaser)
      : _func(func), _context(context), _context_releaser(context_releaser) {}

  ~Provider() override {
    if (_context_releaser && _context) {
      _context_releaser(_context);
    }
  }

  djinni_ref call() const {
    return _func(_context);
  }

private:
  djinni_provider_func_ptr _func;
  void* _context;
  djinni_provider_context_releaser _context_releaser;
};

} // namespace djinni

using namespace djinni;

djinni_ref djinni_provider_call(djinni_provider_ref provider) {
  return fromC<Provider>(provider)->call();
}

djinni_provider_ref djinni_provider_make(djinni_provider_func_ptr func, void* context, djinni_provider_context_releaser context_releaser) {
  return toC(new Provider(func, context, context_releaser));
}
