//
//  DaemonService.swift
//  Compiler
//
//  Created by Simon Corsin on 2/15/19.
//  Copyright © 2019 Snap Inc. All rights reserved.
//

import Foundation
import Chalk

protocol DaemonServiceAutoConnectorDelegate: AnyObject {
    func autoConnector(_ autoConnector: DaemonServiceAutoConnector, newConnectionEstablished connection: DaemonTCPConnection, deviceId : String)
}

protocol DaemonServiceAutoConnector {
    var delegate: DaemonServiceAutoConnectorDelegate? { get set }
    // Throws if the tunnel can't be created. Returns nil if the tunnel is internally managed and doesn't need to be exposed.
    func createTunnel(logger: ILogger, fromLocalPort: Int, toDeviceId: String, devicePort: Int) throws -> TCPTunnel?
}

struct DaemonServiceConnectorPlan: Equatable {
    let usbMuxEnabled: Bool
    let adbEnabled: Bool
    let simulatorPorts: [Int]
    let tcpAcceptorPort: Int?

    init(reloadOverUSB: Bool, port: Int?) {
        if reloadOverUSB {
            if let port = port {
                usbMuxEnabled = false
                adbEnabled = false
                simulatorPorts = [port]
            } else {
                usbMuxEnabled = true
                adbEnabled = true
                simulatorPorts = [Ports.reloaderOverUSB, Ports.reloaderStandalone]
            }
            tcpAcceptorPort = nil
        } else {
            usbMuxEnabled = false
            adbEnabled = false
            simulatorPorts = []
            tcpAcceptorPort = port ?? 0
        }
    }
}

final class DaemonService {

    private let logger: ILogger
    private let queue = DispatchQueue(label: "com.snap.valdi.DaemonService.connections")
    private var connectedClients = [DaemonServiceConnectedClient]()

    private let fileManager: ValdiFileManager

    private let resourceStore: ResourceStore
    private let deviceWhitelist: ReloaderDeviceWhitelist
    private let logsOutputDirURL: URL
    private let deviceStorageURL: URL

    private let serviceAnnouncer: ReloaderServiceAnnouncer?
    private let connectionAcceptor: DaemonTCPConnectionAcceptor?

    private let usbMuxAutoConnector: DaemonServiceUSBMuxAutoConnector?
    private let simulatorAutoConnectors: [DaemonServiceSimulatorAutoConnector]
    private let adbAutoConnector: DaemonServiceADBAutoConnector?
    private var idSequence = 0
    private let companion: CompanionExecutable
    private let lifecycleReporter: HotReloadLifecycleReporter?

    init(logger: ILogger,
         fileManager: ValdiFileManager,
         userConfig: ValdiUserConfig,
         projectConfig: ValdiProjectConfig,
         compilerConfig: CompilerConfig,
         companion: CompanionExecutable,
         reloadOverUSB: Bool,
         port: Int?,
         lifecycleReporter: HotReloadLifecycleReporter?) throws {
        self.logger = logger
        self.fileManager = fileManager
        let deviceWhitelist = ReloaderDeviceWhitelist(deviceIds: Set(userConfig.deviceIds.map({ $0.uppercased() })),
                                                      usernames: Set(userConfig.usernames.map({ $0.uppercased() })),
                                                      ipAddresses: Set(userConfig.ipAddresses))
        self.deviceWhitelist = deviceWhitelist
        self.logsOutputDirURL = userConfig.logsOutputDir ?? URL.logsDirectoryURL
        self.deviceStorageURL = userConfig.deviceStorageDir ?? URL.deviceStorageDirectoryURL

        self.companion = companion
        self.lifecycleReporter = lifecycleReporter
        self.resourceStore = ResourceStore()

        let logsCleaner = LogsCleaner(logger: logger, logsDirectoryURL: logsOutputDirURL)
        logsCleaner.cleanLogsFilesIfNeeded()

        if !compilerConfig.disableDebuggingProxy {
            // It's fine to wait here, since DaemonService is only initialized when we're in hot reload mode and this is something
            // we're going to want to hear back about.
            do {
                let startDebuggingProxyResponse = try companion.startDebuggingProxy().waitForData()
                logger.info("Valdi debugging proxy is listening on port \(startDebuggingProxyResponse.actualPort)")
            } catch {
                logger.error("Failed to start debugging proxy")
            }
        }

        let connectorPlan = DaemonServiceConnectorPlan(reloadOverUSB: reloadOverUSB, port: port)
        if let tcpAcceptorPort = connectorPlan.tcpAcceptorPort {
            self.usbMuxAutoConnector = nil
            self.simulatorAutoConnectors = []
            adbAutoConnector = nil

            let connectionAcceptor = try DaemonTCPConnectionAcceptor(logger: logger, port: tcpAcceptorPort)
            self.connectionAcceptor = connectionAcceptor
            let serviceAnnouncer = try ReloaderServiceAnnouncer(logger: logger, deviceWhitelist: deviceWhitelist, projectConfig: projectConfig, servicePort: connectionAcceptor.listeningPort)
            self.serviceAnnouncer = serviceAnnouncer

            connectionAcceptor.delegate = self
            connectionAcceptor.startAccepting()
        } else {
            let usbMuxAutoConnector = connectorPlan.usbMuxEnabled ? DaemonServiceUSBMuxAutoConnector(logger: logger) : nil
            self.usbMuxAutoConnector = usbMuxAutoConnector

            self.simulatorAutoConnectors = connectorPlan.simulatorPorts.map {
                DaemonServiceSimulatorAutoConnector(logger: logger, port: $0)
            }
            let adbAutoConnector = connectorPlan.adbEnabled ? DaemonServiceADBAutoConnector(logger: logger, companion: companion) : nil
            self.adbAutoConnector = adbAutoConnector

            self.connectionAcceptor = nil
            self.serviceAnnouncer = nil

            if let usbMuxAutoConnector = usbMuxAutoConnector {
                usbMuxAutoConnector.delegate = self
                usbMuxAutoConnector.start()
            }

            for simulatorAutoConnector in simulatorAutoConnectors {
                simulatorAutoConnector.delegate = self
                simulatorAutoConnector.start()
            }

            if let adbAutoConnector = adbAutoConnector {
                adbAutoConnector.delegate = self
                adbAutoConnector.start()
            }
        }
    }

    func resourcesDidChange(resources: [Resource]) {
        for resource in resources {
            self.resourceStore.addResource(resource)
        }

        queue.async {
            let readyClients = self.connectedClients.filter(\.clientIsReady)
            guard !readyClients.isEmpty else {
                self.logger.info("[Hot reloader] \(resources.count) updated resources, but no connected clients.")
                return
            }
            self.logger.info("[Hot reloader] sending \(resources.count) updated resources to \(readyClients.count) connected client(s)...")
            for client in readyClients {
                self.send(resources: resources, to: client)
            }
        }
    }

    private func sendAllResourcesToNewlyConnected(client: DaemonServiceConnectedClient) {
        guard !client.hotReloadDisabled else { return }
        logger.info("[Hot reloader] Sending all resources to newly connected client \(client)")
        self.send(resources: resourceStore.allResources, to: client)
    }

    private func shouldClientReceiveResource(client: DaemonServiceConnectedClient, resource: Resource) -> Bool {
        guard let platform = client.platform else {
            return true
        }

        return platform == resource.finalFile.platform
    }

    /// Buffering a packet costs the device 2-3x its size in contiguous allocations
    /// (power-of-two growth plus a copy); a ~200 MB packet OOM'd in COMPOSER-6067.
    /// Resources above this are skipped with an error instead of breaking the session.
    private static let maxResourceBytesPerPacket = 384 * 1024 * 1024
    /// Batch resources into multiple packets so devices never have to buffer a
    /// whole resource store in one allocation (COMPOSER-6067).
    private static let targetPacketBytes = 32 * 1024 * 1024

    private func send(resources: [Resource], to client: DaemonServiceConnectedClient) {
        guard !resources.isEmpty && !client.hotReloadDisabled else {
            return
        }

        guard client.wasConfigured else {
            logger.error("Client \(client) was never configured, can't send the updated resources event!")
            return
        }

        var batch: [DaemonResource] = []
        var batchBytes = 0
        var resourceCount = 0

        func flushBatch() {
            guard !batch.isEmpty else { return }
            var event = DaemonServerEvent()
            event.updated_resources = DaemonUpdatedResourcesEvent(resources: batch)
            let updatedResources = batch.map { "\($0.bundle_name):\($0.file_path_within_bundle)" }
            logger.verbose("Sending updated resources event with \(String(describing: updatedResources)) to client \(client)")
            client.protocolConnection.send(event: event)
            batch = []
            batchBytes = 0
        }

        for resource in resources where shouldClientReceiveResource(client: client, resource: resource) {

            var dataString: String?
            var data: Data?
            if let asString = String(data: resource.data, encoding: .utf8) {
                dataString = asString
                data = nil
            } else {
                dataString = nil
                data = resource.data
            }

            // base64 inflates binary payloads by 4/3 on the wire
            let encodedBytes = data.map { $0.count * 4 / 3 } ?? dataString!.utf8.count
            if encodedBytes > Self.maxResourceBytesPerPacket {
                logger.error("[Hot reloader] NOT sending resource \(resource.item.bundleInfo.name):\(resource.filePathInBundle) to \(client): \(encodedBytes / 1024 / 1024) MB encoded exceeds the \(Self.maxResourceBytesPerPacket / 1024 / 1024) MB per-packet limit; the device can't reliably buffer it. Rebuild and reinstall the app to update this resource, or shrink it to make it hot reloadable.")
                continue
            }

            if batchBytes > 0 && batchBytes + encodedBytes > Self.targetPacketBytes {
                flushBatch()
            }

            batch.append(DaemonResource(bundle_name: resource.item.bundleInfo.name,
                                        file_path_within_bundle: resource.filePathInBundle,
                                        data_base64: data,
                                        data_string: dataString))
            batchBytes += encodedBytes
            resourceCount += 1
        }

        flushBatch()
        if resourceCount > 0 {
            lifecycleReporter?.resourcesSent(clientId: client.id,
                                             applicationId: client.applicationId,
                                             platform: client.platform?.rawValue,
                                             resourceCount: resourceCount)
        }
    }

    private func addClient(_ client: DaemonServiceConnectedClient) {
        self.connectedClients.append(client)
    }

    private func removeClient(_ client: DaemonServiceConnectedClient) {
        if let index = self.connectedClients.firstIndex(where: { $0 === client }) {
            client.logsWriter?.close()
            self.connectedClients.remove(at: index)
        }
    }

    private func retrieveClient(connection: ValdiDaemonProtocolConnection, completion: @escaping (DaemonServiceConnectedClient) -> Void) {
        queue.async {
            if let index = self.connectedClients.firstIndex(where: { $0.protocolConnection === connection }) {
                completion(self.connectedClients[index])
            }
        }
    }

    private func getPathFromJSONRequest(_ request: DaemonFileManagerRequestBody) throws -> URL {
        let path = request.path
        return deviceStorageURL.resolving(path: path)
    }

    private func writeFile(_ request: DaemonFileManagerRequestBody) throws -> DaemonFileManagerPayloadResponse {
        let filePath = try getPathFromJSONRequest(request)
        if let dataBase64 = request.data_base64 {
            try fileManager.save(data: dataBase64, to: filePath)
        } else if let dataString = request.data_string {
            try fileManager.save(data: try dataString.utf8Data(), to: filePath)
        }

        return DaemonFileManagerPayloadResponse()
    }

    private func deleteFile(_ request: DaemonFileManagerRequestBody) throws -> DaemonFileManagerPayloadResponse {
        let filePath = try getPathFromJSONRequest(request)
        try FileManager.default.removeItem(at: filePath)
        return DaemonFileManagerPayloadResponse()
    }

    private func readFile(_ request: DaemonFileManagerRequestBody) throws -> DaemonFileManagerPayloadResponse {
        let filePath = try getPathFromJSONRequest(request)
        let file = File.url(filePath)
        var filePayload = DaemonFileManagerPayloadResponse()

        switch request.output_format {
        case .base64:
            let data = try file.readData()
            let base64 = data.base64EncodedString()
            filePayload.data_base64 = base64
            break
        case .string:
            let dataString = try file.readString()
            filePayload.data_string = dataString
        case .none:
            logger.error("Unknown readFile request output format")
        }
        return filePayload
    }

    private func readDirectory(_ request: DaemonFileManagerRequestBody) throws -> DaemonFileManagerPayloadResponse {

        let filePath = try getPathFromJSONRequest(request)

        let keys = [URLResourceKey.creationDateKey, URLResourceKey.contentModificationDateKey, URLResourceKey.isDirectoryKey]
        let keysSet = Set(keys)

        let items = try FileManager.default.contentsOfDirectory(at: filePath, includingPropertiesForKeys: keys, options: [])

        let outputItems = try items.map { item throws -> DaemonFileManagerFileEntry in
            let filename = item.lastPathComponent
            let absolutePath = item.path
            let resourceValues = try item.resourceValues(forKeys: keysSet)

            let contentModificationDate = resourceValues.contentModificationDate?.timeIntervalSince1970
            let creationDate = resourceValues.creationDate?.timeIntervalSince1970
            let isDirectory = resourceValues.isDirectory ?? false

            return DaemonFileManagerFileEntry(filename: filename, absolute_path: absolutePath, type: isDirectory ? 2  : 1, modification_date: contentModificationDate?.milliseconds, creation_date: creationDate?.milliseconds)
        }
        var filePayload = DaemonFileManagerPayloadResponse()
        filePayload.entries = outputItems
        return filePayload
    }

    private func processRequest(request: DaemonClientRequest, response: Promise<DaemonServerResponse>, client: DaemonServiceConnectedClient) {
        if let req = request.configure {
            let wasConfigured = client.wasConfigured
            client.applicationId = req.application_id
            client.wasConfigured = true
            client.clientIsReady = true

            if let disableHotReload = req.disable_hot_reload {
                client.hotReloadDisabled = disableHotReload
            }

            let platform = req.platform
            switch platform {
            case "ios":
                client.platform = .ios
            case "android":
                client.platform = .android
            default:
                client.platform = nil
            }

            if let applicationId = client.applicationId {
                let logsURL = logsOutputDirURL.appendingPathComponent("\(platform)-\(applicationId).log", isDirectory: false)
                if logsURL != client.logsWriter?.outputURL {
                    client.logsWriter?.close()
                    do {
                        client.logsWriter = try LogsWriter(logger: logger, fileManager: fileManager, outputURL: logsURL)
                    } catch let error {
                        logger.error("Unable to start log writer: \(error.legibleLocalizedDescription)")
                    }
                }
            }
            var serverResponse = DaemonServerResponse()
            serverResponse.configure = DaemonClientConfigureResponse()

            response.resolve(data: serverResponse)

            if !wasConfigured {
                lifecycleReporter?.targetConnected(clientId: client.id,
                                                   applicationId: client.applicationId,
                                                   platform: platform)
                self.sendAllResourcesToNewlyConnected(client: client)
            }
        }

        if request.list_connected_clients != nil {
            var clients: [DaemonConnectedClient] = []

            for existingClient in self.connectedClients where existingClient !== client {
                let connectedClient = DaemonConnectedClient(client_id: Int32(existingClient.id), platform: existingClient.platform?.rawValue ?? "", application_id: existingClient.applicationId)

                clients.append(connectedClient)
            }
            var serverResponse = DaemonServerResponse()
            serverResponse.list_connected_clients = DaemonClientListConnectedClientsResponse(clients: clients)
            response.resolve(data: serverResponse)
        }

        if let req = request.forward_client_payload {
            var serverResponse = DaemonServerResponse()
            guard let recipientClient = self.connectedClients.first(where: { $0.id == (req.client_id) }) else {
                serverResponse.error = DaemonErrorResponse(error_message: "No client with id \(req.client_id)")
                response.resolve(data: serverResponse)
                return
            }
            var event = DaemonServerEvent()
            event.payload_from_client = DaemonPayloadFromClientEvent(sender_client_id: Int32(client.id), payload_base64: req.payload_base64, payload_string: req.payload_string)

            recipientClient.protocolConnection.send(event: event)

            serverResponse.forward_client_payload = DaemonClientForwardPayloadResponse()

            response.resolve(data: serverResponse)
        }

        if let requestEnvelope = request.file_manager {
            var serverResponse = DaemonServerResponse()
            do {
                let output: DaemonFileManagerPayloadResponse
                switch requestEnvelope.type {
                case .write_file:
                    output = try writeFile(requestEnvelope.body)
                case .read_file:
                    output = try readFile(requestEnvelope.body)
                case .read_directory:
                    output = try readDirectory(requestEnvelope.body)
                case .delete_file:
                    output = try deleteFile(requestEnvelope.body)
                }

                serverResponse.file_manager = output
                response.resolve(data: serverResponse)
            } catch let err {
                serverResponse.error = DaemonErrorResponse(error_message: err.legibleLocalizedDescription)
                response.resolve(data: serverResponse)
            }

        }

    }

    private func processEvent(event: DaemonClientEvent, client: DaemonServiceConnectedClient) {
        if let new_logs = event.new_logs {
            for log in new_logs {
                let logMessage = "[\(log.severity)] \(log.log)"
                client.logsWriter?.write(log: logMessage)
            }
        }

        if let debuggerInfo = event.js_debugger_info {
            processJsDebuggerInfoEvent(debuggerInfo: debuggerInfo, client: client)
        }
    }

    private func processJsDebuggerInfoEvent(debuggerInfo: DaemonJsDebuggerInfo, client: DaemonServiceConnectedClient) {
        guard debuggerInfo.port != 0 else {
            return
        }
        let devicePort = debuggerInfo.port
        let localPort = Ports.hermesDebugger
        do {
            if let tunnel = try client.autoConnector.createTunnel(logger: logger, fromLocalPort: localPort, toDeviceId: client.deviceId, devicePort: devicePort) {
                tunnel.delegate = self
                tunnel.start()
                client.tunnels.append(tunnel)
            }
        } catch let error {
            logger.error("Error creating tunnel for port \(devicePort) on \(client): \(error.legibleLocalizedDescription)")
            return
        }

        client.debuggerConnected = true
        logger.info("\("Debugger connected to client at:", color: .green, style: .bold)")
        for target in debuggerInfo.websocket_targets {
            if target == debuggerInfo.websocket_targets.first {
                logger.info("\("    ws://localhost:\(localPort)/\(target) (default)", color: .green, style: .bold)")
            } else {
                logger.info("\("    ws://localhost:\(localPort)/\(target)", color: .green, style: .bold)")
            }
        }
    }

    fileprivate func daemonServiceOnNewConnection(connection: ValdiDaemonProtocolConnection, autoConnector: DaemonServiceAutoConnector, deviceId: String) {
        queue.async {
            self.idSequence += 1
            let clientId = self.idSequence

            let client = DaemonServiceConnectedClient(
                id: clientId,
                deviceId: deviceId,
                protocolConnection: connection,
                autoConnector: autoConnector)
            self.connectedClients.append(client)
        }
    }

}

extension DaemonService: DaemonServiceAutoConnectorDelegate {
    func autoConnector(_ autoConnector: DaemonServiceAutoConnector, newConnectionEstablished connection: DaemonTCPConnection, deviceId: String) {
        let protocolConnection = ValdiDaemonProtocolConnection(logger: logger, connection: connection, delegate: self)
        daemonServiceOnNewConnection(connection: protocolConnection, autoConnector: autoConnector, deviceId: deviceId)
    }

}

extension DaemonService: ValdiDaemonProtocolConnectionDelegate {

    func connection(_ connection: ValdiDaemonProtocolConnection, didDisconnectWithError error: Error?) {
        retrieveClient(connection: connection) { (client) in
            // Sometimes (i.e. when using ADB) we may have an established connection, but it will fail as soon as we read from
            // it, since the stream is actually empty.
            // This condition prevents over
            if connection.isConfirmed {
                self.logger.info("Client \(client) disconnected with error: \(error?.legibleLocalizedDescription ?? "<none>") ")
            } else {
                self.logger.verbose("Client \(client) disconnected with error: \(error?.legibleLocalizedDescription ?? "<none>") ")
            }
            if client.debuggerConnected {
                client.debuggerConnected = false
                self.logger.info("\("Debugger disconnected from client", color: .red, style: .bold)")
                for tunnel in client.tunnels {
                    tunnel.stop()
                }
            }
            client.tunnels.removeAll()
            self.removeClient(client)
        }
    }

    func connection(_ connection: ValdiDaemonProtocolConnection, didReceiveRequest request: DaemonClientRequest, responsePromise: Promise<DaemonServerResponse>) {
        retrieveClient(connection: connection) { (client) in
            self.processRequest(request: request, response: responsePromise, client: client)
        }
    }

    func connection(_ connection: ValdiDaemonProtocolConnection, didReceiveEvent event: DaemonClientEvent) {
        retrieveClient(connection: connection) { (client) in
            self.processEvent(event: event, client: client)
        }
    }

}

extension DaemonService: TCPTunnelManagerDelegate {
    func tunnelConnectionCreated(_ tunnel: TCPTunnel, connection: TCPTunnelConnection, from: TCPConnection, to: TCPConnection) {
    }

    func tunnelConnectionDisconnected(_ tunnel: TCPTunnel, connection: TCPTunnelConnection) {
    }

    func tunnelConnectionError(_ tunnel: TCPTunnel, error: Error) {
        logger.error("Unexpected error with debugger tunnel: \(error). Reconnect device to renabled debugging")
        tunnel.stop()
    }
}
