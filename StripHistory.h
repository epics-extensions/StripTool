/*-----------------------------------------------------------------------------
 * Copyright (c) 1998 Southeastern Universities Research Association,
 *               Thomas Jefferson National Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#ifndef _StripHistory
#define _StripHistory

#include "Strip.h"

/* ====== Data Types ====== */

/* StripHistory
 *
 *	Opaque data type used as handle to private data structure.
 *	Should contain all instance data required to perform archive
 *	service requests.
 */
typedef void *	StripHistory;

/* StripHistoryResult
 *
 *	Structure used for returning results of history request.
 */
typedef struct _StripHistoryResult
{
  struct timeval	req_t0, req_t1;	/* the requested (begin, end) times */
  struct timeval	*time;		/* the corresponding time stamps */
  double		*data;		/* the corresponding data */
  double		null;		/* value indicating data hole */
  int			n;		/* (n=0) -> no data; (n<0) -> status */
} StripHistoryResult;

typedef void	(*StripHistoryCallback)	(StripHistoryResult *, void*);




/* StripHistory_init
 *
 *	Performs whatever initializations are required by history
 *	service, and returns handle of history module.
 */
StripHistory	StripHistory_init	(Strip);


/* StripHistory_delete
 *
 *	Handles shut down tasks for history service, and frees resources.
 */
void	StripHistory_delete	(StripHistory);


/* StripHistory_fetch
 *
 *	Calls down to archive service, requesting data for the given
 *	curve name on the given time range.  This request may be
 *	asynchronous, therefore a callback function is supplied,
 *	which will be invoked once the request has completed.
 *
 *	If the supplied result structure is not empty (i.e., n > 0), then
 *	its contents will be replaced.  If the requested time range intersects
 *	with some data already in the buffer, then this function may attempt
 *	to exploit this in order to reduce the overhead of fetching redundant
 *	data.  In any event, once the callback is issued, the data in
 *	the result structure will be for times within the requested range
 *	only.
 */
int	StripHistory_fetch	(StripHistory,
                                 char *,		/* name */
                                 struct timeval	*,	/* begin */
                                 struct timeval *,	/* end */
                                 StripHistoryResult *,	/* result */
                                 StripHistoryCallback,	/* callback */
                                 void *);		/* call data */


#endif
