import XCTest
@testable import Compiler

final class HotReloadArgumentsTests: XCTestCase {
    func testCompilerArgumentsAcceptValidPort() throws {
        var arguments = try ValdiCompilerArguments.parse([
            "--monitor",
            "--usb",
            "--port",
            "13702",
        ])

        try arguments.validate()

        XCTAssertEqual(arguments.port, 13702)
    }

    func testCompilerArgumentsRejectInvalidPorts() throws {
        for port in ["0", "65536", "-1"] {
            XCTAssertThrowsError(try ValdiCompilerArguments.parse([
                "--monitor",
                "--usb",
                "--port",
                port,
            ]))
        }

        XCTAssertThrowsError(try ValdiCompilerArguments.parse([
            "--monitor",
            "--usb",
            "--port",
            "not-a-port",
        ]))
    }

    func testCompilerArgumentsAcceptHotreloadLifecycleEventsWithTarget() throws {
        var arguments = try ValdiCompilerArguments.parse([
            "--monitor",
            "--hotreload-json-events",
            "--hotreload-target",
            "//modules/example:example_hotreload",
        ])

        try arguments.validate()

        XCTAssertTrue(arguments.hotreloadJsonEvents)
        XCTAssertEqual(arguments.hotreloadTarget, "//modules/example:example_hotreload")
    }

    func testCompilerArgumentsRequireTargetForHotreloadLifecycleEvents() throws {
        XCTAssertThrowsError(try ValdiCompilerArguments.parse([
            "--monitor",
            "--hotreload-json-events",
        ])) { error in
            XCTAssertTrue(
                String(describing: error).contains(
                    "--hotreload-json-events requires --hotreload-target"))
        }
    }

    func testNoPortUSBPlanPreservesLegacyConnectors() {
        let plan = DaemonServiceConnectorPlan(reloadOverUSB: true, port: nil)

        XCTAssertTrue(plan.usbMuxEnabled)
        XCTAssertTrue(plan.adbEnabled)
        XCTAssertEqual(plan.simulatorPorts, [
            Ports.reloaderOverUSB,
            Ports.reloaderStandalone,
        ])
        XCTAssertNil(plan.tcpAcceptorPort)
    }

    func testCustomPortUSBPlanUsesOnlyRequestedLocalhostPort() {
        let plan = DaemonServiceConnectorPlan(reloadOverUSB: true, port: 13702)

        XCTAssertFalse(plan.usbMuxEnabled)
        XCTAssertFalse(plan.adbEnabled)
        XCTAssertEqual(plan.simulatorPorts, [13702])
        XCTAssertNil(plan.tcpAcceptorPort)
    }

    func testTCPAcceptorPlanUsesRequestedPortOrEphemeralDefault() {
        XCTAssertEqual(DaemonServiceConnectorPlan(reloadOverUSB: false, port: nil).tcpAcceptorPort, 0)
        XCTAssertEqual(DaemonServiceConnectorPlan(reloadOverUSB: false, port: 13702).tcpAcceptorPort, 13702)
    }
}
