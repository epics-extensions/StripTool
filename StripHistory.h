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
 *      Opaque data type used as handle to private data structure.
 *      Should contain all instance data required to perform archive
 *      service requests.
 */
typedef void *  StripHistory;


typedef enum _FetchStatus
{
  FETCH_IDLE = 0,       /* uninitialized */
  FETCH_DONE,           /* most recent fetch has completed successfully */
  FETCH_PENDING,        /* most recent fetch is still processing */
  FETCH_NODATA          /* most recent fetch yielded no data */
} FetchStatus;


/* StripHistoryResult
 *
 *      Structure used for returning results of history request.
 */
typedef struct _StripHistoryResult
{
  struct timeval        t0;             /* the requested begin time */
  struct timeval        t1;             /* requested end time */
  struct timeval        *times;         /* time stamps */
  double                *data;          /* real or reduced data values */
  short                 *status;        /* error status */
  int                   n_points;
  FetchStatus           fetch_stat;
} StripHistoryResult;

typedef void    (*StripHistoryCallback) (StripHistoryResult *, void *);




/* StripHistory_init
 *
 *      Performs whatever initializations are required by history
 *      service, and returns handle of history module.
 */
StripHistory    StripHistory_init       (Strip);


/* StripHistory_delete
 *
 *      Handles shut down tasks for history service, and frees resources.
 */
void            StripHistory_delete     (StripHistory);


/* StripHistory_fetch
 *
 *      Calls down to archive service, requesting data for the given curve
 *      name with the specified time parameters.  If the data is available
 *      immediateley, FETCH_DONE will be returned, otherwise FETCH_PENDING.
 *      If the latter, then the supplied callback will be invoked when the
 *      data is available.  If neither of these values is returned then
 *      an error has occurred and no callback will be invoked.
 *
 *      If the supplied result structure is not empty (i.e., n > 0), then
 *      its contents may be replaced.  If the requested time range intersects
 *      with some data already in the buffer, then this function may attempt
 *      to exploit this in order to reduce the overhead of fetching redundant
 *      data.  In any event, once the callback is issued, the data in
 *      the result structure will contain the requested range.
 *
 *      Note that result->t0 and result->t1 must be set to begin and end,
 *      respectively, though this need not guarantee that times[0] == t0
 *      or times[n_point-1] == t1.  The only guarantees (on successful
 *      completion) is:
 *
 *      (1) t0 <= t1
 *      (2) times[i] <= times[i+1]      : 0 <= i < n_points
 *      (3) t0 <= times[n_points-1]
 *      (4) t1 >= times[0]
 */
FetchStatus     StripHistory_fetch      (StripHistory,
                                         char *,                /* name */
                                         struct timeval *,      /* begin */
                                         struct timeval *,      /* end */
                                         StripHistoryResult *,  /* result */
                                         StripHistoryCallback,  /* callback */
                                         void *);               /* call data */

/* StripHistory_cancel
 *
 *      Given a StripHistoryResult structure whose status is
 *      FETCH_PENDING, cancels the pending transaction, otherwise
 *      has no effect.
 */
void            StripHistory_cancel     (StripHistory, StripHistoryResult *);
#endif
