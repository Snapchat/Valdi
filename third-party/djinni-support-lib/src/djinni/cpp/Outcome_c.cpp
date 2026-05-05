#include "Outcome_c.h"
#include "djinni_c_types.hpp"

namespace djinni {

class Outcome : public Object {
public:
  Outcome(djinni_ref value, bool success) : _value(value), _success(success) {
    djinni_ref_retain(value);
  }
  ~Outcome() override { djinni_ref_release(_value); }

  bool success() const { return _success; }

  const djinni_ref &value() const { return _value; }

private:
  djinni_ref _value;
  bool _success;
};

} // namespace djinni

using namespace djinni;

djinni_outcome_ref djinni_outcome_make_success(djinni_ref success_val) {
  return toC(new Outcome(success_val, true));
}

djinni_outcome_ref djinni_outcome_make_error(djinni_ref error_val) {
  return toC(new Outcome(error_val, false));
}

bool djinni_outcome_is_success(djinni_outcome_ref outcome) {
  return fromC<Outcome>(outcome)->success();
}

bool djinni_outcome_is_error(djinni_outcome_ref outcome) {
  return !djinni_outcome_is_success(outcome);
}

djinni_ref djinni_outcome_get_value(djinni_outcome_ref outcome) {
  auto *cpp = fromC<Outcome>(outcome);
  if (!cpp->success()) {
    return nullptr;
  }

  djinni_ref_retain(cpp->value());
  return cpp->value();
}

djinni_ref djinni_outcome_get_error(djinni_outcome_ref outcome) {
  auto *cpp = fromC<Outcome>(outcome);
  if (cpp->success()) {
    return nullptr;
  }

  djinni_ref_retain(cpp->value());
  return cpp->value();
}