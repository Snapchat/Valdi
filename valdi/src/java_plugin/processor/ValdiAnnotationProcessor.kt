package com.snap.valdi.annotation_processor

import javax.annotation.processing.*
import javax.lang.model.SourceVersion
import javax.lang.model.element.Element
import javax.lang.model.element.TypeElement
import javax.tools.Diagnostic
import javax.tools.JavaFileObject
import javax.tools.StandardLocation
import java.io.File
import java.io.FileWriter

@SupportedAnnotationTypes("com.snap.valdi.modules.RegisterValdiModule", "com.snap.valdi.attributes.RegisterAttributesBinder")
@SupportedSourceVersion(SourceVersion.RELEASE_17)
class ValdiAnnotationProcessor: AbstractProcessor() {

    override fun process(
        annotations: Set<TypeElement>,
        roundEnv: RoundEnvironment
    ): Boolean {
        for (anon in annotations) {
            val elements = roundEnv.getElementsAnnotatedWith(anon)

            if (anon.simpleName.contentEquals("RegisterValdiModule")) {
                generateManifest(elements, "valdi_native_modules")
            } else {
                generateManifest(elements, "valdi_attributes_binders")
            }
        }
        return true
    }

    private fun generateManifest(annotatedTypes: Set<Element>, targetDir: String) {
        val filer = processingEnv.filer
        for (element in annotatedTypes) {
            val className = (element as TypeElement).qualifiedName.toString()
            val resourceFile = filer.createResource(
                StandardLocation.CLASS_OUTPUT,
                "",
                "assets/${targetDir}/${className}"
            )
            resourceFile.openWriter().use { _ -> }
        }
    }
}
