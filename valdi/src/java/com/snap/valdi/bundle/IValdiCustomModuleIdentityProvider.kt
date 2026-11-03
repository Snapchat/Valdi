package com.snap.valdi.bundle

/**
 * Optionally implemented by an [IValdiCustomModuleProvider] that can identify the module data it would
 * return without loading it. The runtime can then serve modules from its module archive cache without
 * reading them first. Providers that don't implement it have their module data read on every load.
 */
interface IValdiCustomModuleIdentityProvider {

    /**
     * Returns a non-empty identity of the data [IValdiCustomModuleProvider.getCustomModuleData] would
     * return for the given module path, or null if it would return null and the built-in module is used.
     * The identity must change whenever that data changes.
     */
    fun getCustomModuleIdentity(path: String): String?

}
