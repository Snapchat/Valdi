package com.snap.valdi.network

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.snapchat.client.valdi_core.Cancelable
import com.snapchat.client.valdi_core.HTTPRequest
import com.snapchat.client.valdi_core.HTTPRequestManagerCompletion
import com.snapchat.client.valdi_core.HTTPResponse
import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.Closeable
import java.io.InputStream
import java.io.OutputStream
import java.net.HttpURLConnection
import java.net.InetAddress
import java.net.ServerSocket
import java.net.Socket
import java.net.URL
import java.util.Collections
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

private const val OK_RESPONSE = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok"

private fun await(timeoutMs: Long, condition: () -> Boolean): Boolean {
    val deadline = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(timeoutMs)
    while (System.nanoTime() < deadline) {
        if (condition()) {
            return true
        }
        Thread.sleep(25)
    }
    return condition()
}

/**
 * A loopback HTTP server with two modes. While holding, it accepts a connection, consumes the
 * request head and then leaves the socket open indefinitely, so the client stays blocked in read.
 * That is what lets a test observe whether a cancelled request actually hangs up.
 */
private class FakeServer(respondImmediately: Boolean) : Closeable {

    private val serverSocket = ServerSocket(0, 128, InetAddress.getLoopbackAddress())
    private val accepted = AtomicInteger(0)
    private val clients = Collections.synchronizedList(mutableListOf<Socket>())
    private val requestLines = Collections.synchronizedList(mutableListOf<String>())
    private val hungUp = AtomicInteger(0)

    @Volatile private var respond = respondImmediately
    @Volatile private var running = true
    @Volatile private var releasing = false

    init {
        Thread({ acceptLoop() }, "fake-server-accept").apply { isDaemon = true }.start()
    }

    fun url(path: String): String =
        "http://${serverSocket.inetAddress.hostAddress}:${serverSocket.localPort}$path"

    fun acceptedCount(): Int = accepted.get()

    fun sawRequestFor(path: String): Boolean =
        synchronized(requestLines) { requestLines.toList() }.any { it.contains(" $path ") }

    fun awaitAccepted(count: Int, timeoutMs: Long): Boolean = await(timeoutMs) { accepted.get() >= count }

    /**
     * Waits until the server has read [count] request heads. Stronger than [awaitAccepted]: the
     * kernel completes the TCP handshake off the listen backlog before the client has written
     * anything, so an accepted connection does not yet mean the client is awaiting a response.
     */
    fun awaitRequestsRead(count: Int, timeoutMs: Long): Boolean =
        await(timeoutMs) { synchronized(requestLines) { requestLines.size } >= count }

    fun hungUpCount(): Int = hungUp.get()

    fun awaitHungUp(count: Int, timeoutMs: Long): Boolean = await(timeoutMs) { hungUp.get() >= count }

    fun startResponding() {
        respond = true
    }

    /** Close every held connection so any blocked client unwinds. */
    fun releaseAll() {
        releasing = true
        synchronized(clients) { clients.toList() }.forEach { runCatching { it.close() } }
    }

    override fun close() {
        running = false
        releaseAll()
        runCatching { serverSocket.close() }
    }

    private fun acceptLoop() {
        while (running) {
            val socket = try {
                serverSocket.accept()
            } catch (exception: Exception) {
                return
            }
            clients.add(socket)
            accepted.incrementAndGet()
            Thread({ serve(socket) }, "fake-server-connection").apply { isDaemon = true }.start()
        }
    }

    private fun serve(socket: Socket) {
        try {
            val input = socket.getInputStream()
            readRequestLine(input)?.let { requestLines.add(it) }

            if (respond) {
                socket.getOutputStream().apply {
                    write(OK_RESPONSE.toByteArray())
                    flush()
                }
                socket.close()
                return
            }

            // Hold the connection open. read() returning -1, or throwing a reset, is the server
            // seeing the client hang up, which is what a real cancellation looks like from here.
            while (input.read() != -1) {
                // drain
            }
            if (!releasing) {
                hungUp.incrementAndGet()
            }
        } catch (exception: Exception) {
            if (!releasing) {
                hungUp.incrementAndGet()
            }
        }
    }

    private fun readRequestLine(input: InputStream): String? {
        val head = StringBuilder()
        var newlines = 0
        while (newlines < 2) {
            val byte = input.read()
            if (byte == -1) {
                return null
            }
            head.append(byte.toChar())
            when (byte) {
                '\n'.code -> newlines++
                '\r'.code -> Unit
                else -> newlines = 0
            }
        }
        return head.lineSequence().firstOrNull()
    }
}

/**
 * Parks the worker inside the header loop: past the point where the task has bound its connection,
 * but before anything has touched the network. That is the window a cancel has to be caught in,
 * because [HttpURLConnection.disconnect] cannot tear down a connection that has not connected yet.
 */
private class ParkingConnection(
    url: URL,
    private val failDisconnect: Boolean = false,
) : HttpURLConnection(url) {

    val reachedHeaders = CountDownLatch(1)
    val release = CountDownLatch(1)

    private val disconnects = AtomicInteger(0)
    private val sentBody = ByteArrayOutputStream()

    @Volatile var openedOutputStream = false
        private set

    /** cancel() and the worker's finally block both disconnect, so two means the worker unwound. */
    fun awaitUnwound(timeoutMs: Long): Boolean = await(timeoutMs) { disconnects.get() >= 2 }

    override fun setRequestProperty(key: String, value: String) {
        reachedHeaders.countDown()
        release.await()
        super.setRequestProperty(key, value)
    }

    override fun getOutputStream(): OutputStream {
        openedOutputStream = true
        return sentBody
    }

    override fun getInputStream(): InputStream = ByteArrayInputStream(ByteArray(0))

    override fun getResponseCode(): Int = 200

    override fun getHeaderFields(): Map<String, List<String>> = emptyMap()

    override fun connect() = Unit

    override fun disconnect() {
        disconnects.incrementAndGet()
        if (failDisconnect) {
            throw IllegalStateException("disconnect failed")
        }
    }

    override fun usingProxy(): Boolean = false
}

private class RecordingCompletion : HTTPRequestManagerCompletion() {

    private val latch = CountDownLatch(1)

    @Volatile var response: HTTPResponse? = null
        private set

    @Volatile var error: String? = null
        private set

    override fun onComplete(response: HTTPResponse) {
        this.response = response
        latch.countDown()
    }

    override fun onFail(error: String) {
        this.error = error
        latch.countDown()
    }

    fun awaitSettled(timeoutMs: Long): Boolean = latch.await(timeoutMs, TimeUnit.MILLISECONDS)

    fun hasSettled(): Boolean = latch.count == 0L

    fun describeOutcome(): String = response?.let { "onComplete(${it.statusCode})" } ?: "onFail($error)"
}

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
class DefaultHTTPRequestManagerTest {

    private val servers = mutableListOf<FakeServer>()
    private lateinit var manager: DefaultHTTPRequestManager

    @Before
    fun setUp() {
        manager = DefaultHTTPRequestManager(ApplicationProvider.getApplicationContext<Context>())
    }

    @After
    fun tearDown() {
        servers.forEach { it.close() }
    }

    @Test(timeout = 30_000)
    fun cancelDisconnectsInFlightRequest() {
        val server = holdingServer()
        val cancelable = perform(server.url("/hold"))

        assertTrue("the server never read the request", server.awaitRequestsRead(1, 5_000))

        cancelable.cancel()

        assertTrue(
            "cancel() left the connection open: the server never saw the client hang up",
            server.awaitHungUp(1, 2_000),
        )
    }

    @Test(timeout = 30_000)
    fun cancelReturnsCapacityToThePool() {
        val holding = holdingServer()
        val inFlight = (0 until DefaultHTTPRequestManager.MAX_CONCURRENT_REQUESTS).map {
            perform(holding.url("/hold"))
        }

        // Every worker has to be awaiting a response before cancelling. A connection that has not
        // started its call yet cannot be torn down, because openConnection() does not connect and
        // disconnect() is a no-op until it has.
        assertTrue(
            "the pool never filled",
            holding.awaitRequestsRead(DefaultHTTPRequestManager.MAX_CONCURRENT_REQUESTS, 5_000),
        )

        inFlight.forEach { it.cancel() }

        val expected = DefaultHTTPRequestManager.MAX_CONCURRENT_REQUESTS
        val allHungUp = holding.awaitHungUp(expected, 5_000)
        assertTrue(
            "cancel() tore down only ${holding.hungUpCount()} of $expected connections",
            allHungUp,
        )

        val responding = respondingServer()
        val completion = RecordingCompletion()
        manager.performRequest(getRequest(responding.url("/probe")), completion)
        val probeSettled = completion.awaitSettled(5_000)

        assertTrue(
            "every connection was torn down but the pool never took a new request",
            probeSettled,
        )
    }

    @Test(timeout = 30_000)
    fun cancelWhileQueuedNeitherConnectsNorCompletes() {
        val server = holdingServer()

        // Saturate the pool so the next request has to queue. Waiting for every worker to be
        // awaiting a response, rather than just for connections to be accepted, is what makes the
        // next request certain to still be queued when it is cancelled.
        repeat(DefaultHTTPRequestManager.MAX_CONCURRENT_REQUESTS) { perform(server.url("/hold")) }
        assertTrue(
            "the pool never filled",
            server.awaitRequestsRead(DefaultHTTPRequestManager.MAX_CONCURRENT_REQUESTS, 5_000),
        )

        val queued = RecordingCompletion()
        manager.performRequest(getRequest(server.url("/queued")), queued).cancel()

        // Drain the queue: unblock the workers and stop holding new connections.
        server.startResponding()
        server.releaseAll()

        // The queue is FIFO, so a probe submitted after the cancelled request cannot reach a
        // worker before it. A settled probe means the cancelled request has had its turn.
        val probe = RecordingCompletion()
        manager.performRequest(getRequest(respondingServer().url("/probe")), probe)
        assertTrue("the pool never drained", probe.awaitSettled(5_000))

        assertFalse(
            "a request cancelled while queued still opened a connection",
            server.sawRequestFor("/queued"),
        )
        assertFalse(
            "a request cancelled while queued reported ${queued.describeOutcome()}",
            queued.hasSettled(),
        )
    }

    @Test(timeout = 30_000)
    fun concurrentRequestsAreNotSerialised() {
        val server = holdingServer()
        val expected = DefaultHTTPRequestManager.MAX_CONCURRENT_REQUESTS

        repeat(expected) { perform(server.url("/hold")) }

        val reachedAll = server.awaitAccepted(expected, 5_000)

        assertTrue(
            "expected $expected connections in flight, saw ${server.acceptedCount()}",
            reachedAll,
        )
    }

    @Test(timeout = 30_000)
    fun cancelSuppressesCompletion() {
        val server = holdingServer()
        val completion = RecordingCompletion()
        val cancelable = manager.performRequest(getRequest(server.url("/hold")), completion)

        assertTrue("the server never read the request", server.awaitRequestsRead(1, 5_000))

        cancelable.cancel()

        val settled = completion.awaitSettled(1_500)

        assertFalse("a cancelled request reported ${completion.describeOutcome()}", settled)
    }

    @Test(timeout = 30_000)
    fun cancelledPostNeverWritesItsBody() {
        val url = "http://example.invalid/post"
        val connection = ParkingConnection(URL(url))
        val postManager = DefaultHTTPRequestManager(
            ApplicationProvider.getApplicationContext<Context>(),
        ) { connection }

        val request = HTTPRequest(
            url,
            "POST",
            hashMapOf("Content-Type" to "application/json"),
            """{"cancelled":true}""".toByteArray(),
            0,
        )

        val cancelable = postManager.performRequest(request, RecordingCompletion())

        assertTrue(
            "the worker never reached header setup",
            connection.reachedHeaders.await(5, TimeUnit.SECONDS),
        )

        cancelable.cancel()
        connection.release.countDown()

        assertTrue("the worker never unwound", connection.awaitUnwound(5_000))
        assertFalse(
            "a POST cancelled before it connected still wrote its body",
            connection.openedOutputStream,
        )
    }

    @Test(timeout = 30_000)
    fun cancelSurvivesAThrowingDisconnect() {
        val url = "http://example.invalid/throwing"
        val connection = ParkingConnection(URL(url), failDisconnect = true)
        val throwingManager = DefaultHTTPRequestManager(
            ApplicationProvider.getApplicationContext<Context>(),
        ) { connection }

        val request = HTTPRequest(url, "GET", hashMapOf("Accept" to "*/*"), null, 0)
        val cancelable = throwingManager.performRequest(request, RecordingCompletion())

        assertTrue(
            "the worker never reached header setup",
            connection.reachedHeaders.await(5, TimeUnit.SECONDS),
        )

        // cancel() is reached from native through the djinni Cancelable bridge, so anything it
        // throws escapes into JNI rather than into a caller that can handle it.
        try {
            cancelable.cancel()
        } catch (exception: Exception) {
            connection.release.countDown()
            fail("cancel() let $exception escape to its caller")
        }

        connection.release.countDown()

        assertTrue("the worker never unwound", connection.awaitUnwound(5_000))
    }

    private fun holdingServer(): FakeServer = FakeServer(respondImmediately = false).also { servers.add(it) }

    private fun respondingServer(): FakeServer = FakeServer(respondImmediately = true).also { servers.add(it) }

    private fun perform(url: String): Cancelable =
        manager.performRequest(getRequest(url), RecordingCompletion())

    private fun getRequest(url: String): HTTPRequest =
        HTTPRequest(url, "GET", HashMap<String, String>(), null, 0)
}
