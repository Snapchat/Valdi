#include "TestDataUtils.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include <unistd.h>

using namespace Valdi;

namespace snap::drawing {

Path resolveTestPath(const std::string& path) {
    char cwdBuffer[PATH_MAX];
    (void)::getcwd(cwdBuffer, PATH_MAX);

    auto basePath = Path(cwdBuffer);

    basePath.append("../+local_repos+valdi/snap_drawing/testdata");
    basePath.append(path);
    basePath.normalize();

    return basePath;
}

Valdi::Result<Valdi::BytesView> getTestData(const std::string& filename) {
    auto path = resolveTestPath(filename);
    return DiskUtils::load(path);
}

} // namespace snap::drawing
