package com.snap.valdi.bundle

import android.content.Context
import android.graphics.Bitmap
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.imageloading.ValdiImageLoaderPostprocessor
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.BitmapPool
import com.snap.valdi.utils.CoordinateResolver
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
class ResourceResolverModuleIdentityTest {
    private object NoopLogger : Logger {
        override fun log(level: Int, message: String?) = Unit
        override fun log(level: Int, err: Throwable?, message: String?) = Unit
    }

    private open class DataOnlyProvider : IValdiCustomModuleProvider {
        override fun getCustomModuleData(path: String): ByteArray? = null
    }

    private class IdentityProvider(private val identity: () -> String?) : DataOnlyProvider(),
        IValdiCustomModuleIdentityProvider {
        override fun getCustomModuleIdentity(path: String): String? = identity()
    }

    private fun resolver(provider: IValdiCustomModuleProvider?): ResourceResolver {
        val context = getApplicationContext<Context>()
        val postprocessor = ValdiImageLoaderPostprocessor(
            context, BitmapPool(context, Bitmap.Config.ARGB_8888, NoopLogger))
        return ResourceResolver(context, CoordinateResolver(1f), postprocessor, NoopLogger, provider)
    }

    @Test
    fun providerIdentityIsPassedThrough() {
        assertEquals("container", resolver(IdentityProvider { "container" }).getCustomModuleIdentity("m.valdimodule"))
    }

    @Test
    fun bundledModuleIsAnsweredWithAnEmptyIdentity() {
        assertEquals("", resolver(IdentityProvider { null }).getCustomModuleIdentity("m.valdimodule"))
    }

    @Test
    fun providerWithoutIdentityCannotTell() {
        assertNull(resolver(DataOnlyProvider()).getCustomModuleIdentity("m.valdimodule"))
    }

    @Test
    fun noProviderCannotTell() {
        assertNull(resolver(null).getCustomModuleIdentity("m.valdimodule"))
    }

    @Test
    fun failingProviderCannotTell() {
        val provider = IdentityProvider { throw IllegalStateException("container not loaded") }
        assertNull(resolver(provider).getCustomModuleIdentity("m.valdimodule"))
    }
}
