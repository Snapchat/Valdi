#include "TestDataUtils.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include <cstdlib>
#include <optional>
#include <string_view>
#include <unistd.h>

using namespace Valdi;

namespace snap::drawing {

namespace {

constexpr const char* testDataSubDir = "snap_drawing/testdata";

Path resolveRunfilesTestPath(std::initializer_list<const char*> subDirsToCheck) {
    char cwdBuffer[PATH_MAX];
    (void)::getcwd(cwdBuffer, PATH_MAX);

    auto cwdPath = Path(cwdBuffer);
    Path basePath;

    for (const auto& subDir : subDirsToCheck) {
        basePath = cwdPath;
        basePath.append(subDir);
        basePath.append(testDataSubDir);
        basePath.normalize();

        if (DiskUtils::isDirectory(basePath)) {
            return basePath;
        }
    }

    return basePath;
}

} // namespace

Path resolveTestPath(const std::string& path) {
    auto testPath = resolveRunfilesTestPath({".", "../+local_repos+valdi"});
    testPath.append(path);
    return testPath;
}

Valdi::Result<Valdi::BytesView> getTestData(const std::string& filename) {
    auto path = resolveTestPath(filename);
    return DiskUtils::load(path);
}

} // namespace snap::drawing
