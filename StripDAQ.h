/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#ifndef _StripDAQ
#define _StripDAQ

#include "Strip.h"
#include "StripCurve.h"
#include "StripDefines.h"
#include "StripMisc.h"

typedef void *  StripDAQ;

/*
 * StripDAQ_initialize
 *
 *      Initializes DAQ subsystem and allocates internal resources.
 */
StripDAQ        StripDAQ_initialize     (Strip);


/*
 * StripDAQ_terminate
 *
 *      Exits Channel Access, freeing allocated resources.
 */
void            StripDAQ_terminate      (StripDAQ);


/*
 * StripDAQ_request_connect
 *
 *      Requests that the signal specified in the StripCurve be connected
 *      to the DAQ subsystem.
 */
int             StripDAQ_request_connect        (StripCurve, void *);


/*
 * StripDAQ_request_disconnect
 *
 *      Requests that the signal specified in the StripCurve be disconnected
 *      from the DAQ subsystem.
 */
int             StripDAQ_request_disconnect     (StripCurve, void *);

#endif
