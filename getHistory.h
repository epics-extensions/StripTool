#ifndef _getHistory_h
#define _getHistory_h

/* StripHistoryInfo
 *
 *      Contains instance data for the archive service.
 */
typedef struct _StripHistoryInfo
{
  Strip         strip;
  char          *archiverInfo;
}
StripHistoryInfo;


u_long getHistory(StripHistory     the_shi,
                  char                   *name,
		  struct timeval         *begin,  
		  struct timeval         *end,
		  struct timeval        **times,
		  short                 **status,
		  double                **data,
		  u_long                 *count);
#endif  /* _getHistory_h */
