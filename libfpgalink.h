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
 * - Load device firmware and EEPROM (specific to Cypress FX2LP)
 * - Play an SVF, XSVF or CSVF file into a JTAG chain for FPGA programming
 * - Read and write (over USB) up to 128 byte-wide data channels in the target FPGA
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
	 * @name Enumerations
	 * @{
	 */
	/**
	 * Return codes from the functions.
	 */
	typedef enum {
		FL_SUCCESS = 0,          ///< The operation completed successfully.
		FL_ALLOC_ERR,            ///< There was a memory allocation error.
		FL_USB_ERR,              ///< There was some USB-related problem.
		FL_PROTOCOL_ERR,         ///< The device is probably not a valid \b FPGALink device.
		FL_SYNC_ERR,             ///< The library was unable to synchronise the device's bulk endpoints.
		FL_FX2_ERR,              ///< There was some problem talking to the FX2 chip.
		FL_JTAG_ERR,             ///< There was some problem with the \b NeroJTAG interface.
		FL_FILE_ERR,             ///< There was a file-related problem.
		FL_WBUF_ERR,             ///< There was some problem with the write buffer.
		FL_BUF_INIT_ERR,         ///< The CSVF buffer could not be allocated.
		FL_BUF_APPEND_ERR,       ///< The CSVF buffer could not be grown.
		FL_BUF_LOAD_ERR,         ///< The XSVF file could not be loaded.
		FL_UNSUPPORTED_CMD_ERR,  ///< The XSVF file contains an unsupported command.
		FL_UNSUPPORTED_DATA_ERR, ///< The XSVF file contains an unsupported XENDIR or XENDDR.
		FL_UNSUPPORTED_SIZE_ERR, ///< The XSVF file requires more buffer space than is available.
		FL_SVF_PARSE_ERR,        ///< The SVF file was not parseable.
		FL_INTERNAL_ERR          ///< An internal error occurred. Please report it!
	} FLStatus;
	//@}

	struct FLContext;  // Opaque context type
	struct Buffer;     // Forward declaration of Buffer

	// ---------------------------------------------------------------------------------------------
	// Miscellaneous functions
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name Miscellaneous Functions
	 * @{
	 */
	/**
	 * @brief Initialise the library.
	 */
	DLLEXPORT(void) flInitialise(void);

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
	 * @brief Open a connection to the \b FPGALink device at the specified VID & PID.
	 *
	 * Connects to the device and verifies it is an \b FPGALink device, then queries its
	 * capabilities and synchronises the USB bulk endpoints.
	 *
	 * @param vp The Vendor/Product (i.e VVVV:PPPP) of the \b FPGALink device.
	 * @param handle A pointer to a <code>struct FLContext*</code> which will be set on exit to
	 *            point at a newly-allocated context structure. Responsibility for this allocated
	 *            memory (and its associated USB resources) passes to the caller and must be freed
	 *            with \c flClose().
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if all is well (\c *handle is valid).
	 *     - \c FL_USB_ERR if the VID/PID is invalid or the device cannot be found or opened.
	 *     - \c FL_PROTOCOL_ERR if the device is not an \b FPGALink device.
	 *     - \c FL_SYNC_ERR if the bulk endpoint pairs cannot be synchronised.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 */
	DLLEXPORT(FLStatus) flOpen(
		const char *vp, struct FLContext **handle, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Close the connection to the \b FPGALink device.
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
	 * The \b LibUSB devices in the system are searched for a device with the given VID/PID. On
	 * Linux this will always work. On Windows it's necessary to install a \b LibUSB-Win32 driver
	 * for the device beforehand.
	 *
	 * There is a short period of time following a call to \c flLoadStandardFirmware() or
	 * \c flLoadCustomFirmware() during which this function will still return true for the
	 * "current" VID/PID, so when you load new firmware, it's important to ensure that the "new"
	 * VID/PID is different from the "current" VID/PID to avoid such false positives.
	 *
	 * @param vp The Vendor/Product (i.e VVVV:PPPP) of the \b FPGALink device.
	 * @param isAvailable A pointer to an 8-bit integer which will be set on exit to 1 if available
	 *            else 0.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if all is well (\c *isAvailable is valid).
	 *     - \c FL_USB_ERR if the VID/PID is invalid or if no USB buses were found (did you
	 *            remember to call \c flInitialise()?).
	 */
	DLLEXPORT(FLStatus) flIsDeviceAvailable(
		const char *vp, bool *isAvailable, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Check to see if the device supports \b NeroJTAG.
	 *
	 * \b NeroJTAG is a simple JTAG-over-USB protocol, currently implemented for Atmel AVR and
	 * Cypress FX2. It uses bulk endpoints 2 and 4. An affirmative response means you can call
	 * \c flPlayXSVF() and \c flScanChain().
	 *
	 * This function merely returns a flag determined by \c flOpen(), so it cannot fail.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @returns An 8-bit integer: 1 if the device supports NeroJTAG, else 0.
	 */
	DLLEXPORT(bool) flIsNeroCapable(struct FLContext *handle);

	/**
	 * @brief Check to see if the device supports \b CommFPGA.
	 *
	 * \b CommFPGA is a simple channel read/write protocol using USB bulk endpoints 6 and 8. This
	 * only returns whether the micro itself supports the protocol, not whether the target logic
	 * device has been configured with a suitable design implementing the other end of the protocol.
	 * An affirmative response means you are free to call \c flReadChannel(), \c flWriteChannel()
	 * and \c flIsFPGARunning().
	 *
	 * This function merely returns a flag determined by \c flOpen(), so it cannot fail.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @returns An 8-bit integer: 1 if the device supports \b CommFPGA, else 0.
	 */
	DLLEXPORT(bool) flIsCommCapable(struct FLContext *handle);
	//@}

	// ---------------------------------------------------------------------------------------------
	// CommFPGA channel read/write functions (only if flIsCommCapable() returns true)
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name CommFPGA Operations
	 * @{
	 */
	/**
	 * @brief Check to see if the FPGA is running.
	 *
	 * This may only be called if \c flIsCommCapable() returns true. It merely verifies that
	 * the FPGA is asserting that it's ready to read. Before calling \c flIsFPGARunning(), you
	 * should verify that the \b FPGALink device actually supports \b CommFPGA using
	 * \c flIsCommCapable().
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param isRunning A pointer to an 8-bit integer which will be set on exit to 1 if available
	 *            else 0.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if all is well (<code>*isRunning</code> is valid).
	 *     - \c FL_PROTOCOL_ERR if the device does not support \b CommFPGA.
	 *     - \c FL_USB_ERR if the device no longer responds.
	 */
	DLLEXPORT(FLStatus) flIsFPGARunning(
		struct FLContext *handle, bool *isRunning, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Read the specified channel into the supplied buffer.
	 *
	 * Read \c count bytes from the FPGA channel \c chan to the \c data array, with the given
	 * \c timeout in milliseconds. In the event of a timeout, the connection between host and FPGA
	 * will be left in an undefined state; before the two can resynchronise it's likely the FPGA
	 * will need to be reset and the host side disconnected (\c flOpen()) and reconnected
	 * (\c flClose()) again. Before calling \c flReadChannel(), you should verify that the
	 * \b FPGALink device actually supports \b CommFPGA using \c flIsCommCapable().
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param timeout The time to wait (in milliseconds) for the read to complete before giving up.
	 * @param chan The FPGA channel to read.
	 * @param count The number of bytes to read.
	 * @param buf The address of a buffer to store the bytes read from the FPGA.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the write completed successfully.
	 *     - \c FL_PROTOCOL_ERR if the device does not support \b CommFPGA.
	 *     - \c FL_USB_ERR if a USB error (including timeout) occurred.
	 */
	DLLEXPORT(FLStatus) flReadChannel(
		struct FLContext *handle, uint32 timeout, uint8 chan, uint32 count, uint8 *buf,
		const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Write the supplied data to the specified channel.
	 *
	 * Write \c count bytes from the \c data array to FPGA channel \c chan, with the given
	 * \c timeout in milliseconds. In the event of a timeout, the connection between host and FPGA
	 * will be left in an undefined state; before the two can resynchronise it's likely the FPGA
	 * will need to be reset and the host side disconnected (\c flOpen()) and reconnected
	 * (\c flClose()) again. Before calling \c flWriteChannel(), you should verify that the
	 * \b FPGALink device actually supports \b CommFPGA using \c flIsCommCapable().
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param timeout The time to wait (in milliseconds) for the write to complete before giving up.
	 * @param chan The FPGA channel to write.
	 * @param count The number of bytes to write.
	 * @param data The address of the array of bytes to be written to the FPGA.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the write completed successfully.
	 *     - \c FL_PROTOCOL_ERR if the device does not support \b CommFPGA.
	 *     - \c FL_USB_ERR if a USB error (including timeout) occurred.
	 */
	DLLEXPORT(FLStatus) flWriteChannel(
		struct FLContext *handle, uint32 timeout, uint8 chan, uint32 count, const uint8 *data,
		const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Append a write command to the end of the write buffer.
	 *
	 * The write buffer is like a notepad onto which one or more FPGA channel write commands can be
	 * written by calls to this function. The current state of the notepad can then be played in one
	 * go (by \c flPlayWriteBuffer()) or written (by \c flFlashStandardFirmware()) to the FX2's
	 * EEPROM for execution on power-on.
	 *
	 * You'll notice that apart from the lack of a \c timeout parameter, the signature of this
	 * function is identical to that of \c flWriteChannel(), but rather than executing the write
	 * immediately, it just appends the write command to the write buffer for playback later.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param chan The FPGA channel to write.
	 * @param count The number of bytes to write.
	 * @param data The address of the array of bytes to be written to the FPGA.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the write command was successfully appended to the write buffer.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 */
	DLLEXPORT(FLStatus) flAppendWriteChannelCommand(
		struct FLContext *handle, uint8 chan, uint32 count, const uint8 *data, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Play the write buffer into the \b FPGALink device immediately.
	 *
	 * Appending several small (i.e <10KiB) channel writes to the write buffer and playing them in
	 * one go is more efficient than making several calls to \c flWriteChannel().
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param timeout The time to wait (in milliseconds) for the operation to complete before giving
	 *            up.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the write buffer played successfully.
	 *     - \c FL_WBUF_ERR if there was no write buffer.
	 *     - \c FL_USB_ERR if a USB error (including timeout) occurred.
	 */
	DLLEXPORT(FLStatus) flPlayWriteBuffer(
		struct FLContext *handle, uint32 timeout, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Clean the write buffer (if any).
	 * 
	 * The write buffer is like a notepad onto which one or more FPGA channel write commands can be
	 * written (by \c flAppendWriteChannelCommand()). The current state of the notepad can then be
	 * played in one go (by \c flPlayWriteBuffer()) or written (by \c flFlashStandardFirmware()) to
	 * the FX2's EEPROM for execution on power-on.
	 *
	 * @param handle The handle returned by \c flOpen().
	 */
	DLLEXPORT(void) flCleanWriteBuffer(
		struct FLContext *handle
	);
	//@}

	// ---------------------------------------------------------------------------------------------
	// JTAG functions (only if flIsNeroCapable() returns true)
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name NeroJTAG Operations
	 * @{
	 */
	/**
	 * @brief Play an SVF, XSVF or CSVF file into the JTAG chain.
	 *
	 * Despite the name of the function, it does actually support regular Serial Vector Format files
	 * and Compressed Serial Vector Format files in addition to Xilinx Serial Vector Format files.
	 * All are simply played into the JTAG chain, so it's your responsibility to ensure that the
	 * file is created for the appropriate chain configuration. Typically, this is used for
	 * programming devices, but it doesn't have to be - the file can be arbitrary JTAG operations.
	 * Before calling \c flPlayXSVF(), you should verify that the \b FPGALink device actually
	 * supports \b NeroJTAG using \c flIsNeroCapable().
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param xsvfFile An SVF, XSVF or CSVF file.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the file played successfully.
	 *     - \c FL_PROTOCOL_ERR if the device does not support \b NeroJTAG.
	 *     - \c FL_FILE_ERR if the file could not be loaded.
	 *     - \c FL_JTAG_ERR if an error occurred during the JTAG operation.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 */
	DLLEXPORT(FLStatus) flPlayXSVF(
		struct FLContext *handle, const char *xsvfFile, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Scan the JTAG chain and return an array of IDCODEs.
	 *
	 * Count the number of devices on the JTAG chain, and set \c *numDevices accordingly. Then, if
	 * \c deviceArray is not \c NULL, populate it with at most \c arraySize IDCODEs, in chain order.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param numDevices A pointer to a \c uint32 which will be set on exit to the number of devices
	 *            in the JTAG chain.
	 * @param deviceArray A pointer to an array of \c uint32, which will be populated on exit with a
	 *            list of IDCODEs in chain order. May be \c NULL, in which case the function returns
	 *            after setting \c *numDevices.
	 * @param arraySize The number of 32-bit IDCODE slots available in \c deviceArray.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the write completed successfully.
	 *     - \c FL_PROTOCOL_ERR if the device does not support \b NeroJTAG.
	 *     - \c FL_JTAG_ERR if an error occurred during the JTAG operation.
	 */
	DLLEXPORT(FLStatus) flScanChain(
		struct FLContext *handle, uint32 *numDevices, uint32 *deviceArray, uint32 arraySize,
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
	 * @brief Load standard \b FPGALink firmware into the FX2's RAM.
	 *
	 * Load the FX2 chip at the "current" VID/PID with a precompiled firmware such that it will
	 * renumerate as the "new" VID/PID. The firmware is loaded into RAM, so the change is not
	 * permanent. Typically after calling \c flLoadStandardFirmware() applications should wait for
	 * the renumeration to complete by calling \c flIsDeviceAvailable() repeatedly until the "new"
	 * VID/PID becomes active.
	 *
	 * In addition to the "new" VID/PID, you can also customise the port pins used for JTAG
	 * operations. For this you must specify an FX2 port (C or D) and the bits within that port to
	 * be used for TDO, TDI, TMS and TCK respectively. For example, the port specification "D0234"
	 * means PD0=TDO, PD2=TDI, PD3=TMS and PD4=TCK, and is appropriate for Digilent boards (Nexys2,
	 * Nexys3, Atlys etc).
	 *
	 * @param curVidPid The current Vendor/Product (i.e VVVV:PPPP) of the FX2 device.
	 * @param newVidPid The Vendor/Product (i.e VVVV:PPPP) that you \b want the FX2 device to be.
	 * @param jtagPort A string describing the JTAG port, e.g "D0234".
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the firmware loaded successfully.
	 *     - \c FL_USB_ERR if one of the VID/PIDs was invalid or the current VID/PID was not found.
	 *     - \c FL_FX2_ERR if there was a problem talking to the FX2.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 */
	DLLEXPORT(FLStatus) flLoadStandardFirmware(
		const char *curVidPid, const char *newVidPid, const char *jtagPort, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Flash standard \b FPGALink firmware into the FX2's EEPROM, optionally appending an
	 * SVF, XSVF or CSVF initialisation stream and an FPGA initialisation stream.
	 *
	 * @warning This function will make permanent changes to your hardware. Remember to make a
	 * backup copy of the existing EEPROM firmware with \c flSaveFirmware() before calling it.
	 *
	 * Load a precompiled firmware into the FX2's EEPROM such that it will enumerate on power-on as
	 * the "new" VID/PID. If \c xsvfFile is not \c NULL, its contents are compressed and appended to
	 * the end of the FX2 firmware, and played into the JTAG chain on power-on. If a write buffer
	 * has been built (by calls to \c flAppendWriteChannelCommand()), this will be appended to the
	 * end of the FPGA configuration data and will be played into the FPGA on power-on config.
	 *
	 * In addition to the "new" VID/PID, you can also customise the port pins used for JTAG
	 * operations. For this you must specify an FX2 port (C or D) and the bits within that port to
	 * be used for TDO, TDI, TMS and TCK respectively. For example, the port specification "D0234"
	 * means PD0=TDO, PD2=TDI, PD3=TMS and PD4=TCK, and is appropriate for Digilent boards (Nexys2,
	 * Nexys3, Atlys etc).
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param newVidPid The Vendor/Product (i.e VVVV:PPPP) you want the FX2 to be on power-on.
	 * @param jtagPort A string describing the JTAG port, e.g "D0234".
	 * @param eepromSize The size in kilobits of the EEPROM (e.g Nexys2's EEPROM is 128kbit).
	 * @param xsvfFile An SVF, XSVF or CSVF file to play on power-up, or \c NULL.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the firmware loaded successfully.
	 *     - \c FL_USB_ERR if the VID/PID was invalid.
	 *     - \c FL_FX2_ERR if there was a problem talking to the FX2, or the EEPROM was too small.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 */
	DLLEXPORT(FLStatus) flFlashStandardFirmware(
		struct FLContext *handle, const char *newVidPid, const char *jtagPort,
		uint32 eepromSize, const char *xsvfFile, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Load custom firmware (<code>.hex</code>) into the FX2's RAM.
	 *
	 * Load the FX2 chip at the given VID/PID with a <code>.hex</code> firmware file. The firmware
	 * is loaded into RAM, so the change is not permanent.
	 *
	 * @param curVidPid The current Vendor/Product (i.e VVVV:PPPP) of the FX2 device.
	 * @param fwFile A <code>.hex</code> file containing new FX2 firmware to be loaded into the
                  FX2's RAM.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the firmware loaded successfully.
	 *     - \c FL_USB_ERR if the VID/PID was invalid.
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
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
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
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
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
	 * @param portWrite Value to write to the port lines.
	 * @param ddr Value to write to the DDR registers.
	 * @param portRead Pointer to a \c uint16 to be populated with the value read back from the port
	 *            lines. May be \c NULL if you're not interested.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the port access command completed successfully.
	 *     - \c FL_USB_ERR if the micro failed to respond to the port access command.
	 */
	DLLEXPORT(FLStatus) flPortAccess(
		struct FLContext *handle, uint16 portWrite, uint16 ddr, uint16 *portRead, const char **error
	) WARN_UNUSED_RESULT;
	//@}

#ifdef __cplusplus
}
#endif

#endif
