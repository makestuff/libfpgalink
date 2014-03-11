package foo;

import eu.makestuff.fpgalink.FL;
import eu.makestuff.fpgalink.FL.BitOrder;
import eu.makestuff.fpgalink.FL.LogicalPort;
import eu.makestuff.fpgalink.FL.PinConfig;
import eu.makestuff.fpgalink.FLException;

public class Main {
	public static void main(String[] args) {
		final String VID_PID = "1d50:602b:0002";
		final String PROG_CONFIG = "D0D2D3D4";
		final int CONDUIT = 1;
		final byte[] BYTE_ARRAY = new byte[] {
			(byte)0xCA, (byte)0xFE, (byte)0xBA, (byte)0xBE
		};
		FL conn = null;
		try {
			FL.flInitialise(0);
			
			if ( !FL.flIsDeviceAvailable(VID_PID) ) {
				System.out.println("Loading firmware...");
				FL.flLoadStandardFirmware("04B4:8613", VID_PID);

				System.out.println("Awaiting...");
				FL.flAwaitDevice(VID_PID, 600);
			}

			conn = FL.flOpen(VID_PID);
			
			conn.flSingleBitPortAccess(3, 7, PinConfig.LOW);
			System.out.println("flMultiBitPortAccess() returned " + String.format("%32s", Integer.toBinaryString(
				conn.flMultiBitPortAccess("D7+"))).replace(" ", "0"));
			FL.flSleep(100);
			
			System.out.println("flGetFirmwareID(): " + String.format("%04X", conn.flGetFirmwareID()));
			System.out.println("flGetFirmwareVersion(): " + String.format("%08X", conn.flGetFirmwareVersion()));
			System.out.println("flIsNeroCapable(): " + conn.flIsNeroCapable());
			System.out.println("flIsCommCapable(): " + conn.flIsCommCapable(CONDUIT));

			conn.progOpen(PROG_CONFIG);
			System.out.println("progGetPort(): {");
			System.out.println("  MISO: " + conn.progGetPort(LogicalPort.MISO));
			System.out.println("  MOSI: " + conn.progGetPort(LogicalPort.MOSI));
			System.out.println("  SS:   " + conn.progGetPort(LogicalPort.SS));
			System.out.println("  SCK:  " + conn.progGetPort(LogicalPort.SCK));
			System.out.println("}");

			conn.jtagClockFSM(0x0000005F, 9);
			conn.jtagShiftInOnly(32, BYTE_ARRAY, false);
			byte[] bs = conn.jtagShiftInOut(128, FL.SHIFT_ONES, false);
			System.out.print("jtagShiftInOut() got " + bs.length + " bytes: {\n ");
			for ( int i = 0; i < bs.length; i++ ) {
				System.out.format(" %02X", bs[i]);
			}
			System.out.println("\n}");

			conn.jtagClockFSM(0x0000005F, 9);
			conn.spiSend(BYTE_ARRAY, BitOrder.LSBFIRST);
			bs = conn.spiRecv(8, BitOrder.LSBFIRST);
			System.out.print("spiRecv() got " + bs.length + " bytes: {\n ");
			for ( int i = 0; i < bs.length; i++ ) {
				System.out.format(" %02X", bs[i]);
			}
			System.out.println("\n}");
			conn.progClose();

			System.out.println("jtagScanChain(): {");
			int[] idCodes = conn.jtagScanChain(PROG_CONFIG);
			for ( int i = 0; i < idCodes.length; i++ ) {
				System.out.format("  0x%08X\n", idCodes[i]);
			}
			System.out.println("}");
			
			System.out.println("flProgram()...");
			conn.flProgram("J:" + PROG_CONFIG + ":../../../../hdlmake/apps/makestuff/swled/cksum/vhdl/fpga.xsvf");
			System.out.println("...done.");

			conn.flSelectConduit(CONDUIT);
			System.out.println("flIsFPGARunning(): " + conn.flIsFPGARunning());

			conn.flWriteChannel(0, BYTE_ARRAY);
			
			bs = conn.flReadChannel(1, 16);
			System.out.print("flReadChannel(1, 16) got " + bs.length + " bytes: {\n ");
			for ( int i = 0; i < bs.length; i++ ) {
				System.out.format(" %02X", bs[i]);
			}
			System.out.println("\n}");

			System.out.format("flReadChannel(2): %02X\n", conn.flReadChannel(2));
			
			conn.flReadChannelAsyncSubmit(0, 4);
			conn.flReadChannelAsyncSubmit(1, 8);
			conn.flReadChannelAsyncSubmit(2, 16);
			for ( int j = 0; j < 3; j++ ) {
				bs = conn.flReadChannelAsyncAwait();
				System.out.print("flReadChannelAsyncAwait() got " + bs.length + " bytes: {\n ");
				for ( int i = 0; i < bs.length; i++ ) {
					System.out.format(" %02X", bs[i]);
				}
				System.out.println("\n}");
			}
		}
		catch ( FLException ex ) {
			ex.printStackTrace();
		}
		finally {
			if ( conn != null ) {
				conn.flClose();
			}
		}
	}
}
