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
 *	Contains instance data for the archive service.
 */
typedef struct _StripHistoryInfo
{
  Strip		strip;
}
StripHistoryInfo;


/* StripHistory_init
 */
StripHistory	StripHistory_init	(Strip strip)
{
  StripHistoryInfo	*shi = 0;

  if (shi = (StripHistoryInfo *)malloc (sizeof(StripHistoryInfo)))
  {
    shi->strip = strip;
  }

  return (StripHistory)shi;
}


/* StripHistory_delete
 */
void	StripHistory_delete	(StripHistory the_shi)
{
  StripHistoryInfo	*shi = (StripHistoryInfo *)the_shi;

  free (shi);
}


/* StripHistory_fetch
 */
int	StripHistory_fetch	(StripHistory		the_shi,
                                 char 			*name,
                                 struct timeval 	*t0,
                                 struct timeval 	*t1,
                                 StripHistoryResult 	*result,
                                 StripHistoryCallback	cb_func,
                                 void			*cb_data)
{
  fprintf (stderr, "StripHistory_fetch: not yet implemented!\n");
  return 0;
}
 
