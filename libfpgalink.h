/* 
 * Copyright (C) 2011 Chris McClelland
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file libfpgalink.h
 *
 * The <b>FPGALink</b> library makes it easier to talk to an FPGA over USB (via a suitable micro).
 *
 * It performs three classes of function:
 * - Load firmware into an FX2LP chip
 * - Play an XSVF file into a JTAG chain
 * - Communicate with a target device over USB
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
		FL_SUCCESS = 0,   ///< The operation completed successfully.
		FL_ALLOC_ERR,     ///< There was a memory allocation error.
		FL_USB_ERR,       ///< There was some USB-related problem.
		FL_PROTOCOL_ERR,  ///< The device is probably not a valid \b FPGALink device.
		FL_SYNC_ERR,      ///< The library was unable to synchronise the device's bulk endpoints.
		FL_FX2_ERR,       ///< There was some problem talking to the FX2 chip.
		FL_JTAG_ERR,      ///< There was some problem with the \b NeroJTAG interface.
		FL_FILE_ERR,      ///< There was a file-related problem.
		FL_INTERNAL_ERR   ///< An internal error occurred. Please report it!
	} FLStatus;

	/**
	 * Boolean type; maps to signed 32-bit integer on Windows and Linux. This is here just to make
	 * boolean parameter lengths explicit, thereby simplifying interfacing from other languages.
	 */
	typedef enum {
		FL_FALSE,   ///< False (=0)
		FL_TRUE     ///< True (=1)
	} FLBool;
	//@}

	struct FLContext;  // Opaque context type

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
	 * @param vid The Vendor ID of the device.
	 * @param pid The Product ID of the device.
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
	 *     - \c FL_USB_ERR if the device cannot be found or opened.
	 *     - \c FL_PROTOCOL_ERR if the device is not an \b FPGALink device.
	 *     - \c FL_SYNC_ERR if the bulk endpoint pairs cannot be synchronised.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 */
	DLLEXPORT(FLStatus) flOpen(
		uint16 vid, uint16 pid, struct FLContext **handle, const char **error
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
	 * \c flLoadCustomFirmware() during which this function will still return \c FL_TRUE for the
	 * "current" VID/PID, so when you load new firmware, it's important to ensure that the "new"
	 * VID/PID is different from the "current" VID/PID to avoid such false positives.
	 *
	 * @param vid The Vendor ID of the device.
	 * @param pid The Product ID of the device.
	 * @param isAvailable A pointer to a 32-bit integer which will be set on exit to \c FL_FALSE
	 *            or \c FL_TRUE.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if all is well (\c *isAvailable is valid).
	 *     - \c FL_USB_ERR if no USB buses were found; did you remember to call \c flInitialise()?
	 */
	DLLEXPORT(FLStatus) flIsDeviceAvailable(
		uint16 vid, uint16 pid, FLBool *isAvailable, const char **error
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
	 * @returns \c FL_TRUE if the device supports NeroJTAG, else \c FL_FALSE.
	 */
	DLLEXPORT(FLBool) flIsNeroCapable(struct FLContext *handle);

	/**
	 * @brief Check to see if the device supports \b CommFPGA.
	 *
	 * \b CommFPGA is a simple register read/write protocol using USB bulk endpoints 6 and 8. This
	 * only returns whether the micro itself supports the protocol, not whether the target logic
	 * device has been configured with a suitable design implementing the other end of the protocol.
	 * An affirmative response means you are free to call \c flReadRegister(), \c flWriteRegister()
	 * and \c flIsFPGARunning().
	 *
	 * This function merely returns a flag determined by \c flOpen(), so it cannot fail.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @returns \c FL_TRUE if the device supports \b CommFPGA, else \c FL_FALSE.
	 */
	DLLEXPORT(FLBool) flIsCommCapable(struct FLContext *handle);
	//@}

	// ---------------------------------------------------------------------------------------------
	// CommFPGA register read/write functions (only if flIsCommCapable() returns \c FL_TRUE)
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name CommFPGA Operations
	 * @{
	 */
	/**
	 * @brief Check to see if the FPGA is running.
	 *
	 * This may only be called if \c flIsCommCapable() returns \c FL_TRUE. It merely verifies that
	 * the FPGA is asserting that it's ready to read. Before calling \c flIsFPGARunning(), you
	 * should verify that the \b FPGALink device actually supports \b CommFPGA using
	 * \c flIsCommCapable().
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param isRunning A pointer to a 32-bit integer which will be set on exit to \c FL_FALSE
	 *            or \c FL_TRUE.
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
		struct FLContext *handle, FLBool *isRunning, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Write the supplied data to the specified register.
	 *
	 * Write \c count bytes from the \c data array to FPGA register \c reg, with the given
	 * \c timeout in milliseconds. In the event of a timeout, the connection between host and FPGA
	 * will be left in an undefined state; before the two can resynchronise it's likely the FPGA
	 * will need to be reset and the host side disconnected (\c flOpen()) and reconnected
	 * (\c flClose()) again. Before calling \c flWriteRegister(), you should verify that the
	 * \b FPGALink device actually supports \b CommFPGA using \c flIsCommCapable().
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param timeout The time to wait (in milliseconds) for the write to complete before giving up.
	 * @param reg The FPGA register to write.
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
	DLLEXPORT(FLStatus) flWriteRegister(
		struct FLContext *handle, uint32 timeout, uint8 reg, uint32 count, const uint8 *data,
		const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Read the specified register into the supplied buffer.
	 *
	 * Read \c count bytes from the FPGA register \c reg to the \c data array, with the given
	 * \c timeout in milliseconds. In the event of a timeout, the connection between host and FPGA
	 * will be left in an undefined state; before the two can resynchronise it's likely the FPGA
	 * will need to be reset and the host side disconnected (\c flOpen()) and reconnected
	 * (\c flClose()) again. Before calling \c flReadRegister(), you should verify that the
	 * \b FPGALink device actually supports \b CommFPGA using \c flIsCommCapable().
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param timeout The time to wait (in milliseconds) for the read to complete before giving up.
	 * @param reg The FPGA register to read.
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
	DLLEXPORT(FLStatus) flReadRegister(
		struct FLContext *handle, uint32 timeout, uint8 reg, uint32 count, uint8 *buf,
		const char **error
	) WARN_UNUSED_RESULT;
	//@}

	// ---------------------------------------------------------------------------------------------
	// JTAG functions (only if flIsNeroCapable() returns \c FL_TRUE)
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name NeroJTAG Operations
	 * @{
	 */
	/**
	 * @brief Play an XSVF file into the JTAG chain.
	 *
	 * Play a Xilinx Serial Vector Format file into the JTAG chain. This is typically used for
	 * programming devices. Before calling \c flPlayXSVF(), you should verify that the \b FPGALink
	 * device actually supports \b NeroJTAG using \c flIsNeroCapable().
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param xsvfFile An XSVF file (generated by Xilinx \b iMPACT).
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the XSVF file played successfully.
	 *     - \c FL_PROTOCOL_ERR if the device does not support \b NeroJTAG.
	 *     - \c FL_FILE_ERR if the XSVF file could not be loaded.
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
	 * @param currentVid The Vendor ID of the FX2 device.
	 * @param currentPid The Product ID of the FX2 device.
	 * @param newVid The Vendor ID you want the FX2 to renumerate as.
	 * @param newPid The Product ID you want the FX2 to renumerate as.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the firmware loaded successfully.
	 *     - \c FL_USB_ERR if the current VID/PID was not found.
	 *     - \c FL_FX2_ERR if there was a problem talking to the FX2.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 */
	DLLEXPORT(FLStatus) flLoadStandardFirmware(
		uint16 currentVid, uint16 currentPid, uint16 newVid, uint16 newPid, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Flash standard \b FPGALink firmware into the FX2's EEPROM, optionally appending an
	 * XSVF initialisation stream and an FPGA initialisation stream.
	 *
	 * @warning This function will make permanent changes to your hardware. Remember to make a
	 * backup copy of the existing EEPROM firmware with \c flSaveFirmware() before calling it.
	 *
	 * Load a precompiled firmware into the FX2's EEPROM such that it will enumerate on power-on as
	 * the "new" VID/PID. If \c xsvfFile is not \c NULL, its contents are compressed and appended to
	 * the end of the FX2 firmware, and played into the JTAG chain on power-on. If an initialisation
	 * buffer has been built (by calls to \c flAppendWriteRegisterCommand()), this will be appended
	 * to the end of the XSVF data and played into the FPGA on power-on.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param newVid The Vendor ID you want the FX2 to enumerate as on power-on.
	 * @param newPid The Product ID you want the FX2 to enumerate as on power-on.
	 * @param eepromSize The size in kilobits of the EEPROM (e.g Nexys2's EEPROM is 128kbit).
	 * @param xsvfFile An XSVF file to play on power-up, or \c NULL.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the firmware loaded successfully.
	 *     - \c FL_FX2_ERR if there was a problem talking to the FX2, or if the EEPROM was too
	 *            small.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 */
	DLLEXPORT(FLStatus) flFlashStandardFirmware(
		struct FLContext *handle, uint16 newVid, uint16 newPid, uint32 eepromSize,
		const char *xsvfFile, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Load custom firmware (<code>.hex</code>) into the FX2's RAM.
	 *
	 * Load the FX2 chip at the given VID/PID with a <code>.hex</code> firmware file. The firmware
	 * is loaded into RAM, so the change is not permanent.
	 *
	 * @param vid The Vendor ID of the FX2 device.
	 * @param pid The Product ID of the FX2 device.
	 * @param fwFile A <code>.hex</code> file containing new FX2 firmware to be loaded into the
                  FX2's RAM.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the firmware loaded successfully.
	 *     - \c FL_FILE_ERR if the firmware file has a bad extension or could not be loaded.
	 *     - \c FL_FX2_ERR if there was a problem talking to the FX2.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 */
	DLLEXPORT(FLStatus) flLoadCustomFirmware(
		uint16 vid, uint16 pid, const char *fwFile, const char **error
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
	 * @brief Clean the init buffer (if any).
	 * 
	 * The init buffer is like a notepad onto which one or more FPGA register write commands can be
	 * written (by \c flAppendWriteRegisterCommand()). The current state of the notepad can then be
	 * written (by \c flFlashStandardFirmware()) to the FX2's EEPROM for execution on power-on.
	 *
	 * @param handle The handle returned by \c flOpen().
	 */
	DLLEXPORT(void) flCleanInitBuffer(
		struct FLContext *handle
	);

	/**
	 * @brief Append a write command to the end of the init buffer.
	 *
	 * The init buffer is like a notepad onto which one or more FPGA register write commands can be
	 * written by calls to this function. The current state of the notepad can then be written (by
	 * \c flFlashStandardFirmware()) to the FX2's EEPROM for execution on power-on.
	 *
	 * You'll notice that apart from the lack of a \c timeout parameter, the signature of this
	 * function is identical to that of \c flWriteRegister(), but rather than executing the write
	 * immediately, it appends the write command to the init buffer for playback on power-on.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param reg The FPGA register to write.
	 * @param count The number of bytes to write.
	 * @param data The address of the array of bytes to be written to the FPGA.
	 * @param error A pointer to a <code>char*</code> which will be set on exit to an allocated
	 *            error message if something goes wrong. Responsibility for this allocated memory
	 *            passes to the caller and must be freed with \c flFreeError(). If \c error is
	 *            \c NULL, no allocation is done and no message is returned, but the return code
	 *            will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the write command was successfully appended to the init buffer.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 */
	DLLEXPORT(FLStatus) flAppendWriteRegisterCommand(
		struct FLContext *handle, uint8 reg, uint32 count, const uint8 *data, const char **error
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
	//@}


#ifdef __cplusplus
}
#endif

#endif
