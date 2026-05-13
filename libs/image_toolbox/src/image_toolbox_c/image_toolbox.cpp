#include "image_toolbox.h"
#include "image_toolbox/ImageToolbox.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

extern "C" {

static void setAsError(char** error, const Valdi::StringBox& errorMessage) {
    char* allocatedError = reinterpret_cast<char*>(malloc(sizeof(char) * (errorMessage.length() + 1)));
    std::memcpy(allocatedError, errorMessage.getCStr(), errorMessage.length());
    allocatedError[errorMessage.length()] = 0;

    *error = allocatedError;
}

image_toolbox_size imagetoolbox_get_size(const char* input_path, char** error) {
    image_toolbox_size size;
    size.width = 0;
    size.height = 0;

    auto imageInfo =
        snap::imagetoolbox::getImageInfo(Valdi::StringCache::getGlobal().makeStringFromLiteral(input_path));
    if (imageInfo) {
        size.width = imageInfo.value().width;
        size.height = imageInfo.value().height;
    } else {
        setAsError(error, imageInfo.error().getMessage());
    }

    return size;
}

int imagetoolbox_convert(
    const char* input_path, const char* output_path, int outputWidth, int outputHeight, char** error) {
    auto result = snap::imagetoolbox::convertImage(Valdi::StringCache::getGlobal().makeStringFromLiteral(input_path),
                                                   Valdi::StringCache::getGlobal().makeStringFromLiteral(output_path),
                                                   {outputWidth},
                                                   {outputHeight},
                                                   1.0,
                                                   false);

    if (result) {
        return 0;
    }

    setAsError(error, result.error().getMessage());
    return -1;
}

void imagetoolbox_free_error(char* error) {
    free(error);
}
}
