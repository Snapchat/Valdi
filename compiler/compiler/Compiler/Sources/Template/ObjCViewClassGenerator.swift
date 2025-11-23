//
//  ObjCViewClassGenerator.swift
//  Compiler
//
//  Created by Simon Corsin on 5/17/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

import Foundation

class ObjCViewClassGenerator: LanguageSpecificViewClassGenerator {
    static let platform: Platform = .ios

    let className: String
    let componentPath: String

    private let iosType: IOSType
    private let bundleInfo: CompilationItem.BundleInfo
    private let header = CodeWriter()
    private let impl = CodeWriter()
    private let viewModelClass: IOSType?
    private let componentContextClass: IOSType?
    private let sourceFilename: GeneratedSourceFilename

    init(bundleInfo: CompilationItem.BundleInfo,
         iosType: IOSType,
         componentPath: String,
         sourceFilename: GeneratedSourceFilename,
         viewModelClass: IOSType?,
         componentContextClass: IOSType?) {
        self.bundleInfo = bundleInfo
        self.iosType = iosType
        self.className = iosType.name.replacingOccurrences(of: "-", with: "_")
        self.componentPath = componentPath
        self.sourceFilename = sourceFilename
        self.viewModelClass = viewModelClass
        self.componentContextClass = componentContextClass
    }

    func start() throws {
        // TODO(3521): update to valdi_core
        header.appendHeader("#import <UIKit/UIKit.h>\n")
        header.appendHeader("#import <valdi_core/SCValdiRootView.h>\n")
        let superclassName = "SCValdiRootView"

        impl.appendHeader("#import \(iosType.importHeaderStatement(kind: .withUtilities))\n")
        impl.appendHeader("#import <valdi_core/SCValdiRuntimeProtocol.h>\n")

        header.appendBody(FileHeaderCommentGenerator.generateComment(sourceFilename: sourceFilename))
        header.appendBody("\n")
        header.appendBody("@interface \(className): \(superclassName)\n\n")
        impl.appendBody("@implementation \(className)\n\n")

        impl.appendBody("""
            + (NSString *)componentPath
            {
            return @"\(componentPath)";
            }
            \n
            """)
    }

    func finish() {
        header.appendBody("\n@end\n")
        impl.appendBody("@end\n")

        impl.appendHeader("#import <valdi_core/SCValdiContextProtocol.h>\n")
    }

    func appendAccessor(className: String, nodeId: String) {
        header.appendBody("@property (readonly, nonatomic) \(className) * _Nullable \(nodeId.camelCased);\n")
        // TODO(3521): Update to SCValdiView
        if !className.hasPrefix("UI") && className != "SCValdiView" {
            header.appendHeader("@class \(className.replacingOccurences(of: "-", with: "_");\n")
        }

        impl.appendBody("""
            - (\(className) *)\(nodeId.camelCased)
            {
            return (\(className) *)[self.valdiContext viewForNodeId:@"\(nodeId)"];
            }
            \n
            """)
    }

    func appendConstructor() throws {
        var viewModelType = "NSDictionary<NSString *, id> *"
        if let viewModelClass = viewModelClass {
            header.appendHeader("#import \(viewModelClass.importHeaderStatement(kind: .withUtilities))\n")
            viewModelType = viewModelClass.typeDeclaration
        }

        var contextType = "NSDictionary<NSString *, id> *"
        if let contextClass = componentContextClass {
            header.appendHeader("#import \(contextClass.importHeaderStatement(kind: .withUtilities))\n")
            contextType = contextClass.typeDeclaration
        }

        header.appendBody("- (instancetype _Nonnull)initWithViewModel:(\(viewModelType) _Nullable)viewModel componentContext:(\(contextType) _Nullable)componentContext runtime:(id<SCValdiRuntimeProtocol> _Nonnull)runtime;\n\n")
        header.appendBody("@property (strong, nonatomic) \(viewModelType) _Nullable viewModel;\n")

        impl.appendBody("""
            - (instancetype)initWithViewModel:(\(viewModelType))viewModel componentContext:(\(contextType))componentContext runtime:(id<SCValdiRuntimeProtocol>)runtime
            {
                return [super initWithViewModelUntyped:viewModel componentContextUntyped:componentContext runtime:runtime];
            }
            \n
            """)

        impl.appendBody("""
            - (void)setViewModel:(\(viewModelType))viewModel
            {
                self.valdiContext.viewModel = viewModel;
            }

            - (\(viewModelType))viewModel
            {
                return self.valdiContext.viewModel;
            }
            \n
            """)
    }

    func appendNativeActions(nativeFunctionNames: [String]) {
        guard !nativeFunctionNames.isEmpty else {
            return
        }

        let protocolName = className + "ActionHandler"

        let methods: [String] = nativeFunctionNames.map {
            "-(void)\($0):(NSArray<id> * _Nonnull)parameters;"
        }

        let protocolDecl = """
        @protocol \(protocolName)

        \(methods.joined(separator: "\n"))

        @end
        \n
        """

        header.prependBody(protocolDecl)
        header.appendBody("\n@property (weak, nonatomic) id<\(protocolName)> _Nullable actionHandler;\n\n")

        impl.appendBody("""
            -(void)setActionHandler:(id<\(protocolName)>)actionHandler
            {
            self.valdiContext.actionHandler = actionHandler;
            }
            \n
            """)
        impl.appendBody("""
            - (id<\(protocolName)>)actionHandler
            {
            return self.valdiContext.actionHandler;
            }
            \n
            """)
    }

    func appendEmitActions(actions: [String]) {
        guard !actions.isEmpty else {
            return
        }

        header.appendHeader("#import <valdi_core/SCValdiActions.h>\n")

        for action in actions {
            let functionName = String(action.split(separator: "_").last!).pascalCased

            header.appendBody("-(void)emit\(functionName):(nonnull NSArray<id> *)parameters;\n")
            impl.appendBody("""
                -(void)emit\(functionName):(NSArray<id> *)parameters
                {
                [self.valdiContext performJsAction:@"\(action)" parameters:parameters];
                }
                \n
                """)
        }

        // Append empty line
        header.appendBody("\n")
    }

    func write() throws -> [NativeSource] {
        let headerData = try header.content.indented(indentPattern: "    ").utf8Data()
        let implData = try impl.content.indented(indentPattern: "    ").utf8Data()

        return [NativeSource(relativePath: nil,
                             filename: "\(className).h",
                             file: .data(headerData),
                             groupingIdentifier: "\(bundleInfo.iosModuleName).h",
                             groupingPriority: IOSType.viewTypeGroupingPriority
                            ),
                NativeSource(relativePath: nil,
                             filename: "\(className).m",
                             file: .data(implData),
                             groupingIdentifier: "\(bundleInfo.iosModuleName).m",
                             groupingPriority: IOSType.viewTypeGroupingPriority
                            )]
    }

}
