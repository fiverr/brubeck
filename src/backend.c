#include <stddef.h>
#include <time.h>
#include "brubeck.h"

void brubeck_backend_register_metric(struct brubeck_backend *self, struct brubeck_metric *metric)
{
	for (;;) {
		struct brubeck_metric *next = self->queue;
		metric->next = next;

		if (__sync_bool_compare_and_swap(&self->queue, next, metric))
			break;
	}
}

/* Unlinks purged metrics from the queue and frees the ones unlinked on the previous flush.
 * Only this thread walks or unlinks queue nodes; workers only push new metrics at the head
 * (CAS on self->queue), so the head node is never unlinked here, only the ones after it. */
static void reap_purged(struct brubeck_backend *self)
{
	struct brubeck_metric *mt, *prev, *next;

	for (mt = self->to_free; mt; mt = next) {
		next = mt->next;
		brubeck_metric_free(mt);
	}
	self->to_free = self->unlinked;
	self->unlinked = NULL;

	prev = self->queue;
	if (!prev)
		return;

	for (mt = prev->next; mt; mt = next) {
		next = mt->next;
		if (__atomic_load_n(&mt->expire, __ATOMIC_ACQUIRE) == BRUBECK_EXPIRE_PURGED) {
			prev->next = next;
			mt->next = self->unlinked;
			self->unlinked = mt;
		} else {
			prev = mt;
		}
	}
}

static void *backend__thread(void *_ptr)
{
	struct brubeck_backend *self = (struct brubeck_backend *)_ptr;

	for (;;) {
		struct timespec now, then;

		clock_gettime(CLOCK_MONOTONIC, &then);
		then.tv_sec += self->sample_freq;

		if (!self->connect(self)) {
			struct brubeck_metric *mt;

			clock_gettime(CLOCK_REALTIME, &now);
			self->tick_time = now.tv_sec;

			for (mt = self->queue; mt; mt = mt->next) {
				if (brubeck_metric_is_live(mt))
					brubeck_metric_sample(mt, self->sample, self);
			}

			if (self->flush)
				self->flush(self);
		}

		reap_purged(self);

		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &then, NULL);
	}
	return NULL;
}

void brubeck_backend_run_threaded(struct brubeck_backend *self)
{
	if (pthread_create(&self->thread, NULL, &backend__thread, self) != 0)
		die("failed to start backend thread");
}

