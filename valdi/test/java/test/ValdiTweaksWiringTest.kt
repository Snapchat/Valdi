package com.snap.valdi.test

import com.snap.valdi.ValdiRuntimeManager
import com.snap.valdi.ValdiTweaks
import java.io.File
import java.util.jar.JarFile
import java.util.zip.ZipEntry
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Guards against a [ValdiTweaks] field being silently dropped on its way to the code that consumes
 * it.
 *
 * PR #107's reland dropped two of them this way — `editTextResetSelectionMatchesIos` was replaced
 * with a literal `false` at the `EditTextAttributesBinder` call site, and `useLocaleLanguageTag` was
 * omitted from the `ValdiDeviceModule` constructor so it fell back to its default. Both compiled,
 * both left a COF-gated feature permanently off, and no test noticed: the tweaks are plumbing, and
 * the tests that touch the affected constructors pass their own literals rather than asserting the
 * wiring.
 *
 * The check is deliberately structural rather than behavioral. Asserting real behavior would mean
 * constructing [ValdiRuntimeManager], which builds native handles and cannot run in a JVM test. What
 * a dropped tweak always looks like in bytecode is the absence of its getter, so that is what this
 * asserts.
 *
 * Consumers are discovered from the classpath rather than hardcoded, for the same reason
 * `SnapTextViewBindingCoverageTest.everyHostAppValdiTextViewIsListed` discovers its domain: a
 * hardcoded "add new consumers here" list is the thing that rots.
 */
class ValdiTweaksWiringTest {

    /**
     * Tweaks this test knowingly does not require a consumer for. Each entry needs a reason — an
     * unexplained entry is the same silent drop this test exists to catch.
     *
     * The three below are dead on arrival and predate this change: repo-wide, each appears only in
     * its own declaration in `ValdiTweaks`. Nothing under `client/` or `android/` reads them, and
     * nothing sets them either, so they are not a regression to fix here — but they should be deleted
     * or wired up rather than left to accumulate.
     */
    private val knownUnconsumed = mapOf(
        "maxJsStackSize" to "declared only; no reader or writer anywhere in client/ or android/",
        "maxJsStackSizePercentToNative" to "declared only; no reader or writer anywhere in client/ or android/",
        "enableSkia" to "declared only; no reader or writer anywhere in client/ or android/",
    )

    @Test
    fun everyTweakIsReadBySomeConsumer() {
        val tweakNames = ValdiTweaks::class.java.declaredFields
            .filterNot { it.isSynthetic }
            .map { it.name }
            .filterNot { it == "Companion" || it.startsWith("\$") }

        assertTrue(
            "Expected to discover ValdiTweaks fields via reflection, found none. " +
                "If ValdiTweaks stopped being a data class, this test needs updating rather than deleting.",
            tweakNames.size > 5,
        )

        val classBytes = valdiClassBytes()
        assertTrue(
            "Expected to read com.snap.valdi class bytes off the classpath, found none.",
            classBytes.isNotEmpty(),
        )

        val unread = tweakNames.filter { name ->
            val accessors = accessorNamesFor(name)
            classBytes.none { bytes -> accessors.any { bytes.containsAscii(it) } }
        }

        val unexplained = unread.filterNot { knownUnconsumed.containsKey(it) }

        assertTrue(
            buildString {
                append("These ValdiTweaks are never read by any com.snap.valdi class, so setting ")
                append("them has no effect:\n")
                unexplained.forEach { append("  - ").append(it).append('\n') }
                append("\nEither wire the tweak up at its consumer, or add it to knownUnconsumed ")
                append("with a reason.")
            },
            unexplained.isEmpty(),
        )

        val staleWaivers = knownUnconsumed.keys.filter { it !in unread }
        assertTrue(
            "These knownUnconsumed entries now have a consumer and should be removed from the " +
                "waiver list: $staleWaivers",
            staleWaivers.isEmpty(),
        )
    }

    /** Kotlin emits `val foo` as `getFoo()`, and `val isFoo` as `isFoo()`. */
    private fun accessorNamesFor(propertyName: String): List<String> =
        if (propertyName.startsWith("is") && propertyName.length > 2 && propertyName[2].isUpperCase()) {
            listOf(propertyName)
        } else {
            listOf("get" + propertyName.replaceFirstChar { it.uppercaseChar() })
        }

    /**
     * Bytes of every compiled class under the `com.snap.valdi` package on the classpath. Reads
     * whichever container [ValdiRuntimeManager] came from, so it follows the build rather than
     * assuming a jar layout.
     */
    private fun valdiClassBytes(): List<ByteArray> {
        val location = ValdiRuntimeManager::class.java.protectionDomain?.codeSource?.location
            ?: return emptyList()
        val file = File(location.toURI())

        return if (file.isDirectory) {
            file.walkTopDown()
                .filter { it.isFile && it.extension == "class" }
                .filter { it.path.contains("com/snap/valdi") }
                .filterNot { isDeclaringClass(it.name) }
                .map { it.readBytes() }
                .toList()
        } else {
            JarFile(file).use { jar ->
                jar.entries().asSequence()
                    .filter { entry: ZipEntry ->
                        entry.name.startsWith("com/snap/valdi/") && entry.name.endsWith(".class")
                    }
                    .filterNot { entry -> isDeclaringClass(entry.name.substringAfterLast('/')) }
                    .map { entry -> jar.getInputStream(entry).use { it.readBytes() } }
                    .toList()
            }
        }
    }

    /**
     * [ValdiTweaks] itself must be excluded: as a data class it *declares* every getter, so its own
     * class file contains all of their names and would satisfy this check unconditionally. Leaving it
     * in makes the whole test vacuous — verified by reintroducing both dropped tweaks and watching it
     * still pass.
     */
    private fun isDeclaringClass(simpleName: String): Boolean =
        simpleName == "ValdiTweaks.class" || simpleName.startsWith("ValdiTweaks\$")

    private fun ByteArray.containsAscii(needle: String): Boolean {
        val target = needle.toByteArray(Charsets.US_ASCII)
        if (target.isEmpty() || target.size > size) return false
        outer@ for (start in 0..(size - target.size)) {
            for (i in target.indices) {
                if (this[start + i] != target[i]) continue@outer
            }
            return true
        }
        return false
    }
}
