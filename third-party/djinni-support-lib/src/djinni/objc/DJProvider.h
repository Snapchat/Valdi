/**
 * Copyright 2025 Snap, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#import <Foundation/Foundation.h>

/**
 * DJProvider represents a provider of a value - computed when get is called.
 *
 * This is used for deferred evaluation, allowing expensive computations or object
 * creations to be deferred until actually needed. Memoization is optional (caller's choice).
 *
 * Analogous to Java's javax.inject.Provider<T> interface.
 *
 * Example:
 *   DJProvider<NSString *> *providerValue = [DJProvider providerWithBlock:^NSString *() {
 *       return @"computed value";
 *   }];
 *   NSString *result = [providerValue get];  // Compute and get the value
 */
@interface DJProvider<__covariant Value> : NSObject

- (nonnull instancetype)init NS_UNAVAILABLE;

/**
 * Creates a provider from a block.
 * The block will be called when `get` is invoked.
 */
+ (nonnull instancetype)providerWithBlock:(Value _Nonnull (^_Nonnull)(void))block;

/**
 * Initializes a provider with a block.
 * The block will be called when `get` is invoked.
 */
- (nonnull instancetype)initWithBlock:(Value _Nonnull (^_Nonnull)(void))block NS_DESIGNATED_INITIALIZER;

/**
 * Gets the value by executing the provider.
 * The block is executed each time this is called (no memoization by default).
 * Analogous to Java's Provider.get() method.
 */
- (Value _Nonnull)get;

@end
