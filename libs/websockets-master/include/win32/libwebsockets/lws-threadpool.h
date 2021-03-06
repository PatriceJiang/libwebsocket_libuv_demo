/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010-2018 Andy Green <andy@warmcat.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation:
 *  version 2.1 of the License.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA
 *
 * included from libwebsockets.h
 */

/** \defgroup threadpool Threadpool related functions
 * ##Threadpool
 * \ingroup lwsapi
 *
 * This allows you to create one or more pool of threads which can run tasks
 * associated with a wsi.  If the pool is busy, tasks wait on a queue.
 *
 * Tasks don't have to be atomic, if they will take more than a few tens of ms
 * they should return back to the threadpool worker with a return of 0.  This
 * will allow them to abort cleanly.
 */
//@{

struct lws_threadpool;
struct lws_threadpool_task;

enum lws_threadpool_task_status {
	LWS_TP_STATUS_QUEUED,
	LWS_TP_STATUS_RUNNING,
	LWS_TP_STATUS_SYNCING,
	LWS_TP_STATUS_STOPPING,
	LWS_TP_STATUS_FINISHED, /* lws_threadpool_task_status() frees task */
	LWS_TP_STATUS_STOPPED, /* lws_threadpool_task_status() frees task */
};

enum lws_threadpool_task_return {
	/** Still work to do, just confirming not being stopped */
	LWS_TP_RETURN_CHECKING_IN,
	/** Still work to do, enter cond_wait until service thread syncs.  This
	 * is used if you have filled your buffer(s) of data to the service
	 * thread and are blocked until the service thread completes sending at
	 * least one.
	 */
	LWS_TP_RETURN_SYNC,
	/** No more work to do... */
	LWS_TP_RETURN_FINISHED,
	/** Responding to request to stop */
	LWS_TP_RETURN_STOPPED
};

struct lws_threadpool_create_args {
	const char *name;
	int threads;
	int max_queue_depth;
};

struct lws_threadpool_task_args {
	struct lws *wsi;	/**< user must set to wsi task is bound to */
	void *user;		/**< user may set (user-private pointer) */
	const char *name;	/**< user may set to describe task */
	enum lws_threadpool_task_return (*task)(void *user,
					enum lws_threadpool_task_status s);
	/**< user must set to actual task function */
	void (*cleanup)(struct lws *wsi, void *user);
	/**< socket lifecycle may end while task is not stoppable, so the task
	 * must be able to detach from any wsi and clean itself up when it does
	 * stop.  If NULL, no cleanup necessary, otherwise point to a user-
	 * supplied function that destroys the stuff in \p user.
	 *
	 * wsi may be NULL on entry, indicating the task got detached due to the
	 * wsi closing before.
	 */
};

/**
 * lws_threadpool_create() - create a pool of worker threads
 *
 * \param args: argument struct prepared by caller
 *
 * Creates a pool of worker threads with \p threads and a queue of up to
 * \p max_queue_depth waiting tasks if all the threads are busy.
 *
 * Returns NULL if OOM, or a struct lws_threadpool pointer that must be
 * destroyed by lws_threadpool_destroy().
 */
LWS_VISIBLE LWS_EXTERN struct lws_threadpool *
lws_threadpool_create(const struct lws_threadpool_create_args *args);

/**
 * lws_threadpool_finish() - Stop all pending and running tasks
 *
 * \param tp: the threadpool object
 *
 * Marks the threadpool as under destruction.  Removes everything from the
 * pending queue and completes those tasks as LWS_TP_STATUS_STOPPED.
 *
 * Running tasks will also get LWS_TP_STATUS_STOPPED as soon as they
 * "resurface".
 *
 * This doesn't reap tasks or free the threadpool, the reaping is done by the
 * lws_threadpool_task_status() on the done task.
 */
LWS_VISIBLE LWS_EXTERN void
lws_threadpool_finish(struct lws_threadpool *tp);

/**
 * lws_threadpool_destroy() - Destroy a threadpool
 *
 * \param tp: the threadpool object
 *
 * Waits for all worker threads to stop, ends the threads and frees the tp.
 */
LWS_VISIBLE LWS_EXTERN void
lws_threadpool_destroy(struct lws_threadpool *tp);

/**
 * lws_threadpool_enqueue() - Queue the task and run it on a worker thread when possible
 *
 * \param tp: the threadpool to queue / run on
 * \param args: information about what to run
 *
 * This asks for a task to run ASAP on a worker thread in threadpool \p tp.
 *
 * The args defines the wsi, a user-private pointer, a timeout in secs and
 * a pointer to the task function.
 *
 * Returns NULL or an opaque pointer to the queued (or running, or completed)
 * task.
 *
 * Once a task is created and enqueued, it can only be destroyed by calling
 * lws_threadpool_task_status() on it after it has reached the state
 * LWS_TP_STATUS_FINISHED or LWS_TP_STATUS_STOPPED.
 */
LWS_VISIBLE LWS_EXTERN struct lws_threadpool_task *
lws_threadpool_enqueue(struct lws_threadpool *tp,
		       const struct lws_threadpool_task_args *args);

/**
 * lws_threadpool_dequeue() - Dequeue or try to stop a running task
 *
 * \param task: the task to stop
 *
 * Returns 0 is the task was dequeued or already compeleted, or 1 if the task
 * has been asked to stop asynchronously.
 *
 * This doesn't free the task.  It only shortcuts it to state
 * LWS_TP_STATUS_STOPPED.  lws_threadpool_task_status() must be performed on
 * the task separately once it is in LWS_TP_STATUS_STOPPED to free the task.
 */
LWS_VISIBLE LWS_EXTERN int
lws_threadpool_dequeue(struct lws_threadpool_task *task);

/**
 * lws_threadpool_task_status() - Dequeue or try to stop a running task
 *
 * \param task: the task to query
 *
 * This is the equivalent of posix waitpid()... it returns the status of the
 * task, and if the task is in state LWS_TP_STATUS_FINISHED or
 * LWS_TP_STATUS_STOPPED, frees \p task.  If in another state, the task
 * continues to exist.
 *
 * This is designed to be called from the service thread.
 *
 * Its use is to make sure the service thread has seen the state of the task
 * before deleting it.
 */
LWS_VISIBLE LWS_EXTERN enum lws_threadpool_task_status
lws_threadpool_task_status(struct lws_threadpool_task *task);

/**
 * lws_threadpool_task_sync() - Indicate to a stalled task it may continue
 *
 * \param task: the task to unblock
 * \param stop: 0 = run after unblock, 1 = when he unblocks, stop him
 *
 * Inform the task that the service thread has finished with the shared data
 * and that the task, if blocked in LWS_TP_RETURN_SYNC, may continue.
 *
 * If the lws service context determined that the task must be aborted, it
 * should still call this but with stop = 1, causing the task to finish.
 */
LWS_VISIBLE LWS_EXTERN void
lws_threadpool_task_sync(struct lws_threadpool_task *task, int stop);

//@}
