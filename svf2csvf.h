#ifndef SVF2CSVF_H
#define SVF2CSVF_H

#include <libbuffer.h>
#include "libfpgalink.h"

#ifdef __cplusplus
extern "C" {
#endif

	struct BitStore {
		uint32 numBits;
		struct Buffer tdi;
		struct Buffer tdo;
		struct Buffer mask;
		// TODO: smask?
	};
	struct ParseContext {
		struct BitStore dataHead;
		struct BitStore insnHead;
		struct BitStore dataBody;
		struct BitStore insnBody;
		struct BitStore dataTail;
		struct BitStore insnTail;
		uint32 curLength;
		struct Buffer curMaskBuf;
		uint32 curMaskBits;
		uint32 runTestOffset;
	};

	#define bitsToBytes(x) (((x)>>3) + ((x)&7 ? 1 : 0))
	#define CHOMP() while ( *p == ' ' || *p == '\t' || *p == '\r' ) { p++; }
	
	FLStatus headTail(
		struct Buffer *dataBuf, struct Buffer *hdrBuf, struct Buffer *tailBuf,
		uint32 dataBits, uint32 hdrBits, uint32 tailBits,
		const char **error
	) WARN_UNUSED_RESULT;
	
	FLStatus readBytes(
		struct Buffer *buffer, const char *hexDigits, const char **error
	) WARN_UNUSED_RESULT;
	
	FLStatus cxtInitialise(
		struct ParseContext *cxt, const char **error
	) WARN_UNUSED_RESULT;
	
	void cxtDestroy(
		struct ParseContext *cxt);
	
	FLStatus parseLine(
		struct ParseContext *cxt, const struct Buffer *lineBuf, struct Buffer *csvfBuf,
		const char **error
	) WARN_UNUSED_RESULT;


#ifdef __cplusplus
}
#endif

#endif
