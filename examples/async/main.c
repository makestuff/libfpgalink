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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libfpgalink.h>
#include <liberror.h>
#include "args.h"
#include "unbounded_queue.h"
#include "thread.h"
#include <libusbwrap.h>
#ifdef WIN32
	#include <libusbx-1.0/libusb.h>
#else
	#include <libusb-1.0/libusb.h>
#endif

struct FLContext {
	struct USBDevice *device;
	bool isCommCapable;
	uint8 commOutEP;
	uint8 commInEP;
};

struct libusb_device_handle;

struct MyContext {
	struct libusb_device_handle *const device;
	const int outEndpoint;
	const int inEndpoint;
	struct UnboundedQueue requestQueue;
	struct UnboundedQueue responseQueue;
};

/*void myThreadFunc(struct MyContext *myContext) {
	struct UnboundedQueue *requestQueue = &myContext->requestQueue;
	struct UnboundedQueue *responseQueue = &myContext->responseQueue;
	size_t num = (size_t)queueTake(requestQueue);
	while ( num ) {
		num += 1000;
		queuePut(responseQueue, (void*)num);
		num = (size_t)queueTake(requestQueue);
	}
}*/

void myThreadFunc(struct MyContext *myContext) {
	struct UnboundedQueue *const requestQueue = &myContext->requestQueue;
	//struct UnboundedQueue *const responseQueue = &myContext->responseQueue;
	struct libusb_transfer *transfer = (struct libusb_transfer *)queueTake(requestQueue);
	while ( transfer ) {
		if ( transfer->endpoint & LIBUSB_ENDPOINT_IN ) {
			printf("flReadChannel(%d)\n", transfer->length);
		} else {
			printf("flWriteChannel("PFSZH", %d)\n", (size_t)transfer->buffer, transfer->length);
		}
		free(transfer->buffer);
		libusb_free_transfer(transfer);
		//queuePut(responseQueue, (void*)num);
		//num = (size_t)queueTake(requestQueue);
		transfer = (struct libusb_transfer *)queueTake(requestQueue);
	}
}

int writeData(struct MyContext *cxt, const uint8 *data, size_t length, const char **error) {
	int retVal = 0;
	struct libusb_transfer *transfer = NULL;
	uint8 *buf = NULL;

	// Allocate space for data
	buf = malloc(length);
	CHECK_STATUS(!buf, 101, cleanup, "writeData(): transfer buffer allocation failed");

	// Copy data over
	memcpy(buf, data, length);

	// Allocate transfer struct
	transfer = libusb_alloc_transfer(0);
	CHECK_STATUS(!transfer, 101, cleanup, "writeData(): transfer allocation failed");

	// Populate transfer struct
	libusb_fill_bulk_transfer(
		transfer,                                        // transfer struct
		cxt->device,                                     // USB device
		(uint8)(LIBUSB_ENDPOINT_OUT | cxt->outEndpoint), // endpoint to send to
		buf,                                             // data to send
		(int)length,                                     // number of bytes to send
		NULL,                                            // completion callback (populated later)
		NULL,                                            // user data
		0                                                // no timeout
	);

	// Put it on the work queue
	queuePut(&cxt->requestQueue, transfer);

cleanup:
	return retVal;
}

void shutdownWorker(struct MyContext *cxt) {
	queuePut(&cxt->requestQueue, NULL);
}

int doWriteRead(struct libusb_device_handle *device, uint8 inEndpoint, uint8 outEndpoint, const char **error) {
	int retVal = 0;
	Thread workerThread = 0;
	struct MyContext myContext = {device, outEndpoint, inEndpoint, {0,}, {0,}};
	struct UnboundedQueue *requestQueue = &myContext.requestQueue;
	struct UnboundedQueue *responseQueue = &myContext.responseQueue;
	int status;
	size_t i;

	status = queueInit(requestQueue, 16);
	CHECK_STATUS(status, status, cleanup, "doWriteRead(): failed to initialize queue");

	status = queueInit(responseQueue, 16);
	CHECK_STATUS(status, status, cleanup, "doWriteRead(): failed to initialize queue");

	status = threadCreate(&workerThread, (ThreadFunc)myThreadFunc, &myContext);
	CHECK_STATUS(status, status, cleanup, "doWriteRead(): failed to initialize queue");

	for ( i = 0; i < 1024; i++ ) {
		uint8 foo[1000];
		writeData(&myContext, foo, i, error);
		//queuePut(requestQueue, (void*)i);
		//num = (size_t)queueTake(responseQueue);
		//printf(PFSZD"\n", num);
	}

cleanup:
	shutdownWorker(&myContext);
	if ( workerThread ) {
		threadJoin(workerThread);
	}
	queueDestroy(responseQueue);
	queueDestroy(requestQueue);
	return retVal;
}

int main(int argc, const char *argv[]) {
	int retVal;
	struct FLContext *handle = NULL;
	FLStatus status;
	const char *error = NULL;
	bool flag;
	bool isNeroCapable, isCommCapable;
	uint8 *buffer = NULL;
	uint32 numDevices, scanChain[16], i;
	const char *vp = NULL, *ivp = NULL, *queryPort = NULL, *portConfig = NULL, *progConfig = NULL;
	const char *const prog = argv[0];

	printf("FPGALink \"C\" Example Copyright (C) 2011-2013 Chris McClelland\n\n");
	argv++;
	argc--;
	while ( argc ) {
		if ( argv[0][0] != '-' ) {
			unexpected(prog, *argv);
			FAIL(1, cleanup);
		}
		switch ( argv[0][1] ) {
		case 'h':
			usage(prog);
			FAIL(0, cleanup);
			break;
		case 'q':
			GET_ARG("q", queryPort, 2, cleanup);
			break;
		case 'd':
			GET_ARG("d", portConfig, 3, cleanup);
			break;
		case 'v':
			GET_ARG("v", vp, 4, cleanup);
			break;
		case 'i':
			GET_ARG("i", ivp, 5, cleanup);
			break;
		case 'p':
			GET_ARG("p", progConfig, 6, cleanup);
			break;
		default:
			invalid(prog, argv[0][1]);
			FAIL(8, cleanup);
		}
		argv++;
		argc--;
	}
	if ( !vp ) {
		missing(prog, "v <VID:PID>");
		FAIL(9, cleanup);
	}

	status = flInitialise(0, &error);
	CHECK_STATUS(status, 10, cleanup);
	
	printf("Attempting to open connection to FPGALink device %s...\n", vp);
	status = flOpen(vp, &handle, NULL);
	if ( status ) {
		if ( ivp ) {
			int count = 60;
			printf("Loading firmware into %s...\n", ivp);
			status = flLoadStandardFirmware(ivp, vp, &error);
			CHECK_STATUS(status, 11, cleanup);
			
			printf("Awaiting renumeration");
			flSleep(1000);
			do {
				printf(".");
				fflush(stdout);
				flSleep(100);
				status = flIsDeviceAvailable(vp, &flag, &error);
				CHECK_STATUS(status, 12, cleanup);
				count--;
			} while ( !flag && count );
			printf("\n");
			if ( !flag ) {
				fprintf(stderr, "FPGALink device did not renumerate properly as %s\n", vp);
				FAIL(13, cleanup);
			}
			
			printf("Attempting to open connection to FPGLink device %s again...\n", vp);
			status = flOpen(vp, &handle, &error);
			CHECK_STATUS(status, 14, cleanup);
		} else {
			fprintf(stderr, "Could not open FPGALink device at %s and no initial VID:PID was supplied\n", vp);
			FAIL(15, cleanup);
		}
	}
	
	if ( portConfig ) {
		printf("Configuring ports...\n");
		status = flPortConfig(handle, portConfig, &error);
		CHECK_STATUS(status, 16, cleanup);
		flSleep(100);
	}

	isNeroCapable = flIsNeroCapable(handle);
	isCommCapable = flIsCommCapable(handle);
	if ( queryPort ) {
		if ( isNeroCapable ) {
			status = jtagScanChain(handle, queryPort, &numDevices, scanChain, 16, &error);
			CHECK_STATUS(status, 17, cleanup);
			if ( numDevices ) {
				printf("The FPGALink device at %s scanned its JTAG chain, yielding:\n", vp);
				for ( i = 0; i < numDevices; i++ ) {
					printf("  0x%08X\n", scanChain[i]);
				}
			} else {
				printf("The FPGALink device at %s scanned its JTAG chain but did not find any attached devices\n", vp);
			}
		} else {
			fprintf(stderr, "JTAG chain scan requested but FPGALink device at %s does not support NeroJTAG\n", vp);
			FAIL(18, cleanup);
		}
	}

	if ( progConfig ) {
		printf("Executing programming configuration \"%s\" on FPGALink device %s...\n", progConfig, vp);
		if ( isNeroCapable ) {
			status = flProgram(handle, progConfig, NULL, &error);
			CHECK_STATUS(status, 19, cleanup);
		} else {
			fprintf(stderr, "Program operation requested but device at %s does not support NeroProg\n", vp);
			FAIL(20, cleanup);
		}
	}
	
	if ( isCommCapable ) {
		printf("Enabling FIFO mode...\n");
		status = flFifoMode(handle, true, &error);
		CHECK_STATUS(status, 21, cleanup);
		
		status = doWriteRead((struct libusb_device_handle *)handle->device, handle->commInEP, handle->commOutEP, &error);
		CHECK_STATUS(status, 22, cleanup);
	}
	retVal = 0;

cleanup:
	if ( error ) {
		fprintf(stderr, "%s\n", error);
		flFreeError(error);
	}
	flFreeFile(buffer);
	flClose(handle);
	return retVal;
}

void usage(const char *prog) {
	printf("Usage: %s [-h] [-i <VID:PID>] -v <VID:PID> [-d <portConfig>]\n", prog);
	printf("          [-q <jtagPort>] [-p <progConfig>]\n\n");
	printf("Load FX2LP firmware, load the FPGA, interact with the FPGA.\n\n");
	printf("  -i <VID:PID>    initial vendor and product ID of the FPGALink device\n");
	printf("  -v <VID:PID>    renumerated vendor and product ID of the FPGALink device\n");
	printf("  -d <portConfig> configure the ports\n");
	printf("  -q <jtagPort>   scan the JTAG chain\n");
	printf("  -p <progConfig> configuration and programming file\n");
	printf("  -h              print this help and exit\n");
}
