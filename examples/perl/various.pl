#!/usr/bin/env perl
#
# Copyright (C) 2014 Chris McClelland
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

use fl;
use strict;
use warnings;

my ($conn, $bs, $port, $bit, $i, $VID_PID, $PROG_CONFIG, $CONDUIT, $BYTE_ARRAY);

$VID_PID = "1d50:602b:0002";
$PROG_CONFIG = "D0D2D3D4";
$CONDUIT = 1;
$BYTE_ARRAY = "\xCA\xFE\xBA\xBE";
eval {
	fl::flInitialise(0);

	if ( !fl::flIsDeviceAvailable($VID_PID) ) {
		print "Loading firmware...\n";
		fl::flLoadStandardFirmware("04b4:8613", $VID_PID);

		print "Awaiting...\n";
		fl::flAwaitDevice($VID_PID, 600);
	}

	$conn = fl::flOpen($VID_PID);

	fl::flSingleBitPortAccess($conn, 3, 7, $fl::PIN_LOW);
	printf("flMultiBitPortAccess() returned %032b\n",
		fl::flMultiBitPortAccess($conn, "D7+"));
	fl::flSleep(100);

	printf("flGetFirmwareID(): %04X\n", fl::flGetFirmwareID($conn));
	printf("flGetFirmwareVersion(): %08X\n", fl::flGetFirmwareVersion($conn));
	printf("flIsNeroCapable(): %s\n", fl::flIsNeroCapable($conn) ? "True" : "False");
	printf("flIsCommCapable(): %s\n", fl::flIsCommCapable($conn, $CONDUIT) ? "True" : "False");

	fl::progOpen($conn, $PROG_CONFIG);
	print "progGetPort(): {\n";
	($port, $bit) = fl::progGetPort($conn, $fl::LP_MISO);
	printf("  MISO: %c%d\n", ord('A')+$port, $bit);
	($port, $bit) = fl::progGetPort($conn, $fl::LP_MOSI);
	printf("  MOSI: %c%d\n", ord('A')+$port, $bit);
	($port, $bit) = fl::progGetPort($conn, $fl::LP_SS);
	printf("  SS:   %c%d\n", ord('A')+$port, $bit);
	($port, $bit) = fl::progGetPort($conn, $fl::LP_SCK);
	printf("  SCK:  %c%d\n", ord('A')+$port, $bit);
	print "}\n";

	fl::jtagClockFSM($conn, 0x0000005F, 9);
	fl::jtagShiftInOnly($conn, 32, $BYTE_ARRAY);
	$bs = fl::jtagShiftInOut($conn, 128, "\xCA\xFE\xBA\xBE\x15\xF0\x0D\x1E\xFF\xFF\xFF\xFF\x00\x00\x00\x00");
	printf("jtagShiftInOut() got %d bytes: {\n  ", length($bs));
	foreach ( unpack("(a1)*", $bs) ) {
		printf(" %02X", ord);
	}
	print "\n}\n";

	fl::jtagClockFSM($conn, 0x0000005F, 9);
	fl::spiSend($conn, $BYTE_ARRAY, $fl::SPI_LSBFIRST);
	$bs = fl::spiRecv($conn, 8, $fl::SPI_LSBFIRST);
	printf("spiRecv() got %d bytes: {\n  ", length($bs));
	foreach ( unpack("(a1)*", $bs) ) {
		printf(" %02X", ord);
	}
	print "\n}\n";

	fl::progClose($conn);

	print "jtagScanChain(): {\n";
	foreach ( fl::jtagScanChain($conn, $PROG_CONFIG) ) {
		printf("  0x%08X\n", $_);
	}
	print "}\n";

	print "flProgram()...\n";
	fl::flProgram($conn, "J:$PROG_CONFIG:../../../../hdlmake/apps/makestuff/swled/cksum/vhdl/fpga.xsvf");
	print "...done.\n";

	fl::flSelectConduit($conn, $CONDUIT);
	printf("flIsFPGARunning(): %s\n", fl::flIsFPGARunning($conn) ? "True" : "False");

	fl::flWriteChannel($conn, 0, $BYTE_ARRAY);

	$bs = fl::flReadChannel($conn, 1, 16);
	printf("flReadChannel(1, 16) got %d bytes: {\n  ", length($bs));
	foreach ( unpack("(a1)*", $bs) ) {
		printf(" %02X", ord);
	}
	print "\n}\n";

	printf("flReadChannel(2): %02X\n", fl::flReadChannel($conn, 2));

	fl::flReadChannelAsyncSubmit($conn, 0, 4);
	fl::flReadChannelAsyncSubmit($conn, 1, 8);
	fl::flReadChannelAsyncSubmit($conn, 2, 16);
	for ( $i = 0; $i < 3; $i++ ) {
		$bs = fl::flReadChannelAsyncAwait($conn);
		printf("flReadChannelAsyncAwait() got %d bytes: {\n  ", length($bs));
		foreach(unpack("(a1)*", $bs)) {
			printf(" %02X", ord);
		}
		print "\n}\n";
	}
};
if ( $@ ) {
	warn "$@";
};
if ( defined($conn) ) {
	fl::flClose($conn);
}
