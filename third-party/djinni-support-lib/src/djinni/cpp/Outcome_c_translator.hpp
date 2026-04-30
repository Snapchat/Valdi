#pragma once

#include "Outcome_c.h"
#include "expected.hpp"

namespace djinni::c_api {

template <typename Tr1, typename Tr2> struct OutcomeTranslator {
  using CppType =
      ::djinni::expected<typename Tr1::CppType, typename Tr2::CppType>;
  using CType = djinni_outcome_ref;

  static CppType toCpp(djinni_outcome_ref outcome) {
    if (djinni_outcome_is_success(outcome)) {
      auto success = djinni_outcome_get_value(outcome);
      auto out = CppType(Tr1::toCpp(success));
      djinni_ref_release(success);
      return out;
    } else {
      auto error = djinni_outcome_get_error(outcome);
      auto out =
          ::djinni::make_unexpected<typename Tr2::CppType>(Tr2::toCpp(error));
      djinni_ref_release(error);
      return out;
    }
  }

  static CType fromCpp(CppType &&outcome) {
    if (outcome) {
      auto success = Tr1::fromCpp(std::move(outcome.value()));
      djinni_outcome_ref result = djinni_outcome_make_success(success);
      djinni_ref_release(success);
      return result;
    } else {
      auto error = Tr2::fromCpp(std::move(outcome.error()));
      djinni_outcome_ref result = djinni_outcome_make_error(error);
      djinni_ref_release(error);
      return result;
    }
  }

  static CType fromCpp(const CppType &outcome) {
    if (outcome) {
      auto success = Tr1::fromCpp(outcome.value());
      djinni_outcome_ref result = djinni_outcome_make_success(success);
      djinni_ref_release(success);
      return result;
    } else {
      auto error = Tr2::fromCpp(outcome.error());
      djinni_outcome_ref result = djinni_outcome_make_error(error);
      djinni_ref_release(error);
      return result;
    }
  }
};

} // namespace djinni::c_api