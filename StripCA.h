/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#ifndef _StripCA
#define _StripCA

#include "Strip.h"
#include "StripCurve.h"
#include "StripDefines.h"
#include "StripMisc.h"

typedef void *	StripCA;

/*
 * StripCA_initialize
 *
 *	Initializes Channel Access and allocates internal resources.
 */
StripCA		StripCA_initialize	(Strip);


/*
 * StripCA_terminate
 *
 *	Exits Channel Access, freeing allocated resources.
 */
void		StripCA_terminate	(StripCA);


/*
 * StripCA_request_connect
 *
 *	Requests that the signal specified in the StripCurve be connected
 *	to a channel access server.
 */
int		StripCA_request_connect		(StripCurve, void *);


/*
 * StripCA_request_disconnect
 *
 *	Requests that the signal specified in the StripCurve be disconnected
 *	from its channel access server.
 */
int		StripCA_request_disconnect	(StripCurve, void *);

#endif
