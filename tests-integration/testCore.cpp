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
#include <string.h>
#include <UnitTest++.h>
#include "../libfpgalink.h"

#define FX2_VIDPID "04B4:8613"
#define AVR_VIDPID "03EB:3002"
#define MAGIC32 0xDEADDEAD

static struct FLContext *fx2Handle;
static struct FLContext *avrHandle;

TEST(FL_isDeviceAvailable) {
	FLStatus fStatus;
	bool isAvailable;
	const char *error = NULL;

	// Verify error message allocation
	fStatus = flIsDeviceAvailable("0000:0000", &isAvailable, &error);
	CHECK_EQUAL(FL_USB_ERR, fStatus);
	CHECK(error);
	flFreeError(error);

	// Verify that isAvailable remains unchanged on error
	isAvailable = false;
	fStatus = flIsDeviceAvailable("0000:0000", &isAvailable, NULL);
	CHECK_EQUAL(FL_USB_ERR, fStatus);
	CHECK_EQUAL(false, isAvailable);

	isAvailable = true;
	fStatus = flIsDeviceAvailable("0000:0000", &isAvailable, NULL);
	CHECK_EQUAL(FL_USB_ERR, fStatus);
	CHECK_EQUAL(true, isAvailable);

	// After this things should stop failing
	flInitialise();

	// Verify that a silly VID/PID is not found
	fStatus = flIsDeviceAvailable("0000:0000", &isAvailable, NULL);
	CHECK_EQUAL(FL_SUCCESS, fStatus);
	CHECK_EQUAL(false, isAvailable);

	// Verify that a good VID/PID IS found
	fStatus = flIsDeviceAvailable(FX2_VIDPID, &isAvailable, NULL);
	CHECK_EQUAL(FL_SUCCESS, fStatus);
	CHECK_EQUAL(true, isAvailable);

	// Verify that a good VID/PID IS found
	fStatus = flIsDeviceAvailable(AVR_VIDPID, &isAvailable, NULL);
	CHECK_EQUAL(FL_SUCCESS, fStatus);
	CHECK_EQUAL(true, isAvailable);
}

TEST(FL_openFails) {
	FLStatus fStatus;

	// Verify FL_USB_ERR on silly VID/PID:
	fx2Handle = (struct FLContext *)MAGIC32;
	fStatus = flOpen("0000:0000", &fx2Handle, NULL);
	CHECK_EQUAL(FL_USB_ERR, fStatus);
	CHECK_EQUAL((struct FLContext *)0UL, fx2Handle);

	// The firmware has not been loaded yet so we should get FL_PROTOCOL_ERR
	fx2Handle = (struct FLContext *)MAGIC32;
	fStatus = flOpen(FX2_VIDPID, &fx2Handle, NULL);
	CHECK_EQUAL(FL_PROTOCOL_ERR, fStatus);
	CHECK_EQUAL((struct FLContext *)0UL, fx2Handle);
}

TEST(FL_closeNull) {
	fx2Handle = (struct FLContext *)0;
	flClose(fx2Handle);
}

TEST(FL_loadCustomFirmware) {
	FLStatus fStatus;

	// Check bad file extension
	fStatus = flLoadCustomFirmware(FX2_VIDPID, "bad.dat", NULL);
	CHECK_EQUAL(FL_FILE_ERR, fStatus);

	// Check bad VID/PID
	fStatus = flLoadCustomFirmware("0000:0000", "nonExistentFile.hex", NULL);
	CHECK_EQUAL(FL_USB_ERR, fStatus);

	// Check bad hex file
	fStatus = flLoadCustomFirmware(FX2_VIDPID, "bad.hex", NULL);
	CHECK_EQUAL(FL_FILE_ERR, fStatus);

	// Check bad target device
	fStatus = flLoadCustomFirmware(AVR_VIDPID, "../gen_fw/ramFirmware1.hex", NULL);
	CHECK_EQUAL(FL_FX2_ERR, fStatus);
}

TEST(FL_loadStandardFirmware) {
	FLStatus fStatus;
	bool isAvailable;
	int count;

	// Verify that a silly current VID/PID gives an error
	fStatus = flLoadStandardFirmware("0000:0000", FX2_VIDPID, "D0234", NULL);
	CHECK_EQUAL(FL_USB_ERR, fStatus);

	// Verify that trying to load an FX2 firmware into an AVR gives an error
	fStatus = flLoadStandardFirmware(AVR_VIDPID, AVR_VIDPID, "D0234", NULL);
	CHECK_EQUAL(FL_FX2_ERR, fStatus);

	// Verify that a good current VID/PID works
	fStatus = flLoadStandardFirmware(FX2_VIDPID, FX2_VIDPID, "D0234", NULL);
	CHECK_EQUAL(FL_SUCCESS, fStatus);

	// We've got to go, cos if we don't go...
	count = 100;
	do {
		flSleep(100);
		fStatus = flIsDeviceAvailable(FX2_VIDPID, &isAvailable, NULL);
		CHECK_EQUAL(FL_SUCCESS, fStatus);
		count--;
	} while ( count && isAvailable );
	CHECK(!isAvailable);

	// ...we can't come back!
	count = 100;
	do {
		flSleep(100);
		fStatus = flIsDeviceAvailable(FX2_VIDPID, &isAvailable, NULL);
		CHECK_EQUAL(FL_SUCCESS, fStatus);
		count--;
	} while ( count && !isAvailable );
	CHECK(isAvailable);
}

TEST(FL_openSucceeds) {
	FLStatus fStatus;

	// Verify that the FX2 now opens correctly
	fx2Handle = (struct FLContext *)0;
	fStatus = flOpen(FX2_VIDPID, &fx2Handle, NULL);
	CHECK_EQUAL(FL_SUCCESS, fStatus);
	CHECK(fx2Handle);

	// Verify that the AVR opens correctly
	avrHandle = (struct FLContext *)0;
	fStatus = flOpen(AVR_VIDPID, &avrHandle, NULL);
	CHECK_EQUAL(FL_SUCCESS, fStatus);
	CHECK(avrHandle);
}

TEST(FL_deviceInUse) {
	FLStatus fStatus;

	// Device already open, so this should give FL_USB_ERR
	fStatus = flLoadStandardFirmware(FX2_VIDPID, FX2_VIDPID, "D0234", NULL);
	CHECK_EQUAL(FL_USB_ERR, fStatus);

	// This too
	fStatus = flLoadCustomFirmware(FX2_VIDPID, "../../temp/ramFirmware1.hex", NULL);
	CHECK_EQUAL(FL_USB_ERR, fStatus);
}

TEST(FL_verifyCapabilities) {
	// FX2 supports both CommFPGA and NeroJTAG
	CHECK(flIsCommCapable(fx2Handle));
	CHECK(flIsNeroCapable(fx2Handle));

	// AVR supports only NeroJTAG
	CHECK(!flIsCommCapable(avrHandle));
	CHECK(flIsNeroCapable(avrHandle));
}

TEST(FL_playXsvf) {
	FLStatus fStatus;

	// Verify that a file with a bad extension gives FL_FILE_ERR
	fStatus = flPlayXSVF(fx2Handle, "bad.dat", NULL);
	CHECK_EQUAL(FL_FILE_ERR, fStatus);

	// Verify that a non-existent XSVF file gives FL_BUF_LOAD_ERR
	fStatus = flPlayXSVF(fx2Handle, "nonExistentFile.xsvf", NULL);
	CHECK_EQUAL(FL_BUF_LOAD_ERR, fStatus);

	// Verify that a badly-formed XSVF file gives FL_UNSUPPORTED_CMD_ERR
	fStatus = flPlayXSVF(fx2Handle, "bad.xsvf", NULL);
	CHECK_EQUAL(FL_UNSUPPORTED_CMD_ERR, fStatus);

	// Verify that the AVR (having nothing on its JTAG lines) gives FL_JTAG_ERR because the XSVF's IDCODE check fails
	fStatus = flPlayXSVF(avrHandle, "../gen_xsvf/ex_cksum_s3board_vhdl.xsvf", NULL);
	CHECK_EQUAL(FL_JTAG_ERR, fStatus);

	// Verify that the FX2 (having an s3board on its JTAG lines) gives FL_JTAG_ERR because the XSVF's IDCODE check fails
	fStatus = flPlayXSVF(fx2Handle, "../gen_xsvf/ex_cksum_nexys2-1200_vhdl.xsvf", NULL);
	CHECK_EQUAL(FL_JTAG_ERR, fStatus);
}

TEST(FL_close) {
	flClose(avrHandle);
	avrHandle = (struct FLContext *)0;
	flClose(fx2Handle);
	fx2Handle = (struct FLContext *)0;
}
