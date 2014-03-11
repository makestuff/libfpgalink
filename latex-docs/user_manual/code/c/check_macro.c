#define CHECK(x)                       \
   if ( status != FL_SUCCESS ) {       \
      returnCode = x;                  \
      fprintf(stderr, "%s\n", error);  \
      flFreeError(error);              \
      goto cleanup;                    \
   }
   :
status = flWriteChannel(handle, 1000, 0x01, 1, &byte, &error);
CHECK(21);
