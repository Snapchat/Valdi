//
//  ResourceDeliveryPlanner.swift
//  Compiler
//
//  Copyright © 2026 Snap Inc. All rights reserved.
//

import Foundation

/// Snapshot of the connected-client state that decides hot-reload delivery.
struct ClientDeliveryInfo {
    let id: Int
    let isReady: Bool
    let hotReloadDisabled: Bool
    let platform: Platform?
}

/// Decides which updated resources each connected hot-reload client receives.
enum ResourceDeliveryPlanner {

    struct Delivery<R> {
        let clientId: Int
        let resources: [R]
    }

    struct Plan<R> {
        let deliveries: [Delivery<R>]
        /// Ready clients, including those with hot reload disabled.
        let readyClientCount: Int
    }

    /// A client that hasn't reported a platform receives every resource.
    static func shouldDeliver(resourcePlatform: Platform?, toClientPlatform clientPlatform: Platform?) -> Bool {
        guard let clientPlatform else {
            return true
        }
        return clientPlatform == resourcePlatform
    }

    static func resources<R>(_ resources: [R], platform: KeyPath<R, Platform?>, forClientPlatform clientPlatform: Platform?) -> [R] {
        return resources.filter { shouldDeliver(resourcePlatform: $0[keyPath: platform], toClientPlatform: clientPlatform) }
    }

    static func plan<R>(resources: [R], platform: KeyPath<R, Platform?>, clients: [ClientDeliveryInfo]) -> Plan<R> {
        let readyClients = clients.filter(\.isReady)
        let deliveries = readyClients
            .filter { !$0.hotReloadDisabled }
            .compactMap { client -> Delivery<R>? in
                let clientResources = Self.resources(resources, platform: platform, forClientPlatform: client.platform)
                guard !clientResources.isEmpty else {
                    return nil
                }
                return Delivery(clientId: client.id, resources: clientResources)
            }
        return Plan(deliveries: deliveries, readyClientCount: readyClients.count)
    }

    static func resources(_ resources: [Resource], forClientPlatform clientPlatform: Platform?) -> [Resource] {
        return self.resources(resources, platform: \.finalFile.platform, forClientPlatform: clientPlatform)
    }

    static func plan(resources: [Resource], clients: [ClientDeliveryInfo]) -> Plan<Resource> {
        return plan(resources: resources, platform: \.finalFile.platform, clients: clients)
    }
}

extension DaemonServiceConnectedClient {
    var deliveryInfo: ClientDeliveryInfo {
        return ClientDeliveryInfo(id: id, isReady: clientIsReady, hotReloadDisabled: hotReloadDisabled, platform: platform)
    }
}
