package com.snap.valdi

import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertFalse
import org.junit.jupiter.api.Assertions.assertSame
import org.junit.jupiter.api.Assertions.assertThrows
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test
import java.util.Collections
import java.util.Random
import java.util.concurrent.CountDownLatch
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger

/**
 * Lifecycle tests for [WorkerRuntimeCache]. Creation and dispatch are driven by hand through
 * [Harness], so every ordering these tests assert is the cache's own and not a scheduler's.
 */
class WorkerRuntimeCacheTest {

    private class TestWorker(val executor: String)

    private enum class Callback { CREATE, POST }

    /**
     * Stands in for the runtime manager's `create`/`post` pair. By default nothing the cache asks
     * for happens until the test asks for it: `create` records the request and hands back a
     * completion the test fires, and `post` appends to a queue the test drains. `completeInline`
     * and `postInline` instead model the production shape, where both callbacks can complete on the
     * calling thread.
     *
     * Posts are classified by what running them does rather than by inspecting the block: a post
     * that runs one caller block dispatched a caller, a post that runs none is a keep-alive no-op.
     */
    private class Harness(
        private val completeInline: Boolean = false,
        private val postInline: Boolean = false,
        private val onCreate: () -> Unit = NO_HOOK,
        private val onPost: () -> Unit = NO_HOOK
    ) {

        class CreateRequest(val executor: String, private val onReady: (TestWorker) -> Unit) {
            val worker = TestWorker(executor)
            private val completed = AtomicBoolean(false)

            fun complete() {
                if (completed.compareAndSet(false, true)) {
                    onReady(worker)
                }
            }

            fun isCompleted(): Boolean = completed.get()
        }

        class PostedBlock(
            val worker: TestWorker,
            private val block: (TestWorker) -> Unit,
            /** Whether the cache asked for this post after `destroy()` had returned. */
            val postedAfterDestroy: Boolean
        ) {
            /** Caller blocks this post ran; -1 until it has been drained. */
            var callerRuns: Int = -1
                private set

            fun run(callerRunCounter: AtomicInteger) {
                val before = callerRunCounter.get()
                block(worker)
                callerRuns = callerRunCounter.get() - before
            }
        }

        private val createLog = mutableListOf<CreateRequest>()

        // Append-only, so a drained post can still be inspected; `drained` is the queue head.
        private val postLog = mutableListOf<PostedBlock>()
        private var drained = 0

        /** Flipped by [destroy] once `cache.destroy()` has returned, so the fakes spot late calls. */
        val destroyReturned = AtomicBoolean(false)
        val createAfterDestroy = AtomicBoolean(false)
        val postCount = AtomicInteger()

        /**
         * Whether a `getWorker` call that itself *started* after `destroy()` returned reached the
         * fake. A call that straddles destroy legitimately may, so origin is tracked per call
         * rather than by wall clock.
         */
        val createFromCallStartedAfterDestroy = AtomicBoolean(false)
        val postFromCallStartedAfterDestroy = AtomicBoolean(false)

        /** Every caller block that has run, counted by the wrapper [request] installs. */
        val callerRuns = AtomicInteger()

        /** Caller blocks reached by a post made after `destroy()` returned. */
        val callerRunsAfterDestroy = AtomicInteger()

        /** Set for the duration of a [request] call, recording the state destroy was in then. */
        private val callStartedAfterDestroy = ThreadLocal<Boolean>()

        private fun startedAfterDestroy(): Boolean = callStartedAfterDestroy.get() == true

        val cache = WorkerRuntimeCache<TestWorker>(
            create = { executor, onReady ->
                if (destroyReturned.get()) {
                    createAfterDestroy.set(true)
                }
                if (startedAfterDestroy()) {
                    createFromCallStartedAfterDestroy.set(true)
                }
                val request = CreateRequest(executor, onReady)
                synchronized(createLog) { createLog.add(request) }
                onCreate()
                if (completeInline) {
                    request.complete()
                }
            },
            post = { worker, block ->
                postCount.incrementAndGet()
                if (startedAfterDestroy()) {
                    postFromCallStartedAfterDestroy.set(true)
                }
                val posted = PostedBlock(worker, block, destroyReturned.get())
                synchronized(postLog) {
                    postLog.add(posted)
                    if (postInline) {
                        drained = postLog.size
                    }
                }
                onPost()
                if (postInline) {
                    // Outside the harness lock, so the block may re-enter the cache.
                    posted.run(callerRuns)
                    if (posted.postedAfterDestroy) {
                        callerRunsAfterDestroy.addAndGet(posted.callerRuns)
                    }
                }
            }
        )

        /**
         * Ask for a worker on behalf of a caller. [block] is wrapped so that draining can tell a
         * dispatched caller block from the cache's own keep-alive no-op.
         */
        fun request(executor: String, block: (TestWorker) -> Unit = {}) {
            val outer = startedAfterDestroy()
            callStartedAfterDestroy.set(destroyReturned.get())
            try {
                cache.getWorker(executor) { worker ->
                    callerRuns.incrementAndGet()
                    block(worker)
                }
            } finally {
                callStartedAfterDestroy.set(outer)
            }
        }

        fun destroy() {
            cache.destroy()
            destroyReturned.set(true)
        }

        fun creates(): List<CreateRequest> = synchronized(createLog) { createLog.toList() }

        fun createCount(): Int = synchronized(createLog) { createLog.size }

        fun posts(): List<PostedBlock> = synchronized(postLog) { postLog.toList() }

        fun postsAfterDestroy(): List<PostedBlock> = posts().filter { it.postedAfterDestroy }

        /** Drained posts that ran no caller block, i.e. the cache's keep-alive no-ops. */
        fun noopPosts(): List<PostedBlock> = posts().filter { it.callerRuns == 0 }

        fun pendingPostCount(): Int = synchronized(postLog) { postLog.size - drained }

        /** Runs every queued block. Blocks run outside the harness lock so they may re-enter. */
        fun drainPosted(): Int {
            val batch = synchronized(postLog) {
                postLog.subList(drained, postLog.size).toList().also { drained = postLog.size }
            }
            batch.forEach { posted ->
                posted.run(callerRuns)
                if (posted.postedAfterDestroy) {
                    callerRunsAfterDestroy.addAndGet(posted.callerRuns)
                }
            }
            return batch.size
        }

        fun drainPostedUntilQuiet() {
            while (drainPosted() > 0) {
                // Draining a block can post another one.
            }
        }

        fun completeOutstandingCreates() {
            creates().filterNot { it.isCompleted() }.forEach { it.complete() }
        }
    }

    /** Concurrent first callers each elect themselves creator unless election and queueing are atomic. */
    @Test
    fun concurrentFirstCallersCreateOneWorkerAndEveryBlockRunsOnce() {
        val callerCount = 16
        val errors = Collections.synchronizedList(mutableListOf<Throwable>())

        // Repeated because a single round only sometimes lands inside the election window.
        repeat(50) { round ->
            val harness = Harness()
            val runs = Array(callerCount) { AtomicInteger() }
            val startGate = CountDownLatch(1)

            val threads = (0 until callerCount).map { id ->
                newThread(errors) {
                    startGate.await()
                    harness.request("shared") { runs[id].incrementAndGet() }
                }
            }
            threads.forEach { it.start() }
            startGate.countDown()
            threads.forEach { it.join() }

            assertEquals(emptyList<Throwable>(), errors, "round $round")
            assertEquals(1, harness.createCount(), "round $round")
            // Nothing may be dispatched before the worker exists.
            assertEquals(0, harness.pendingPostCount(), "round $round")
            assertTrue(runs.all { it.get() == 0 }, "round $round")

            harness.creates()[0].complete()

            assertEquals(callerCount, harness.pendingPostCount(), "round $round")
            harness.drainPostedUntilQuiet()
            assertTrue(runs.all { it.get() == 1 }, "round $round: every queued block must run once")
            assertEquals(emptyList<Harness.PostedBlock>(), harness.noopPosts(), "round $round")
        }
    }

    /**
     * Blocks queued behind an unfinished creation must dispatch ahead of a later arrival, whether
     * that arrival lands while the batch is still being handed out or after the drain finished.
     */
    @Test
    fun queuedBlocksDispatchBeforeACallerThatArrivesAfterPublish() {
        val errors = Collections.synchronizedList(mutableListOf<Throwable>())
        val order = Collections.synchronizedList(mutableListOf<String>())
        val armed = AtomicBoolean(true)
        var midDrain: Thread? = null
        var target: Harness? = null

        val harness = Harness(onPost = {
            if (armed.compareAndSet(true, false)) {
                val cache = target!!
                // Arrives from another thread while the first batch is still being handed out, so
                // it has to queue behind that batch rather than post for itself.
                midDrain = newThread(errors) { cache.request("shared") { order.add("midDrain") } }
                    .apply {
                        isDaemon = true
                        start()
                        join(PROBE_TIMEOUT_MS)
                    }
            }
        })
        target = harness

        harness.request("shared") { order.add("queuedFirst") }
        harness.request("shared") { order.add("queuedSecond") }

        assertEquals(1, harness.createCount())
        assertEquals(0, harness.pendingPostCount())

        harness.creates()[0].complete()
        assertFalse(midDrain?.isAlive ?: true, "the mid-drain caller never finished")

        harness.request("shared") { order.add("afterDrain") }
        assertEquals(1, harness.createCount())

        harness.drainPostedUntilQuiet()
        assertEquals(listOf("queuedFirst", "queuedSecond", "midDrain", "afterDrain"), order)
        assertEquals(emptyList<Throwable>(), errors)
    }

    /** A worker arriving after destroy() must dispatch no caller block, and later requests must be inert. */
    @Test
    fun destroyBeforeWorkerArrivesDropsQueuedBlocksAndLaterRequests() {
        val harness = Harness()
        val ran = mutableListOf<String>()

        harness.request("shared") { ran.add("queued") }
        assertEquals(1, harness.createCount())

        harness.destroy()

        harness.creates()[0].complete()
        // The only post after destroy is the keep-alive for the worker that arrived late.
        assertEquals(1, harness.postsAfterDestroy().size)
        harness.drainPostedUntilQuiet()
        assertEquals(0, harness.callerRuns.get())
        assertEquals(0, harness.callerRunsAfterDestroy.get())

        harness.request("shared") { ran.add("afterDestroy") }
        assertEquals(1, harness.createCount())
        assertEquals(0, harness.pendingPostCount())
        assertEquals(1, harness.postCount.get())
        assertFalse(harness.createAfterDestroy.get())
        assertEquals(emptyList<String>(), ran)
    }

    /**
     * A worker whose creation answers after destroy() cannot be pinned in the cache: the cache's
     * owner is going away with it. It is instead held by a no-op posted to its own thread, queued
     * behind the bootstrap work already there, so it stays reachable until that thread is idle.
     */
    @Test
    fun workerArrivingAfterDestroyIsKeptReachableUntilItsThreadIsIdle() {
        val harness = Harness()
        val ran = mutableListOf<String>()

        harness.request("shared") { ran.add("queued") }
        assertEquals(1, harness.createCount())
        val lateWorker = harness.creates()[0].worker

        harness.destroy()
        harness.creates()[0].complete()

        // Exactly one post, for that worker, and it is the keep-alive rather than a caller block.
        assertEquals(1, harness.postCount.get())
        assertEquals(1, harness.pendingPostCount())
        assertEquals(listOf(lateWorker), harness.postsAfterDestroy().map { it.worker })

        harness.drainPostedUntilQuiet()
        assertEquals(listOf(lateWorker), harness.noopPosts().map { it.worker })
        assertEquals(0, harness.callerRuns.get())
        assertEquals(0, harness.callerRunsAfterDestroy.get())
        assertEquals(emptyList<String>(), ran)

        // The worker is never handed to a caller, and nothing more is scheduled or created.
        harness.request("shared") { ran.add("afterDestroy") }
        assertEquals(1, harness.createCount())
        assertEquals(1, harness.postCount.get())
        assertEquals(0, harness.pendingPostCount())
        assertFalse(harness.createAfterDestroy.get())
        assertEquals(emptyList<String>(), ran)
    }

    /**
     * destroy() landing while a freshly published worker's queue is being drained must not leave
     * that worker with nothing posted for it: the batch is taken in the same critical section that
     * publishes the worker, so a destroy that arrives afterwards can no longer empty it. Without
     * that, the drain aborted on the destroyed flag having posted nothing, and the worker was
     * collectable while its own thread was still bootstrapping.
     */
    @Test
    fun aWorkerPublishedJustBeforeDestroyStillReceivesAPost() {
        val armed = AtomicBoolean(true)
        var target: Harness? = null

        val harness = Harness(onPost = {
            if (armed.compareAndSet(true, false)) {
                // Tears the cache down from inside the first post of the batch, so the drain's next
                // pass sees a destroyed cache with an emptied queue.
                target!!.destroy()
            }
        })
        target = harness

        harness.request("shared")
        harness.request("shared")
        assertEquals(1, harness.createCount())
        val worker = harness.creates()[0].worker

        harness.creates()[0].complete()

        assertTrue(harness.postCount.get() >= 1, "the published worker was handed nothing")
        assertEquals(listOf(worker), harness.posts().map { it.worker }.distinct())
        // Blocks queued before the worker was published belong to the batch taken with it, so both
        // are handed out even though destroy() landed in the middle.
        assertEquals(2, harness.postCount.get())
        assertEquals(1, harness.postsAfterDestroy().size)

        harness.drainPostedUntilQuiet()
        // Caller blocks reached the worker's thread, so it needed no keep-alive to hold it.
        assertEquals(emptyList<Harness.PostedBlock>(), harness.noopPosts())
        assertFalse(harness.createFromCallStartedAfterDestroy.get())
        assertFalse(harness.postFromCallStartedAfterDestroy.get())
        assertFalse(harness.createAfterDestroy.get())

        // The cache is still inert afterwards: `draining` was released, not left set.
        val ran = AtomicBoolean(false)
        harness.request("shared") { ran.set(true) }
        assertFalse(ran.get())
        assertEquals(1, harness.createCount())
        assertEquals(2, harness.postCount.get())
    }

    /**
     * The same window, hit for real rather than simulated: a creation answering and destroy() race
     * each other, so on some rounds destroy lands between the worker being published and the drain
     * looking for something to hand it. However the two interleave, the worker must come away with
     * a post, or nothing holds it while its thread bootstraps.
     */
    @Test
    fun destroyRacingAFreshlyPublishedWorkerStillLeavesItAPost() {
        val random = Random(PUBLISH_RACE_SEED)
        val errors = Collections.synchronizedList(mutableListOf<Throwable>())

        repeat(2000) { round ->
            val harness = Harness()
            harness.request("shared")
            val create = harness.creates()[0]
            val destroySpin = random.nextInt(4)
            val startGate = CountDownLatch(1)

            val publisher = newThread(errors) {
                startGate.await()
                create.complete()
            }
            val destroyer = newThread(errors) {
                startGate.await()
                spin(destroySpin)
                harness.destroy()
            }

            val both = listOf(publisher, destroyer)
            both.forEach { it.start() }
            startGate.countDown()
            both.forEach { it.join() }

            val context = "round $round, seed $PUBLISH_RACE_SEED"
            assertTrue(
                harness.posts().any { it.worker === create.worker },
                "$context: the published worker was handed nothing"
            )
            assertFalse(harness.postFromCallStartedAfterDestroy.get(), context)
        }
        assertEquals(emptyList<Throwable>(), errors)
    }

    /** A destroyed cache must never reach `create`, which for the manager would revive its runtime. */
    @Test
    fun getWorkerOnADestroyedCacheNeverRequestsCreation() {
        val harness = Harness()
        harness.destroy()

        val ran = AtomicBoolean(false)
        harness.request("shared") { ran.set(true) }

        assertEquals(0, harness.createCount())
        assertEquals(0, harness.postCount.get())
        assertFalse(harness.createAfterDestroy.get())
        assertFalse(ran.get())
    }

    /** Two owners sharing an executor name must not share an entry, a worker or a queue. */
    @Test
    fun twoCachesOnTheSameExecutorNameAreIndependent() {
        val first = Harness()
        val second = Harness()
        val order = mutableListOf<String>()

        first.request("shared") { worker -> order.add("first:${worker.executor}") }
        second.request("shared") { worker -> order.add("second:${worker.executor}") }

        assertEquals(1, first.createCount())
        assertEquals(1, second.createCount())

        first.creates()[0].complete()
        assertEquals(1, first.pendingPostCount())
        assertEquals(0, second.pendingPostCount())
        first.drainPostedUntilQuiet()

        first.destroy()

        second.creates()[0].complete()
        assertEquals(1, second.pendingPostCount())
        second.drainPostedUntilQuiet()

        assertEquals(listOf("first:shared", "second:shared"), order)
        assertEquals(1, first.createCount())
        assertEquals(1, second.createCount())
        // First published before it was destroyed, so it never needed a keep-alive.
        assertEquals(emptyList<Harness.PostedBlock>(), first.postsAfterDestroy())
    }

    /** A destroyed cache's queued blocks must not be revived by another cache's publish. */
    @Test
    fun blocksQueuedOnADestroyedCacheNeverRunWhenAnotherCachePublishes() {
        val first = Harness()
        val second = Harness()
        val ran = mutableListOf<String>()

        first.request("shared") { ran.add("first") }
        second.request("shared") { ran.add("second") }

        first.destroy()

        first.creates()[0].complete()
        second.creates()[0].complete()

        first.drainPostedUntilQuiet()
        second.drainPostedUntilQuiet()

        assertEquals(listOf("second"), ran)
        assertEquals(1, first.createCount())
        assertEquals(1, second.createCount())
        // First's only post is the keep-alive for its late worker; no caller block was dispatched.
        assertEquals(1, first.postCount.get())
        assertEquals(listOf(first.creates()[0].worker), first.noopPosts().map { it.worker })
        assertEquals(0, first.callerRuns.get())
        assertEquals(0, first.callerRunsAfterDestroy.get())
    }

    /** The state machine must tolerate a dispatched block asking for the same worker again. */
    @Test
    fun aQueuedBlockMayReenterGetWorker() {
        val harness = Harness()
        val order = mutableListOf<String>()

        harness.request("shared") {
            order.add("outer")
            harness.request("shared") { order.add("inner") }
        }
        harness.creates()[0].complete()

        assertEquals(1, harness.drainPosted())
        assertEquals(1, harness.drainPosted())
        assertEquals(0, harness.drainPosted())
        assertEquals(listOf("outer", "inner"), order)
        assertEquals(1, harness.createCount())
        assertEquals(emptyList<Harness.PostedBlock>(), harness.noopPosts())
    }

    /**
     * The same re-entry, but with `post` running the block inline the way a dispatch onto a thread
     * the caller already is does. The re-entering request lands mid-drain, so it must neither
     * deadlock nor overtake the block queued ahead of it.
     */
    @Test
    fun aQueuedBlockMayReenterGetWorkerWhilePostRunsItInline() {
        val harness = Harness(postInline = true)
        val order = mutableListOf<String>()

        harness.request("shared") {
            order.add("outer")
            harness.request("shared") { order.add("inner") }
        }
        harness.request("shared") { order.add("second") }

        harness.creates()[0].complete()

        assertEquals(listOf("outer", "second", "inner"), order)
        assertEquals(1, harness.createCount())
        assertEquals(0, harness.pendingPostCount())
        assertEquals(emptyList<Harness.PostedBlock>(), harness.noopPosts())
    }

    /**
     * A block that throws must not wedge the executor. The rest of the batch still dispatches, in
     * order, the `draining` claim is released so later callers are served rather than queued behind
     * a drain that will never come, and the failure still surfaces to whoever ran the dispatch.
     */
    @Test
    fun aThrowingBlockDoesNotWedgeDispatch() {
        val harness = Harness(postInline = true)
        val order = mutableListOf<String>()
        val boom = RuntimeException("second block")

        harness.request("shared") { order.add("first") }
        harness.request("shared") {
            order.add("second")
            throw boom
        }
        harness.request("shared") { order.add("third") }
        harness.request("shared") { order.add("fourth") }

        val thrown = assertThrows(RuntimeException::class.java) { harness.creates()[0].complete() }

        assertSame(boom, thrown, "the failure was swallowed or replaced")
        assertEquals(listOf("first", "second", "third", "fourth"), order)
        assertEquals(4, harness.postCount.get())

        // Served directly rather than queued, which is what a leaked `draining` would have done.
        harness.request("shared") { order.add("afterFailure") }
        assertEquals(5, harness.postCount.get())
        assertEquals(listOf("first", "second", "third", "fourth", "afterFailure"), order)
        assertEquals(0, harness.pendingPostCount())
        assertEquals(1, harness.createCount())
        assertEquals(emptyList<Harness.PostedBlock>(), harness.noopPosts())
    }

    /**
     * The same, for a dispatch that fails outright rather than a block that throws. A second
     * failing dispatch is reported too, as a suppressed exception on the first.
     */
    @Test
    fun aFailedDispatchStillDispatchesTheRestOfTheBatch() {
        val boom = RuntimeException("second dispatch")
        val laterBoom = IllegalStateException("fourth dispatch")
        val attempts = AtomicInteger()
        val harness = Harness(onPost = {
            when (attempts.incrementAndGet()) {
                2 -> throw boom
                4 -> throw laterBoom
            }
        })
        val order = mutableListOf<String>()

        repeat(4) { id -> harness.request("shared") { order.add("block$id") } }

        val thrown = assertThrows(RuntimeException::class.java) { harness.creates()[0].complete() }

        assertSame(boom, thrown, "the failure was swallowed or replaced")
        assertEquals(listOf<Throwable>(laterBoom), thrown.suppressed.toList(), "later failure lost")
        // Every block was handed to `post`, including the two behind the one that failed.
        assertEquals(4, harness.postCount.get())
        harness.drainPostedUntilQuiet()
        assertEquals(listOf("block0", "block1", "block2", "block3"), order)
        assertEquals(emptyList<Harness.PostedBlock>(), harness.noopPosts())

        harness.request("shared") { order.add("afterFailure") }
        assertEquals(5, harness.postCount.get())
        harness.drainPostedUntilQuiet()
        assertEquals("afterFailure", order.last())
        assertEquals(1, harness.createCount())
    }

    /**
     * When more than one block in a drain fails, every failure reaches the caller: the first is
     * thrown and the later ones hang off it as suppressed exceptions, so a block failing for a
     * second, unrelated reason is not lost behind the first.
     */
    @Test
    fun laterDispatchFailuresRideAlongAsSuppressed() {
        val harness = Harness(postInline = true)
        val order = mutableListOf<String>()
        val secondBoom = RuntimeException("second block")
        val fourthBoom = IllegalStateException("fourth block")

        harness.request("shared") { order.add("first") }
        harness.request("shared") {
            order.add("second")
            throw secondBoom
        }
        harness.request("shared") { order.add("third") }
        harness.request("shared") {
            order.add("fourth")
            throw fourthBoom
        }

        val thrown = assertThrows(RuntimeException::class.java) { harness.creates()[0].complete() }

        assertSame(secondBoom, thrown, "the first failure was swallowed or replaced")
        assertEquals(
            listOf<Throwable>(fourthBoom),
            thrown.suppressed.toList(),
            "the later failure was discarded instead of being attached to the first"
        )
        assertEquals(listOf("first", "second", "third", "fourth"), order)
        assertEquals(4, harness.postCount.get())

        // Served directly rather than queued, which is what a leaked `draining` would have done.
        harness.request("shared") { order.add("afterFailure") }
        assertEquals(5, harness.postCount.get())
        assertEquals(listOf("first", "second", "third", "fourth", "afterFailure"), order)
        assertEquals(0, harness.pendingPostCount())
        assertEquals(1, harness.createCount())
    }

    /**
     * Answers the fidelity gap the previous suite had: in production both callbacks can complete on
     * the calling thread, so both fakes complete inline here and, while each is running, another
     * thread uses the cache. A callback invoked under the cache's monitor would block that thread.
     */
    @Test
    fun createAndPostAreInvokedWithoutTheCacheLockHeld() {
        val errors = Collections.synchronizedList(mutableListOf<Throwable>())

        Callback.values().forEach { callback ->
            val (harness, probe) = probeFromInside(callback, errors) { it.request("probe") }

            assertFalse(probe.isAlive, "$callback ran under the cache lock: the probe blocked")
            assertEquals(
                setOf("shared", "probe"),
                harness.creates().map { it.executor }.toSet(),
                "$callback"
            )
        }
        assertEquals(emptyList<Throwable>(), errors)
    }

    /** The same probe, but tearing the cache down, which is the other thing that takes its lock. */
    @Test
    fun createAndPostDoNotBlockADestroyFromAnotherThread() {
        val errors = Collections.synchronizedList(mutableListOf<Throwable>())

        Callback.values().forEach { callback ->
            val (harness, probe) = probeFromInside(callback, errors) { it.destroy() }

            assertFalse(probe.isAlive, "$callback ran under the cache lock: destroy() blocked")
            assertTrue(harness.destroyReturned.get(), "$callback")
            assertFalse(harness.createFromCallStartedAfterDestroy.get(), "$callback")
            assertFalse(harness.postFromCallStartedAfterDestroy.get(), "$callback")
        }
        assertEquals(emptyList<Throwable>(), errors)
    }

    /** `create` may answer inline on the calling thread; queued blocks still dispatch exactly once. */
    @Test
    fun creationThatAnswersInlineDispatchesQueuedBlocksExactlyOnce() {
        val harness = Harness(completeInline = true)
        val order = mutableListOf<String>()

        harness.request("shared") { order.add("first") }
        assertEquals(1, harness.createCount())
        assertEquals(1, harness.pendingPostCount())

        harness.request("shared") { order.add("second") }
        assertEquals(1, harness.createCount())

        harness.drainPostedUntilQuiet()
        assertEquals(listOf("first", "second"), order)
    }

    @Test
    fun repeatedRequestsAfterPublishReuseTheSameWorker() {
        val harness = Harness(completeInline = true)
        val runs = AtomicInteger()

        repeat(100) { harness.request("shared") { runs.incrementAndGet() } }

        assertEquals(1, harness.createCount())
        assertEquals(100, harness.pendingPostCount())
        harness.drainPostedUntilQuiet()
        assertEquals(100, runs.get())
        assertEquals(emptyList<Harness.PostedBlock>(), harness.noopPosts())
    }

    @Test
    fun destroyIsIdempotent() {
        val harness = Harness(completeInline = true)
        harness.request("shared")
        harness.drainPostedUntilQuiet()

        repeat(3) { harness.destroy() }

        val ran = AtomicBoolean(false)
        harness.request("shared") { ran.set(true) }

        assertEquals(1, harness.createCount())
        assertEquals(0, harness.pendingPostCount())
        assertFalse(ran.get())
        assertFalse(harness.createAfterDestroy.get())
        // The worker was published before destroy, so nothing is posted afterwards at all.
        assertEquals(emptyList<Harness.PostedBlock>(), harness.postsAfterDestroy())
    }

    /**
     * A getWorker call that starts after destroy() returned must reach neither `create` nor `post`.
     * A call that started earlier may still reach them, because dispatch happens outside the lock,
     * so the check is on where the call began rather than on wall clock.
     */
    @Test
    fun destroyRacingGetWorkerNeverRequestsCreationAfterwards() {
        val random = Random(RACE_SEED)
        val errors = Collections.synchronizedList(mutableListOf<Throwable>())

        repeat(400) { round ->
            val harness = Harness(completeInline = true)
            val destroySpin = random.nextInt(32)
            val startGate = CountDownLatch(1)

            val callers = (0 until 4).map { id ->
                newThread(errors) {
                    startGate.await()
                    harness.request("shared$id")
                }
            }
            val destroyer = newThread(errors) {
                startGate.await()
                spin(destroySpin)
                harness.destroy()
            }

            (callers + destroyer).forEach { it.start() }
            startGate.countDown()
            (callers + destroyer).forEach { it.join() }

            val context = "round $round, seed $RACE_SEED"
            assertFalse(harness.createFromCallStartedAfterDestroy.get(), context)
            assertFalse(harness.postFromCallStartedAfterDestroy.get(), context)
        }
        assertEquals(emptyList<Throwable>(), errors)
    }

    /**
     * Randomized interleavings of concurrent requests, asynchronous completion and destroy. Per
     * iteration: at most one creation per executor name, every post either dispatches exactly one
     * caller block or is a keep-alive no-op, no caller block runs twice, at most one keep-alive per
     * worker, every worker the cache was handed received at least one post, and no `getWorker` call
     * that started after destroy() returned reaches `create` or `post`. A call that straddles
     * destroy may reach either, which is the relaxed contract that comes with never dispatching
     * under the cache lock.
     */
    @Test
    fun randomizedInterleavingsHoldTheCacheInvariants() {
        val random = Random(STRESS_SEED)
        val errors = Collections.synchronizedList(mutableListOf<Throwable>())

        repeat(2000) { iteration ->
            val context = "iteration $iteration, seed $STRESS_SEED"
            val harness = Harness()
            val executors = if (random.nextBoolean()) listOf("a") else listOf("a", "b")
            val callerCount = 2 + random.nextInt(7)
            // Every choice is drawn on this thread so the schedule of decisions is seed-stable.
            val chosenExecutors = List(callerCount) { executors[random.nextInt(executors.size)] }
            val completerSpin = random.nextInt(16)
            val destroySpin = random.nextInt(24)
            val destroys = random.nextInt(3) == 0

            val runs = List(callerCount) { AtomicInteger() }
            val startGate = CountDownLatch(1)

            val callers = (0 until callerCount).map { id ->
                newThread(errors) {
                    startGate.await()
                    harness.request(chosenExecutors[id]) { runs[id].incrementAndGet() }
                }
            }
            val completer = newThread(errors) {
                startGate.await()
                spin(completerSpin)
                repeat(3) {
                    harness.completeOutstandingCreates()
                    Thread.yield()
                }
            }
            val destroyer = if (!destroys) null else newThread(errors) {
                startGate.await()
                spin(destroySpin)
                harness.destroy()
            }

            val all = callers + completer + listOfNotNull(destroyer)
            all.forEach { it.start() }
            startGate.countDown()
            all.forEach { it.join() }

            harness.completeOutstandingCreates()
            harness.drainPostedUntilQuiet()

            assertEquals(emptyList<Throwable>(), errors, context)
            assertFalse(harness.createFromCallStartedAfterDestroy.get(), context)
            assertFalse(harness.postFromCallStartedAfterDestroy.get(), context)

            val createsByExecutor = harness.creates().groupingBy { it.executor }.eachCount()
            createsByExecutor.forEach { (executor, count) ->
                assertEquals(1, count, "$context: executor $executor")
            }
            assertTrue(createsByExecutor.keys.all { it in executors }, context)
            assertTrue(runs.all { it.get() <= 1 }, "$context: a block ran more than once")

            val posts = harness.posts()
            assertTrue(posts.all { it.callerRuns in 0..1 }, "$context: a post ran two caller blocks")
            assertEquals(runs.sumOf { it.get() }, posts.count { it.callerRuns == 1 }, context)

            val noops = harness.noopPosts()
            noops.groupingBy { it.worker }.eachCount().forEach { (_, count) ->
                assertEquals(1, count, "$context: a worker was kept alive more than once")
            }

            // Nothing holds a worker while its JS thread is still bootstrapping except a block on
            // that thread's own queue, so every worker handed to the cache must have got one --
            // a caller's block, or the keep-alive when destroy took the callers away.
            val workersPosted = posts.map { it.worker }.toSet()
            harness.creates().filter { it.isCompleted() }.forEach { create ->
                assertTrue(
                    create.worker in workersPosted,
                    "$context: a published worker got no post"
                )
            }
            if (!destroys) {
                // No destroy: creation is requested for every name asked for, every block runs,
                // and no worker ever needs keeping alive.
                assertEquals(chosenExecutors.toSet(), createsByExecutor.keys, context)
                assertTrue(runs.all { it.get() == 1 }, "$context: a block never ran")
                assertEquals(emptyList<Harness.PostedBlock>(), noops, context)
            }
        }
    }

    /**
     * Drives one request through fakes that complete inline, and while the given callback is
     * running has another thread run [probeBody] against the cache. The probe is joined with a
     * bound, so a caller can tell a served probe from one the cache's monitor blocked.
     */
    private fun probeFromInside(
        callback: Callback,
        errors: MutableList<Throwable>,
        probeBody: (Harness) -> Unit
    ): Pair<Harness, Thread> {
        val armed = AtomicBoolean(true)
        var probe: Thread? = null
        var target: Harness? = null

        val hook = {
            if (armed.compareAndSet(true, false)) {
                val cache = target!!
                probe = newThread(errors) { probeBody(cache) }.apply {
                    isDaemon = true
                    start()
                    join(PROBE_TIMEOUT_MS)
                }
            }
        }
        val harness = Harness(
            completeInline = true,
            postInline = true,
            onCreate = if (callback == Callback.CREATE) hook else NO_HOOK,
            onPost = if (callback == Callback.POST) hook else NO_HOOK
        )
        target = harness

        harness.request("shared")

        return harness to (probe ?: throw AssertionError("the $callback callback never ran"))
    }

    private fun newThread(errors: MutableList<Throwable>, body: () -> Unit): Thread =
        Thread(Runnable { body() }).apply {
            setUncaughtExceptionHandler { _, error -> errors.add(error) }
        }

    private fun spin(iterations: Int) {
        repeat(iterations) { Thread.yield() }
    }

    private companion object {
        const val RACE_SEED = 20260909L
        const val PUBLISH_RACE_SEED = 424242L
        const val STRESS_SEED = 987654321L

        /** Generous enough that only a blocked probe times out, short enough not to hang the run. */
        const val PROBE_TIMEOUT_MS = 2000L

        val NO_HOOK: () -> Unit = {}
    }
}
