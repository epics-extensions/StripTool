/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#ifndef _StripDataSource
#define _StripDataSource

#include "StripCurve.h"

/* ======= Data Types ======= */

typedef void *	StripDataSource;

typedef enum
{
  DATASTAT_PLOTABLE	= 1	/* the point is plotable */
} DataStatus;

/* ======= Attributes ======= */
typedef enum
{
  SDS_NUMSAMPLES = 1,	/* (size_t)	number of samples to keep 	rw */
  SDS_LAST_ATTRIBUTE
} SDSAttribute;



/* ======= Functions ======= */

/*
 * StripDataSource_init
 *
 *	Creates a new strip data structure, setting all values to defaults.
 */
StripDataSource 	StripDataSource_init	(void);


/*
 * StripDataSource_copy
 *
 *	Creates a new data buffer structure, copying the attributes of the
 *	parameter into it
 */
/*
StripDataSource 	StripDataSource_copy	(StripDataSource);
*/


/*
 * StripDataSource_delete
 *
 *	Destroys the specified data buffer.
 */
void 	StripDataSource_delete	(StripDataSource);


/*
 * StripDataSource_set/getattr
 *
 *	Sets or gets the specified attribute, returning true on success.
 */
int 	StripDataSource_setattr	(StripDataSource, ...);
int	StripDataSource_getattr	(StripDataSource, ...);


/*
 * StripDataSource_addcurve
 *
 *	Tells the DataSource to acquire data for the given curve whenever
 *	a sample is requested.
 */
int 	StripDataSource_addcurve	(StripDataSource, StripCurve);


/*
 * StripDataSource_removecurve
 *
 *	Removes the given curve from those the DataSource knows.
 */
int	StripDataSource_removecurve	(StripDataSource, StripCurve);


/*
 * StripDataSource_sample
 *
 *	Tells the buffer to sample the data for all curves it knows about.
 */
void	StripDataSource_sample	(StripDataSource);


/*
 * StripDataSource_init_range
 *
 *	Initializes DataSource for subsequent retrievals.  After this routine
 *	is called, and until it is called again, all requests for data or
 *	time stamps will only return data inside the range specified here.
 *	(The endpoints are included).  The number returned is the total
 *	number of samples contained on the range.
 */
size_t	StripDataSource_init_range	(StripDataSource,
					 struct timeval *,  /* begin */
					 struct timeval *); /* end */


/*
 * StripDataSource_get_times
 *
 *	Points the parameter argumnet to an array of time stamps.  The length
 *	Of the array is returned.  Subsequent calls will return consecutive
 *	chunks of time stamps until no more exist on the initialized range,
 *	at which point a value of zero will be returned.
 */
size_t	StripDataSource_get_times	(StripDataSource, struct timeval **);


/*
 * StripDataSource_get_data
 *
 *	Behavior is same as get_times(), above, except that the data points
 *	for the specified curve are contained in the returned array.
 */
size_t	StripDataSource_get_data	(StripDataSource,
					 StripCurve,
                                         double **,
                                         char **);


/*
 * StripDataSource_dump
 *
 *	Causes all data for the specified curves, on the given time range,
 *	to be dumped out to the specified file.  If the curves array is
 *	NULL, all curves are assumed.  If begin and/or end times are NULL,
 *	then all available data are dumped.
 */
int	StripDataSource_dump		(StripDataSource,
					 StripCurve[],
					 struct timeval *,	/* begin */
					 struct timeval *,	/* end */
					 FILE *);

#endif
