#include "benchmark_utils.hpp"

#include <random>

std::vector<StringBox> internStrings(StringCache& stringCache, const std::vector<std::string>& strings) {
    std::vector<StringBox> cachedStrings;

    for (const auto& str : strings) {
        cachedStrings.emplace_back(stringCache.makeString(str));
    }

    return cachedStrings;
}

std::default_random_engine generator;
std::uniform_int_distribution<int> distribution(' ', '~');

char randomCharacterGenerator() {
    return static_cast<char>(distribution(generator));
}

std::string makeRandomString(size_t strLength) {
    std::string out;

    for (size_t i = 0; i < strLength; i++) {
        out.append(1, randomCharacterGenerator());
    }

    return out;
}

std::vector<std::string> makeRandomStrings(size_t strLength) {
    std::vector<std::string> out;

    for (size_t i = 0; i < 10; i++) {
        out.emplace_back(makeRandomString(strLength));
    }

    return out;
}

std::vector<StringBox> makeRandomInternedStrings(StringCache& stringCache, size_t numberOfStrings, size_t strLength) {
    std::vector<StringBox> out;

    for (size_t i = 0; i < numberOfStrings; i++) {
        out.emplace_back(stringCache.makeString(makeRandomString(strLength)));
    }

    return out;
}
