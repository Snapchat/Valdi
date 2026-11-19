import XCTest
import Foundation
@testable import Compiler

final class BundleInfoRelativeBundleURLTests: XCTestCase {

    private func makeBundle(name: String, baseDir: URL, relativeProjectPathPrefix: String) throws -> CompilationItem.BundleInfo {
        let configDir = baseDir.deletingLastPathComponent()
        let projectConfig = ValdiProjectConfig(
            configDirectoryUrl: configDir,
            projectName: "test",
            baseDir: configDir,
            webEnabled: false,
            iosOutput: nil,
            androidOutput: nil,
            webOutput: nil,
            cppOutput: nil,
            iosBuildFileConfig: nil,
            androidBuildFileConfig: nil,
            webBuildFileConfig: nil,
            codeCoverageResultConfig: ValdiCodeCoverageConfig(reportFolder: configDir,
                                                              reportResultFile: configDir,
                                                              instrumentedFilesResult: configDir),
            iosDefaultModuleNamePrefix: nil,
            iosDefaultLanguage: .swift,
            ignoredFiles: [],
            directoriesToKeepRegexes: [],
            moduleUploadBaseURL: nil,
            diagnosticsDir: configDir,
            compilerCompanionBinaryURL: nil,
            shouldDebugCompilerCompanion: false,
            shouldEmitDiagnostics: false,
            shouldSkipRemoveOrphanFiles: false,
            compilerToolboxURL: configDir,
            minifyConfigURL: nil,
            clientSqlURL: nil,
            sqlExportPathTemplate: nil,
            androidDefaultClassPath: nil,
            cppDefaultClassPrefix: nil,
            cppImportPathPrefix: nil,
            preparedUploadArtifactOutput: nil,
            buildDirectoryURL: configDir,
            generatedTsDirectoryURL: configDir,
            androidJsBytecodeFormat: nil,
            nodeModulesDir: nil,
            openSourceDir: nil,
            nodeModulesTarget: nil,
            nodeModulesWorkspace: nil,
            externalModulesTarget: nil,
            externalModulesWorkspace: nil,
            nativeApiMinVersion: nil
        )

        return try CompilationItem.BundleInfo(
            name: name,
            baseDir: baseDir,
            relativeProjectPathPrefix: relativeProjectPathPrefix,
            disableHotReload: true,
            disableAnnotationProcessing: true,
            disableDependencyVerification: true,
            disableBazelBuildFileGeneration: true,
            asyncStrictMode: false,
            webNpmScope: "",
            webVersion: "",
            webPublishConfig: "",
            webMain: "",
            iosModuleName: name,
            iosLanguage: .swift,
            iosClassPrefix: nil,
            androidClassPath: nil,
            androidExportStrings: false,
            cppClassPrefix: nil,
            iosCodegenEnabled: false,
            androidCodegenEnabled: false,
            cppCodegenEnabled: false,
            disableCodeCoverage: true,
            singleFileCodegen: false,
            compilationModeConfig: .forJs(),
            compilationModeExplicit: false,
            iosOutputTarget: nil,
            iosGeneratedContextFactories: [],
            androidOutputTarget: nil,
            webOutputTarget: nil,
            cppOutputTarget: nil,
            downloadableAssets: false,
            downloadableSources: false,
            inclusionConfig: .alwaysIncluded,
            excludeGlobs: [],
            dependencies: [],
            allowedDebugDependencies: [],
            stringsConfig: nil,
            projectConfig: projectConfig,
            outputTarget: .all
        )
    }

    func testBaseBundleWithEmptyPrefixUsesExplicitRelativeProjectPath() throws {
        let treeA = URL(fileURLWithPath: "/tmp/composer-6492/resolved/modules")
        let treeB = URL(fileURLWithPath: "/tmp/composer-6492/execroot/external/valdi/modules")
        let bundle = try makeBundle(name: "__valdi_base__", baseDir: treeA, relativeProjectPathPrefix: "")

        let url = bundle.relativeBundleURL(forRelativeProjectPath: "tsconfig.json",
                                           sourceURL: treeB.appendingPathComponent("tsconfig.json"))

        XCTAssertEqual(url.relativeString, "tsconfig.json")
    }

    func testDefaultModulePrefixStripsModuleName() throws {
        let baseDir = URL(fileURLWithPath: "/tmp/composer-6492/project/mod")
        let bundle = try makeBundle(name: "mod", baseDir: baseDir, relativeProjectPathPrefix: "mod/")

        let url = bundle.relativeBundleURL(forRelativeProjectPath: "mod/src/Foo.ts",
                                           sourceURL: baseDir.appendingPathComponent("src/Foo.ts"))

        XCTAssertEqual(url.relativeString, "src/Foo.ts")
    }

    func testPrefixMismatchFallsBackToBaseDirRelativePath() throws {
        let baseDir = URL(fileURLWithPath: "/tmp/composer-6492/a/mod")
        let sourceURL = URL(fileURLWithPath: "/tmp/composer-6492/b/other/Foo.ts")
        let bundle = try makeBundle(name: "mod", baseDir: baseDir, relativeProjectPathPrefix: "mod/")

        let url = bundle.relativeBundleURL(forRelativeProjectPath: "other/Foo.ts", sourceURL: sourceURL)

        XCTAssertEqual(url.relativeString, baseDir.relativePath(to: sourceURL))
    }
}
