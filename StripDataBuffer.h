/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#ifndef _StripDataBuffer
#define _StripDataBuffer

#include "StripCurve.h"

/* ======= Data Types ======= */

typedef void *	StripDataBuffer;

typedef enum
{
  DATASTAT_PLOTABLE	= 1,	/* the point is plotable */
} DataStatus;

/* ======= Attributes ======= */
typedef enum
{
  SDB_NUMSAMPLES = 1,	/* (size_t)	number of samples to keep 	rw */
  SDB_LAST_ATTRIBUTE,
} SDBAttribute;



/* ======= Functions ======= */

/*
 * StripDataBuffer_init
 *
 *	Creates a new strip data structure, setting all values to defaults.
 */
StripDataBuffer 	StripDataBuffer_init	(void);


/*
 * StripDataBuffer_copy
 *
 *	Creates a new data buffer structure, copying the attributes of the
 *	parameter into it
 */
/*
StripDataBuffer 	StripDataBuffer_copy	(StripDataBuffer);
*/


/*
 * StripDataBuffer_delete
 *
 *	Destroys the specified data buffer.
 */
void 	StripDataBuffer_delete	(StripDataBuffer);


/*
 * StripDataBuffer_set/getattr
 *
 *	Sets or gets the specified attribute, returning true on success.
 */
int 	StripDataBuffer_setattr	(StripDataBuffer, ...);
int	StripDataBuffer_getattr	(StripDataBuffer, ...);


/*
 * StripDataBuffer_addcurve
 *
 *	Tells the DataBuffer to acquire data for the given curve whenever
 *	a sample is requested.
 */
int 	StripDataBuffer_addcurve	(StripDataBuffer, StripCurve);


/*
 * StripDataBuffer_removecurve
 *
 *	Removes the given curve from those the DataBuffer knows.
 */
int	StripDataBuffer_removecurve	(StripDataBuffer, StripCurve);


/*
 * StripDataBuffer_sample
 *
 *	Tells the buffer to sample the data for all curves it knows about.
 */
void	StripDataBuffer_sample	(StripDataBuffer);


/*
 * StripDataBuffer_init_range
 *
 *	Initializes DataBuffer for subsequent retrievals.  After this routine
 *	is called, and until it is called again, all requests for data or
 *	time stamps will only return data inside the range specified here.
 *	(The endpoints are included).  The number returned is the total
 *	number of samples contained on the range.
 */
size_t	StripDataBuffer_init_range	(StripDataBuffer,
					 struct timeval *,  /* begin */
					 struct timeval *); /* end */


/*
 * StripDataBuffer_get_times
 *
 *	Points the parameter argumnet to an array of time stamps.  The length
 *	Of the array is returned.  Subsequent calls will return consecutive
 *	chunks of time stamps until no more exist on the initialized range,
 *	at which point a value of zero will be returned.
 */
size_t	StripDataBuffer_get_times	(StripDataBuffer, struct timeval **);


/*
 * StripDataBuffer_get_data
 *
 *	Behavior is same as get_times(), above, except that the data points
 *	for the specified curve are contained in the returned array.
 */
size_t	StripDataBuffer_get_data	(StripDataBuffer,
					 StripCurve,
                                         double **,
                                         char **);


/*
 * StripDataBuffer_dump
 *
 *	Causes all data for the specified curves, on the given time range,
 *	to be dumped out to the specified file.  If the curves array is
 *	NULL, all curves are assumed.  If begin and/or end times are NULL,
 *	then all available data are dumped.
 */
int	StripDataBuffer_dump		(StripDataBuffer,
					 StripCurve[],
					 struct timeval *,	/* begin */
					 struct timeval *,	/* end */
					 FILE *);

#endif
