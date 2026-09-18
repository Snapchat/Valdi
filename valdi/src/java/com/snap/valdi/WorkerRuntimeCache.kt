package com.snap.valdi

/**
 * Caches at most one worker runtime per executor name, for the lifetime of the owner that holds
 * the cache.
 *
 * Creation is asynchronous: [create] is asked for a worker and answers later, on the owner's JS
 * thread. The first caller for an executor name triggers creation; callers that arrive while
 * creation is in flight, or while an earlier batch of blocks is still being handed out, have their
 * block queued and dispatched, in arrival order, as soon as the worker exists.
 *
 * A created worker is held for as long as this cache is, and is never handed back or discarded. A
 * djinni JS runtime has no teardown entry point, so its only teardown is the GC-timed native
 * destructor, which can run while the worker's own JS thread is still loading modules and destroy
 * the runtime underneath that work. iOS pins workers per manager for the same reason. Every worker
 * the cache is handed therefore receives at least one post -- a caller's block, or a keep-alive
 * no-op when the cache has no caller left to serve -- so it stays reachable from its own thread's
 * queue no matter how a teardown interleaves with its arrival.
 *
 * All members are thread-safe. Neither [create] nor [post] is ever invoked while the cache's lock
 * is held, and both are allowed to complete inline: dispatching onto a JS thread runs the task
 * immediately when the caller already is that thread, so a completion invoked under the lock would
 * take the runtime's own locks and run consumer code underneath the cache's monitor.
 *
 * [destroy] makes the cache inert, but because dispatch happens outside the lock it cannot retract
 * a request that is already past its check; see [destroy] for the exact guarantee.
 *
 * @param W the worker type handed to callers.
 * @param create requests a worker for an executor name and invokes `onReady` with it once it
 *   exists. Called at most once per executor name, and never while the cache lock is held.
 *   Invoking `onReady` inline, on the calling thread, is allowed.
 * @param post schedules a block to run against a worker on that worker's own thread. Never called
 *   while the cache lock is held, and running the block inline is allowed. It is called with a
 *   caller's block only for callers that reached the cache before it was destroyed; the only other
 *   thing it is asked to schedule is a keep-alive no-op, posted once for a worker that arrived
 *   with no caller block left to hand it. An exception out of it, including one thrown by a block
 *   it ran inline, stops neither the batch nor the drain; the first such failure is rethrown to
 *   whichever thread ran the dispatch once the queue has been emptied, carrying every later
 *   failure with it as a suppressed exception.
 */
class WorkerRuntimeCache<W : Any>(
    private val create: (executor: String, onReady: (W) -> Unit) -> Unit,
    private val post: (worker: W, block: (W) -> Unit) -> Unit
) {

    private class Entry<W : Any> {
        var worker: W? = null

        /**
         * Whether a batch of [pending] is being handed to `post` right now, outside the lock. While
         * it is, an arriving caller queues rather than posting for itself, so it cannot overtake
         * the blocks queued before it.
         */
        var draining = false
        val pending = mutableListOf<(W) -> Unit>()
    }

    private val entries = mutableMapOf<String, Entry<W>>()

    // Guarded by `entries`.
    private var destroyed = false

    /**
     * Run [block] against the worker runtime for [executor], creating that worker if this is the
     * first request for the name.
     *
     * [block] is handed to [post], so it runs on the worker's own thread rather than here, though
     * [post] may run it inline. It never runs at all if the cache is destroyed before the worker
     * exists, which is the same outcome a caller saw before such blocks were queued.
     */
    fun getWorker(executor: String, block: (W) -> Unit) {
        var isCreator = false
        var readyWorker: W? = null
        synchronized(entries) {
            if (destroyed) {
                return
            }
            val entry = entries[executor]
            if (entry == null) {
                // Elect this caller as creator and queue its block in one step, so concurrent
                // first callers cannot each create a worker and overwrite each other's.
                entries[executor] = Entry<W>().apply { pending.add(block) }
                isCreator = true
            } else {
                val worker = entry.worker
                if (worker == null || entry.draining) {
                    entry.pending.add(block)
                } else {
                    readyWorker = worker
                }
            }
        }
        if (isCreator) {
            create(executor) { worker -> publish(executor, worker) }
        } else {
            // This caller took no `draining` claim and left nothing queued -- it captured a worker
            // nobody was draining for -- so a failure out of `post` here has no state to unwind.
            readyWorker?.let { worker -> post(worker, block) }
        }
    }

    /**
     * Make the cache inert: no further creation is requested, blocks still queued behind an
     * unfinished creation are dropped, and a caller arriving afterwards is not served.
     *
     * Because the cache calls neither [create] nor [post] under its lock, this cannot retract work
     * already past its check. A [getWorker] call that had passed the destroyed check, and a batch
     * of queued blocks already taken for dispatch, may still reach [create] or [post] after this
     * has returned; a [create] implementation must therefore be inert on its own once its owner is
     * being torn down, since the owner's lifecycle state rather than this cache is what stops a
     * late request from reviving a runtime. Reaching [post] late is harmless because tearing the
     * owner down does not destroy the worker runtimes.
     *
     * Idempotent.
     */
    fun destroy() {
        synchronized(entries) {
            destroyed = true
            entries.values.forEach { it.pending.clear() }
        }
    }

    private fun publish(executor: String, worker: W) {
        var firstBatch: List<(W) -> Unit> = emptyList()
        val entry = synchronized(entries) {
            val published = if (destroyed) null else entries[executor]
            if (published != null) {
                published.worker = worker
                published.draining = true
                // Taken in the same critical section that publishes the worker, so a destroy()
                // landing between the two cannot empty the queue this drain was about to serve and
                // leave the worker with nothing posted for it at all. While the cache is live this
                // batch is never empty: the creator's own block was queued when the entry was
                // created, and the only thing that clears `pending` is destroy() -- which is also
                // what makes `published` null here. A worker is therefore never published without
                // at least one block already on its way to its own thread, which is what keeps the
                // worker reachable until that thread runs it.
                firstBatch = published.pending.toList().also { published.pending.clear() }
            }
            published
        }
        dispatchToWorker(worker, entry, firstBatch)
    }

    /**
     * Hand [firstBatch], and every batch queued behind it, to [post] in arrival order until none
     * are left, taking the later batches one at a time so that nothing is dispatched under the
     * lock. A caller arriving mid-batch queues behind the batch in flight instead of overtaking
     * it, and is picked up by a later pass.
     *
     * [entry] is null when [worker] arrived at an already-destroyed cache, so there is nothing to
     * drain and the keep-alive is that worker's only post.
     */
    private fun dispatchToWorker(worker: W, entry: Entry<W>?, firstBatch: List<(W) -> Unit>) {
        var batch = firstBatch
        var posted = false
        var failure: Throwable? = null

        // Later failures ride along on the first rather than being dropped, so a block that
        // failed for a second, unrelated reason is still visible at the throw site. A throwable is
        // never suppressed onto itself, which would be rejected, and `addSuppressed` is a silent
        // no-op on a throwable built with suppression disabled, which is all such a one allows.
        fun record(error: Throwable) {
            val first = failure
            if (first == null) {
                failure = error
            } else if (first !== error) {
                first.addSuppressed(error)
            }
        }

        while (true) {
            for (queued in batch) {
                // A failure must not end the drain: the blocks behind this one have no other
                // drainer, and abandoning the loop would leave `draining` set and strand every
                // caller that arrives afterwards. The first failure is rethrown once the queue is
                // empty, and the rest are suppressed exceptions on it, so nothing is swallowed
                // either. A block that reached `post` counts as posted even if it threw, since a
                // keep-alive down the same path would fare no better.
                posted = true
                try {
                    post(worker, queued)
                } catch (error: Throwable) {
                    record(error)
                }
            }
            if (entry == null) {
                break
            }
            batch = synchronized(entries) {
                if (destroyed || entry.pending.isEmpty()) {
                    entry.draining = false
                    null
                } else {
                    entry.pending.toList().also { entry.pending.clear() }
                }
            } ?: break
        }

        if (!posted) {
            // Nothing reached this worker's thread, because the cache's owner is on its way out and
            // took the queued blocks with it. Storing the worker in the cache would keep it alive
            // no longer than that owner, and a worker collected while its own thread is still
            // bootstrapping modules is the GC race this cache exists to prevent, so hold it from
            // that thread instead: a no-op queued behind the bootstrap work keeps the worker
            // reachable until the thread is idle, where collecting it is safe.
            try {
                post(worker) { }
            } catch (error: Throwable) {
                record(error)
            }
        }

        failure?.let { throw it }
    }
}
