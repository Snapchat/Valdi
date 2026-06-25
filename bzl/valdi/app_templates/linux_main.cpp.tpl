#include "valdi/linux/Bootstrap/ValdiLinuxMain.hpp"

int main(int argc, const char** argv) {
    return ValdiLinux::valdiLinuxMain("@VALDI_ROOT_COMPONENT_PATH@", argc, argv);
}
