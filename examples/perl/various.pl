#!/usr/bin/env perl

use fl;
use strict;
use warnings;

my ($handle, $bytes, $found, $port, $bit, $VID_PID, $PROG_CONFIG);

$VID_PID = "1d50:602b:0002";
$PROG_CONFIG = "D0D2D3D4";

eval {
	fl::flInitialise(0);

	if ( !fl::flIsDeviceAvailable($VID_PID) ) {
		print "Loading firmware...\n";
		fl::flLoadStandardFirmware("04b4:8613", $VID_PID);

		print "Awaiting $VID_PID...\n";
		$found = fl::flAwaitDevice($VID_PID, 600);
		print "Result: ".$found."\n";
	}

	$handle = fl::flOpen($VID_PID);

	fl::flSingleBitPortAccess($handle, 3, 7, $fl::PIN_LOW);
	printf("fl::flMultiBitPortAccess() returned 0x%08X\n", fl::flMultiBitPortAccess($handle, "D7+"));
	fl::flSleep(100);

	fl::flSelectConduit($handle, 1);
	print "fl::flIsFPGARunning(): ".fl::flIsFPGARunning($handle)."\n";
	print "fl::flIsNeroCapable(): ".fl::flIsNeroCapable($handle)."\n";
	print "fl::flIsCommCapable(): ".fl::flIsCommCapable($handle, 1)."\n";
	print "fl::flGetFirmwareID(): ".sprintf("%04X", fl::flGetFirmwareID($handle))."\n";
	printf("fl::flGetFirmwareVersion(): %08X\n", fl::flGetFirmwareVersion($handle));

	print "fl::flProgram()...\n";
	fl::flProgram($handle, "J:$PROG_CONFIG:../../../../hdlmake/apps/makestuff/swled/cksum/vhdl/fpga.xsvf");
	print "...done.\n";

	fl::progOpen($handle, $PROG_CONFIG);
	print "fl::progGetPort(): {\n";
	($port, $bit) = fl::progGetPort($handle, $fl::LP_MISO);
	printf("  MISO: %c%d\n", ord('A')+$port, $bit);
	($port, $bit) = fl::progGetPort($handle, $fl::LP_MOSI);
	printf("  MOSI: %c%d\n", ord('A')+$port, $bit);
	($port, $bit) = fl::progGetPort($handle, $fl::LP_SS);
	printf("  SS:   %c%d\n", ord('A')+$port, $bit);
	($port, $bit) = fl::progGetPort($handle, $fl::LP_SCK);
	printf("  SCK:  %c%d\n", ord('A')+$port, $bit);
	print "}\n";

	fl::jtagClockFSM($handle, 0x0000005F, 9);
	fl::jtagShiftInOnly($handle, 32, "\xCA\xFE\xBA\xBE");
	$bytes = fl::jtagShiftInOut($handle, 128, "\xCA\xFE\xBA\xBE\x15\xF0\x0D\x1E\xFF\xFF\xFF\xFF\x00\x00\x00\x00");
	printf("fl::jtagShiftInOut() got %d bytes: {\n  ", length($bytes));
	foreach ( unpack("(a1)*", $bytes) ) {
		printf(" %02X", ord);
	}
	print "\n}\n";

	fl::jtagClockFSM($handle, 0x0000005F, 9);
	fl::spiSend($handle, "\xCA\xFE\xBA\xBE", $fl::SPI_LSBFIRST);
	$bytes = fl::spiRecv($handle, 8, $fl::SPI_LSBFIRST);
	printf("fl::spiRecv() got %d bytes: {\n  ", length($bytes));
	foreach ( unpack("(a1)*", $bytes) ) {
		printf(" %02X", ord);
	}
	print "\n}\n";

	fl::progClose($handle);

	print "fl::jtagScanChain(): {\n";
	foreach ( fl::jtagScanChain($handle, $PROG_CONFIG) ) {
		printf("  0x%08X\n", $_);
	}
	print "}\n";

	fl::flWriteChannel($handle, 0, "\x01");

	$bytes = fl::flReadChannel($handle, 2, 16);
	printf("Sync read got %d bytes: {\n  ", length($bytes));
	foreach ( unpack("(a1)*", $bytes) ) {
		printf(" %02X", ord);
	}
	print "\n}\n";

	printf("Single byte read: %02X\n", fl::flReadChannel($handle, 2));

	#for ( ; ; ) {
	fl::flReadChannelAsyncSubmit($handle, 2, 16);
	fl::flReadChannelAsyncSubmit($handle, 2);
	$bytes = fl::flReadChannelAsyncAwait($handle);
	printf("First async read got %d bytes: {\n  ", length($bytes));
	foreach(unpack("(a1)*", $bytes)) {
		printf(" %02X", ord);
	}
	print "\n}\n";
	$bytes = fl::flReadChannelAsyncAwait($handle);
	printf("Second async read got %d bytes: {\n  ", length($bytes));
	foreach(unpack("(a1)*", $bytes)) {
		printf(" %02X", ord);
	}
	print "\n}\n";
	#}
};
if ( $@ ) {
	warn "WARNING: $@";
};
if ( defined($handle) ) {
	fl::flClose($handle);
}
