package com.snap.valdi.network

import android.content.Context
import androidx.annotation.VisibleForTesting
import java.io.IOException
import java.io.InputStream
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLConnection
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger
import com.snapchat.client.valdi.*
import com.snapchat.client.valdi_core.*

class DefaultHTTPRequestManager @VisibleForTesting constructor(
    context: Context,
    private val keepAliveMs: Long = DEFAULT_KEEP_ALIVE_MS,
    private val openConnection: (URL) -> URLConnection,
): HTTPRequestManager() {

    constructor(context: Context): this(context, openConnection = { it.openConnection() })

    private class RequestTask(val url: URL, val method: String, val body: ByteArray?, val headers: Map<String, String>, val openConnection: (URL) -> URLConnection, completion: HTTPRequestManagerCompletion): HTTPRequestTask(completion), Runnable {

        private var connection: HttpURLConnection? = null
        private var cancelled = false

        override fun cancel() {
            super.cancel()

            val connectionToClose = synchronized(this) {
                cancelled = true
                connection
            }

            // Closing from another thread makes the worker's blocked read throw, which is
            // how the thread gets reclaimed. Do it outside the lock so a slow close cannot stall
            // the task binding its connection. Swallow like the worker's own teardown does: this
            // races that teardown, and native is the only caller left to hand a throw to.
            try {
                connectionToClose?.disconnect()
            } catch (exc: Exception) {}
        }

        private fun isCancelled(): Boolean = synchronized(this) { cancelled }

        /**
         * Hands the connection to [cancel] so it can be torn down. Returns false when the request
         * was already cancelled, in which case the caller must not go on to perform it.
         */
        private fun bindConnection(urlConnection: HttpURLConnection): Boolean = synchronized(this) {
            if (cancelled) {
                false
            } else {
                connection = urlConnection
                true
            }
        }

        private fun doPerformRequestWithURLConnection(urlConnection: HttpURLConnection): HTTPResponse {
            urlConnection.instanceFollowRedirects = true

            try {
                urlConnection.requestMethod = method

                for (entry in headers.entries) {
                    urlConnection.setRequestProperty(entry.key, entry.value)
                }

                urlConnection.doInput = true

                // Nothing above this point touches the network. Writing the body connects, and so
                // does reading responseCode when there is no body, and disconnect() cannot tear
                // down a connection that has not connected yet -- so this is the last point a
                // cancel that arrived during setup can be honoured.
                if (isCancelled()) {
                    throw IOException("Request was cancelled")
                }

                if (body != null) {
                    urlConnection.doOutput = true
                    urlConnection.outputStream.write(body)
                    urlConnection.outputStream.close()
                }

                val responseCode = urlConnection.responseCode

                val responseHeaders = hashMapOf<String, String>()
                for (responseHeader in urlConnection.headerFields) {
                    if (!responseHeader.key.isNullOrEmpty() && responseHeader.value.isNotEmpty()) {
                        responseHeaders[responseHeader.key] = responseHeader.value.first()
                    }
                }

                val stream: InputStream? = if (responseCode >= 300) urlConnection.errorStream else urlConnection.inputStream

                val bodyResponse = stream?.readBytes()

                return HTTPResponse(responseCode, responseHeaders, bodyResponse)
            } finally {
                try {
                    urlConnection.disconnect()
                } catch (exc: Exception) {}
            }
        }

        private fun performRequest(): HTTPResponse {
            val urlConnection = openConnection(url)

            if (urlConnection is HttpURLConnection) {
                if (!bindConnection(urlConnection)) {
                    urlConnection.disconnect()
                    throw IOException("Request was cancelled")
                }

                return doPerformRequestWithURLConnection(urlConnection)
            } else {
                urlConnection.doInput = true
                val bodyResponse = urlConnection.getInputStream().readBytes()

                return HTTPResponse(200, hashMapOf<String, String>(), bodyResponse)
            }
        }

        override fun run() {
            // Cancelled while queued: opening the connection at all would be wasted work.
            if (isCancelled()) {
                return
            }

            try {
                val response = performRequest()
                notifySuccess(response)
            } catch (error: Throwable) {
                // Once cancelled the completion is already gone, so a teardown IOException lands
                // here and goes nowhere -- matching how iOS swallows NSURLErrorCancelled.
                // Catches Throwable so that reading an oversized body into memory fails the
                // request rather than leaving the completion unsettled.
                notifyFailure("HTTP Request failed: ${error.message}")
            }
        }

        companion object {

            fun from(request: HTTPRequest, openConnection: (URL) -> URLConnection, completion: HTTPRequestManagerCompletion): RequestTask {
                val url = URL(request.url)
                val method = request.method
                val body = request.body
                val headers = hashMapOf<String, String>()

                val headersMap = request.headers as? Map<*, *>
                if (headersMap != null) {
                    for (entry in headersMap.entries) {
                        val headerName = entry.key as? String
                        val headerValue = entry.value as? String
                        if (headerName != null && headerValue != null) {
                            headers[headerName] = headerValue
                        }
                    }
                }

                return RequestTask(url, method, body, headers, openConnection, completion)
            }
        }
    }

    private val threadCount = AtomicInteger(0)

    // One pool per host, matching NSURLSession.httpMaximumConnectionsPerHost, so a stalled host
    // cannot starve requests to other hosts.
    private val executors = ConcurrentHashMap<String, ThreadPoolExecutor>()

    private fun newHostPool(): ThreadPoolExecutor =
        ThreadPoolExecutor(
            MAX_CONCURRENT_REQUESTS_PER_HOST,
            MAX_CONCURRENT_REQUESTS_PER_HOST,
            keepAliveMs,
            TimeUnit.MILLISECONDS,
            LinkedBlockingQueue(),
        ) { r ->
            Thread(r).apply {
                name = "Valdi Network Thread ${threadCount.incrementAndGet()}"
                priority = Thread.NORM_PRIORITY
            }
        }.apply {
            // An unbounded queue never rejects, so a pool only ever grows to its core size and
            // maximumPoolSize is inert. Concurrency has to come from the core size, and this
            // restores the idle reaping that the previous core size of zero provided.
            allowCoreThreadTimeOut(true)
        }

    // Picking the pool and submitting to it as two steps would let a sweep shut it down in between,
    // so compute() does both under the bin lock for the host.
    private fun submitToHostPool(task: RequestTask) {
        executors.compute(task.url.host.orEmpty().lowercase()) { _, pool ->
            (pool ?: newHostPool()).also { it.execute(task) }
        }
    }

    /**
     * A pool with no threads has had none for a full keep-alive window, and [ThreadPoolExecutor]
     * adds a worker back whenever the queue is non-empty, so no threads also means nothing queued.
     *
     * Runs after a request has been submitted rather than on a timer: [HTTPRequestManager] has no
     * dispose hook, so there would be nowhere to stop a scheduler thread. Sweeping after the submit
     * rather than before it keeps the pool this request just used out of the sweep.
     */
    private fun evictDormantHostPools() {
        for (host in executors.keys) {
            executors.computeIfPresent(host) { _, pool ->
                if (pool.poolSize == 0) {
                    pool.shutdown()
                    null
                } else {
                    pool
                }
            }
        }
    }

    @VisibleForTesting
    fun hostPools(): Set<String> = executors.keys.toSet()

    override fun performRequest(request: HTTPRequest, completion: HTTPRequestManagerCompletion): Cancelable {
        try {
            val task = RequestTask.from(request, openConnection, completion)

            submitToHostPool(task)
            evictDormantHostPools()

            return task
        } catch (throwable: Throwable) {
            // Anything escaping to JNI leaves the completion unsettled, which hangs the native
            // request instead of failing it. Catches Throwable because starting a pool thread
            // fails with OutOfMemoryError, not an Exception.
            completion.onFail("Failed to perform request: ${throwable.message}")

            return object: Cancelable() {
                override fun cancel() {
                }
            }
        }
    }

    companion object {
        const val MAX_CONCURRENT_REQUESTS_PER_HOST = 4

        private const val DEFAULT_KEEP_ALIVE_MS = 60_000L
    }

}
