/*
 * Copyright (C) 2009-2012 Chris McClelland
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <UnitTest++.h>
#include "../unbounded_queue.h"
#include <iostream>

using namespace std;

TEST(Queue_testInit) {
	struct UnboundedQueue queue;

	// Create queue
	int status = queueInit(&queue, 4);
	CHECK_EQUAL(0, status);

	// Verify
	CHECK_EQUAL(4UL, queue.capacity);
	CHECK_EQUAL(0UL, queue.pushIndex);
	CHECK_EQUAL(0UL, queue.popIndex);
	CHECK_EQUAL(0UL, queue.numItems);

	// Clean up
	queueDestroy(&queue);
}

TEST(Queue_testPutNoWrap) {
	const char *dummy = NULL;
	struct UnboundedQueue queue;

	// Create queue
	int status = queueInit(&queue, 4);
	CHECK_EQUAL(0, status);

	// Put three items into the queue
	status = queuePut(&queue, ++dummy);
	CHECK_EQUAL(0, status);
	status = queuePut(&queue, ++dummy);
	CHECK_EQUAL(0, status);
	status = queuePut(&queue, ++dummy);
	CHECK_EQUAL(0, status);

	// Verify
	CHECK_EQUAL(4UL, queue.capacity);
	CHECK_EQUAL(3UL, queue.pushIndex);
	CHECK_EQUAL(0UL, queue.popIndex);
	CHECK_EQUAL(3UL, queue.numItems);

	// Clean up
	queueDestroy(&queue);
}

TEST(Queue_testPutWrap) {
	const char *dummy = NULL;
	struct UnboundedQueue queue;

	// Create queue
	int status = queueInit(&queue, 4);
	CHECK_EQUAL(0, status);

	// Put four items into the queue
	status = queuePut(&queue, ++dummy);
	CHECK_EQUAL(0, status);
	status = queuePut(&queue, ++dummy);
	CHECK_EQUAL(0, status);
	status = queuePut(&queue, ++dummy);
	CHECK_EQUAL(0, status);
	status = queuePut(&queue, ++dummy);
	CHECK_EQUAL(0, status);

	// Verify
	CHECK_EQUAL(4UL, queue.capacity);
	CHECK_EQUAL(0UL, queue.pushIndex);
	CHECK_EQUAL(0UL, queue.popIndex);
	CHECK_EQUAL(4UL, queue.numItems);

	// Clean up
	queueDestroy(&queue);
}

TEST(Queue_testPutTake) {
	const char *dummy = NULL;
	struct UnboundedQueue queue;

	// Create queue
	int status = queueInit(&queue, 4);
	CHECK_EQUAL(0, status);

	// Put four items into the queue
	status = queuePut(&queue, ++dummy);
	CHECK_EQUAL(0, status);
	status = queuePut(&queue, ++dummy);
	CHECK_EQUAL(0, status);
	status = queuePut(&queue, ++dummy);
	CHECK_EQUAL(0, status);
	status = queuePut(&queue, ++dummy);
	CHECK_EQUAL(0, status);

	// Take four items out of the queue
	dummy = NULL;
	CHECK_EQUAL(queueTake(&queue), ++dummy);
	CHECK_EQUAL(queueTake(&queue), ++dummy);
	CHECK_EQUAL(queueTake(&queue), ++dummy);
	CHECK_EQUAL(queueTake(&queue), ++dummy);

	// Verify
	CHECK_EQUAL(4UL, queue.capacity);
	CHECK_EQUAL(0UL, queue.pushIndex);
	CHECK_EQUAL(0UL, queue.popIndex);
	CHECK_EQUAL(0UL, queue.numItems);

	// Clean up
	queueDestroy(&queue);
}

void testRealloc(const size_t offset) {
	const char *dummy = NULL;
	struct UnboundedQueue queue;

	//cout << "testRealloc(" << offset << ")" << endl;

	// Create queue
	int status = queueInit(&queue, 4);
	CHECK_EQUAL(0, status);

	// Shift indices so realloc has to do more work
	queue.pushIndex = offset;
	queue.popIndex = offset;

	// Now fill the queue to capacity
	dummy = NULL;
	status = queuePut(&queue, ++dummy);
	CHECK_EQUAL(0, status);
	status = queuePut(&queue, ++dummy);
	CHECK_EQUAL(0, status);
	status = queuePut(&queue, ++dummy);
	CHECK_EQUAL(0, status);
	status = queuePut(&queue, ++dummy);
	CHECK_EQUAL(0, status);

	// Verify
	CHECK_EQUAL(4UL, queue.capacity);
	CHECK_EQUAL(offset, queue.pushIndex);
	CHECK_EQUAL(offset, queue.popIndex);
	CHECK_EQUAL(4UL, queue.numItems);

	// Force reallocation
	status = queuePut(&queue, ++dummy);
	CHECK_EQUAL(0, status);

	// Verify
	CHECK_EQUAL(8UL, queue.capacity);
	CHECK_EQUAL(5UL, queue.pushIndex);
	CHECK_EQUAL(0UL, queue.popIndex);
	CHECK_EQUAL(5UL, queue.numItems);

	dummy = NULL;
	const Item expected[] = {
		++dummy, ++dummy, ++dummy, ++dummy, ++dummy
	};
	CHECK_ARRAY_EQUAL(expected, queue.itemArray, 5);

	dummy = NULL;
	CHECK_EQUAL(queueTake(&queue), ++dummy);
	CHECK_EQUAL(queueTake(&queue), ++dummy);
	CHECK_EQUAL(queueTake(&queue), ++dummy);
	CHECK_EQUAL(queueTake(&queue), ++dummy);
	CHECK_EQUAL(queueTake(&queue), ++dummy);

	CHECK_EQUAL(8UL, queue.capacity);
	CHECK_EQUAL(5UL, queue.pushIndex);
	CHECK_EQUAL(5UL, queue.popIndex);
	CHECK_EQUAL(0UL, queue.numItems);

	queueDestroy(&queue);
}

TEST(Queue_testRealloc) {
	testRealloc(0);
	testRealloc(1);
	testRealloc(2);
	testRealloc(3);
}
