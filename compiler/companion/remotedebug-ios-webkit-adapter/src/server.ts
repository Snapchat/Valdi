//
// Copyright (C) Microsoft. All rights reserved.
//

import * as http from 'http';
import * as express from 'express';
import * as ws from 'ws';
import { Server as WebSocketServer } from 'ws';
import { EventEmitter } from 'events';
import { Logger, debug } from './logger';

import { Adapter } from './adapters/adapter';
import { IOSAdapter } from './adapters/iosAdapter';
import { IIOSProxySettings } from './adapters/adapterInterfaces';
import { AddressInfo } from 'net';
import { AdapterCollection } from './adapters/adapterCollection';
import { UniversalAdapter } from './adapters/universalAdapter';
import { AndroidAdapter, AndroidDevice } from './adapters/androidAdapter';
import { IOSSimulatorSocketFinder } from './iosSimulatorSocketFinder';
import { HermesAdapter, HermesDevice } from './adapters/hermesAdapter';
// import { TestAdapter } from './adapters/testAdapter';

export class ProxyServer extends EventEmitter {
    private _hs?: http.Server;
    private _es?: express.Application;
    private _wss?: WebSocketServer;
    private _serverPort?: number;
    private _ios?: IOSAdapter;
    private _android?: AndroidAdapter;
    private _hermes?: HermesAdapter;
    private _adapter?: UniversalAdapter;
    private _targetFetcherInterval?: ReturnType<typeof setInterval>;
    private _simulatorSocketFinder = new IOSSimulatorSocketFinder();

    private _injectedTargets = new Array();

    constructor() {
        super();
    }

    public updateKnownHermesDevices(devices: HermesDevice[]) {
        this._hermes?.updateKnownDevices(devices);
    }

    public updateKnownAndroidDevices(devices: AndroidDevice[]) {
        this._android?.updateKnownDevices(devices);
    }

    public async run(serverPort: number): Promise<number> {
        this._serverPort = serverPort;

        debug('server.run, port=%s', serverPort);

        this._simulatorSocketFinder.start();

        this._es = express();
        this._hs = http.createServer(this._es);
        this._wss = new WebSocketServer({
            server: this._hs,
        });
        this._wss.on('connection', (a, req) => {
            this.onWSSConnection(a, req);
        });

        this.setupHttpHandlers();

        // Start server and return the port number
        this._hs.listen(this._serverPort);
        const port = (<AddressInfo>this._hs.address()).port;

        if (process.platform !== "linux") {
            const settings = await IOSAdapter.getProxySettings({
                proxyPort: port + 100,
                simulatorSocketFinder: this._simulatorSocketFinder,
            });
            this._ios = new IOSAdapter(`/ios`, `ws://localhost:${port}`, settings);
        }
        this._simulatorSocketFinder.observeSockets(simulatorSockets => {
            this._ios?.forceRefresh();
        });
        this._android = new AndroidAdapter(`/android`, `ws://localhost:${port}`);
        this._hermes = new HermesAdapter(`/hermes`, `ws://localhost:${port}`)

        const allAdapters: Adapter<any>[] = [];
        if (this._ios){
            allAdapters.push(this._ios);
        }
        allAdapters.push(this._hermes);
        allAdapters.push(this._android);
        this._adapter = new UniversalAdapter(allAdapters);

        return this._adapter
            .start()
            .then(() => {
                this.startTargetFetcher();
            })
            .then(() => {
                return port;
            });
    }

    public stop(): void {
        debug('server.stop');
        this._simulatorSocketFinder.stop();

        if (this._hs) {
            this._hs.close();
            this._hs = undefined;
        }

        this.stopTargetFetcher();
        this._adapter?.stop();
    }

    private startTargetFetcher(): void {
        debug('server.startTargetFetcher');

        let fetch = () => {
            this._adapter?.getTargets().then(
                targets => {
                    debug(`server.startTargetFetcher.fetched.${targets.length}`);
                },
                err => {
                    debug(`server.startTargetFetcher.error`, err);
                },
            );
        };

        this._targetFetcherInterval = setInterval(fetch, 5000);
    }

    private stopTargetFetcher(): void {
        debug('server.stopTargetFetcher');
        if (!this._targetFetcherInterval) {
            return;
        }
        clearInterval(this._targetFetcherInterval);
    }

    private setupHttpHandlers(): void {
        debug('server.setupHttpHandlers');

        this._es?.get('/', (req, res) => {
            debug('server.http.endpoint/');
            res.json({
                msg: 'Hello from RemoteDebug iOS WebKit Adapter',
            });
        });

        this._es?.get('/refresh', (req, res) => {
            this._adapter?.forceRefresh();
            this.emit('forceRefresh');
            res.json({
                status: 'ok',
            });
        });

        this._es?.get('/json', (req, res) => {
            debug('server.http.endpoint/json');
            this._adapter?.getTargets().then(targets => {
                res.json(targets);
            });
        });

        this._es?.get('/json/list', (req, res) => {
            debug('server.http.endpoint/json/list');
            this._adapter?.getTargets().then(targets => {
                res.json(targets);
            });
        });

        this._es?.get('/json/version', (req, res) => {
            debug('server.http.endpoint/json/version');
            res.json({
                Browser: 'Safari/RemoteDebug iOS Webkit Adapter',
                'Protocol-Version': '1.2',
                'User-Agent':
                    'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_12_0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/57.0.2926.0 Safari/537.36',
                'WebKit-Version': '537.36 (@da59d418f54604ba2451cd0ef3a9cd42c05ca530)',
            });
        });

        this._es?.get('/json/protocol', (req, res) => {
            debug('server.http.endpoint/json/protocol');
            res.json();
        });
    }

    private onWSSConnection(websocket: ws, req: http.IncomingMessage): void {
        const url = req.url || '';

        debug('server.ws.onWSSConnection', url);

        let connection = <EventEmitter>websocket;

        try {
            this._adapter?.on('socketClosed', id => {
                websocket.close();
            });
            this._adapter?.connectTo(url, websocket);
        } catch (err) {
            debug(`server.onWSSConnection.connectTo.error.${err}`);
        }

        connection.on('close', () => {
            this._adapter?.closeConnectionTo(url);
        });
        connection.on('message', msg => {
            this._adapter?.forwardTo(url, msg);
        });
    }
}
