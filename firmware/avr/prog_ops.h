static void CONCAT(OP_HDR, IsSendingIsReceiving)(void) {
	// The host is giving us data and is expecting a response
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
			uint8 tdoByte, tdiByte, leftOver, i;
			bitsRemaining = (bitsRead-1) & 0xFFF8;        // Now an integer number of bytes
			leftOver = (uint8)(bitsRead - bitsRemaining); // How many bits in last byte (1-8)
			bytesRemaining = (bitsRemaining>>3);
			while ( bytesRemaining-- ) {
				*ptr = CONCAT(OP_HDR, ShiftInOut)(*ptr);
				ptr++;
			}
			CONCAT(OP_HDR, SpiDisable)();
			tdiByte = *ptr;  // Now do the bits in the final byte
			tdoByte = 0x00;
			i = 1;
			while ( i && leftOver ) {
				leftOver--;
				if ( (m_flagByte & bmISLAST) && !leftOver ) {
					TMS_OUT |= bmTMS; // Exit Shift-DR state on next clock
				}
				tdiSet(tdiByte & 0x01);
				tdiByte >>= 1;
				if ( TDO_IN & bmTDO ) {
					tdoByte |= i;
				}
				TCK_OUT |= bmTCK;
				TCK_OUT &= ~bmTCK;
				i <<= 1;
			}
			*ptr = tdoByte;
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

static void CONCAT(OP_HDR, IsSendingNotReceiving)(void) {
	// The host is giving us data, but does not need a response
	uint16 bitsRead, bitsRemaining;
	uint8 bytesRead, bytesRemaining;
	uint8 buf[ENDPOINT_SIZE], *ptr;
	usbSelectEndpoint(OUT_ENDPOINT_ADDR);
	CONCAT(OP_HDR, SpiEnable)();
	while ( m_numBits ) {
		bitsRead = (m_numBits >= (ENDPOINT_SIZE<<3)) ? ENDPOINT_SIZE<<3 : m_numBits;
		bytesRead = bitsToBytes(bitsRead);
		Endpoint_Read_Stream_LE(buf, bytesRead, NULL);
		ptr = buf;
		if ( bitsRead == m_numBits ) {
			// This is the last chunk
			uint8 tdiByte, leftOver, i;
			bitsRemaining = (bitsRead-1) & 0xFFF8;        // Now an integer number of bytes
			leftOver = (uint8)(bitsRead - bitsRemaining); // How many bits in last byte (1-8)
			bytesRemaining = (bitsRemaining>>3);
			while ( bytesRemaining-- ) {
				CONCAT(OP_HDR, ShiftOut)(*ptr++);
			}
			CONCAT(OP_HDR, SpiDisable)();
			tdiByte = *ptr;  // Now do the bits in the final byte
			i = 1;
			while ( i && leftOver ) {
				leftOver--;
				if ( (m_flagByte & bmISLAST) && !leftOver ) {
					TMS_OUT |= bmTMS; // Exit Shift-DR state on next clock
				}
				tdiSet(tdiByte & 0x01);
				tdiByte >>= 1;
				TCK_OUT |= bmTCK;
				TCK_OUT &= ~bmTCK;
				i <<= 1;
			}
		} else {
			// This is not the last chunk
			bytesRemaining = (bitsRead>>3);
			while ( bytesRemaining-- ) {
				CONCAT(OP_HDR, ShiftOut)(*ptr++);
			}
		}
		m_numBits -= bitsRead;
	}
	usbAckPacket();
	m_progOp = PROG_NOP;
}

static void CONCAT(OP_HDR, NotSendingIsReceiving)(void) {
	// The host is not giving us data, but is expecting a response
	uint16 bitsRead, bitsRemaining;
	uint8 bytesRead, bytesRemaining;
	uint8 buf[ENDPOINT_SIZE], *ptr;
	const uint8 tdiByte = (m_flagByte & bmSENDONES) ? 0xFF : 0x00;
	usbSelectEndpoint(IN_ENDPOINT_ADDR);
	while ( !usbInPacketReady() );
	CONCAT(OP_HDR, SpiEnable)();
	while ( m_numBits ) {
		bitsRead = (m_numBits >= (ENDPOINT_SIZE<<3)) ? ENDPOINT_SIZE<<3 : m_numBits;
		bytesRead = bitsToBytes(bitsRead);
		ptr = buf;
		if ( bitsRead == m_numBits ) {
			// This is the last chunk
			uint8 tdoByte, leftOver, i;
			bitsRemaining = (bitsRead-1) & 0xFFF8;        // Now an integer number of bytes
			leftOver = (uint8)(bitsRead - bitsRemaining); // How many bits in last byte (1-8)
			bytesRemaining = (bitsRemaining>>3);
			while ( bytesRemaining-- ) {
				*ptr++ = CONCAT(OP_HDR, ShiftInOut)(tdiByte);
			}
			CONCAT(OP_HDR, SpiDisable)();
			tdoByte = 0x00;  // Now do the bits in the final byte
			i = 1;
			tdiSet(tdiByte & 0x01);
			while ( i && leftOver ) {
				leftOver--;
				if ( (m_flagByte & bmISLAST) && !leftOver ) {
					TMS_OUT |= bmTMS; // Exit Shift-DR state on next clock
				}
				if ( TDO_IN & bmTDO ) {
					tdoByte |= i;
				}
				TCK_OUT |= bmTCK;
				TCK_OUT &= ~bmTCK;
				i <<= 1;
			}
			*ptr = tdoByte;
		} else {
			// This is not the last chunk
			bytesRemaining = (bitsRead>>3);
			while ( bytesRemaining-- ) {
				*ptr++ = CONCAT(OP_HDR, ShiftInOut)(tdiByte);
			}
		}
		Endpoint_Write_Stream_LE(buf, bytesRead, NULL);
		m_numBits -= bitsRead;
	}
	usbFlushPacket();
	m_progOp = PROG_NOP;
}

static void CONCAT(OP_HDR, NotSendingNotReceiving)(void) {
	// The host is not giving us data, and does not need a response
	uint32 bitsRemaining, bytesRemaining;
	uint8 leftOver;
	const uint8 tdiByte = (m_flagByte & bmSENDONES) ? 0xFF : 0x00;
	bitsRemaining = (m_numBits-1) & 0xFFFFFFF8;    // Now an integer number of bytes
	leftOver = (uint8)(m_numBits - bitsRemaining); // How many bits in last byte (1-8)
	bytesRemaining = (bitsRemaining>>3);
	CONCAT(OP_HDR, SpiEnable)();
	while ( bytesRemaining-- ) {
		CONCAT(OP_HDR, ShiftOut)(tdiByte);
	}
	CONCAT(OP_HDR, SpiDisable)();
	tdiSet(tdiByte & 0x01);
	while ( leftOver ) {
		leftOver--;
		if ( (m_flagByte & bmISLAST) && !leftOver ) {
			TMS_OUT |= bmTMS; // Exit Shift-DR state on next clock
		}
		TCK_OUT |= bmTCK;
		TCK_OUT &= ~bmTCK;
	}
	m_numBits = 0;
	m_progOp = PROG_NOP;
}

static void CONCAT(OP_HDR, ParSend)(void) {
	uint8 byte;
	usbSelectEndpoint(OUT_ENDPOINT_ADDR);
	parEnable();
	while ( m_numBits-- ) {
		byte = usbRecvByte();
		parSendByte(byte);
	}
	parDisable();
	usbAckPacket();
	m_progOp = PROG_NOP;
}

static void CONCAT(OP_HDR, SerSend)(void) {
	uint8 byte;
	usbSelectEndpoint(OUT_ENDPOINT_ADDR);
	CONCAT(OP_HDR, SpiEnable)();
	while ( m_numBits-- ) {
		byte = usbRecvByte();
		bbShiftOut(byte);
	}
	CONCAT(OP_HDR, SpiDisable)();
	usbAckPacket();
	m_progOp = PROG_NOP;
}

static void CONCAT(OP_HDR, SerRecv)(void) {
	uint8 byte;
	usbSelectEndpoint(IN_ENDPOINT_ADDR);
	while ( !usbInPacketReady() );
	CONCAT(OP_HDR, SpiEnable)();
	while ( m_numBits-- ) {
		byte = bbShiftInOut(0x00);
		usbSendByte(byte);
	}
	CONCAT(OP_HDR, SpiDisable)();
	usbFlushPacket();
	m_progOp = PROG_NOP;
}

static void CONCAT(OP_HDR, ProgClocks)(uint32 numClocks) {
	while ( numClocks-- ) {
		TCK_OUT |= bmTCK;
		TCK_OUT &= ~bmTCK;
	}
}
