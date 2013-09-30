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

/**
 * @file libfpgalink.h
 *
 * The <b>FPGALink</b> library makes it easier to talk to an FPGA over USB (via a suitable micro).
 *
 * It performs three classes of function:
 * - Load device firmware and EEPROM (specific to Cypress FX2LP).
 * - Program an FPGA or CPLD using JTAG or one of the proprietary serial or parallel algorithms.
 * - Read and write (over USB) up to 128 byte-wide data channels in the target FPGA.
 */
#ifndef FPGALINK_H
#define FPGALINK_H

#include <makestuff.h>

#ifdef __cplusplus
extern "C" {
#endif

	// ---------------------------------------------------------------------------------------------
	// Type declarations
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name Types
	 * @{
	 */
	/**
	 * Report struct populated by \c flReadChannelAsyncAwait().
	 */
	struct ReadReport {
		const uint8 *data;     ///< The result of the asynchronous read.
		uint32 requestLength;  ///< The number of bytes requested.
		uint32 actualLength;   ///< The number of bytes actualy read.
	};

	/**
	 * Return codes from the functions.
	 */
	typedef enum {
		FL_SUCCESS = 0,          ///< The operation completed successfully.
		FL_ALLOC_ERR,            ///< There was a memory allocation error.
		FL_USB_ERR,              ///< There was some USB-related problem.
		FL_PROTOCOL_ERR,         ///< The device is probably not a valid FPGALink device.
		FL_FX2_ERR,              ///< There was some problem talking to the FX2 chip.
		FL_FILE_ERR,             ///< There was a file-related problem.
		FL_BUF_INIT_ERR,         ///< The CSVF buffer could not be allocated.
		FL_BUF_APPEND_ERR,       ///< The CSVF buffer could not be grown.
		FL_BUF_LOAD_ERR,         ///< The XSVF file could not be loaded.
		FL_UNSUPPORTED_CMD_ERR,  ///< The XSVF file contains an unsupported command.
		FL_UNSUPPORTED_DATA_ERR, ///< The XSVF file contains an unsupported XENDIR or XENDDR.
		FL_UNSUPPORTED_SIZE_ERR, ///< The XSVF file requires more buffer space than is available.
		FL_SVF_PARSE_ERR,        ///< The SVF file was not parseable.
		FL_CONF_FORMAT,          ///< The supplied programing config was not parseable.
		FL_PROG_PORTMAP,         ///< There was a problem remapping ports for programming.
		FL_PROG_SEND,            ///< There was a problem sending data during programming.
		FL_PROG_RECV,            ///< There was a problem receiving data during programming.
		FL_PROG_SHIFT,           ///< There was a problem with the requested shift operation.
		FL_PROG_JTAG_FSM,        ///< There was a problem navigating the JTAG state machine.
		FL_PROG_SVF_COMPARE,     ///< An SVF compare operation failed.
		FL_PROG_SVF_UNKNOWN_CMD, ///< An unknown SVF command was encountered.
		FL_PROG_CLOCKS,          ///< There was a problem issuing clocks during programming.
		FL_PROG_ERR,             ///< The device failed to start after programmign.
		FL_BAD_STATE,            ///< You're trying to do something that is illegal in this state.
		FL_INTERNAL_ERR          ///< An internal error occurred. Please report it!
	} FLStatus;

	/**
	 * Enum used by \c flSingleBitPortAccess() to configure the pin direction and drive.
	 */
	typedef enum {
		UNUSED, ///< These aren't the droids you're looking for. Move along.
		HIGH,   ///< Configure the pin as an output and drive it high.
		LOW,    ///< Configure the pin as an output and drive it low.
		INPUT   ///< Configure the pin as an input.
	} PinConfig;
	//@}

	// Forward declarations
	struct FLContext; // Opaque FPGALink context
	struct Buffer;    // Dynamic binary buffer (see libbuffer)

	// ---------------------------------------------------------------------------------------------
	// Miscellaneous functions
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name Miscellaneous Functions
	 * @{
	 */
	/**
	 * @brief Initialise the library with the given log level.
	 *
	 * This may fail if LibUSB cannot talk to the USB host controllers through its kernel driver
	 * (e.g a Linux kernel with USB support disabled, or a machine lacking a USB host controller).
	 *
	 * @param debugLevel 0->none, 1, 2, 3->lots.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c usbFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_USB_ERR if there were problems initialising LibUSB.
	 */
	DLLEXPORT(FLStatus) flInitialise(int debugLevel, const char **error);

	/**
	 * @brief Free an error allocated when one of the other functions fails.
	 *
	 * @param err An error message previously allocated by one of the other library functions.
	 */
	DLLEXPORT(void) flFreeError(const char *err);
	//@}

	// ---------------------------------------------------------------------------------------------
	// Connection lifecycle functions
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name Connection Lifecycle
	 * @{
	 */
	/**
	 * @brief Open a connection to the FPGALink device at the specified VID & PID.
	 *
	 * Connects to the device and verifies it is an FPGALink device, then queries its
	 * capabilities and synchronises the USB bulk endpoints.
	 *
	 * @param vp The Vendor/Product (i.e VVVV:PPPP) of the FPGALink device. You may also specify
	 *            an optional device ID (e.g 1D50:602B:0004). If no device ID is supplied, it
	 *            selects the first device with matching VID:PID.
	 * @param handle A pointer to a <code>struct FLContext*</code> which will be set on exit to
	 *            point at a newly-allocated context structure. Responsibility for this allocated
	 *            memory (and its associated USB resources) passes to the caller and must be freed
	 *            with \c flClose(). Will be set NULL if an error occurs.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if all is well (\c *handle is valid).
	 *     - \c FL_USB_ERR if the VID:PID is invalid or the device cannot be found or opened.
	 *     - \c FL_PROTOCOL_ERR if the device is not an FPGALink device.
	 *     - \c FL_SYNC_ERR if the bulk endpoint pairs cannot be synchronised.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 */
	DLLEXPORT(FLStatus) flOpen(
		const char *vp, struct FLContext **handle, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Close the connection to the FPGALink device.
	 *
	 * @param handle The handle returned by \c flOpen().
	 */
	DLLEXPORT(void) flClose(
		struct FLContext *handle
	);
	//@}

	// ---------------------------------------------------------------------------------------------
	// Get device capabilities & status
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name Device Capabilities and Status
	 * @{
	 */
	/**
	 * @brief Check if the given device is actually connected to the system.
	 *
	 * The LibUSB devices in the system are searched for a device with the given VID:PID.
	 *
	 * There is a short period of time following a call to \c flLoadStandardFirmware() or
	 * \c flLoadCustomFirmware() during which this function will still return true for the
	 * "current" VID:PID, so when you load new firmware, it's important to either wait a while before
	 * calling this function, or alternatively just ensure that the "new" VID:PID is different from
	 * the "current" VID:PID to avoid such false positives.
	 *
	 * @param vp The Vendor/Product (i.e VVVV:PPPP) of the FPGALink device. You may also specify
	 *            an optional device ID (e.g 1D50:602B:0004). If no device ID is supplied, it
	 *            selects the first device with matching VID:PID.
	 * @param isAvailable A pointer to an 8-bit integer which will be set on exit to 1 if available
	 *            else 0.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if all is well (\c *isAvailable is valid).
	 *     - \c FL_USB_ERR if the VID:PID is invalid or if no USB buses were found (did you
	 *            remember to call \c flInitialise()?).
	 */
	DLLEXPORT(FLStatus) flIsDeviceAvailable(
		const char *vp, uint8 *isAvailable, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Check to see if the device supports NeroProg.
	 *
	 * NeroProg is the collective name for all the various programming algorithms supported by
	 * FPGALink, including but not limited to JTAG. An affirmative response means you are free to
	 * call \c flProgram(), \c jtagScanChain(), \c jtagOpen(), \c jtagClose(), \c jtagShift(),
	 * \c jtagClockFSM() and \c jtagClocks().
	 *
	 * This function merely returns a flag determined by \c flOpen(), so it cannot fail.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @returns An 8-bit integer: 1 if the device supports NeroProg, else 0.
	 */
	DLLEXPORT(uint8) flIsNeroCapable(struct FLContext *handle);

	/**
	 * @brief Check to see if the device supports CommFPGA.
	 *
	 * CommFPGA is a set of channel read/write protocols. The micro may implement several
	 * different CommFPGA protocols, distinguished by the chosen conduit. A micro will typically
	 * implement its first CommFPGA protocol on conduit 1, and additional protocols on conduit
	 * 0x02, 0x03 etc. Conduit 0 is reserved for communication over JTAG using a virtual TAP
	 * state machine implemented in the FPGA.
	 *
	 * This function returns 1 if the micro supports CommFPGA on the chosen conduit, else 0.
	 *
	 * Note that this function only knows the capabilities of the micro itself; it cannot determine
	 * whether the FPGA contains suitable logic to implement the protocol, or even whether there is
	 * an FPGA physically wired to the micro in the first place.
	 *
	 * An affirmative response means you are free to call \c flReadChannel(),
	 * \c flReadChannelAsyncSubmit(), \c flReadChannelAsyncAwait(), \c flWriteChannel(),
	 * \c flWriteChannelAsync() and \c flIsFPGARunning().
	 *
	 * This function merely returns information determined by \c flOpen(), so it cannot fail.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param conduit The conduit you're interested in (this will typically be 1).
	 * @returns An 8-bit integer: 1 if the device supports CommFPGA, else 0.
	 */
	DLLEXPORT(uint8) flIsCommCapable(struct FLContext *handle, uint8 conduit);
	//@}

	// ---------------------------------------------------------------------------------------------
	// CommFPGA channel read/write functions (only if flIsCommCapable() returns true)
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name CommFPGA Operations
	 * @{
	 */

	/**
	 * @brief Select a different conduit.
	 *
	 * Select a different conduit for CommFPGA communication. Typically a micro will implement its
	 * first CommFPGA protocol on conduit 1. It may or may not also implement others on conduit 2, 3,
	 * 4 etc. It may also implement comms-over-JTAG using a virtual TAP FSM on the FPGA. You can use
	 * \c flIsCommCapable() to determine whether the micro supports CommFPGA on a given conduit.
	 *
	 * If mixing NeroProg operations with CommFPGA operations, it *may* be necessary to switch
	 * conduits. For example, if your PCB is wired to use some of the CommFPGA signals during
	 * programming, you will have to switch back and forth. But if the pins used for CommFPGA are
	 * independent of the pins used for NeroProg, you need only select the correct conduit on startup
	 * and then leave it alone.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param conduit The conduit to select (current range 0-15).
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if all is well (<code>*isRunning</code> is valid).
	 *     - \c FL_USB_ERR if the device doesn't respond, or the conduit is out of range.
	 */
	DLLEXPORT(FLStatus) flSelectConduit(
		struct FLContext *handle, uint8 conduit, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Check to see if the FPGA is running.
	 *
	 * This may only be called if \c flIsCommCapable() returns true. It merely verifies that
	 * the FPGA is asserting that it's ready to read commands on the chosen conduit. Some conduits
	 * may not have the capability to determine this, and will therefore just optimistically report
	 * true. Before calling \c flIsFPGARunning(), you should verify that the FPGALink device
	 * actually supports CommFPGA using \c flIsCommCapable(), and select the conduit you wish to
	 * use.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param isRunning A pointer to an 8-bit integer which will be set on exit to 1 if the FPGA
	 *            is running, else 0.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if all is well (<code>*isRunning</code> is valid).
	 *     - \c FL_PROTOCOL_ERR if the device does not support CommFPGA.
	 *     - \c FL_USB_ERR if the device no longer responds.
	 */
	DLLEXPORT(FLStatus) flIsFPGARunning(
		struct FLContext *handle, uint8 *isRunning, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Synchronously read the specified channel into the supplied buffer.
	 *
	 * Read \c count bytes from the FPGA channel \c chan to the \c data array. Before calling
	 * \c flReadChannel(), you should verify that the FPGALink device actually supports
	 * CommFPGA using \c flIsCommCapable().
	 *
	 * Because this function is synchronous, it will block until the data has been returned.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param chan The FPGA channel to read.
	 * @param count The number of bytes to read.
	 * @param buf The address of a buffer to store the bytes read from the FPGA.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the read completed successfully.
	 *     - \c FL_PROTOCOL_ERR if the device does not support CommFPGA.
	 *     - \c FL_BAD_STATE if there are async reads in progress.
	 *     - \c FL_USB_ERR if a USB error occurred.
	 */
	DLLEXPORT(FLStatus) flReadChannel(
		struct FLContext *handle, uint8 chan, uint32 count, uint8 *buf,
		const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Synchronously write the supplied data to the specified channel.
	 *
	 * Write \c count bytes from the \c data array to FPGA channel \c chan. Before calling
	 * \c flWriteChannel(), you should verify that the FPGALink device actually supports
	 * CommFPGA using \c flIsCommCapable().
	 *
	 * Because this function is synchronous, it will block until the OS has confirmed that the data
	 * has been correctly sent over USB and received by the micro. It cannot confirm that the data
	 * has been received by the FPGA however: it may be waiting in the micro's output buffer.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param chan The FPGA channel to write.
	 * @param count The number of bytes to write.
	 * @param data The address of the array of bytes to be written to the FPGA.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the write completed successfully.
	 *     - \c FL_PROTOCOL_ERR if the device does not support CommFPGA.
	 *     - \c FL_BAD_STATE if there are async reads in progress.
	 *     - \c FL_USB_ERR if a USB error occurred.
	 */
	DLLEXPORT(FLStatus) flWriteChannel(
		struct FLContext *handle, uint8 chan, uint32 count, const uint8 *data,
		const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Set the chunk size to be used for future async writes.
	 *
	 * By default, the \c flWriteChannelAsync() function buffers up to 64KiB of data before sending
	 * anything over USB. Chunking the data in this way is more efficient than sending lots of little
	 * messages. However, the choice of chunk size affects the steady-state throughput in interesting
	 * ways. If you need to, you can choose to make the chunks smaller than 64KiB.
	 *
	 * You should not call this when there is some send data buffered. You should either call this
	 * before the first call to \c flWriteChannelAsync(), or call it immediately after a call to
	 * \c flFlushAsyncWrites().
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param chunkSize The new chunksize in bytes. Passing zero sets the chunkSize to 64KiB.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the write completed successfully.
	 *     - \c FL_BAD_STATE if there is some outstanding send data.
	 */
	DLLEXPORT(FLStatus) flSetAsyncWriteChunkSize(
		struct FLContext *handle, uint16 chunkSize, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Asynchronously write the supplied data to the specified channel.
	 *
	 * Write \c count bytes from the \c data array to FPGA channel \c chan. Before calling
	 * \c flWriteChannelAsync(), you should verify that the FPGALink device actually supports
	 * CommFPGA using \c flIsCommCapable().
	 *
	 * This function is asynchronous. That means it will return immediately, usually before anything
	 * has been actually sent over USB. If the operation fails, you will not be notified of the
	 * failure until a future call to \c flAwaitAsyncWrites() or \c flReadChannelAsyncAwait(). The
	 * data is copied internally, so there's no need to worry about preserving the data: it's safe to
	 * call \c flWriteChannelAsync() on a stack-allocated array, for example.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param chan The FPGA channel to write.
	 * @param count The number of bytes to write.
	 * @param data The address of the array of bytes to be written to the FPGA.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the write completed successfully.
	 *     - \c FL_PROTOCOL_ERR if the device does not support CommFPGA.
	 *     - \c FL_ALLOC_ERR if we ran out of memory.
	 *     - \c FL_USB_ERR if a USB error occurred.
	 */
	DLLEXPORT(FLStatus) flWriteChannelAsync(
		struct FLContext *handle, uint8 chan, uint32 count, const uint8 *data,
		const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Flush any outstanding writes.
	 *
	 * Flush any writes that have been buffered up, or do nothing if no writes have been buffered.
	 * This only triggers the send over USB; it does not guarantee the micro successfully received
	 * the data. See \c flAwaitAsyncWrites().
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the write completed successfully.
	 *     - \c FL_PROTOCOL_ERR if the device does not support CommFPGA.
	 *     - \c FL_USB_ERR if a USB error occurred.
	 */
	DLLEXPORT(FLStatus) flFlushAsyncWrites(
		struct FLContext *handle, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Actually wait for confirmation that async writes were received by the micro.
	 *
	 * The first thing this does is to call \c flFlushAsyncWrites() to flush out any outstanding
	 * write commands. It will then block until the OS confirms that all the asynchronous write
	 * commands sent by \c flWriteChannelAsync() were correctly sent over USB and recieved by the
	 * micro. It cannot confirm that that the writes were received by the FPGA however: they may be
	 * waiting in the micro's output buffer.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the write completed successfully.
	 *     - \c FL_PROTOCOL_ERR if the device does not support CommFPGA.
	 *     - \c FL_BAD_STATE if there are async reads in progress.
	 *     - \c FL_USB_ERR if a USB error occurred.
	 */
	DLLEXPORT(FLStatus) flAwaitAsyncWrites(
		struct FLContext *handle, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Submit an asynchronous read on the specified channel.
	 *
	 * Submit an asynchronous read of \c count bytes from the FPGA channel \c chan. You can request
	 * at most 64KiB of data asynchronously. Before calling \c flReadChannelAsyncSubmit(), you should
	 * verify that the FPGALink device actually supports CommFPGA using \c flIsCommCapable().
	 *
	 * This function is asynchronous. That means it will return immediately, usually before the read
	 * request has been sent over USB. You will not find out the result of the read until you later
	 * call \c flReadChannelAsyncAwait() - this will give you your data, or tell you what went wrong.
	 *
	 * You should always ensure that for each call to \c flReadChannelAsyncSubmit(), there is a
	 * matching call to \c flReadChannelAsyncAwait(). You should not call any of
	 * \c flSetAsyncWriteChunkSize(), \c flAwaitAsyncWrites(), \c flWriteChannel() or
	 * \c flReadChannel() between a submit...await pair.
	 *
	 * USB host controllers typically need just one level of nesting of submit...await pairs to keep
	 * them busy. That means sequences like submit, submit, await, submit, await, submit, ..., await,
	 * await.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param chan The FPGA channel to read.
	 * @param count The number of bytes to read. Must be <= 64KiB.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the read completed successfully.
	 *     - \c FL_PROTOCOL_ERR if the device does not support CommFPGA.
	 *     - \c FL_USB_ERR if a USB error occurred.
	 */
	DLLEXPORT(FLStatus) flReadChannelAsyncSubmit(
		struct FLContext *handle, uint8 chan, uint32 count, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Await the result of a previous call to \c flReadChannelAsyncSubmit().
	 *
	 * Block until the outcome of a previous call to \c flReadChannelAsyncSubmit() is known. If the
	 * read was successful, you are given the resulting data. If not, an error code/message.
	 *
	 * On successful outcome, the \c ReadReport structure is populated with a pointer to the data,
	 * the number of bytes read, etc. This is an "out" parameter: the current state of the struct
	 * is not considered. It remains unchanged on failure.
	 *
	 * The data returned is stored in an internal buffer. It is guaranteed to remain valid until your
	 * next call to any of the CommFPGA functions.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param readReport A pointer to a <code>struct ReadReport</code> which will be populated with
	 *            the result of successful read, or left unchanged in the event of an error.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the read completed successfully.
	 *     - \c FL_USB_ERR if a USB error occurred.
	 */
	DLLEXPORT(FLStatus) flReadChannelAsyncAwait(
		struct FLContext *handle, struct ReadReport *readReport, const char **error
	) WARN_UNUSED_RESULT;
	//@}

	// ---------------------------------------------------------------------------------------------
	// JTAG functions (only if flIsNeroCapable() returns true)
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name JTAG Operations
	 * @{
	 */
	/**
	 * @brief Scan the JTAG chain and return an array of IDCODEs.
	 *
	 * Count the number of devices on the JTAG chain, and set \c *numDevices accordingly. Then, if
	 * \c deviceArray is not \c NULL, populate it with at most \c arraySize IDCODEs, in chain order.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param portConfig The port bits to use for TDO, TDI, TMS & TCK, or NULL to use the default.
	 * @param numDevices A pointer to a \c uint32 which will be set on exit to the number of devices
	 *            in the JTAG chain.
	 * @param deviceArray A pointer to an array of \c uint32, which will be populated on exit with a
	 *            list of IDCODEs in chain order. May be \c NULL, in which case the function returns
	 *            after setting \c *numDevices.
	 * @param arraySize The number of 32-bit IDCODE slots available in \c deviceArray.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the write completed successfully.
	 *     - \c FL_PROTOCOL_ERR if the device does not support NeroProg.
	 *     - \c FL_JTAG_ERR if an error occurred during the JTAG operation.
	 */
	DLLEXPORT(FLStatus) jtagScanChain(
		struct FLContext *handle, const char *portConfig,
		uint32 *numDevices, uint32 *deviceArray, uint32 arraySize,
		const char **error
	) WARN_UNUSED_RESULT;
	//@}

	// ---------------------------------------------------------------------------------------------
	// FX2 firmware functions
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name FX2 Firmware Operations
	 * @{
	 */
	/**
	 * @brief Load standard FPGALink firmware into the FX2's RAM.
	 *
	 * Load the FX2 chip at the "current" VID:PID with a precompiled firmware such that it will
	 * renumerate as the "new" VID:PID. The firmware is loaded into RAM, so the change is not
	 * permanent. Typically after calling \c flLoadStandardFirmware() applications should wait for
	 * the renumeration to complete by calling \c flIsDeviceAvailable() repeatedly until the "new"
	 * VID:PID becomes active.
	 *
	 * In addition to the "new" VID:PID, you can also customise the port pins used for JTAG
	 * operations. For this you must specify an FX2 port (C or D) and the bits within that port to
	 * be used for TDO, TDI, TMS and TCK respectively. For example, the port specification "D0234"
	 * means PD0=TDO, PD2=TDI, PD3=TMS and PD4=TCK, and is appropriate for Digilent boards (Nexys2,
	 * Nexys3, Atlys etc).
	 *
	 * @param curVidPid The current Vendor/Product (i.e VVVV:PPPP) of the FX2 device.
	 * @param newVidPid The Vendor/Product (i.e VVVV:PPPP) that you \b want the FX2 device to be.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the firmware loaded successfully.
	 *     - \c FL_USB_ERR if one of the VID:PIDs was invalid or the current VID:PID was not found.
	 *     - \c FL_FX2_ERR if there was a problem talking to the FX2.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 */
	DLLEXPORT(FLStatus) flLoadStandardFirmware(
		const char *curVidPid, const char *newVidPid, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Flash standard FPGALink firmware into the FX2's EEPROM, optionally appending an
	 * SVF, XSVF or CSVF initialisation stream and an FPGA initialisation stream.
	 *
	 * @warning This function will make permanent changes to your hardware. Remember to make a
	 * backup copy of the existing EEPROM firmware with \c flSaveFirmware() before calling it.
	 *
	 * Load a precompiled firmware into the FX2's EEPROM such that it will enumerate on power-on as
	 * the "new" VID:PID. If \c xsvfFile is not \c NULL, its contents are compressed and appended to
	 * the end of the FX2 firmware, and played into the JTAG chain on power-on. If a write buffer
	 * has been built (by calls to \c flAppendWriteChannelCommand()), this will be appended to the
	 * end of the FPGA configuration data and will be played into the FPGA on power-on config.
	 *
	 * In addition to the "new" VID:PID, you can also customise the port pins used for JTAG
	 * operations. For this you must specify an FX2 port (C or D) and the bits within that port to
	 * be used for TDO, TDI, TMS and TCK respectively. For example, the port specification "D0234"
	 * means PD0=TDO, PD2=TDI, PD3=TMS and PD4=TCK, and is appropriate for Digilent boards (Nexys2,
	 * Nexys3, Atlys etc).
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param newVidPid The Vendor/Product (i.e VVVV:PPPP) you want the FX2 to be on power-on.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the firmware loaded successfully.
	 *     - \c FL_USB_ERR if the VID:PID was invalid.
	 *     - \c FL_FX2_ERR if there was a problem talking to the FX2, or the EEPROM was too small.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 */
	DLLEXPORT(FLStatus) flFlashStandardFirmware(
		struct FLContext *handle, const char *newVidPid, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Load custom firmware (<code>.hex</code>) into the FX2's RAM.
	 *
	 * Load the FX2 chip at the given VID:PID with a <code>.hex</code> firmware file. The firmware
	 * is loaded into RAM, so the change is not permanent.
	 *
	 * @param curVidPid The current Vendor/Product (i.e VVVV:PPPP) of the FX2 device.
	 * @param fwFile A <code>.hex</code> file containing new FX2 firmware to be loaded into the
                  FX2's RAM.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the firmware loaded successfully.
	 *     - \c FL_USB_ERR if the VID:PID was invalid.
	 *     - \c FL_FILE_ERR if the firmware file has a bad extension or could not be loaded.
	 *     - \c FL_FX2_ERR if there was a problem talking to the FX2.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 */
	DLLEXPORT(FLStatus) flLoadCustomFirmware(
		const char *curVidPid, const char *fwFile, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Flash a custom firmware from a <code>.hex</code> or <code>.iic</code> file into the
	 *            FX2's EEPROM.
	 *
	 * @warning This function will make permanent changes to your hardware. Remember to make a
	 * backup copy of the existing EEPROM firmware with \c flSaveFirmware() before calling it.
	 *
	 * Load a custom firmware from a <code>.hex</code> or <code>.iic</code> file into the FX2's
	 * EEPROM.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param fwFile A <code>.hex</code> or <code>.iic</code> file containing new FX2 firmware to be
	 *            loaded into the FX2's EEPROM.
	 * @param eepromSize The size in kilobits of the EEPROM (e.g Nexys2's EEPROM is 128kbit).
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the firmware loaded successfully.
	 *     - \c FL_FILE_ERR if the firmware file could not be loaded.
	 *     - \c FL_FX2_ERR if there was a problem talking to the FX2, or if the EEPROM was too
	 *            small.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 */
	DLLEXPORT(FLStatus) flFlashCustomFirmware(
		struct FLContext *handle, const char *fwFile, uint32 eepromSize, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Save existing EEPROM data to an <code>.iic</code> file.
	 *
	 * The existing EEPROM firmware is saved to a file, for backup purposes.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param eepromSize The size in kilobits of the EEPROM (e.g Nexys2's EEPROM is 128kbit).
	 * @param saveFile An <code>.iic</code> file to save the EEPROM to.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the firmware loaded successfully.
	 *     - \c FL_FILE_ERR if the firmware file could not be loaded.
	 *     - \c FL_FX2_ERR if there was a problem talking to the FX2.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 */
	DLLEXPORT(FLStatus) flSaveFirmware(
		struct FLContext *handle, uint32 eepromSize, const char *saveFile, const char **error
	) WARN_UNUSED_RESULT;
	//@}

	// ---------------------------------------------------------------------------------------------
	// Utility functions
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name Utility Functions
	 * @{
	 */
	/**
	 * @brief Sleep for the specified number of milliseconds.
	 * @param ms The number of milliseconds to sleep.
	 */
	DLLEXPORT(void) flSleep(
		uint32 ms
	);

	/**
	 * @brief Return a newly-allocated buffer with the specified file loaded into it.
	 *
	 * The specified file is queried for its length, that length is written to the \c length
	 * parameter. Then a buffer of that length is allocated, and the file is loaded into it and a
	 * pointer to the buffer returned. The responsibility for the buffer passes to the caller; it
	 * must be freed later by a call to \c flFreeFile().
	 *
	 * @param name The name of the file to load.
	 * @param length A pointer to a \c uint32 which will be populated on exit with the length of the
	 *            file.
	 * @returns A pointer to the allocated buffer, or NULL if the file could not be loaded.
	 */
	DLLEXPORT(uint8*) flLoadFile(
		const char *name, uint32 *length
	);

	/**
	 * @brief Free a buffer previously returned by \c flLoadFile().
	 * @param buffer A buffer previously returned by \c flLoadFile().
	 */
	DLLEXPORT(void) flFreeFile(
		uint8 *buffer
	);

	/**
	 * @brief Access port lines on the microcontroller
	 *
	 * With this function you can set a 16-bit data direction register and write a 16-bit number to
	 * the corresponding port lines. You can also optionally query the state of the port lines. The
	 * actual physical ports used will differ from micro to micro.
	 *
	 * On the FX2LP, the low order bytes access port D (which is also used for JTAG, so only four
	 * bits are actually available) and the high order bytes access port C (which is only available
	 * on the larger FX2LP chips).
	 *
	 * On the AVR, the low order bytes access port B (which is also used for JTAG, so only four bits
	 * are actually available) and the high order bytes access port D.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param portNumber Which port to use (i.e 0=PortA, 1=PortB, 2=PortC, etc).
	 * @param bitNumber The bit within the chosen port to use.
	 * @param pinConfig Either INPUT, HIGH or LOW.
	 * @param pinRead Pointer to a <code>uint8</code> to be set on exit to 0 or 1 depending on
	 *            the current state of the pin. May be \c NULL if you're not interested.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the port access command completed successfully.
	 *     - \c FL_USB_ERR if the micro failed to respond to the port access command.
	 */
	DLLEXPORT(FLStatus) flSingleBitPortAccess(
		struct FLContext *handle, uint8 portNumber, uint8 bitNumber,
		PinConfig pinConfig, uint8 *pinRead, const char **error
	) WARN_UNUSED_RESULT;

	DLLEXPORT(FLStatus) flMultiBitPortAccess(
		struct FLContext *handle, const char *portConfig, uint32 *readState, const char **error
	) WARN_UNUSED_RESULT;

	DLLEXPORT(FLStatus) flResetToggle(
		struct FLContext *handle, const char **error
	) WARN_UNUSED_RESULT;

	DLLEXPORT(FLStatus) flProgram(
		struct FLContext *handle, const char *portConfig, const char *progFile, const char **error
	) WARN_UNUSED_RESULT;

	DLLEXPORT(FLStatus) flProgramBlob(
		struct FLContext *handle, const char *portConfig, const uint8 *blobData, uint32 blobLength,
		const char **error
	) WARN_UNUSED_RESULT;

	DLLEXPORT(FLStatus) jtagOpen(
		struct FLContext *handle, const char *portConfig, const char **error
	) WARN_UNUSED_RESULT;

	DLLEXPORT(FLStatus) jtagClose(
		struct FLContext *handle, const char **error
	) WARN_UNUSED_RESULT;

	// Special values for inData parameter of jtagShift() declared below
	// @cond NEVER
	#define ZEROS (const uint8*)NULL
	#define ONES (ZEROS - 1)
	// @endcond

	// Shift "numBits" bits from "inData" into TDI, at the same time shifting the same number of
	// bits from TDO into "outData". If "isLast" is true, leave Shift-DR state on final bit. If you
	// want inData to be all zeros or all ones, you can use ZEROS or ONES respectively. This is more
	// efficient than physically sending an array containing all zeros or all 0xFFs.
	DLLEXPORT(FLStatus) jtagShift(
		struct FLContext *handle, uint32 numBits, const uint8 *inData, uint8 *outData, uint8 isLast,
		const char **error
	) WARN_UNUSED_RESULT;
	
	// Clock "transitionCount" bits from "bitPattern" into TMS, starting with the LSB.
	DLLEXPORT(FLStatus) jtagClockFSM(
		struct FLContext *handle, uint32 bitPattern, uint8 transitionCount, const char **error
	) WARN_UNUSED_RESULT;
	
	// Toggle TCK "numClocks" times.
	DLLEXPORT(FLStatus) jtagClocks(
		struct FLContext *handle, uint32 numClocks, const char **error
	) WARN_UNUSED_RESULT;

	//@}

#ifdef __cplusplus
}
#endif

#endif
