/*
 * Copyright (C) 2014 Chris McClelland
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
%module fl

%include "stdint.i"
%include "typemaps.i"

// -------------------------------------------------------------------------------------------------
// Init code...
//
%{
#include <libfpgalink.h>
#include <makestuff.h>

#define FL_ERROR(msg) sv_setpvf(get_sv("@", GV_ADD), "%s", msg)
#define FL_CHECK(x)         \
	if ( result ) {         \
		x;                  \
		FL_ERROR(error);    \
		flFreeError(error); \
		SWIG_fail;          \
	}
#define bitsToBytes(x) (((x)>>3) + ((x)&7 ? 1 : 0))
%}

// -------------------------------------------------------------------------------------------------
// Handle error message idiom
//
%typemap(in, numinputs=0) const char **OUTPUT {
	$1 = &error;
}
%typemap(out) FLStatus(char *error = NULL) {
	FL_CHECK();
}

// -------------------------------------------------------------------------------------------------
// Constants...
//
enum {
	LP_MISO,
	LP_MOSI,
	LP_SS,
	LP_SCK,
	PIN_HIGH,
	PIN_LOW,
	PIN_INPUT,
	SPI_MSBFIRST,
	SPI_LSBFIRST
};

// -------------------------------------------------------------------------------------------------
// Exposed API
//
FLStatus flInitialise(int debugLevel, const char **OUTPUT);

// The handle is returned in an "out" parameter, which needs to be given to the caller as a return.
%typemap(in, numinputs=0) struct FLContext **OUTPUT {
	$1 = &handle;
}
%typemap(out) FLStatus flOpen(struct FLContext *handle = NULL, char *error = NULL) {
	FL_CHECK();
	ST(argvi) = SWIG_NewPointerObj(SWIG_as_voidptr(handle), SWIGTYPE_p_FLContext, 0 | 0);
	argvi++;
}
FLStatus flOpen(
	const char *vp, struct FLContext **OUTPUT, const char **OUTPUT
);

void flClose(
	struct FLContext *handle
);

FLStatus flIsDeviceAvailable(
	const char *vp, uint8_t *OUTPUT, const char **OUTPUT
);

uint8_t flIsNeroCapable(
	struct FLContext *handle
);

uint8_t flIsCommCapable(
	struct FLContext *handle, uint8_t conduit
);

uint16_t flGetFirmwareID(
	struct FLContext *handle
);

uint32_t flGetFirmwareVersion(
	struct FLContext *handle
);

FLStatus flSelectConduit(
	struct FLContext *handle, uint8_t conduit, const char **OUTPUT
);

FLStatus flIsFPGARunning(
	struct FLContext *handle, uint8_t *OUTPUT, const char **OUTPUT
);

// Create a buffer, populate it with data, then give it to the caller as a return.
%typemap(default) size_t numBytes {
	$1 = 1;
}
%typemap(in, numinputs=0) uint8_t* {
	// arg4 created by the binding
}
%typemap(check) size_t {
	if ( arg3 == 1 ) {
		arg4 = &singleByte;
	} else {
		retVal = newSV(arg3);     // allocate enough space
		sv_2mortal(retVal);       // make it mortal
		sv_setpv(retVal, "");     // make it defined (i.e not undef)
		SvCUR_set(retVal, arg3);  // set its length to its capacity
		arg4 = SvPVX(retVal);     // get a raw uint8* pointing to its storage
	}
}
%typemap(out) FLStatus flReadChannel(SV *retVal = NULL, uint8 singleByte, char *error = NULL) {
	FL_CHECK();
	if ( arg3 == 1 ) {
		ST(argvi) = SWIG_From_unsigned_SS_char  SWIG_PERL_CALL_ARGS_1((unsigned char)(singleByte));
	} else {
		ST(argvi) = retVal;
	}
	argvi++;
}
FLStatus flReadChannel(
	struct FLContext *handle, uint8_t channel, size_t numBytes, uint8_t *, const char **OUTPUT
);
%typemap(check) size_t;
%typemap(in, numinputs=0) uint8_t*;
%typemap(default) size_t numBytes;

// Allow the caller to pass in a string/byte-array to send.
%apply (size_t LENGTH, const char *STRING) { (size_t numBytes, const uint8_t *sendData) };
FLStatus flWriteChannel(
	struct FLContext *handle, uint8_t channel, size_t numBytes, const uint8_t *sendData,
	const char **OUTPUT
);

FLStatus flSetAsyncWriteChunkSize(
	struct FLContext *handle, uint16_t chunkSize, const char **OUTPUT
);

// Allow the caller to pass in a string/byte-array to send.
%apply (size_t LENGTH, const char *STRING) { (size_t numBytes, const uint8_t *sendData) };
FLStatus flWriteChannelAsync(
	struct FLContext *handle, uint8_t channel, size_t numBytes, const uint8_t *sendData,
	const char **OUTPUT
);

FLStatus flFlushAsyncWrites(
	struct FLContext *handle, const char **OUTPUT
);

FLStatus flAwaitAsyncWrites(
	struct FLContext *handle, const char **OUTPUT
);

// Call flReadChannelAsyncSubmit() with NULL pointer (i.e use internal buffer).
%typemap(default) uint32_t numBytes {
	$1 = 1;
}
%typemap(in, numinputs=0) uint8_t* {
	// arg4 created by the binding
}
FLStatus flReadChannelAsyncSubmit(
	struct FLContext *handle, uint8_t channel, uint32_t numBytes, uint8_t *data, const char **OUTPUT
);
%typemap(default) uint32_t numBytes;
%typemap(in, numinputs=0) uint8_t*;

// Copy the returned data into a new byte-array and return it to the caller.
%typemap(in, numinputs=0) const uint8_t **ppData {
	$1 = &pData;
}
%typemap(in, numinputs=0) uint32_t *pRequestLength {
	$1 = &requestLength;
}
%typemap(in, numinputs=0) uint32_t *pActualLength {
	$1 = &actualLength;
}
%typemap(out) FLStatus flReadChannelAsyncAwait(
	char *error = NULL, const uint8 *pData, uint32 requestLength, uint32 actualLength, SV *retVal = NULL)
{
	FL_CHECK();
	retVal = newSV(actualLength);     // allocate enough space
	sv_2mortal(retVal);               // make it mortal
	sv_setpv(retVal, "");             // make it defined (i.e not undef)
	SvCUR_set(retVal, actualLength);  // set its length to the returned chunk-length
	memcpy(SvPVX(retVal), pData, actualLength); // copy the chunk into it
	ST(argvi) = retVal;
	argvi++;
}
FLStatus flReadChannelAsyncAwait(
	struct FLContext *handle, const uint8_t **ppData, uint32_t *pRequestLength,
	uint32_t *pActualLength, const char **OUTPUT
);
%typemap(in, numinputs=0) const uint8_t **ppData;
%typemap(in, numinputs=0) uint32_t *pRequestLength;
%typemap(in, numinputs=0) uint32_t *pActualLength;


// Default progFile to NULL (i.e assume programming file is specified in progConfig).
%typemap(default) const char *progFile {
	// If not supplied, progFile is NULL
}
FLStatus flProgram(
	struct FLContext *handle, const char *progConfig, const char *progFile, const char **OUTPUT
);
%typemap(default) const char *progFile;

// Allocate space for 16 IDs & populate it. If there are more than 16 devices, reallocate and retry.
%typemap(in, numinputs=0) uint32_t* {
	// arg3 & arg4 created by the binding
}
%typemap(in, numinputs=0) uint32_t {
	// arg5 created by the binding
}
%typemap(check) const char* {
	arg5 = 16; // initially guess 16 devices in chain
	arg4 = malloc(arg5 * sizeof(uint32));
	if ( !arg4 ) {
		FL_ERROR("jtagScanChain(): Failed to allocate temporary buffer");
		SWIG_fail;
	}
	arg3 = &arg5;
}
%typemap(out) FLStatus jtagScanChain(size_t i, char *error = NULL) {
	FL_CHECK(free(arg4));
	if ( arg5 > 16 ) {
		// Initial guess was bad, reallocate and try again
		free(arg4);
		arg4 = malloc(arg5 * sizeof(uint32));
		if ( !arg4 ) {
			FL_ERROR("jtagScanChain(): Failed to reallocate temporary buffer");
			SWIG_fail;
		}
		result = jtagScanChain(arg1, arg2, arg3, arg4, arg5, (const char **)&error);
		FL_CHECK(free(arg4));
	}
	for ( i = 0; i < arg5; i++ ) {
		ST(argvi) = sv_2mortal(newSViv(arg4[i]));
		argvi++;
	}
	free(arg4);
}
FLStatus jtagScanChain(
	struct FLContext *handle, const char *portConfig,
	uint32_t *numDevices, uint32_t *deviceArray, uint32_t arraySize,
	const char **OUTPUT
);
%typemap(in, numinputs=0) uint32_t*;
%typemap(in, numinputs=0) uint32_t;
%typemap(check) const char*;

FLStatus progOpen(
	struct FLContext *handle, const char *portConfig, const char **OUTPUT
);

FLStatus progClose(
	struct FLContext *handle, const char **OUTPUT
);

%typemap(default) uint8_t isLast {
	// If not supplied, isLast == 0 (false)
	$1 = 0;
}
FLStatus jtagShiftInOnly(
	struct FLContext *handle,
	uint32_t numBits, const char *tdiData, uint8_t isLast,
	const char **OUTPUT
);

%typemap(in, numinputs=0) uint8_t *tdoData {
	// arg4 created by the binding
}
%typemap(check) uint32_t {
	const uint32 numBytes = bitsToBytes(arg2);
	retVal = newSV(numBytes);     // allocate enough space
	sv_2mortal(retVal);           // make it mortal
	sv_setpv(retVal, "");         // make it defined (i.e not undef)
	SvCUR_set(retVal, numBytes);  // set its length to its capacity
	arg4 = SvPVX(retVal);         // get a raw uint8* pointing to its storage
}
%typemap(out) FLStatus jtagShiftInOut(SV *retVal = NULL, char *error = NULL) {
	FL_CHECK();
	ST(argvi) = retVal;
	argvi++;
}
FLStatus jtagShiftInOut(
	struct FLContext *handle,
	uint32_t numBits, const char *tdiData, uint8_t *tdoData, uint8_t isLast,
	const char **OUTPUT
);
%typemap(check) uint32_t;
%typemap(in, numinputs=0) uint8_t*;
%typemap(default) uint8_t isLast;

FLStatus jtagClockFSM(
	struct FLContext *handle, uint32_t bitPattern, uint8_t transitionCount, const char **OUTPUT
);

FLStatus jtagClocks(
	struct FLContext *handle, uint32_t numClocks, const char **OUTPUT
);

// Also call progGetBit and return a (port, bit) tuple.
%typemap(out) uint8_t progGetPort() {
	ST(argvi) = SWIG_From_unsigned_SS_char  SWIG_PERL_CALL_ARGS_1((unsigned char)(result));
	argvi++;
	ST(argvi) = SWIG_From_unsigned_SS_char  SWIG_PERL_CALL_ARGS_1((unsigned char)progGetBit(arg1,arg2));
	argvi++;
}
uint8_t progGetPort(
	struct FLContext *handle, uint8_t logicalPort
);

%apply (size_t LENGTH, const char *STRING) { (uint32_t numBytes, const uint8_t *sendData) };
FLStatus spiSend(
	struct FLContext *handle,
	uint32_t numBytes, const uint8_t *sendData, uint8_t bitOrder,
	const char **OUTPUT
);

%typemap(in, numinputs=0) uint8_t* {
	// arg3 created by the binding
}
%typemap(check) uint32_t {
	retVal = newSV(arg2);     // allocate enough space
	sv_2mortal(retVal);       // make it mortal
	sv_setpv(retVal, "");     // make it defined (i.e not undef)
	SvCUR_set(retVal, arg2);  // set its length to its capacity
	arg3 = SvPVX(retVal);     // get a raw uint8* pointing to its storage
}
%typemap(out) FLStatus spiRecv(SV *retVal = NULL, char *error = NULL) {
	FL_CHECK();
	ST(argvi) = retVal;
	argvi++;
}
FLStatus spiRecv(
	struct FLContext *handle, uint32_t numBytes, uint8_t *, uint8_t bitOrder,
	const char **OUTPUT
);
%typemap(in, numinputs=0) uint8_t*;
%typemap(check) uint32_t;

FLStatus flLoadStandardFirmware(
	const char *curVidPid, const char *newVidPid, const char **OUTPUT
);
FLStatus flFlashStandardFirmware(
	struct FLContext *handle, const char *newVidPid, const char **OUTPUT
);
FLStatus flLoadCustomFirmware(
	const char *curVidPid, const char *fwFile, const char **OUTPUT
);
FLStatus flFlashCustomFirmware(
	struct FLContext *handle, const char *fwFile, const char **OUTPUT
);
FLStatus flSaveFirmware(
	struct FLContext *handle, uint32_t eepromSize, const char *saveFile, const char **OUTPUT
);
FLStatus flBootloader(
	struct FLContext *handle, const char **OUTPUT
);

%typemap(in, numinputs=0) uint8_t* {
	// arg5 created by the binding
	arg5 = &singleByte;
}
%typemap(out) FLStatus flSingleBitPortAccess(uint8 singleByte, char *error = NULL) {
	FL_CHECK();
	ST(argvi) = SWIG_From_unsigned_SS_char  SWIG_PERL_CALL_ARGS_1((unsigned char)(singleByte));
	argvi++;
}
FLStatus flSingleBitPortAccess(
	struct FLContext *handle,
	uint8_t portNumber, uint8_t bitNumber, uint8_t pinConfig, uint8_t *pinRead,
	const char **OUTPUT
);
%typemap(in, numinputs=0) uint8_t*;

%typemap(in, numinputs=0) uint32_t* {
	arg3 = &singleU32;
}
%typemap(out) FLStatus flMultiBitPortAccess(uint32 singleU32, char *error = NULL) {
	FL_CHECK();
	ST(argvi) = SWIG_From_unsigned_SS_int  SWIG_PERL_CALL_ARGS_1((unsigned int)(singleU32));
	argvi++;
}
FLStatus flMultiBitPortAccess(
	struct FLContext *handle, const char *portConfig, uint32_t *readState, const char **OUTPUT
);

void flSleep(
	uint32_t ms
);
