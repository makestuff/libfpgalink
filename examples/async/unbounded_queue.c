#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "unbounded_queue.h"
#include "thread.h"
#include <makestuff.h>

int queueInit(struct UnboundedQueue *self, size_t capacity) {
	int retVal;
	int status;
	self->itemArray = (Item *)malloc(capacity * sizeof(Item));
	CHECK_STATUS(self->itemArray == NULL, ENOMEM, exit);
	self->capacity = capacity;
	self->pushIndex = 0;
	self->popIndex = 0;
	self->numItems = 0;
	status = threadMutexInit(&self->mutex);
	CHECK_STATUS(status, status, cleanMem);
	status = threadCondInit(&self->blocker);
	CHECK_STATUS(status, status, cleanMutex);
	return 0;
cleanMutex:
	threadMutexDestroy(&self->mutex);
cleanMem:
	free((void*)self->itemArray);
	self->itemArray = NULL;
exit:
	return retVal;
}

void queueDestroy(struct UnboundedQueue *self) {
	if ( self->itemArray ) {
		free((void*)self->itemArray);
		threadCondDestroy(&self->blocker);
		threadMutexDestroy(&self->mutex);
	}
}

// Everything is preserved if a realloc() fails
//
int queuePut(struct UnboundedQueue *self, Item item) {
	int retVal = 0;
	threadMutexLock(&self->mutex);
	if ( self->numItems == self->capacity ) {
		size_t i;
		Item *newArray;
		Item *const ptr = self->itemArray + self->popIndex;
		const size_t firstHalfLength = self->capacity - self->popIndex;
		const size_t secondHalfLength = self->popIndex;
		const size_t newCapacity = 2 * self->capacity;
		newArray = (Item *)malloc(newCapacity * sizeof(Item));
		CHECK_STATUS(newArray == NULL, ENOMEM, unlock);
		for ( i = 0; i < newCapacity; i++ ) {
			newArray[i] = (Item)(-1);
		}
		memcpy(newArray, ptr, firstHalfLength * sizeof(Item));
		if ( secondHalfLength ) {
			memcpy(
				newArray + firstHalfLength,
				self->itemArray,
				secondHalfLength * sizeof(Item)
			);
		}
		self->itemArray = newArray;
		self->popIndex = 0;
		self->pushIndex = self->capacity;
		self->capacity = newCapacity;
	}
	self->itemArray[self->pushIndex++] = item;
	if ( self->pushIndex == self->capacity ) {
		self->pushIndex = 0;
	}
	self->numItems++;
	threadCondSignal(&self->blocker);  // wake up consumer
unlock:
	threadMutexUnlock(&self->mutex);
	return retVal;
}

Item queueTake(struct UnboundedQueue *self) {
	Item item = NULL;
	threadMutexLock(&self->mutex);
	while ( self->numItems == 0 ) {
		threadCondWait(&self->blocker, &self->mutex);  // wait for producer
	}
	item = self->itemArray[self->popIndex++];
	if ( self->popIndex == self->capacity ) {
		self->popIndex = 0;
	}
	self->numItems--;
	threadMutexUnlock(&self->mutex);
	return item;
}

Item queuePoll(struct UnboundedQueue *self) {
	Item item = NULL;
	threadMutexLock(&self->mutex);
	if ( self->numItems > 0 ) {
		item = self->itemArray[self->popIndex++];
		if ( self->popIndex == self->capacity ) {
			self->popIndex = 0;
		}
		self->numItems--;
	}
	threadMutexUnlock(&self->mutex);
	return item;
}
