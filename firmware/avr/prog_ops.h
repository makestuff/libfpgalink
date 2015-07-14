#undef MISO_IN
#undef bmMISO
#undef MOSI_OUT
#undef bmMOSI
#undef SS_OUT
#undef bmSS
#undef SCK_OUT
#undef bmSCK
#undef mosiSet
#undef mosiBit

#define MISO_IN  PIN(MISO_PORT)
#define bmMISO   (1<<MISO_BIT)
#define MOSI_OUT PORT(MOSI_PORT)
#define bmMOSI   (1<<MOSI_BIT)
#define SS_OUT   PORT(SS_PORT)
#define bmSS     (1<<SS_BIT)
#define SCK_OUT  PORT(SCK_PORT)
#define bmSCK    (1<<SCK_BIT)

static inline void CONCAT(OP_HDR, ShiftOut)(uint8 byte);
static inline uint8 CONCAT(OP_HDR, ShiftInOut)(uint8 byte);

// Utility macros for bit-banging
#define mosiSet(x) if ( x ) { MOSI_OUT |= bmMOSI; } else { MOSI_OUT &= ~bmMOSI; }
#define mosiBit(x) mosiSet(byte & x); SCK_OUT |= bmSCK; SCK_OUT &= ~bmSCK

// Send a byte over the parallel port
static inline void CONCAT(OP_HDR, ParSendByte)(uint8 byte) {
	PAR_IO = byte;
	SCK_OUT |= bmSCK; SCK_OUT &= ~bmSCK;
}

// The host is giving us data and is expecting a response. This is useful for simultaneously
// clocking data into and out of the TAP.
//
static void CONCAT(OP_HDR, IsSendingIsReceiving)(void) {
	uint16 bitsRead, bitsRemaining;
	uint8 bytesRead, bytesRemaining;
	uint8 buf[ENDPOINT_SIZE], *ptr;
	CONCAT(OP_HDR, SpiEnable)();
	while ( m_numBits ) {
		bitsRead = (m_numBits >= (ENDPOINT_SIZE<<3)) ? ENDPOINT_SIZE<<3 : m_numBits;
		bytesRead = bitsToBytes(bitsRead);
		usbSelectEndpoint(OUT_ENDPOINT_ADDR);
		Endpoint_Read_Stream_LE(buf, bytesRead, NULL);
		ptr = buf;
		if ( bitsRead == m_numBits ) {
			// This is the last chunk
			uint8 misoByte, mosiByte, leftOver, i;
			bitsRemaining = (bitsRead-1) & 0xFFF8;        // Now an integer number of bytes
			leftOver = (uint8)(bitsRead - bitsRemaining); // How many bits in last byte (1-8)
			bytesRemaining = (bitsRemaining>>3);
			while ( bytesRemaining-- ) {
				*ptr = CONCAT(OP_HDR, ShiftInOut)(*ptr);
				ptr++;
			}
			CONCAT(OP_HDR, SpiDisable)();
			mosiByte = *ptr;  // Now do the bits in the final byte
			misoByte = 0x00;
			i = 1;
			while ( i && leftOver ) {
				leftOver--;
				if ( (m_flagByte & bmISLAST) && !leftOver ) {
					SS_OUT |= bmSS; // Exit Shift-DR state on next clock
				}
				mosiSet(mosiByte & 0x01);
				mosiByte >>= 1;
				if ( MISO_IN & bmMISO ) {
					misoByte |= i;
				}
				SCK_OUT |= bmSCK;
				SCK_OUT &= ~bmSCK;
				i <<= 1;
			}
			*ptr = misoByte;
		} else {
			// This is not the last chunk
			bytesRemaining = (bitsRead>>3);
			while ( bytesRemaining-- ) {
				*ptr = CONCAT(OP_HDR, ShiftInOut)(*ptr);
				ptr++;
			}
		}
		usbSelectEndpoint(IN_ENDPOINT_ADDR);
		Endpoint_Write_Stream_LE(buf, bytesRead, NULL);
		m_numBits -= bitsRead;
		usbFlushPacket();
		usbAckPacket();
	}
	m_progOp = PROG_NOP;
}

// The host is giving us data, but does not need a response. This is useful for clocking
// data into the TAP, whilst discarding the data which is clocked out.
//
static void CONCAT(OP_HDR, IsSendingNotReceiving)(void) {
	uint32 bytesRemaining = (m_numBits >> 3);
	uint8 byte, bitsRemaining = (uint8)(m_numBits & 0x07);
	if ( !bitsRemaining ) {
		// Ensure at least one bit is handled manually
		bytesRemaining--;
		bitsRemaining = 8;
	}
	usbSelectEndpoint(OUT_ENDPOINT_ADDR);
	CONCAT(OP_HDR, SpiEnable)();
	while ( bytesRemaining-- ) {
		byte = usbRecvByte();
		CONCAT(OP_HDR, ShiftOut)(byte);
	}
	CONCAT(OP_HDR, SpiDisable)();

	// Now do the bits in the final byte - there'll be at least one bit, and <= 8
	byte = usbRecvByte();
	bitsRemaining--;  // handle final bit separately
	while ( bitsRemaining ) {
		bitsRemaining--;
		mosiSet(byte & 0x01);
		byte >>= 1;
		SCK_OUT |= bmSCK;
		SCK_OUT &= ~bmSCK;
	}

	// Now do the final bit
	mosiSet(byte & 0x01);
	byte >>= 1;
	if ( m_flagByte & bmISLAST ) {
		SS_OUT |= bmSS; // Exit Shift-DR state on next clock
	}
	SCK_OUT |= bmSCK;
	SCK_OUT &= ~bmSCK;
	usbAckPacket();
	m_progOp = PROG_NOP;
}

// The host is not giving us data, but is expecting a response. This is useful for clocking
// data out of the TAP, but clocking in only a stream of zeros or ones (depending on the
// state of the bmSENDONES flag).
//
static void CONCAT(OP_HDR, NotSendingIsReceiving)(void) {
	uint32 bytesRemaining = (m_numBits >> 3);
	uint8 byte, bitsRemaining = (uint8)(m_numBits & 0x07);
	const uint8 mosiByte = (m_flagByte & bmSENDONES) ? 0xFF : 0x00;
	uint8 mask = 0x01;
	if ( !bitsRemaining ) {
		// Ensure at least one bit is handled manually
		bytesRemaining--;
		bitsRemaining = 8;
	}
	usbSelectEndpoint(IN_ENDPOINT_ADDR);
	while ( !usbInPacketReady() );
	CONCAT(OP_HDR, SpiEnable)();
	while ( bytesRemaining-- ) {
		byte = CONCAT(OP_HDR, ShiftInOut)(mosiByte);
		usbSendByte(byte);
	}
	CONCAT(OP_HDR, SpiDisable)();

	// Now do the bits in the final byte - there'll be at least one bit, and <= 8
	byte = 0x00;
	mosiSet(mosiByte & 0x01);
	bitsRemaining--;  // handle final bit separately
	while ( bitsRemaining ) {
		bitsRemaining--;
		if ( MISO_IN & bmMISO ) {
			byte |= mask;
		}
		SCK_OUT |= bmSCK;
		SCK_OUT &= ~bmSCK;
		mask <<= 1;
	}

	// Now do the final bit
	if ( m_flagByte & bmISLAST ) {
		SS_OUT |= bmSS; // Exit Shift-DR state on next clock
	}
	if ( MISO_IN & bmMISO ) {
		byte |= mask;
	}
	SCK_OUT |= bmSCK;
	SCK_OUT &= ~bmSCK;
	usbSendByte(byte);
	usbFlushPacket();
	m_progOp = PROG_NOP;
}

// The host is not giving us data, and does not need a response. This might seem a bit pointless,
// but bits are still clocked in, and the bmSENDONES & bmISLAST flags are honoured, so it's
// actually an efficient way of transferring a lot of zeros or ones into the TAP.
//
static void CONCAT(OP_HDR, NotSendingNotReceiving)(void) {
	uint32 bitsRemaining, bytesRemaining;
	uint8 leftOver;
	const uint8 mosiByte = (m_flagByte & bmSENDONES) ? 0xFF : 0x00;
	bitsRemaining = (m_numBits-1) & 0xFFFFFFF8;    // Now an integer number of bytes
	leftOver = (uint8)(m_numBits - bitsRemaining); // How many bits in last byte (1-8)
	bytesRemaining = (bitsRemaining>>3);
	CONCAT(OP_HDR, SpiEnable)();
	while ( bytesRemaining-- ) {
		CONCAT(OP_HDR, ShiftOut)(mosiByte);
	}
	CONCAT(OP_HDR, SpiDisable)();
	mosiSet(mosiByte & 0x01);
	while ( leftOver ) {
		leftOver--;
		if ( (m_flagByte & bmISLAST) && !leftOver ) {
			SS_OUT |= bmSS; // Exit Shift-DR state on next clock
		}
		SCK_OUT |= bmSCK;
		SCK_OUT &= ~bmSCK;
	}
	m_numBits = 0;
	m_progOp = PROG_NOP;
}

// The host is sending us data to be clocked in parallel into the device.
//
static void CONCAT(OP_HDR, ParSend)(void) {
	uint8 byte;
	usbSelectEndpoint(OUT_ENDPOINT_ADDR);
	parEnable();
	while ( m_numBits-- ) {
		byte = usbRecvByte();
		CONCAT(OP_HDR, ParSendByte)(byte);
	}
	parDisable();
	usbAckPacket();
	m_progOp = PROG_NOP;
}

// The host is sending us data to be clocked serially into the device.
//
static void CONCAT(OP_HDR, SerSend)(void) {
	uint8 byte;
	usbSelectEndpoint(OUT_ENDPOINT_ADDR);
	CONCAT(OP_HDR, SpiEnable)();
	while ( m_numBits-- ) {
		byte = usbRecvByte();
		CONCAT(OP_HDR, ShiftOut)(byte);
	}
	CONCAT(OP_HDR, SpiDisable)();
	usbAckPacket();
	m_progOp = PROG_NOP;
}

// The host wants us to clock data out of the device and send back the result.
//
static void CONCAT(OP_HDR, SerRecv)(void) {
	uint8 byte;
	usbSelectEndpoint(IN_ENDPOINT_ADDR);
	while ( !usbInPacketReady() );
	CONCAT(OP_HDR, SpiEnable)();
	while ( m_numBits-- ) {
		byte = CONCAT(OP_HDR, ShiftInOut)(0x00);
		usbSendByte(byte);
	}
	CONCAT(OP_HDR, SpiDisable)();
	usbFlushPacket();
	m_progOp = PROG_NOP;
}

// Keep TMS and TDI as they are, and clock the JTAG state machine "numClocks" times.
//
static void CONCAT(OP_HDR, ProgClocks)(uint32 numClocks) {
	while ( numClocks-- ) {
		SCK_OUT |= bmSCK;
		SCK_OUT &= ~bmSCK;
	}
}

// Transition the JTAG state machine to another state: clock "transitionCount" bits from
// "bitPattern" into TMS, LSB-first.
//
static void CONCAT(OP_HDR, ProgClockFSM)(uint32 bitPattern, uint8 transitionCount) {
	while ( transitionCount-- ) {
		if ( bitPattern & 1 ) {
			SS_OUT |= bmSS;
		} else {
			SS_OUT &= ~bmSS;
		}
		SCK_OUT |= bmSCK;
		SCK_OUT &= ~bmSCK;
		bitPattern >>= 1;
	}
}

#undef OP_HDR
