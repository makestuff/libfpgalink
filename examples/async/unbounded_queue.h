#ifndef UNBOUNDED_QUEUE_H
#define UNBOUNDED_QUEUE_H

#include "thread.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef const void* Item;

	struct UnboundedQueue {
		Item *itemArray;
		size_t capacity;
		size_t pushIndex;
		size_t popIndex;
		size_t numItems;
		Mutex mutex;
		ConditionVariable blocker;
	};

	int queueInit(struct UnboundedQueue *self, size_t capacity);
	int queuePut(struct UnboundedQueue *self, Item item); // never blocks, can ENOMEM
	Item queuePoll(struct UnboundedQueue *self); // returns NULL on empty
	Item queueTake(struct UnboundedQueue *self); // blocks on empty
	void queueDestroy(struct UnboundedQueue *self);

#ifdef __cplusplus
}
#endif

#endif
