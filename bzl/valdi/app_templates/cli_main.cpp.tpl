// Paired with cli_main_http.cpp.tpl, which valdi_cli_application picks when enable_http is set.
#include "valdi/cli_runner/CLIRunner.hpp"

int main(int argc, const char** argv) {
    // No backend, so valdi_http rejects every request with "No RequestManager set".
    return Valdi::valdiCLIRun("@VALDI_SCRIPT_PATH@", argc, argv, nullptr);
}
