// Paired with cli_main.cpp.tpl, which valdi_cli_application picks when enable_http is not set.
#include "valdi/cli_runner/CLIRunner.hpp"
#include "valdi/standalone_http/CurlHTTPRequestManager.hpp"

int main(int argc, const char** argv) {
    return Valdi::valdiCLIRun("@VALDI_SCRIPT_PATH@", argc, argv, Valdi::makeCurlHTTPRequestManager());
}
