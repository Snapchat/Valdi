import XCTest
import Foundation
@testable import Compiler

final class DaemonFramingTests: XCTestCase {

    private final class CapturingTCPConnection: TCPConnection {
        weak var delegate: TCPConnectionDelegate?
        var dataProcessor: TCPConnectionDataProcessor?
        let key = "test-connection"

        var onSend: ((Data) -> Void)?
        private(set) var sentPackets: [Data] = []

        func startListening() {}

        func send(data: Data) {
            sentPackets.append(data)
            onSend?(data)
        }

        func close() {}
    }

    private final class CapturingDaemonTCPConnectionDelegate: DaemonTCPConnectionDelegate {
        var receivedPackets: [Data] = []

        func connection(_ connection: DaemonTCPConnection, didReceiveData data: Data) {
            receivedPackets.append(data)
        }

        func connection(_ connection: DaemonTCPConnection, didDisconnectWithError error: Error?) {}
    }

    private final class NoopProtocolConnectionDelegate: ValdiDaemonProtocolConnectionDelegate {
        func connection(_ connection: ValdiDaemonProtocolConnection, didDisconnectWithError error: Error?) {}
        func connection(_ connection: ValdiDaemonProtocolConnection, didReceiveRequest request: DaemonClientRequest, responsePromise: Promise<DaemonServerResponse>) {}
        func connection(_ connection: ValdiDaemonProtocolConnection, didReceiveEvent event: DaemonClientEvent) {}
    }

    private let resources = [
        DaemonResource(bundle_name: "my_module", file_path_within_bundle: "src/Component.js", data_base64: nil, data_string: "module.exports = {};"),
        DaemonResource(bundle_name: "my_module", file_path_within_bundle: "res/image.png", data_base64: Data([0x89, 0x50, 0x4E, 0x47, 0x00, 0xFF]), data_string: nil),
    ]

    private func sendUpdatedResourcesEvent() throws -> (packet: Data, tcpConnection: CapturingTCPConnection, daemonConnection: DaemonTCPConnection) {
        let tcpConnection = CapturingTCPConnection()
        let daemonConnection = DaemonTCPConnection(connection: tcpConnection)
        let delegate = NoopProtocolConnectionDelegate()
        let protocolConnection = ValdiDaemonProtocolConnection(logger: NullLogger(), connection: daemonConnection, delegate: delegate)

        let sent = expectation(description: "packet sent")
        tcpConnection.onSend = { _ in sent.fulfill() }

        var event = DaemonServerEvent()
        event.updated_resources = DaemonUpdatedResourcesEvent(resources: resources)
        protocolConnection.send(event: event)

        wait(for: [sent], timeout: 5)
        withExtendedLifetime(delegate) {}
        let packet = try XCTUnwrap(tcpConnection.sentPackets.first)
        return (packet, tcpConnection, daemonConnection)
    }

    func testPacketHeaderIsMagicThenLittleEndianLength() throws {
        let (packet, _, _) = try sendUpdatedResourcesEvent()

        XCTAssertGreaterThan(packet.count, 8)
        XCTAssertEqual(Array(packet.prefix(4)), [0x33, 0xC6, 0x00, 0x01])
        let lengthBytes = Array(packet[4..<8])
        let length = UInt32(lengthBytes[0])
            | UInt32(lengthBytes[1]) << 8
            | UInt32(lengthBytes[2]) << 16
            | UInt32(lengthBytes[3]) << 24
        XCTAssertEqual(Int(length), packet.count - 8)
    }

    func testDataProcessorAcceptsSentPacket() throws {
        let (packet, tcpConnection, _) = try sendUpdatedResourcesEvent()
        let processor = try XCTUnwrap(tcpConnection.dataProcessor)

        guard case let .valid(dataLength) = processor.process(data: packet) else {
            return XCTFail("Expected the sent packet to be a valid frame")
        }
        XCTAssertEqual(dataLength, packet.count)

        guard case .notEnoughData = processor.process(data: packet.prefix(packet.count - 1)) else {
            return XCTFail("Expected a truncated packet to need more data")
        }
    }

    func testUpdatedResourcesRoundTrip() throws {
        let (packet, tcpConnection, daemonConnection) = try sendUpdatedResourcesEvent()
        let receiver = CapturingDaemonTCPConnectionDelegate()
        daemonConnection.delegate = receiver

        daemonConnection.connection(tcpConnection, didReceiveData: packet)

        let body = try XCTUnwrap(receiver.receivedPackets.first)
        let payload = try JSONDecoder().decode(DaemonServerPayload.self, from: body)
        let decoded = try XCTUnwrap(payload.event?.updated_resources?.resources)

        XCTAssertEqual(decoded.count, resources.count)
        for (actual, expected) in zip(decoded, resources) {
            XCTAssertEqual(actual.bundle_name, expected.bundle_name)
            XCTAssertEqual(actual.file_path_within_bundle, expected.file_path_within_bundle)
            XCTAssertEqual(actual.data_string, expected.data_string)
            XCTAssertEqual(actual.data_base64, expected.data_base64)
        }
    }
}
