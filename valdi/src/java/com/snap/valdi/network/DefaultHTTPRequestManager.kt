package com.snap.valdi.network

import android.content.Context
import java.io.IOException
import java.io.InputStream
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLConnection
import java.util.concurrent.ExecutorService
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger
import com.snapchat.client.valdi.*
import com.snapchat.client.valdi_core.*

class DefaultHTTPRequestManager(
    context: Context,
    private val openConnection: (URL) -> URLConnection = { it.openConnection() },
): HTTPRequestManager() {

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
            // the task binding its connection.
            connectionToClose?.disconnect()
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
            } catch (error: Exception) {
                // Once cancelled the completion is already gone, so a teardown IOException lands
                // here and goes nowhere -- matching how iOS swallows NSURLErrorCancelled.
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

    private var executors: ExecutorService = ThreadPoolExecutor(
        MAX_CONCURRENT_REQUESTS,
        MAX_CONCURRENT_REQUESTS,
        60L,
        TimeUnit.SECONDS,
        LinkedBlockingQueue(),
    ) { r ->
        Thread(r).apply {
            name = "Valdi Network Thread ${threadCount.incrementAndGet()}"
            priority = Thread.NORM_PRIORITY
        }
    }.apply {
        // An unbounded queue never rejects, so a pool only ever grows to its core size and
        // maximumPoolSize is inert. Concurrency has to come from the core size, and this restores
        // the idle reaping that the previous core size of zero provided.
        allowCoreThreadTimeOut(true)
    }

    override fun performRequest(request: HTTPRequest, completion: HTTPRequestManagerCompletion): Cancelable {
        val task: RequestTask
        try {
            task = RequestTask.from(request, openConnection, completion)
        } catch (exception: Exception) {
            completion.onFail("Failed to build request: ${exception.message}")

            return object: Cancelable() {
                override fun cancel() {
                }
            }
        }

        executors.submit(task)

        return task
    }

    companion object {
        const val MAX_CONCURRENT_REQUESTS = 4
    }

}
