#ifndef _getArchiveRecord_h
#define _getArchiveRecord_h

#define REQUEST_MODE_ONE_SHOT    1  /* please,don't use it, eat IOC mem */
#define REQUEST_MODE_CONTINUE    2
#define ERROR 1
#define OK 0
#include <tsDefs.h>

int getArchiveRecord(
      char            *name,            /* channal name */
      struct timeval  *from,            /* asking time from */
      struct timeval  *to,              /* asking time up to */
      long             requestMode,     /* short request or continues */
      struct timeval **returnedTime,    /* real time up to */
      double         **returnedData,    /* data */
      short          **returnedStatus,  /* status */  
      long            *returnedCount,   /* real count of data */
      short *needMoreData);
#endif  /* _getArchiveRecord_h */
