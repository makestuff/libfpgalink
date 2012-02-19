/* 
 * Copyright (C) 2012 Chris McClelland
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <makestuff.h>
#include <libfpgalink.h>
#include <libbuffer.h>
#include <liberror.h>
#include <libdump.h>
#include <argtable2.h>
#include <readline/readline.h>
#include <readline/history.h>

static const char *ptr;

static bool isHexDigit(char ch) {
	return
		(ch >= '0' && ch <= '9') ||
		(ch >= 'a' && ch <= 'f') ||
		(ch >= 'A' && ch <= 'F');
}

static bool getHexNibble(char hexDigit, uint8 *nibble) {
	if ( hexDigit >= '0' && hexDigit <= '9' ) {
		*nibble = hexDigit - '0';
		return false;
	} else if ( hexDigit >= 'a' && hexDigit <= 'f' ) {
		*nibble = hexDigit - 'a' + 10;
		return false;
	} else if ( hexDigit >= 'A' && hexDigit <= 'F' ) {
		*nibble = hexDigit - 'A' + 10;
		return false;
	} else {
		return true;
	}
}

static int getHexByte(uint8 *byte) {
	uint8 upperNibble;
	uint8 lowerNibble;
	if ( !getHexNibble(ptr[0], &upperNibble) && !getHexNibble(ptr[1], &lowerNibble) ) {
		*byte = (upperNibble << 4) | lowerNibble;
		byte += 2;
		return 0;
	} else {
		return 1;
	}
}

static const char *const errMessages[] = {
	NULL,
	NULL,
	"Unparseable hex number",
	"Register out of range",
	"Illegal character",
	"Unterminated string",
	"No memory",
	"Empty string",
	"Odd number of digits",
	"Cannot load file",
	"Cannot save file",
	"Bad arguments"
};

typedef enum {
	FLP_SUCCESS,
	FLP_LIBERR,
	FLP_BAD_HEX,
	FLP_REG_RANGE,
	FLP_ILL_CHAR,
	FLP_UNTERM_STRING,
	FLP_NO_MEMORY,
	FLP_EMPTY_STRING,
	FLP_ODD_DIGITS,
	FLP_CANNOT_LOAD,
	FLP_CANNOT_SAVE,
	FLP_ARGS
} ReturnCode;

#define CHECK(condition, retCode) \
	if ( condition ) { \
		FAIL(retCode); \
	}

static int parseLine(struct FLContext *handle, const char *line, const char **error) {
	ReturnCode returnCode = FLP_SUCCESS;
	FLStatus fStatus;
	struct Buffer dataFromFPGA = {0,};
	BufferStatus bStatus;
	uint8 *data = NULL;
	char *fileName = NULL;
	FILE *file = NULL;
	bStatus = bufInitialise(&dataFromFPGA, 1024, 0x00, error);
	CHECK(bStatus, FLP_LIBERR);
	ptr = line;
	do {
		while ( *ptr == ';' ) {
			ptr++;
		}
		switch ( *ptr ) {
		case 'r':{
			uint32 reg;
			uint32 length = 1;
			char *end;
			ptr++;
			
			// Get the register to be read:
			errno = 0;
			reg = strtoul(ptr, &end, 16);
			CHECK(errno, FLP_BAD_HEX);

			// Ensure that it's 0-127
			CHECK(reg > 127, FLP_REG_RANGE);
			ptr = end;

			// Only three valid chars at this point:
			CHECK(*ptr != '\0' && *ptr != ';' && *ptr != ' ', FLP_ILL_CHAR);

			if ( *ptr == ' ' ) {
				ptr++;

				// Get the read count:
				errno = 0;
				length = strtoul(ptr, &end, 16);
				CHECK(errno, FLP_BAD_HEX);
				ptr = end;
				
				// Only three valid chars at this point:
				CHECK(*ptr != '\0' && *ptr != ';' && *ptr != ' ', FLP_ILL_CHAR);
				if ( *ptr == ' ' ) {
					const char *p;
					ptr++;
					CHECK(*ptr != '"', FLP_ILL_CHAR);
					
					// Get the file to write bytes to:
					ptr++;
					p = ptr;
					while ( *p != '"' && *p != '\0' ) {
						p++;
					}
					CHECK(*p == '\0', FLP_UNTERM_STRING);
					fileName = malloc(p - ptr + 1);
					CHECK(!fileName, FLP_NO_MEMORY);
					CHECK(p - ptr == 0, FLP_EMPTY_STRING);
					strncpy(fileName, ptr, p - ptr);
					fileName[p - ptr] = '\0';
					ptr = p + 1;
				}
			}
			if ( fileName ) {
				uint32 bytesWritten;
				data = malloc(length);
				fStatus = flReadRegister(handle, 32000, (uint8)reg, length, data, error);
				CHECK(fStatus, FLP_LIBERR);
				file = fopen(fileName, "wb");
				CHECK(!file, FLP_CANNOT_SAVE);
				free(fileName);
				fileName = NULL;
				bytesWritten = fwrite(data, 1, length, file);
				CHECK(bytesWritten != length, FLP_CANNOT_SAVE);
				free(data);
				data = NULL;
				fclose(file);
				file = NULL;
			} else {
				int oldLength = dataFromFPGA.length;
				bStatus = bufAppendZeros(&dataFromFPGA, length, NULL, error);
				CHECK(bStatus, FLP_LIBERR);
				fStatus = flReadRegister(handle, 32000, (uint8)reg, length, dataFromFPGA.data + oldLength, error);
				CHECK(fStatus, FLP_LIBERR);
			}
			break;
		}
		case 'w':{
			unsigned long int reg;
			uint32 length = 1, i;
			char *end;
			ptr++;
			
			// Get the register to be written:
			errno = 0;
			reg = strtoul(ptr, &end, 16);
			CHECK(errno, FLP_BAD_HEX);

			// Ensure that it's 0-127
			CHECK(reg > 127, FLP_REG_RANGE);
			ptr = end;

			// Only three valid chars at this point:
			CHECK(*ptr != '\0' && *ptr != ';' && *ptr != ' ', FLP_ILL_CHAR);

			if ( *ptr == ' ' ) {
				const char *p;
				ptr++;

				if ( *ptr == '"' ) {
					// Get the file to read bytes from:
					ptr++;
					p = ptr;
					while ( *p != '"' && *p != '\0' ) {
						p++;
					}
					CHECK(*p == '\0', FLP_UNTERM_STRING);
					fileName = malloc(p - ptr + 1);
					CHECK(!fileName, FLP_NO_MEMORY);
					CHECK(p - ptr == 0, FLP_EMPTY_STRING);
					strncpy(fileName, ptr, p - ptr);
					fileName[p - ptr] = '\0';
					
					// Load data from the file:
					data = flLoadFile(fileName, &length);
					free(fileName);
					fileName = NULL;
					CHECK(!data, FLP_CANNOT_LOAD);
					ptr = p + 1;
				} else if ( isHexDigit(*ptr) ) {
					// Read a sequence of hex bytes to write
					uint8 *dataPtr;
					p = ptr + 1;
					while ( isHexDigit(*p) ) {
						p++;
					}
					CHECK((p - ptr) & 1, FLP_ODD_DIGITS);
					length = (p - ptr) / 2;
					data = malloc(length);
					dataPtr = data;
					for ( i = 0; i < length; i++ ) {
						getHexByte(dataPtr++);
						ptr += 2;
					}
				} else {
					FAIL(FLP_ILL_CHAR);
				}
			}
			fStatus = flWriteRegister(handle, 32000, (uint8)reg, length, data, error);
			CHECK(fStatus, FLP_LIBERR);
			free(data);
			data = NULL;
			break;
		}
		default:
			FAIL(FLP_ILL_CHAR);
		}
	} while ( *ptr == ';' );
	CHECK(*ptr != '\0', FLP_ILL_CHAR);

	dump(0x00000000, dataFromFPGA.data, dataFromFPGA.length);

cleanup:
	bufDestroy(&dataFromFPGA);
	if ( file ) {
		fclose(file);
	}
	free(fileName);
	free(data);
	if ( returnCode > FLP_LIBERR ) {
		const int column = ptr-line;
		int i;
		fprintf(stderr, "%s at column %d\n  %s\n  ", errMessages[returnCode], column, line);
		for ( i = 0; i < column; i++ ) {
			fprintf(stderr, " ");
		}
		fprintf(stderr, "^\n");
	}
	return returnCode;
}

int main(int argc, char *argv[]) {

	ReturnCode returnCode = FLP_SUCCESS, pStatus;
	struct arg_str *ivpOpt = arg_str0("i", "ivp", "<VID:PID>", "         vendor ID and product ID (e.g 04B4:8613)");
	struct arg_str *vpOpt = arg_str1("v", "vp", "<VID:PID>", "          vendor ID and product ID (e.g 04B4:8613)");
	struct arg_file *fileOpt = arg_file0("x", "xsvf", "<fileName>", "       XSVF or CSVF file to load");
	struct arg_lit *powOpt = arg_lit0("p", "power", "                 FPGA is powered from USB (Nexys2 only!)");
	struct arg_lit *scanOpt = arg_lit0("s", "scan", "                  scan the JTAG chain");
	struct arg_str *actOpt = arg_str0("a", "action", "<actionString>", " a series of CommFPGA actions");
	struct arg_lit *cliOpt  = arg_lit0("c", "cli", "                  start up an interactive CommFPGA session");
	struct arg_lit *helpOpt  = arg_lit0("h", "help", "                  print this help and exit\n");
	struct arg_end *endOpt   = arg_end(20);
	void *argTable[] = {ivpOpt, vpOpt, fileOpt, powOpt, scanOpt, actOpt, cliOpt, helpOpt, endOpt};
	const char *progName = "flcli";
	int numErrors;
	struct FLContext *handle = NULL;
	FLStatus fStatus;
	const char *error = NULL;
	const char *ivp = NULL;
	const char *vp = NULL;
	bool isNeroCapable, isCommCapable;
	uint32 numDevices, scanChain[16], i;
	const char *line = NULL;

	if ( arg_nullcheck(argTable) != 0 ) {
		errRender(&error, "%s: insufficient memory\n", progName);
		FAIL(1);
	}

	numErrors = arg_parse(argc, argv, argTable);

	if ( helpOpt->count > 0 ) {
		printf("FPGALink Command-Line Interface Copyright (C) 2012 Chris McClelland\n\nUsage: %s", progName);
		arg_print_syntax(stdout, argTable, "\n");
		printf("\nInteract with an FPGALink device.\n\n");
		arg_print_glossary(stdout, argTable,"  %-10s %s\n");
		FAIL(FLP_SUCCESS);
	}

	if ( numErrors > 0 ) {
		arg_print_errors(stdout, endOpt, progName);
		errRender(&error, "Try '%s --help' for more information.\n", progName);
		FAIL(FLP_ARGS);
	}

	flInitialise();

	vp = vpOpt->sval[0];

	printf("Attempting to open connection to FPGALink device %s...\n", vp);
	fStatus = flOpen(vp, &handle, NULL);
	if ( fStatus ) {
		if ( ivpOpt->count ) {
			int count = 60;
			bool flag;
			ivp = ivpOpt->sval[0];
			printf("Loading firmware into %s...\n", ivp);
			fStatus = flLoadStandardFirmware(ivp, vp, &error);
			CHECK(fStatus, FLP_LIBERR);
			
			printf("Awaiting renumeration");
			flSleep(1000);
			do {
				printf(".");
				fflush(stdout);
				flSleep(100);
				fStatus = flIsDeviceAvailable(vp, &flag, &error);
				CHECK(fStatus, FLP_LIBERR);
				count--;
			} while ( !flag && count );
			printf("\n");
			if ( !flag ) {
				errRender(&error, "FPGALink device did not renumerate properly as %s", vp);
				FAIL(FLP_LIBERR);
			}

			printf("Attempting to open connection to FPGLink device %s again...\n", vp);
			fStatus = flOpen(vp, &handle, &error);
			CHECK(fStatus, FLP_LIBERR);
		} else {
			errRender(&error, "Could not open FPGALink device at %s and no initial VID:PID was supplied", vp);
			FAIL(FLP_ARGS);
		}
	}

	if ( powOpt->count ) {
		printf("Connecting USB power to FPGA...\n");
		fStatus = flPortAccess(handle, 0x0080, 0x0080, NULL, &error);
		CHECK(fStatus, FLP_LIBERR);
	}

	isNeroCapable = flIsNeroCapable(handle);
	isCommCapable = flIsCommCapable(handle);

	if ( scanOpt->count ) {
		if ( isNeroCapable ) {
			fStatus = flScanChain(handle, &numDevices, scanChain, 16, &error);
			CHECK(fStatus, FLP_LIBERR);
			if ( numDevices ) {
				printf("The FPGALink device at %s scanned its JTAG chain, yielding:\n", vp);
				for ( i = 0; i < numDevices; i++ ) {
					printf("  0x%08X\n", scanChain[i]);
				}
			} else {
				printf("The FPGALink device at %s scanned its JTAG chain but did not find any attached devices\n", vp);
			}
		} else {
			errRender(&error, "JTAG chain scan requested but FPGALink device at %s does not support NeroJTAG", vp);
			FAIL(FLP_ARGS);
		}
	}

	if ( fileOpt->count ) {
		printf("Playing \"%s\" into the JTAG chain on FPGALink device %s...\n", fileOpt->filename[0], vp);
		if ( isNeroCapable ) {
			fStatus = flPlayXSVF(handle, fileOpt->filename[0], &error);
			CHECK(fStatus, FLP_LIBERR);
		} else {
			errRender(&error, "XSVF/CSVF play requested but device at %s does not support NeroJTAG", vp);
			FAIL(FLP_ARGS);
		}
	}

	if ( actOpt->count ) {
		printf("Executing CommFPGA actions on FPGALink device %s...\n", vp);
		if ( isCommCapable ) {
			bool isRunning;
			fStatus = flIsFPGARunning(handle, &isRunning, &error);
			CHECK(fStatus, FLP_LIBERR);
			if ( isRunning ) {
				pStatus = parseLine(handle, actOpt->sval[0], &error);
				CHECK(pStatus, pStatus);
			} else {
				errRender(&error, "The FPGALink device at %s is not ready to talk - did you forget --xsvf?", vp);
				FAIL(FLP_ARGS);
			}
		} else {
			errRender(&error, "Action requested but device at %s does not support CommFPGA", vp);
			FAIL(FLP_ARGS);
		}
	}

	if ( cliOpt->count ) {
		printf("\nEntering CommFPGA command-line mode:\n");
		if ( isCommCapable ) {
			bool isRunning;
			fStatus = flIsFPGARunning(handle, &isRunning, &error);
			CHECK(fStatus, FLP_LIBERR);
			if ( isRunning ) {
				do {
					do {
						line = readline("> ");
					} while ( line && !line[0] );
					if ( line && line[0] && line[0] != 'q' ) {
						add_history(line);
						pStatus = parseLine(handle, line, &error);
						CHECK(pStatus, pStatus);
						free((void*)line);
					}
				} while ( line && line[0] != 'q' );
			} else {
				errRender(&error, "The FPGALink device at %s is not ready to talk - did you forget --xsvf?", vp);
				FAIL(FLP_ARGS);
			}
		} else {
			errRender(&error, "CLI requested but device at %s does not support CommFPGA", vp);
			FAIL(FLP_ARGS);
		}
	}

cleanup:
	free((void*)line);
	flClose(handle);
	if ( error ) {
		fprintf(stderr, "%s\n", error);
		flFreeError(error);
	}
	return returnCode;
}
