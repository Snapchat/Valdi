#include "valdi/jsbridge/JavaScriptBridge.hpp"
#include "valdi/standalone_runtime/Arguments.hpp"
#include "valdi/standalone_runtime/ArgumentsParser.hpp"
#include "valdi/standalone_runtime/ValdiStandaloneRuntime.hpp"

#include "image_toolbox/ImageToolbox.hpp"
#include "valdi/compiler_toolbox/CompilerToolbox.hpp"
#include "valdi/compiler_toolbox/PNGQuant.hpp"
#include "valdi/compiler_toolbox/RewriteHeader.hpp"
#include "valdi/stamp/Stamp.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"

#include <iostream>
#include <string>

using namespace snap::valdi_core;

namespace Valdi {

static int printHelp(const std::string& errorMessage) {
    std::cerr << errorMessage << std::endl << std::endl;
    std::cerr << R"D3LIM(
Valdi Compiler Toolbox

Available commands:
  precompile      Precompile a JavaScript file into JS ByteCode
  image_info      Retrieves the info of an image
  image_convert   Convert an image into a different format and or size
  pngquant        Optimize PNG images
  rewrite_header  Rewrites imports of a C or Objective-C header
    )D3LIM" << std::endl;
    return -1;
}

static int onError(const Error& error) {
    std::cerr << error.getMessage().toStringView() << std::endl;
    return EXIT_FAILURE;
}

int printErrorAndUsage(const Error& error, const ArgumentsParser& parser, const char* command) {
    onError(error);
    std::cerr << std::endl << "Available parameters for command " << command << ":" << std::endl;
    std::cerr << parser.getFullDescription().toStringView() << std::endl;
    return EXIT_FAILURE;
}

static int precompile(Arguments& arguments) {
    ArgumentsParser parser;
    auto input = parser.addArgument("-i")->setDescription("The input file to precompile")->setRequired();
    auto output = parser.addArgument("-o")->setDescription("Where to store the output file")->setRequired();
    auto engine = parser.addArgument("-e")
                      ->setDescription("The JS engine to use")
                      ->setChoices({/* Must be in the same order as snap::valdi_core::JavaScriptEngineType */
                                    "quickjs",
                                    "jscore",
                                    "v8",
                                    "hermes"})
                      ->setRequired();
    auto filename = parser.addArgument("-f")
                        ->setDescription("The filename used to identify the module in the JS engine")
                        ->setRequired();

    auto result = parser.parse(arguments);
    if (!result) {
        return printErrorAndUsage(result.error(), parser, "precompile");
    }

    auto engineType = static_cast<snap::valdi_core::JavaScriptEngineType>(engine->getChoiceIndex() + 1);

    auto precompileResult = ValdiStandaloneRuntime::preCompile(
        JavaScriptBridge::get(engineType), input->value(), output->value(), filename->value());

    if (!precompileResult) {
        return onError(precompileResult.error());
    }

    return EXIT_SUCCESS;
}

static int imageInfo(Arguments& arguments) {
    ArgumentsParser parser;
    auto input = parser.addArgument("-i")->setDescription("The input image file")->setRequired();

    auto result = parser.parse(arguments);
    if (!result) {
        return printErrorAndUsage(result.error(), parser, "image_info");
    }

    auto jsonResult = snap::imagetoolbox::outputImageInfo(input->value());
    if (!jsonResult) {
        return onError(jsonResult.error());
    }

    std::cout << jsonResult.value().asStringView() << std::endl;

    return EXIT_SUCCESS;
}

static int imageConvert(Arguments& arguments) {
    ArgumentsParser parser;
    auto input = parser.addArgument("-i")->setDescription("The input image file to convert")->setRequired();
    auto output = parser.addArgument("-o")->setDescription("The converted output image file")->setRequired();
    auto width = parser.addArgument("-w")->setDescription("The output width");
    auto height = parser.addArgument("-h")->setDescription("The output height");
    auto quality = parser.addArgument("-q")->setDescription("The quality ratio between 0 and 1, applicable for JPG");
    auto round = parser.addArgument("--round")->setDescription("Clip the output image to a circle")->setAsFlag();

    auto result = parser.parse(arguments);
    if (!result) {
        return printErrorAndUsage(result.error(), parser, "image_convert");
    }

    std::optional<int> outputWidth;
    std::optional<int> outputHeight;

    if (width->hasValue()) {
        outputWidth = {Value(width->value()).toInt()};
    }
    if (height->hasValue()) {
        outputHeight = {Value(height->value()).toInt()};
    }

    double qualityRatio = 1.0;
    if (quality->hasValue()) {
        qualityRatio = Value(quality->value()).toDouble();
        if (qualityRatio < 0 || qualityRatio > 1) {
            return onError(Error("Quality ratio should be between 0 and 1"));
        }
    }

    auto convertResult = snap::imagetoolbox::convertImage(
        input->value(), output->value(), outputWidth, outputHeight, qualityRatio, round->hasValue());
    if (!convertResult) {
        return onError(convertResult.error());
    }

    std::cout << convertResult.value().asStringView();

    return EXIT_SUCCESS;
}

static int pngquant(Arguments& arguments) {
    ArgumentsParser parser;
    auto input = parser.addArgument("-i")->setDescription("The input PNG file to optimize")->setRequired();

    auto result = parser.parse(arguments);
    if (!result) {
        return printErrorAndUsage(result.error(), parser, "pngquant");
    }

    result = optimizePNG(input->value());
    if (!result) {
        return onError(result.error());
    }
    return EXIT_SUCCESS;
}

static int rewriteHeader(Arguments& arguments) {
    ArgumentsParser parser;
    auto input = parser.addArgument("-i")->setDescription("The input header file to process")->setRequired();
    auto output = parser.addArgument("-o")->setDescription("The processed output header file")->setRequired();
    auto moduleName =
        parser.addArgument("-m")->setDescription("The module name to use for renamed imports")->setRequired();
    auto preservedModuleNames =
        parser.addArgument("-p")->setDescription("A list of module names to preserve")->setAllowsMultipleValues();
    auto flattenImportPaths = parser.addArgument("-f")
                                  ->setDescription("Whether to flatten import paths and remove sub directores")
                                  ->setAsFlag();

    auto result = parser.parse(arguments);
    if (!result) {
        return printErrorAndUsage(result.error(), parser, "rewrite_header");
    }

    result = Valdi::rewriteHeader(input->value(),
                                  output->value(),
                                  moduleName->value(),
                                  preservedModuleNames->values(),
                                  flattenImportPaths->hasValue());
    if (!result) {
        return onError(result.error());
    }
    return EXIT_SUCCESS;
}

static int version(Arguments& /*arguments*/) {
    std::cout << Valdi::getToolboxVersion() << std::endl;
    return EXIT_SUCCESS;
}

int runCompilerToolbox(int argc, const char** argv) {
    Arguments arguments(argc, argv);
    arguments.next();

    if (!arguments.hasNext()) {
        return printHelp("Missing command");
    }

    auto command = arguments.next();

    if (command == "precompile") {
        return precompile(arguments);
    } else if (command == "image_info") {
        return imageInfo(arguments);
    } else if (command == "image_convert") {
        return imageConvert(arguments);
    } else if (command == "pngquant") {
        return pngquant(arguments);
    } else if (command == "version") {
        return version(arguments);
    } else if (command == "rewrite_header") {
        return rewriteHeader(arguments);
    } else {
        return printHelp(fmt::format("Unrecognized command '{}'", command));
    }
}

} // namespace Valdi
