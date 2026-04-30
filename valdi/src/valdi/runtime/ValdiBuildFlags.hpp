//
//  ValdiBuildFlags.hpp
//  valdi
//
//  Compile-time flags for optional Valdi behavior (e.g. detailed tracing).
//

#pragma once

// When 1, emit detailed per-update runTreeUpdates trace (component + trigger) and build trigger
// strings at schedule sites. When 0, single span per batch and skip trigger generation.
#ifndef VALDI_DEBUG_TREE_UPDATES
#if defined(DEBUG)
#define VALDI_DEBUG_TREE_UPDATES 1
#else
#define VALDI_DEBUG_TREE_UPDATES 0
#endif
#endif
