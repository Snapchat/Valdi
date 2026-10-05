import XCTest
@testable import Compiler

final class ResourceDeliveryPlannerTests: XCTestCase {

    private struct TestResource: Equatable {
        let name: String
        let platform: Platform?
    }

    private let iosResource = TestResource(name: "ios.js", platform: .ios)
    private let androidResource = TestResource(name: "android.js", platform: .android)
    private let agnosticResource = TestResource(name: "agnostic.json", platform: nil)

    private var mixedResources: [TestResource] {
        return [iosResource, androidResource, agnosticResource]
    }

    private func client(id: Int, isReady: Bool = true, hotReloadDisabled: Bool = false, platform: Platform?) -> ClientDeliveryInfo {
        return ClientDeliveryInfo(id: id, isReady: isReady, hotReloadDisabled: hotReloadDisabled, platform: platform)
    }

    private func plan(_ resources: [TestResource], _ clients: [ClientDeliveryInfo]) -> ResourceDeliveryPlanner.Plan<TestResource> {
        return ResourceDeliveryPlanner.plan(resources: resources, platform: \.platform, clients: clients)
    }

    func testNoReadyClients() {
        let result = plan(mixedResources, [
            client(id: 1, isReady: false, platform: .ios),
            client(id: 2, isReady: false, platform: nil),
        ])

        XCTAssertEqual(result.readyClientCount, 0)
        XCTAssertTrue(result.deliveries.isEmpty)
    }

    func testReadyIOSClientExcludesAndroidResources() {
        let result = plan(mixedResources, [client(id: 1, platform: .ios)])

        XCTAssertEqual(result.readyClientCount, 1)
        XCTAssertEqual(result.deliveries.count, 1)
        XCTAssertEqual(result.deliveries.first?.clientId, 1)
        // A client with a platform doesn't receive platform-agnostic resources.
        XCTAssertEqual(result.deliveries.first?.resources, [iosResource])
    }

    func testHotReloadDisabledClientIsExcludedButCountedAsReady() {
        let result = plan(mixedResources, [client(id: 1, hotReloadDisabled: true, platform: .ios)])

        XCTAssertEqual(result.readyClientCount, 1)
        XCTAssertTrue(result.deliveries.isEmpty)
    }

    func testEachReadyClientGetsItsPlatformSubset() {
        let result = plan(mixedResources, [
            client(id: 1, platform: .ios),
            client(id: 2, platform: .android),
        ])

        XCTAssertEqual(result.readyClientCount, 2)
        XCTAssertEqual(result.deliveries.map(\.clientId), [1, 2])
        XCTAssertEqual(result.deliveries[0].resources, [iosResource])
        XCTAssertEqual(result.deliveries[1].resources, [androidResource])
    }

    func testClientWithoutPlatformGetsAllResources() {
        let result = plan(mixedResources, [client(id: 1, platform: nil)])

        XCTAssertEqual(result.deliveries.count, 1)
        XCTAssertEqual(result.deliveries.first?.resources, mixedResources)
    }

    func testEmptyResourcesProducesNoDeliveries() {
        let result = plan([], [client(id: 1, platform: .ios)])

        XCTAssertEqual(result.readyClientCount, 1)
        XCTAssertTrue(result.deliveries.isEmpty)
    }

    func testClientWithNoMatchingResourcesIsSkipped() {
        let result = plan([androidResource], [
            client(id: 1, platform: .ios),
            client(id: 2, platform: .android),
        ])

        XCTAssertEqual(result.readyClientCount, 2)
        XCTAssertEqual(result.deliveries.map(\.clientId), [2])
    }

    func testUnreadyClientsAreIgnoredAmongReadyOnes() {
        let result = plan(mixedResources, [
            client(id: 1, isReady: false, platform: .android),
            client(id: 2, platform: .ios),
        ])

        XCTAssertEqual(result.readyClientCount, 1)
        XCTAssertEqual(result.deliveries.map(\.clientId), [2])
    }
}
