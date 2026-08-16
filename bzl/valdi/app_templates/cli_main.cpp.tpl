#include "valdi/cli_runner/CLIRunner.hpp"
@VALDI_HTTP_INCLUDE@

int main(int argc, const char** argv) {
    return Valdi::valdiCLIRun("@VALDI_SCRIPT_PATH@", argc, argv@VALDI_HTTP_MANAGER@);
}
