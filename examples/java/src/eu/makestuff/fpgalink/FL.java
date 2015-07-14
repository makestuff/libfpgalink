package eu.makestuff.fpgalink;

import com.sun.jna.Library;
import com.sun.jna.Memory;
import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.ByteByReference;
import com.sun.jna.ptr.IntByReference;
import com.sun.jna.ptr.PointerByReference;

public final class FL {

	/**
	 * Special value used by {@link #jtagShiftInOut(int, byte[], boolean) jtagShiftInOut()} to send
	 * a sequence of 0s into the JTAG chain.
	 */
	public static final byte[] SHIFT_ZEROS = new byte[0];

	/**
	 * Special value used by {@link #jtagShiftInOut(int, byte[], boolean) jtagShiftInOut()} to send
	 * a sequence of 1s into the JTAG chain.
	 */
	public static final byte[] SHIFT_ONES = new byte[0];

	/**
	 * Enum used by {@link #flSingleBitPortAccess(int, int, PinConfig) flSingleBitPortAccess()} to
	 * configure pin direction and drive.
	 */
	public static enum PinConfig {
		HIGH((byte)0x01),
		LOW((byte)0x02),
		INPUT((byte)0x03);
		final byte value;
		private PinConfig(byte v) { value = v; }
	}
	
	/**
	 * Enum used by {@link #spiSend(byte[], BitOrder) spiSend()} and
	 * {@link #spiRecv(int, BitOrder) spiRecv()} to choose bit-endianness.
	 */
	public static enum BitOrder {
		MSBFIRST((byte)0x01),
		LSBFIRST((byte)0x02);
		final byte value;
		private BitOrder(byte v) { value = v; }
	}

	/**
	 * Enum used by {@link #progGetPort(LogicalPort) progGetPort()} to identify the programming
	 * pins.
	 */
	public static enum LogicalPort {
		MISO((byte)0x01),
		MOSI((byte)0x02),
		SS((byte)0x03),
		SCK((byte)0x04);
		final byte value;
		private LogicalPort(byte v) { value = v; }
	}

	/**
	 * A typed pair representing a physical port on a microcontroller (e.g B4, E17).
	 */
	public static final class PhysicalPort {
		final int m_port;
		final int m_bit;
		PhysicalPort(int port, int bit) { m_port = port; m_bit = bit; }
		public int getPort() { return m_port; }
		public int getBit() { return m_bit; }
		public String toString() { return String.format("%c%d", m_port+'A', m_bit); }
	}

	/**
	 * The JNA wrapper interface for the FPGALink native library.
	 */
	private static interface NativeLib extends Library {
		int flInitialise(int debugLevel, PointerByReference pError);
		void flFreeError(Pointer error);
		int flOpen(String vp, PointerByReference pHandle, PointerByReference pError);
		void flClose(Pointer handle);
		int flIsDeviceAvailable(
			String vp, ByteByReference pIsAvailable, PointerByReference pError
		);
		byte flIsNeroCapable(Pointer handle);
		byte flIsCommCapable(Pointer handle, byte conduit);
		short flGetFirmwareID(Pointer handle);
		int flGetFirmwareVersion(Pointer handle);
		int flSelectConduit(Pointer handle, byte conduit, PointerByReference pError);
		int flIsFPGARunning(Pointer handle, ByteByReference pIsRunning, PointerByReference pError);
		int flReadChannel(
			Pointer handle, byte channel, NativeSize count, Pointer buffer,
			PointerByReference pError
		);
		int flWriteChannel(
			Pointer handle, byte channel, NativeSize count, Pointer sendData,
			PointerByReference pError
		);
		int flSetAsyncWriteChunkSize(Pointer handle, short chunkSize, PointerByReference pError);
		int flWriteChannelAsync(
			Pointer handle, byte channel, NativeSize count, Pointer sendData,
			PointerByReference pError
		);
		int flFlushAsyncWrites(Pointer handle, PointerByReference pError);
		int flAwaitAsyncWrites(Pointer handle, PointerByReference pError);
		int flReadChannelAsyncSubmit(
			Pointer handle, byte channel, int count, Pointer buffer, PointerByReference pError
		);
		int flReadChannelAsyncAwait(
			Pointer handle, PointerByReference pData, IntByReference pRequestLength,
			IntByReference pActualLength, PointerByReference pError
		);
		
		int flProgram(
			Pointer handle, String progConfig, String progFile, PointerByReference pError
		);
		int flProgramBlob(
			Pointer handle, String progConfig, int length, Pointer progData,
			PointerByReference pError
		);
		int jtagScanChain(
			Pointer handle, String portConfig, IntByReference pNumDevices, Pointer deviceArray,
			int arraySize, PointerByReference pError
		);
		int progOpen(Pointer handle, String portConfig, PointerByReference pError);
		int progClose(Pointer handle, PointerByReference pError);
		int jtagShiftInOnly(
			Pointer handle, int numBits, Pointer inData, byte isLast, PointerByReference pError
		);
		int jtagShiftInOut(
			Pointer handle, int numBits, Pointer inData, Pointer outData, byte isLast,
			PointerByReference pError
		);
		int jtagClockFSM(
			Pointer handle, int bitPattern, byte transitionCount, PointerByReference pError
		);
		int jtagClocks(Pointer handle, int numClocks, PointerByReference pError);
		byte progGetPort(Pointer handle, byte logicalPort);
		byte progGetBit(Pointer handle, byte logicalPort);
		int spiSend(
			Pointer handle, int numBytes, Pointer sendData, byte bitOrder,
			PointerByReference pError
		);
		int spiRecv(
			Pointer handle, int numBytes, Pointer buffer, byte bitOrder, PointerByReference pError
		);
		int flLoadStandardFirmware(String curVidPid, String newVidPid, PointerByReference pError);
		int flFlashStandardFirmware(Pointer handle, String newVidPid, PointerByReference pError);
		int flLoadCustomFirmware(String curVidPid, String fwFile, PointerByReference pError);
		int flFlashCustomFirmware(Pointer handle, String fwFile, PointerByReference pError);
		int flSaveFirmware(
			Pointer handle, int eepromSize, String saveFile, PointerByReference pError
		);
		int flBootloader(Pointer handle, PointerByReference pError);
		void flSleep(int ms);
		int flSingleBitPortAccess(
			Pointer handle, byte portNumber, byte bitNumber, byte pinConfig,
			ByteByReference pPinRead, PointerByReference pError
		);
		int flMultiBitPortAccess(
			Pointer handle, String portConfig, IntByReference pReadState, PointerByReference pError
		);
	}

	private static final NativeLib LIB;
	private static Memory m_buffer = null;
	private final Pointer m_handle;
	private FL(Pointer handle) {
		m_handle = handle;
	}
	@Override
	public void finalize() {
		flClose();
	}
	static {
		LIB = (NativeLib)Native.loadLibrary("fpgalink", NativeLib.class);
	}
	private static void checkThrow(int retCode, PointerByReference pError) {
		if ( retCode != 0 ) {
			Pointer errPtr = pError.getValue();
			String errStr = errPtr.getString(0);
			LIB.flFreeError(errPtr);
			throw new FLException(errStr, retCode);
		}
	}

	/**
	 * Initialise the library with the given log level.
	 * 
	 * <p>This may fail if LibUSB cannot talk to the USB host controllers through its kernel driver
	 * (e.g a Linux kernel with USB support disabled, or a machine lacking a USB host controller).
	 * </p>
	 * 
	 * @param debugLevel 0->none, 1, 2, 3->lots.
	 * @throws FLException if there were problems initialising LibUSB.
	 */
	public static void flInitialise(int debugLevel) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flInitialise(0, pError);
		checkThrow(retCode, pError);
	}

	/**
	 * Open a connection to the FPGALink device at the specified VID & PID.
	 * 
	 * <p>Connects to the device and verifies it's an FPGALink device, then queries its
	 * capabilities.</p>
	 * 
	 * @param vp The Vendor/Product (i.e VVVV:PPPP) of the FPGALink device. You may also specify
	 *           an optional device ID (e.g 1D50:602B:0004). If no device ID is supplied, it
	 *           selects the first device with matching VID:PID.
	 * @return A handle representing the device connection.
	 * @throws FLException if there was a native memory allocation failure.
	 * @throws FLException if the VID:PID is invalid or the device cannot be found or opened.
	 * @throws FLException if the device is not an FPGALink device.
	 */
	public static FL flOpen(String vp) {
		PointerByReference pError = new PointerByReference();
		PointerByReference pHandle = new PointerByReference();
		int retCode = LIB.flOpen(vp, pHandle, pError);
		checkThrow(retCode, pError);
		return new FL(pHandle.getValue());
	}

	/**
	 * Close an existing connection to an FPGALink device.
	 * 
	 * <p>This should be done deterministically (e.g in a {@code finally} block).</p>
	 */
	public void flClose() {
		LIB.flClose(m_handle);
	}
	
	/**
	 * Check if the given device is actually connected to the system.
	 * 
	 * <p>The LibUSB devices in the system are searched for a device with the given VID:PID.</p>
	 * 
	 * <p>There is a short period of time following a call to
	 * {@link #flLoadStandardFirmware(String, String) flLoadStandardFirmware()} or
	 * {@link #flLoadCustomFirmware(String, String) flLoadCustomFirmware()} during which this
	 * function will still return {@code true} for the "current" VID:PID, so when you load new
	 * firmware, it's important to either wait a while before calling this function, or
	 * alternatively just ensure that the "new" VID:PID is different from the "current" VID:PID to
	 * avoid such false positives.</p>
	 * 
	 * @param vp The Vendor/Product (i.e VVVV:PPPP) of the FPGALink device. You may also specify an
	 *            optional device ID (e.g 1D50:602B:0004). If no device ID is supplied, it selects
	 *            the first device with matching VID:PID. 
	 * @return {@code true} if the specified device is connected, else {@code false}.
	 * @throws FLException if the VID:PID is invalid or if no USB buses were found (did you
	 *             remember to call {@link #flInitialise(int) flInitialise()}?).
	 */
	public static boolean flIsDeviceAvailable(String vp) {
		ByteByReference pIsAvail = new ByteByReference();
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flIsDeviceAvailable(vp, pIsAvail, pError);
		checkThrow(retCode, pError);
		return pIsAvail.getValue() != (byte)0x00;
	}

	/**
	 * Await renumeration; throw on timeout.
	 * 
	 * <p>This function will wait for the specified VID:PID to be added to the system (either due
	 * to a renumerating device, or due to a new physical connection). It will wait for a fixed
	 * period of 1s and then start polling the USB bus looking for the specified device. If no such
	 * device is detected within {@code timeout} deciseconds after the initial delay, it
	 * throws {@code FLException}.</p>
	 * 
	 * @param vp The Vendor/Product (i.e VVVV:PPPP) of the FPGALink device. You may also specify an
	 *            optional device ID (e.g 1D50:602B:0004). If no device ID is supplied, it awaits
	 *            the first device with matching VID:PID.
	 * @param timeout The number of tenths-of-a-second to wait, after the initial 1s delay.
	 * @throws FLException if the VID:PID is invalid or if no USB buses were found (did you
	 *             remember to call {@link #flInitialise(int) flInitialise()}?).
	 * @throws FLException if the device did not become available within the specified time.
	 */
	public static void flAwaitDevice(String vp, int timeout) {
		boolean isAvailable;
		flSleep(1000);
		do {
			flSleep(100);
			isAvailable = flIsDeviceAvailable(vp);
			timeout = timeout - 1;
			if ( isAvailable ) {
				return;
			}
		} while ( timeout > 0 );
		throw new FLException("flAwaitDevice(): Timed out waiting for USB device", 0);
	}

	/**
	 * Check to see if the device supports NeroProg.
	 * 
	 * <p>NeroProg is the collective name for all the various programming algorithms supported by
	 * FPGALink, including but not limited to JTAG. An affirmative response means you are free to
	 * call {@link #flProgram(String) flProgram()},
	 * {@link #flProgramBlob(String, byte[]) flProgramBlob()},
	 * {@link #jtagScanChain(String) jtagScanChain()}, {@link #progOpen(String) progOpen()},
	 * {@link #progClose() progClose()},
	 * {@link #jtagShiftInOnly(int, byte[], boolean) jtagShiftInOnly()},
	 * {@link #jtagShiftInOut(int, byte[], boolean) jtagShiftInOut()},
	 * {@link #jtagClockFSM(int, int) jtagClockFSM()}, {@link #jtagClocks(int) jtagClocks()},
	 * {@link #progGetPort(LogicalPort) progGetPort()},
	 * {@link #spiSend(byte[], BitOrder) spiSend()} and {@link #spiRecv(int, BitOrder) spiRecv()}.
	 * </p>
	 * 
	 * <p>This function merely returns a flag determined by {@link #flOpen(String) flOpen()}, so it
	 * cannot fail.</p>
	 * 
	 * @return {@code true} if the device supports NeroProg, else {@code false}.
	 */
	public boolean flIsNeroCapable() {
		return LIB.flIsNeroCapable(m_handle) != (byte)0x00;
	}
	
	/**
	 * Check to see if the device supports CommFPGA.
	 * 
	 * <p>CommFPGA is a set of channel read/write protocols. The micro may implement several
	 * different CommFPGA protocols, distinguished by the chosen conduit. A micro will typically
	 * implement its first CommFPGA protocol on conduit 1, and additional protocols on conduit 2,
	 * 3 etc. Conduit 0 is reserved for communication over JTAG using a virtual TAP state machine
	 * implemented in the FPGA, and is not implemented yet.</p>
	 * 
	 * <p>This function returns {@code true} if the micro supports CommFPGA on the chosen
	 * conduit, else {@code false}.
	 * 
	 * <p>Note that this function can only know the capabilities of the micro itself; it cannot
	 * determine whether the FPGA contains suitable logic to implement the protocol, or even
	 * whether there is an FPGA physically wired to the micro in the first place.<p>
	 * 
	 * <p>An affirmative response means you are free to call
	 * {@link #flIsFPGARunning() flIsFPGARunning()}, {@link #flReadChannel(int, int) flReadChannel()},
	 * {@link #flWriteChannel(int, byte[]) flWriteChannel()},
	 * {@link #flSetAsyncWriteChunkSize(int) flSetAsyncWriteChunkSize()},
	 * {@link #flWriteChannelAsync(int, byte[]) flWriteChannelAsync()},
	 * {@link #flFlushAsyncWrites() flFlushAsyncWrites()},
	 * {@link #flAwaitAsyncWrites() flAwaitAsyncWrites()},
	 * {@link #flReadChannelAsyncSubmit(int, int) flReadChannelAsyncSubmit()}, and
	 * {@link #flReadChannelAsyncAwait() flReadChannelAsyncAwait()}.</p>
	 * 
	 * <p>This function merely returns information determined by {@link #flOpen(String) flOpen()},
	 * so it cannot fail.</p>
	 * 
	 * @param conduit The conduit you're interested in (this will typically be 1).
	 * @return {@code true} if the device supports CommFPGA on the specified conduit, else
	 *         {@code false}.
	 */
	public boolean flIsCommCapable(int conduit) {
		return LIB.flIsCommCapable(m_handle, (byte)conduit) != (byte)0x00;
	}
	
	/**
	 * Get the firmware ID.
	 * 
	 * <p>Each firmware (or fork of an existing firmware) has its own 16-bit ID, which this function
	 * retrieves.</p>
	 *
	 * <p>This function merely returns information determined by {@link #flOpen(String) flOpen()},
	 * so it cannot fail.</p>
	 * 
	 * @return A 16-bit unsigned integer giving the firmware ID.
	 */
	public short flGetFirmwareID() {
		return LIB.flGetFirmwareID(m_handle);
	}

	/**
	 * Get the firmware version.
	 * 
	 * <p>Each firmware knows the GitHub tag from which is was built, or if it was built from a
	 * trunk, it knows the date on which it was built. This function returns a 32-bit integer
	 * giving that information. If printed as a hex number, it gives an eight-digit ISO date.</p>
	 * 
	 * <p>This function merely returns information determined by {@link #flOpen(String) flOpen()},
	 * so it cannot fail.</p>
	 * 
	 * @return A 32-bit unsigned integer giving the firmware version.
	 */
	public int flGetFirmwareVersion() {
		return LIB.flGetFirmwareVersion(m_handle);
	}
	
	/**
	 * Select a different conduit.
	 *
	 * <p>Select a different conduit for CommFPGA communication. Typically a micro will implement
	 * its first CommFPGA protocol on conduit 1. It may or may not also implement others on conduit
	 * 2, 3, 4 etc. It may also implement comms-over-JTAG using a virtual TAP FSM on the FPGA. You
	 * can use {@link #flIsCommCapable(int) flIsCommCapable()} to determine whether the micro
	 * supports CommFPGA on a given conduit.</p>
	 * 
	 * <p>If mixing NeroProg operations with CommFPGA operations, it <b>may</b> be necessary to switch
	 * conduits. For example, if your PCB is wired to use some of the CommFPGA signals during
	 * programming, you will have to switch back and forth. But if the pins used for CommFPGA are
	 * independent of the pins used for NeroProg, you need only select the correct conduit on
	 * startup and then leave it alone.</p>
	 * 
	 * @param conduit The conduit to select (current range 0-15).
	 * @throws FLException if the device doesn't respond, or the conduit is out of range.
	 */
	public void flSelectConduit(int conduit) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flSelectConduit(m_handle, (byte)conduit, pError);
		checkThrow(retCode, pError);
	}
	
	/**
	 * Check to see if the FPGA is running.
	 * 
	 * <p>This may only be called if {@link #flIsCommCapable(int) flIsCommCapable()} returns
	 * {@code true}. It merely verifies that the FPGA is asserting that it's ready to read commands
	 * on the chosen conduit. Some conduits may not have the capability to determine this, and will
	 * therefore just optimistically report {@code true}. Before calling this method you should
	 * verify that the FPGALink device actually supports CommFPGA using
	 * {@link #flIsCommCapable(int) flIsCommCapable()}, and select the conduit you wish to use with
	 * {@link #flSelectConduit(int) flSelectConduit()}.</p>
	 * 
	 * @return {@code true} if the FPGA is ready to accept commands on the chosen conduit, else
	 *         {@code false}.
	 * @throws FLException if the device does not support CommFPGA.
	 */
	public boolean flIsFPGARunning() {
		ByteByReference pIsRunning = new ByteByReference();
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flIsFPGARunning(m_handle, pIsRunning, pError);
		checkThrow(retCode, pError);
		return pIsRunning.getValue() != (byte)0x00;
	}
	
	/**
	 * Low-level synchronous read-channel method.
	 * 
	 * <p>See {@link #flReadChannel(int, int) flReadChannel()} for a more user-friendly equivalent.</p>
	 * 
	 * @param channel The FPGA channel to read (0-127).
	 * @param buffer An allocated {@link Memory} instance to be populated.
	 * @param numBytes The number of bytes to read.
	 * @throws FLException if a USB read or write error occurred.
	 * @throws FLException if the device does not support CommFPGA.
	 */
	public void rawReadChannel(int channel, Memory buffer, int numBytes) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flReadChannel(m_handle, (byte)channel, new NativeSize(numBytes), buffer, pError);
		checkThrow(retCode, pError);
	}

	/**
	 * Low-level synchronous write-channel method.
	 * 
	 * <p>See {@link #flWriteChannel(int, byte[]) flWriteChannel()} for a more user-friendly
	 * equivalent.</p>
	 * 
	 * @param channel The FPGA channel to write (0-127).
	 * @param sendData An allocated {@link Memory} instance containing data to be sent to the FPGA.
	 * @param numBytes The number of bytes to write.
	 * @throws FLException if a native allocation error occurred.
	 * @throws FLException if a USB write error occurred.
	 * @throws FLException if the device does not support CommFPGA.
	 * @throws FLException if there are async reads in progress.
	 */
	public void rawWriteChannel(int channel, Memory sendData, int numBytes) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flWriteChannel(m_handle, (byte)channel, new NativeSize(numBytes), sendData, pError);
		checkThrow(retCode, pError);
	}
	
	/**
	 * Low-level asynchronous write-channel method.
	 * 
	 * <p>See {@link #flWriteChannelAsync(int, byte[]) flWriteChannelAsync()} for a more
	 * user-friendly equivalent.</p>
	 * 
	 * <p>This function is asynchronous. That means it will return immediately, usually before
	 * anything has been actually sent over USB. If the operation fails, you will not be notified
	 * of the failure until a future call to {@link #flAwaitAsyncWrites() flAwaitAsyncWrites()} or
	 * {@link #flReadChannelAsyncAwait() flReadChannelAsyncAwait()}. The data is copied internally,
	 * so there's no need to worry about preserving the {@link Memory} buffer after the call.</p>
	 * 
	 * @param channel The FPGA channel to write (0-127).
	 * @param sendData An allocated {@link Memory} instance containing data to be sent to the FPGA.
	 * @param numBytes The number of bytes to write.
	 * @throws FLException if a native allocation error occurred.
	 * @throws FLException if a USB write error occurred.
	 * @throws FLException if the device does not support CommFPGA.
	 */
	public void rawWriteChannelAsync(int channel, Memory sendData, int numBytes) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flWriteChannelAsync(m_handle, (byte)channel, new NativeSize(numBytes), sendData, pError);
		checkThrow(retCode, pError);
	}

	/**
	 * Synchronously read one or more bytes from the specified channel.
	 * 
	 * <p>Read {@code numBytes} bytes from the FPGA channel {@code channel} and return them in a
	 * new {@code byte[]}. Before calling this method you should verify that the FPGALink device
	 * actually supports CommFPGA using {@link #flIsCommCapable(int) flIsCommCapable()}.</p>
	 * 
	 * <p>Because this function is synchronous, it will block until the data has been returned. You
	 * must not use this function between an async read submit...await pair.</p>
	 * 
	 * @param channel The FPGA channel to read (0-127).
	 * @param numBytes The number of bytes to read.
	 * @return A {@code byte[]} containing the data that was read from the FPGA.
	 * @throws FLException if a USB read or write error occurred.
	 * @throws FLException if the device does not support CommFPGA.
	 */
	public byte[] flReadChannel(int channel, int numBytes) {
		if ( m_buffer == null || m_buffer.size() < numBytes ) {
			m_buffer = new Memory(numBytes);
		}
		rawReadChannel(channel, m_buffer, numBytes);
		return m_buffer.getByteArray(0, numBytes);
	}

	/**
	 * Synchronously read one byte from the specified channel.
	 * 
	 * <p>Read one byte from the FPGA channel {@code channel} and return it to the caller. Before
	 * calling this method you should verify that the FPGALink device actually supports CommFPGA
	 * using {@link #flIsCommCapable(int) flIsCommCapable()}.</p>
	 * 
	 * <p>Because this function is synchronous, it will block until the data has been returned. You
	 * must not use this function between an async read submit...await pair.</p>
	 * 
	 * @param channel The FPGA channel to read (0-127).
	 * @return The {@code byte} read from the FPGA.
	 * @throws FLException if a USB read or write error occurred.
	 * @throws FLException if the device does not support CommFPGA.
	 */
	public byte flReadChannel(int channel) {
		if ( m_buffer == null || m_buffer.size() < 1024 ) {
			m_buffer = new Memory(1024);
		}
		rawReadChannel(channel, m_buffer, 1);
		return m_buffer.getByte(0);
	}
	
	/**
	 * Synchronously write one or more bytes to the specified channel.
	 * 
	 * <p>Write the {@code sendData} byte-array to FPGA channel {@code channel}. Before calling
	 * this method you should verify that the FPGALink device actually supports CommFPGA using
	 * {@link #flIsCommCapable(int) flIsCommCapable()}.</p>
	 * 
	 * <p>Because this function is synchronous, it will block until the OS has confirmed that the data
	 * has been correctly sent over USB and received by the micro. It cannot confirm that the data
	 * has been received by the FPGA however: it may be waiting in the micro's output buffer.</p>
	 * 
	 * @param channel The FPGA channel to write (0-127).
	 * @param sendData The bytes to be written to the FPGA.
	 * @throws FLException if a native allocation error occurred.
	 * @throws FLException if a USB write error occurred.
	 * @throws FLException if the device does not support CommFPGA.
	 * @throws FLException if there are async reads in progress.
	 */
	public void flWriteChannel(int channel, byte[] sendData) {
		if ( m_buffer == null || m_buffer.size() < sendData.length ) {
			m_buffer = new Memory(sendData.length);
		}
		m_buffer.write(0, sendData, 0, sendData.length);
		rawWriteChannel(channel, m_buffer, sendData.length);
	}
	
	/**
	 * Set the chunk size to be used for future async writes.
	 * 
	 * <p>By default, the {@link #flWriteChannelAsync(int, byte[]) flWriteChannelAsync()} function
	 * buffers up to 64KiB of data before sending anything over USB. Chunking the data in this way
	 * is more efficient than sending lots of little messages. However, the choice of chunk size
	 * affects the steady-state throughput in interesting ways. If you need to, you can choose to
	 * make the chunks smaller than 64KiB.</p>
	 * 
	 * <p>You should not call this when there is some send data buffered. You should either call
	 * this before the first call to {@link #flWriteChannelAsync(int, byte[]) flWriteChannelAsync()},
	 * or call it immediately after a call to {@link #flFlushAsyncWrites() flFlushAsyncWrites()}.
	 * </p>
	 * 
	 * @param chunkSize The new chunk size, where 0 < chunkSize <= 64KiB.
	 * @throws FLException if there is some outstanding send data.
	 * @throws FLException if {@code chunkSize} > 0x10000.
	 */
	public void flSetAsyncWriteChunkSize(int chunkSize) {
		if ( chunkSize > 0x10000 ) {
			throw new FLException("flSetAsyncWriteChunkSize(): chunk sizes > 0x10000 are illegal", 0);
		}
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flSetAsyncWriteChunkSize(m_handle, (short)chunkSize, pError);
		checkThrow(retCode, pError);
	}
	
	/**
	 * Asynchronously write one or more bytes to the specified channel.
	 * 
	 * <p>Write the {@code sendData} byte-array to FPGA channel {@code channel}. Before calling
	 * this method you should verify that the FPGALink device actually supports CommFPGA using
	 * {@link #flIsCommCapable(int) flIsCommCapable()}.</p>
	 * 
	 * <p>This function is asynchronous. That means it will return immediately, usually before
	 * anything has been actually sent over USB. If the operation fails, you will not be notified
	 * of the failure until a future call to {@link #flAwaitAsyncWrites() flAwaitAsyncWrites()} or
	 * {@link #flReadChannelAsyncAwait() flReadChannelAsyncAwait()}. The data is copied internally,
	 * so there's no need to worry about preserving the data in the array after the call returns.
	 * </p>
	 * 
	 * @param channel The FPGA channel to write (0-127).
	 * @param sendData The bytes to be written to the FPGA.
	 * @throws FLException if there was a native memory allocation failure.
	 * @throws FLException if a USB write error occurred.
	 * @throws FLException if the device does not support CommFPGA. 
	 */
	public void flWriteChannelAsync(int channel, byte[] sendData) {
		if ( m_buffer == null || m_buffer.size() < sendData.length ) {
			m_buffer = new Memory(sendData.length);
		}
		m_buffer.write(0, sendData, 0, sendData.length);
		rawWriteChannelAsync(channel, m_buffer, sendData.length);
	}
	
	/**
	 * Flush out any pending asynchronous writes.
	 * 
	 * Flush any writes that have been buffered up, or do nothing if no writes have been buffered.
	 * This only triggers the send over USB; it does not guarantee the micro successfully received
	 * the data. See {@link #flAwaitAsyncWrites() flAwaitAsyncWrites()} for that.
	 * 
	 * @throws FLException if a USB write error occurred.
	 * @throws FLException if the device does not support CommFPGA.
	 */
	public void flFlushAsyncWrites() {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flFlushAsyncWrites(m_handle, pError);
		checkThrow(retCode, pError);
	}

	/**
	 * Wait for confirmation that pending asynchronous writes were received by the micro.
	 * 
	 * <p>The first thing this does is to call {@link #flFlushAsyncWrites() flFlushAsyncWrites()}
	 * to flush out any outstanding write commands. It will then block until the OS confirms that
	 * all the asynchronous write commands sent by
	 * {@link #flWriteChannelAsync(int, byte[]) flWriteChannelAsync()} were correctly sent over USB
	 * and received by the micro. It cannot confirm that that the writes were received by the FPGA
	 * however: they may be waiting in the micro's output buffer.</p>
	 * 
	 * @throws FLException if one of the outstanding async operations failed.
	 * @throws FLException if the device does not support CommFPGA.
	 * @throws FLException if there are async reads in progress.
	 */
	public void flAwaitAsyncWrites() {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flAwaitAsyncWrites(m_handle, pError);
		checkThrow(retCode, pError);
	}

	/**
	 * Submit an asynchronous read of one or more bytes from the specified channel.
	 * 
	 * <p>Submit an asynchronous read of {@code numBytes} bytes from the FPGA channel
	 * {@code channel}. You can request at most 64KiB of data asynchronously. Before calling this
	 * method you should verify that the FPGALink device actually supports CommFPGA using
	 * {@link #flIsCommCapable(int) flIsCommCapable()}.</p>
	 * 
	 * <p>This function is asynchronous. That means it will return immediately, usually before the
	 * read request has been sent over USB. You will not find out the result of the read until you
	 * later call {@link #flReadChannelAsyncAwait() flReadChannelAsyncAwait()} - this will give you
	 * your data, or tell you what went wrong.</p>
	 * 
	 * <p>You should always ensure that for each call to
	 * {@link #flReadChannelAsyncSubmit(int, int) flReadChannelAsyncSubmit()}, there is a matching
	 * call to {@link #flReadChannelAsyncAwait() flReadChannelAsyncAwait()}. You should not call
	 * any of {@link #flSetAsyncWriteChunkSize(int) flSetAsyncWriteChunkSize()},
	 * {@link #flAwaitAsyncWrites() flAwaitAsyncWrites()},
	 * {@link #flWriteChannel(int, byte[]) flWriteChannel()} or
	 * {@link #flReadChannel(int, int) flReadChannel()} between a submit...await pair.</p>
	 * 
	 * <p>USB host controllers typically need just one level of nesting of submit...await pairs to
	 * keep them busy. That means sequences like submit, submit, await, submit, await, submit, ...,
	 * await, await.</p>
	 * 
	 * @param channel The FPGA channel to read (0-127).
	 * @param numBytes The number of bytes to read, <= 64KiB.
	 * @throws FLException if a USB read or write error occurred.
	 * @throws FLException if the device does not support CommFPGA.
	 * @throws FLException if {@code numBytes} > 0x10000.
	 */
	public void flReadChannelAsyncSubmit(int channel, int numBytes) {
		if ( numBytes > 0x10000 ) {
			throw new FLException(
				"flReadChannelAsyncSubmit(): async reads of more than 0x10000 bytes are illegal",
				0
			);
		}
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flReadChannelAsyncSubmit(m_handle, (byte)channel, numBytes, null, pError);
		checkThrow(retCode, pError);
	}
	
	/**
	 * Await the data from a previously-submitted asynchronous read.
	 * 
	 * <p>Block until the outcome of a previous call to
	 * {@link #flReadChannelAsyncSubmit(int, int) flReadChannelAsyncSubmit()} is known. If the read
	 * was successful, you are given the resulting data. If not, you get {@link FLException}.</p>
	 * 
	 * @return A {@code byte[]} containing the data that was read from the FPGA.
	 * @throws FLException if one of the outstanding async operations failed.
	 */
	public byte[] flReadChannelAsyncAwait() {
		PointerByReference pError = new PointerByReference();
		PointerByReference pData = new PointerByReference();
		IntByReference pLength = new IntByReference();
		int retCode = LIB.flReadChannelAsyncAwait(m_handle, pData, pLength, pLength, pError);
		checkThrow(retCode, pError);
		return pData.getValue().getByteArray(0, pLength.getValue());
	}
	
	/**
	 * Program a device using the specified file.
	 * 
	 * <p>This will program an FPGA or CPLD using the specified microcontroller ports and the
	 * specified programming file. Several programming algorithms are supported (JTAG, Xilinx
	 * Slave-Serial, Xilinx SelectMap and Altera Passive-Serial). In each case, it's necessary to
	 * tell the micro which ports to use. Here are some examples:</p>
	 * 
	 * <p>A Digilent board using JTAG: {@code progConfig="J:D0D2D3D4"}:<ul>
	 * <li>TDO: PD0</li>
	 * <li>TDI: PD2</li>
	 * <li>TMS: PD3</li>
	 * <li>TCK: PD4</li></ul></p>
	 * 
	 * <p>MakeStuff LX9 using JTAG: {@code progConfig="J:A7A0A3A1"}:<ul>
	 * <li>TDO: PA7</li>
	 * <li>TDI: PA0</li>
	 * <li>TMS: PA3</li>
	 * <li>TCK: PA1</li></ul></p>
	 * 
	 * <p>EP2C5 Mini Board using Altera Passive-Serial: {@code progConfig="AS:B5B6B1B2"} (note that
	 * the board normally connects {@code MSEL[1:0]} to ground, hard-coding it in Active-Serial
	 * mode. For Passive-Serial to work you need to lift pin 85 and pull it up to VCC):<ul>
	 * <li>nCONFIG: PD5</li>
	 * <li>CONF_DONE: PD6</li>
	 * <li>DCLK: PD1</li>
	 * <li>DATA0: PD2</li></ul></p>
	 * 
	 * <p>Aessent aes220 using Xilinx Slave-Serial:
	 * {@code progConfig="XS:D0D5D1D6A7[D3?,B1+,B5+,B3+]"}:<ul>
	 * <li>PROG_B: PD0</li>
	 * <li>INIT_B: PD5</li>
	 * <li>DONE: PD1</li>
	 * <li>CCLK: PD6</li>
	 * <li>DIN: PA7</li>
	 * <li>Tristate DOUT (PD3)</li>
	 * <li>Drive M[2:0]="111" (PB1, PB5, PB3) for Slave-Serial</li></ul></p>
	 * 
	 * <p>Aessent aes220 using Xilinx SelectMAP:
	 * {@code progConfig="XP:D0D5D1D6A01234567[B4-,D2-,D3?,B1+,B5+,B3-]"}:<ul>
	 * <li>PROG_B: PD0</li>
	 * <li>INIT_B: PD5</li>
	 * <li>DONE: PD1</li>
	 * <li>CCLK: PD6</li>
	 * <li>D[7:0]: PA[7:0]</li>
	 * <li>Drive RDWR_B="0" (PB4)</li>
	 * <li>Drive CSI_B="0" (PD2)</li>
	 * <li>Tristate DOUT (PD3)</li>
	 * <li>Drive M[2:0]="110" (PB1, PB5, PB3) for SelectMAP</li></ul></p>
	 * 
	 * <p>Note that this approach of specifying and implementing many disparate programming
	 * algorithms on the host side in terms of a much smaller set of building-block operations on
	 * the microcontroller is optimised for microcontrollers which support efficient remapping of
	 * I/O pins. For example the FX2 has a Von Neumann architecture where both code and data are
	 * stored in a single RAM-based address space, so port remapping can easily be achieved with
	 * self-modifying code. Conversely, the AVRs have Harvard architecture, where code and data are
	 * in separate address spaces, with code in flash so it cannot be self-modified. And actually,
	 * the AVR firmware is more likely to be tuned to a specific board layout than the more generic
	 * FX2 firmware.</p>
	 * 
	 * <p>So, the bottom line is, even if you're using a microcontroller whose port pins are
	 * hard-coded, you still have to supply the port pins to use when you call functions expecting
	 * {@code progConfig}.</p>
	 * 
	 * <p>You can either append the programming filename to the end of {@code progConfig} (e.g
	 * {@code "J:A7A0A3A1:fpga.xsvf"}) or you can supply the programming filename separately in
	 * {@code progFile}.</p>

	 * @param progConfig The programming configuration string.
	 * @param progFile Either a filename, or {@code null} if the filename is in {@code progConfig}.
	 * @throws FLException if we ran out of memory during programming.
	 * @throws FLException if a USB error occurred.
	 * @throws FLException if the programming file is unreadable or an unexpected format.
	 * @throws FLException if an XSVF file contains an unsupported command.
	 * @throws FLException if an XSVF file contains an unsupported XENDDR/XENDIR.
	 * @throws FLException if an XSVF command is too long.
	 * @throws FLException if an SVF file is unparseable.
	 * @throws FLException if {@code progConfig} is malformed.
	 * @throws FLException if the micro was unable to map its ports to those given.
	 * @throws FLException if the micro refused to accept programming data.
	 * @throws FLException if the micro refused to provide programming data.
	 * @throws FLException if the micro refused to begin a JTAG shift operation.
	 * @throws FLException if the micro refused to navigate the TAP state-machine.
	 * @throws FLException if the micro refused to send JTAG clocks.
	 * @throws FLException if an SVF/XSVF compare operation failed.
	 * @throws FLException if an SVF/XSVF unknown command was encountered.
	 * @throws FLException if the FPGA failed to start after programming.
	 * @throws FLException if the micro refused to configure one of its ports.
	 */
	public void flProgram(String progConfig, String progFile) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flProgram(m_handle, progConfig, progFile, pError);
		checkThrow(retCode, pError);
	}
	
	/**
	 * See {@link #flProgram(String, String) flProgram()}.
	 * 
	 * @param progConfig The programming configuration, including filename.
	 */
	public void flProgram(String progConfig) {
		flProgram(progConfig, null);
	}

	/**
	 * Program a device using the specified programming blob.
	 * 
	 * <p>This is similar to {@link #flProgram(String, String) flProgram()}, except that instead of
	 * reading the programming information from a file, it runs the programming operation from a
	 * binary blob already stored in memory. For JTAG programming this is assumed to be a CSVF
	 * file; for Xilinx programming it is assumed to be a raw bitstream (.bin) file.</p>
	 * 
	 * @param progConfig The programming configuration.
	 * @param progData The binary blob containing programming information.
	 * @throws FLException if a USB error occurred.
	 * @throws FLException if {@code progConfig} is malformed.
	 * @throws FLException if the micro was unable to map its ports to those given.
	 * @throws FLException if the micro refused to accept programming data.
	 * @throws FLException if the micro refused to provide programming data.
	 * @throws FLException if the micro refused to begin a JTAG shift operation.
	 * @throws FLException if the micro refused to navigate the TAP state-machine.
	 * @throws FLException if the micro refused to send JTAG clocks.
	 * @throws FLException if an SVF/XSVF compare operation failed.
	 * @throws FLException if an SVF/XSVF unknown command was encountered.
	 * @throws FLException if the FPGA failed to start after programming.
	 * @throws FLException if the micro refused to configure one of its ports.
	 */
	public void flProgramBlob(String progConfig, byte[] progData) {
		if ( m_buffer == null || m_buffer.size() < progData.length ) {
			m_buffer = new Memory(progData.length);
		}
		m_buffer.write(0, progData, 0, progData.length);
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flProgramBlob(m_handle, progConfig, progData.length, m_buffer, pError);
		checkThrow(retCode, pError);
	}

	/**
	 * Scan the JTAG chain and return an array of IDCODEs.
	 * 
	 * @param portConfig The port bits to use for TDO, TDI, TMS & TCK, e.g "D0D2D3D4".
	 * @return An array of IDCODEs.
	 * @throws FLException if {@code portConfig} is malformed.
	 * @throws FLException if the micro was unable to map its ports to those given.
	 * @throws FLException if the micro refused to accept programming data.
	 * @throws FLException if the micro refused to provide programming data.
	 * @throws FLException if the micro refused to begin a JTAG shift operation.
	 * @throws FLException if the micro refused to navigate the TAP state-machine.
	 * @throws FLException if the micro refused to configure one of its ports.
	 */
	public int[] jtagScanChain(String portConfig) {
		PointerByReference pError = new PointerByReference();
		IntByReference pNumDevices = new IntByReference();
		int numDevices = 16;
		int numBytes = 4*numDevices;
		if ( m_buffer == null || m_buffer.size() < numBytes ) {
			m_buffer = new Memory(numBytes);
		}
		int retCode = LIB.jtagScanChain(m_handle, portConfig, pNumDevices, m_buffer, numDevices, pError);
		checkThrow(retCode, pError);
		if ( pNumDevices.getValue() > numDevices ) {
			numDevices = pNumDevices.getValue();
			numBytes = 4*numDevices;
			if ( m_buffer.size() < numBytes ) {
				m_buffer = new Memory(numBytes);
			}
			retCode = LIB.jtagScanChain(m_handle, portConfig, pNumDevices, m_buffer, numDevices, pError);
			checkThrow(retCode, pError);
		} else {
			numDevices = pNumDevices.getValue();
		}
		return m_buffer.getIntArray(0, numDevices);
	}
	
	/**
	 * Open an SPI/JTAG connection.
	 * 
	 * <p>Open a SPI/JTAG connection using the supplied {@code portConfig}. You must open a
	 * connection before calling {@link #jtagShiftInOut(int, byte[], boolean) jtagShiftInOut()},
	 * {@link #jtagShiftInOnly(int, byte[], boolean) jtagShiftInOnly()},
	 * {@link #jtagClockFSM(int, int) jtagClockFSM()}, {@link #jtagClocks(int) jtagClocks()},
	 * {@link #spiSend(byte[], BitOrder) spiSend()} or {@link #spiRecv(int, BitOrder) spiRecv()}.
	 * And you must close the connection when you're finished, with
	 * {@link #progClose() progClose()}.</p>
	 * 
	 * @param portConfig The port bits to use for MISO(TDO), MOSI(TDI), SS(TMS) & SCK(TCK), e.g
	 *            "D0D2D3D4".
	 * @throws FLException if portConfig is malformed.
	 * @throws FLException if the micro refused to map its ports to those given.
	 * @throws FLException if the micro refused to configure one of its ports.
	 */
	public void progOpen(String portConfig) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.progOpen(m_handle, portConfig, pError);
		checkThrow(retCode, pError);
	}
	
	/**
	 * Close an SPI/JTAG connection.
	 * 
	 * <p>Close an SPI/JTAG connection previously opened by {@link #progOpen(String) progOpen()},
	 * and tri-state the four programming pins.</p>
	 * 
	 * @throws FLException if the micro refused to configure one of its ports. 
	 */
	public void progClose() {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.progClose(m_handle, pError);
		checkThrow(retCode, pError);
	}

	/**
	 * Get the number of bytes needed to store {@code x} bits.
	 * 
	 * @param x The number of bits.
	 * @return The necessary number of bytes.
	 */
	private static int bitsToBytes(int x) {
		return ( (x & 7) == 0 ) ? (x >> 3) : (x >> 3) + 1;
	}
	
	/**
	 * Low-level JTAG shift-in/out operation.
	 * 
	 * <p>See {@link #jtagShiftInOut(int, byte[], boolean) jtagShiftInOut()} for a more
	 * user-friendly method.</p>
	 * 
	 * @param numBits The number of bits to shift into and out of the JTAG chain.
	 * @param tdiData A native {@link Pointer} to the bits that are to be clocked into TDI.
	 * @param tdoData A native {@link Pointer} to a buffer to receive the bits clocked out of TDO.
	 * @param isLast If {@code true}, exit to {@code Exit1-xR} on the final bit; if {@code false},
	 *            remain in {@code Shift-xR}.
	 * @throws FLException if the micro refused to accept programming data.
	 * @throws FLException if the micro refused to provide programming data.
	 * @throws FLException if the micro refused to begin a JTAG shift operation.
	 */
	public void rawShiftInOut(int numBits, Pointer tdiData, Memory tdoData, boolean isLast) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.jtagShiftInOut(
			m_handle, numBits, tdiData, tdoData, isLast ? (byte)0x01 : (byte)0x00, pError
		);
		checkThrow(retCode, pError);
	}
	
	/**
	 * Shift data into and out of the JTAG state-machine.
	 * 
	 * <p>Shift {@code numBits} bits LSB-first from {@code tdiData} into TDI; at the same time
	 * shift the same number of bits LSB-first from TDO into a {@code byte[]} to be returned. If
	 * {@code isLast} is {@code false}, leave the TAP state-machine in {@code Shift-xR}, otherwise
	 * exit to {@code Exit1-xR} on the final bit. If you want {@code tdiData} to be all zeros you
	 * can use {@link #SHIFT_ZEROS}, or if you want it to be all ones you can use
	 * {@link #SHIFT_ONES}. This is more efficient than explicitly sending an array containing all
	 * zeros or all 0xFFs.</p>
	 * 
	 * @param numBits The number of bits to clock into and out of the JTAG state-machine. 
	 * @param tdiData A {@code byte[]} containing the bits to be clocked into TDI, or
	 *            {@link #SHIFT_ZEROS} or {@link #SHIFT_ONES}.
	 * @param isLast If {@code true}, exit to {@code Exit1-xR} on the final bit; if {@code false},
	 *            remain in {@code Shift-xR}.
	 * @return A {@code byte[]} containing the bits shifted out of TDO.
	 * @throws FLException if the micro refused to accept programming data.
	 * @throws FLException if the micro refused to provide programming data.
	 * @throws FLException if the micro refused to begin a JTAG shift operation.
	 * @throws FLException if {@code tdiData} has incorrect length.
	 */
	public byte[] jtagShiftInOut(int numBits, byte[] tdiData, boolean isLast) {
		Pointer inPtr;
		final int numBytes = bitsToBytes(numBits);
		if ( m_buffer == null || m_buffer.size() < numBytes ) {
			m_buffer = new Memory(numBytes);
		}
		if ( tdiData == SHIFT_ZEROS ) {
			inPtr = new Pointer(0);
		} else if ( tdiData == SHIFT_ONES ) {
			inPtr = new Pointer(-1);
		} else {
			if ( tdiData.length != numBytes ) {
				throw new FLException(
					"jtagShiftInOut(): tdiData array has incorrect length (expected " +
					numBytes + ", got " + tdiData.length + ")", 0
				);
			}
			m_buffer.write(0, tdiData, 0, numBytes);
			inPtr = m_buffer;
		}
		rawShiftInOut(numBits, inPtr, m_buffer, isLast);
		return m_buffer.getByteArray(0, numBytes);
	}
	
	/**
	 * Low-level JTAG shift-in operation.
	 * 
	 * <p>See {@link #jtagShiftInOnly(int, byte[], boolean) jtagShiftInOnly()} for a more
	 * user-friendly method.</p>
	 * 
	 * @param numBits The number of bits to clock into the JTAG state-machine.
	 * @param tdiData A native {@link Memory} instance containing bits to be clocked in.
	 * @param isLast If {@code true}, exit to {@code Exit1-xR} on the final bit; if {@code false},
	 *            remain in {@code Shift-xR}.
	 *            
	 * @throws FLException if the micro refused to accept programming data.
	 * @throws FLException if the micro refused to begin a JTAG shift operation. 
	 */
	public void rawShiftInOnly(int numBits, Memory tdiData, boolean isLast) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.jtagShiftInOnly(
			m_handle, numBits, tdiData, isLast ? (byte)0x01 : (byte)0x00, pError
		);
		checkThrow(retCode, pError);
	}

	/**
	 * Shift data into the JTAG state-machine.
	 * 
	 * <p>Shift {@code numBits} bits LSB-first from {@code tdiData} into TDI. If {@code isLast} is
	 * {@code false}, leave the TAP state-machine in {@code Shift-xR}, otherwise exit to
	 * {@code Exit1-xR} on the final bit.</p>
	 * 
	 * @param numBits The number of bits to clock into the JTAG state-machine.
	 * @param tdiData A {@code byte[]} containing the bits to be clocked into TDI.
	 * @param isLast If {@code true}, exit to {@code Exit1-xR} on the final bit; if {@code false},
	 *            remain in {@code Shift-xR}.
	 * 
	 * @throws FLException if the micro refused to accept programming data.
	 * @throws FLException if the micro refused to begin a JTAG shift operation. 
	 * @throws FLException if {@code tdiData} has incorrect length.
	 */
	public void jtagShiftInOnly(int numBits, byte[] tdiData, boolean isLast) {
		final int numBytes = bitsToBytes(numBits);
		if ( m_buffer == null || m_buffer.size() < numBytes ) {
			m_buffer = new Memory(numBytes);
		}
		if ( tdiData.length != numBytes ) {
			throw new FLException(
				"jtagShiftInOnly(): tdiData array has incorrect length (expected " +
				numBytes + ", got " + tdiData.length + ")", 0
			);
		}
		m_buffer.write(0, tdiData, 0, numBytes);
		rawShiftInOnly(numBits, m_buffer, isLast);
	}
	
	/**
	 * Clock {@code transitionCount} bits from {@code bitPattern} into TMS, starting with the LSB.
	 * 
	 * <p>Navigate the TAP state-machine by clocking an arbitrary sequence of bits into TMS.</p>
	 * 
	 * @param bitPattern The pattern of bits to clock into TMS, LSB first.
	 * @param transitionCount The number of bits to clock.
	 * @throws FLException if the micro refused to navigate the TAP state-machine. 
	 */
	public void jtagClockFSM(int bitPattern, int transitionCount) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.jtagClockFSM(m_handle, bitPattern, (byte)transitionCount, pError);
		checkThrow(retCode, pError);
	}

	/**
	 * Toggle TCK numClocks times.
	 * 
	 * @param numClocks The number of clocks to put out on TCK.
	 * @throws FLException if the micro refused to send JTAG clocks.
	 */
	public void jtagClocks(int numClocks) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.jtagClocks(m_handle, numClocks, pError);
		checkThrow(retCode, pError);
	}

	/**
	 * Get the physical port and port-bit of the specified logical port.
	 * 
	 * <p>Get the physical port and bit numbers assigned to the specified logical port by the
	 * preceding call to {@link #progOpen(String) progOpen()}. This is just a convenience function
	 * to avoid re-parsing the port config, which is typically supplied by the user as a string.
	 * For example, to send data to a SPI peripheral, you'll probably want to assert {@code SS}. So
	 * you'll want to call {@code progGetPort(LogicalPort.SS)} to find out which physical port
	 * {@code SS} was assigned to.</p>
	 * 
	 * @param logicalPort The port bits to use for MISO(TDO), MOSI(TDI), SS(TMS) and SCK(TCK).
	 * @return A typed pair specifying the port and the bit within that port.
	 */
	public PhysicalPort progGetPort(LogicalPort logicalPort) {
		return new PhysicalPort(
			LIB.progGetPort(m_handle, logicalPort.value),
			LIB.progGetBit(m_handle, logicalPort.value));
	}

	/**
	 * Low level SPI-send operation.
	 * 
	 * <p>See {@link #spiSend(byte[], BitOrder) spiSend()} for a more user-friendly method.</p>
	 * 
	 * @param numBytes The number of bytes to send.
	 * @param sendData A native {@link Memory} instance containing bytes to be sent.
	 * @param bitOrder Either {@link BitOrder#LSBFIRST BitOrder.LSBFIRST} or
	 *            {@link BitOrder#MSBFIRST BitOrder.MSBFIRST}.
	 * @throws FLException if there was a native memory allocation failure.
	 * @throws FLException if USB communications failed whilst sending the data.
	 * @throws FLException if the device does not support SPI.
	 */
	public void rawSpiSend(int numBytes, Memory sendData, BitOrder bitOrder) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.spiSend(
			m_handle, numBytes, sendData, bitOrder.value, pError
		);
		checkThrow(retCode, pError);
	}

	/**
	 * Send a number of whole bytes over SPI, either LSB-first or MSB-first.
	 * 
	 * <p>Write the {@code sendData} byte-array into the microcontroller's SPI bus (if any), either
	 * MSB-first or LSB-first. You must have previously called
	 * {@link #progOpen(String) progOpen()}.</p>
	 * 
	 * @param sendData The bytes to be written into the SPI bus.
	 * @param bitOrder Either {@link BitOrder#LSBFIRST BitOrder.LSBFIRST} or
	 *            {@link BitOrder#MSBFIRST BitOrder.MSBFIRST}.
	 * @throws FLException if there was a native memory allocation failure.
	 * @throws FLException if USB communications failed whilst sending the data.
	 * @throws FLException if the device does not support SPI.
	 */
	public void spiSend(byte[] sendData, BitOrder bitOrder) {
		final int numBytes = sendData.length;
		if ( m_buffer == null || m_buffer.size() < numBytes ) {
			m_buffer = new Memory(numBytes);
		}
		m_buffer.write(0, sendData, 0, numBytes);
		rawSpiSend(numBytes, m_buffer, bitOrder);
	}
	
	/**
	 * Low-level SPI-receive operation.
	 * 
	 * <p>See {@link #spiRecv(int, BitOrder) spiRecv()} for a more user-friendly method.</p>
	 * 
	 * @param numBytes The number of bytes to receive.
	 * @param recvData A native {@link Memory} instance with space for the received bytes.
	 * @param bitOrder Either {@link BitOrder#LSBFIRST BitOrder.LSBFIRST} or
	 *            {@link BitOrder#MSBFIRST BitOrder.MSBFIRST}.
	 * @throws FLException if USB communications failed whilst receiving the data.
	 * @throws FLException if the device does not support SPI.
	 */
	public void rawSpiRecv(int numBytes, Memory recvData, BitOrder bitOrder) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.spiRecv(
			m_handle, numBytes, recvData, bitOrder.value, pError
		);
		checkThrow(retCode, pError);
	}
	
	/**
	 * Receive a number of whole bytes over SPI, either LSB-first or MSB-first.
	 * 
	 * <p>Return a new {@code byte[]} containing {@code numBytes} bytes shifted out of the
	 * microcontroller's SPI bus (if any), either MSB-first or LSB-first. You must have previously
	 * called {@link #progOpen(String) progOpen()}.</p>
	 * 
	 * @param numBytes The number of bytes to receive.
	 * @param bitOrder Either {@link BitOrder#LSBFIRST BitOrder.LSBFIRST} or
	 *            {@link BitOrder#MSBFIRST BitOrder.MSBFIRST}.
	 * @return A new {@code byte[]} containing {@code numBytes} bytes of received SPI data.
	 * @throws FLException if USB communications failed whilst receiving the data.
	 * @throws FLException if the device does not support SPI.
	 */
	public byte[] spiRecv(int numBytes, BitOrder bitOrder) {
		if ( m_buffer == null || m_buffer.size() < numBytes ) {
			m_buffer = new Memory(numBytes);
		}
		rawSpiRecv(numBytes, m_buffer, bitOrder);
		return m_buffer.getByteArray(0, numBytes);
	}

	/**
	 * Load standard FPGALink firmware into the FX2's RAM.
	 * 
	 * <p>Load the FX2 chip at the "current" VID:PID with a precompiled firmware such that it will
	 * renumerate as the "new" VID:PID. The firmware is loaded into RAM, so the change is not
	 * permanent. Typically after calling
	 * {@link #flLoadStandardFirmware(String, String) flLoadStandardFirmware()}, applications
	 * should wait for the renumeration to complete by calling
	 * {@link #flAwaitDevice(String, int) flAwaitDevice(newVidPid)}.</p>
	 * 
	 * @param curVidPid The current Vendor/Product (i.e VVVV:PPPP) of the FX2 device.
	 * @param newVidPid The Vendor/Product/Device (i.e VVVV:PPPP:DDDD) that you <b>want</b> the FX2
	 *            device to renumerate as. 
	 * @throws FLException if there was a native memory allocation failure.
	 * @throws FLException if one of the VID:PIDs was invalid or the current VID:PID was not found.
	 * @throws FLException if there was a problem talking to the FX2.
	 */
	public static void flLoadStandardFirmware(String curVidPid, String newVidPid) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flLoadStandardFirmware(curVidPid, newVidPid, pError);
		checkThrow(retCode, pError);
	}
	
	/**
	 * Flash standard FPGALink firmware into the FX2's EEPROM.
	 * 
	 * <p><b>Warning:</b> This function will make permanent changes to your hardware. Remember to
	 * make a backup copy of the existing EEPROM firmware with
	 * {@link #flSaveFirmware(int, String) flSaveFirmware()} before calling it.</p>
	 * 
	 * <p>Overwrite the FX2's EEPROM with a precompiled FPGALink firmware such that the board will
	 * enumerate on power-on as the "new" VID:PID.</p>
	 * 
	 * @param newVidPid The Vendor/Product (i.e VVVV:PPPP) you want the FX2 to be on power-on.
	 * @throws FLException if there was a native memory allocation failure.
	 * @throws FLException if the VID:PID was invalid.
	 * @throws FLException if there was a problem talking to the FX2.
	 */
	public void flFlashStandardFirmware(String newVidPid) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flFlashStandardFirmware(m_handle, newVidPid, pError);
		checkThrow(retCode, pError);
	}

	/**
	 * Load custom firmware (.hex) into the FX2's RAM.
	 * 
	 * <p>Load the FX2 chip at the given VID:PID with a {@code .hex} firmware file. The firmware is
	 * loaded into RAM, so the change is not permanent.</p>
	 * 
	 * @param curVidPid The current Vendor/Product (i.e VVVV:PPPP) of the FX2 device. 
	 * @param fwFile A {@code .hex} file containing new FX2 firmware to be loaded into the FX2's
	 *            RAM.
	 * @throws FLException if there was a native memory allocation failure.
	 * @throws FLException if the VID:PID was invalid.
	 * @throws FLException if there was a problem talking to the FX2.
	 * @throws FLException if {@code fwFile} has a bad extension or could not be loaded. 
	 */
	public static void flLoadCustomFirmware(String curVidPid, String fwFile) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flLoadCustomFirmware(curVidPid, fwFile, pError);
		checkThrow(retCode, pError);
	}

	/**
	 * Flash a custom firmware from a file into the FX2's EEPROM.
	 * 
	 * <p><b>Warning:</b> This function will make permanent changes to your hardware. Remember to
	 * make a backup copy of the existing EEPROM firmware with
	 * {@link #flSaveFirmware(int, String) flSaveFirmware()} before calling it.</p>
	 * 
	 * <p>Overwrite the FX2's EEPROM with a custom firmware from a {@code .hex} or {@code .iic}
	 * file.</p>
	 * 
	 * @param fwFile A {@code .hex} or {@code .iic} file containing new FX2 firmware to be loaded
	 *            into the FX2's EEPROM.
	 * @throws FLException if there was a native memory allocation failure.
	 * @throws FLException if there was a problem talking to the FX2.
	 * @throws FLException if {@code fwFile} has a bad extension or could not be loaded. 
	 */
	public void flFlashCustomFirmware(String fwFile) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flFlashCustomFirmware(m_handle, fwFile, pError);
		checkThrow(retCode, pError);
	}

	/**
	 * Save existing EEPROM data to a file.
	 * 
	 * <p>The existing EEPROM firmware is saved to an {@code .iic} file, for backup purposes.</p>
	 * 
	 * @param eepromSize The size in kilobits of the EEPROM (e.g Nexys2's EEPROM is 128kbit).
	 * @param saveFile An {@code .iic} file to save the EEPROM to.
	 * @throws FLException if there was a native memory allocation failure.
	 * @throws FLException if there was a problem talking to the FX2.
	 * @throws FLException if {@code saveFile} could not be written. 
	 */
	public void flSaveFirmware(int eepromSize, String saveFile) {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flSaveFirmware(m_handle, eepromSize, saveFile, pError);
		checkThrow(retCode, pError);
	}
	
	/**
	 * Put the AVR in DFU bootloader mode.
	 * 
	 * <p>This is an AVR-specific utility function to make firmware upgrades easier on boards on
	 * which the /HWB and /RESET pins are not easily accesible. The {@code gordon} utility has an
	 * option to invoke this.</p>
	 * 
	 * @throws FLException if the device is not running suitable FPGALink/AVR firmware.
	 */
	public void flBootloader() {
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flBootloader(m_handle, pError);
		checkThrow(retCode, pError);
	}
	
	/**
	 * Sleep for the specified number of milliseconds.
	 * 
	 * @param ms The number of milliseconds to sleep.
	 */
	public static void flSleep(int ms) {
		LIB.flSleep(ms);
	}

	/**
	 * Configure a single port bit on the microcontroller.
	 * 
	 * <p>With this function you can set a single microcontroller port bit to one of the enums in
	 * {@link PinConfig}, and read back the current state of the bit.</p>
	 * 
	 * @param portNumber Which port to configure (i.e 0=PortA, 1=PortB, 2=PortC, etc).
	 * @param bitNumber The bit within the chosen port to use.
	 * @param pinConfig Either {@link PinConfig#INPUT PinConfig.INPUT},
	 *            {@link PinConfig#HIGH PinConfig.HIGH} or {@link PinConfig#LOW PinConfig.LOW}.
	 * @return {@code true} if the bit was high, else {@code false}.
	 * @throws FLException if the micro failed to respond to the port access command.
	 */
	public boolean flSingleBitPortAccess(int portNumber, int bitNumber, PinConfig pinConfig) {
		ByteByReference pPinRead = new ByteByReference();
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flSingleBitPortAccess(
			m_handle, (byte)portNumber, (byte)bitNumber, pinConfig.value, pPinRead, pError);
		checkThrow(retCode, pError);
		return pPinRead.getValue() != (byte)0x00;
	}

	/**
	 * Configure multiple port bits on the microcontroller.
	 * 
	 * <p>With this function you can set multiple microcontroller port bits to either
	 * {@link PinConfig#INPUT PinConfig.INPUT}, {@link PinConfig#HIGH PinConfig.HIGH} or
	 * {@link PinConfig#LOW PinConfig.LOW}, and read back the current state of each bit. This is
	 * achieved by sending a comma-separated list of port configurations, e.g {@code A12-,B2+,C7?}.
	 * A "+" or a "-" suffix sets the port as an output, driven high or low respectively, and a "?"
	 * suffix sets the port as an input. The current state of up to 32 bits are returned, LSB
	 * first.</p>
	 * 
	 * @param portConfig A comma-separated sequence of port configurations.
	 * @return The high/low state of up to 32 port bits.
	 * @throws FLException if {@code portConfig} is malformed.
	 * @throws FLException if the micro failed to respond to the port access command. 
	 */
	public int flMultiBitPortAccess(String portConfig) {
		IntByReference pReadState = new IntByReference();
		PointerByReference pError = new PointerByReference();
		int retCode = LIB.flMultiBitPortAccess(
			m_handle, portConfig, pReadState, pError);
		checkThrow(retCode, pError);
		return pReadState.getValue();
	}	
}
