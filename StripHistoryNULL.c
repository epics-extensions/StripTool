/*-----------------------------------------------------------------------------
 * Copyright (c) 1998 Southeastern Universities Research Association,
 *               Thomas Jefferson National Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#include "StripHistory.h"

/* StripHistoryInfo
 *
 *      Contains instance data for the archive service.
 */
typedef struct _StripHistoryInfo
{
  Strip         strip;
}
StripHistoryInfo;

static StripHistoryInfo shi;


/* StripHistory_init
 */
StripHistory    StripHistory_init       (Strip BOGUS(1))
{
  return (StripHistory)&shi;
}


/* StripHistory_delete
 */
void    StripHistory_delete             (StripHistory BOGUS(1))
{
}


/* StripHistory_fetch
 */
FetchStatus     StripHistory_fetch      (StripHistory           BOGUS(1),
                                         char                   *BOGUS(2),
                                         struct timeval         *t0,
                                         struct timeval         *t1,
                                         StripHistoryResult     *result,
                                         StripHistoryCallback   BOGUS(3),
                                         void                   *BOGUS(4))
{
  result->t0 = *t0;
  result->t1 = *t1;
  result->n_points = 0;
  result->fetch_stat = FETCH_NODATA;

  return result->fetch_stat;
}
 

/* StripHistory_cancel
 */
void    StripHistory_cancel     (StripHistory           BOGUS(1),
                                 StripHistoryResult     *BOGUS(2))
{
}
