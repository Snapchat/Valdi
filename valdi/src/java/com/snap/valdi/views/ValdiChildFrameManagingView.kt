package com.snap.valdi.views

/**
 * Marker for Valdi views whose direct child frames are applied by the view
 * itself instead of the runtime's normal frame transaction.
 *
 * Labels and text views use this for inline children: Yoga still measures the
 * children, but text layout decides their positions.
 */
interface ValdiChildFrameManagingView
