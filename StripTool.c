/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */
#include "Strip.h"
#include "StripMisc.h"


#undef TEST_BAD_CONNECTION


#ifdef USE_CDEV
#  include "StripCDEV.h"
#  define StripDS_initialize(S)	           StripCDEV_initialize(S)
#  define StripDS_request_connect(C,D)     StripCDEV_request_connect(C,D)
#  define StripDS_request_disconnect(C,D)  StripCDEV_request_disconnect(C,D)
#  define StripDS_terminate(D)	           StripCDEV_terminate(D)
   typedef StripCDEV	StripDS;
#endif

#ifdef USE_CA
#  include "StripCA.h"
#  define StripDS_initialize(S)	           StripCA_initialize(S)
#  define StripDS_request_connect(C,D)     StripCA_request_connect(C,D)
#  define StripDS_request_disconnect(C,D)  StripCA_request_disconnect(C,D)
#  define StripDS_terminate(D)	           StripCA_terminate(D)
   typedef StripCA	StripDS;
#endif

#define CPU_CURVE_NAME		"CPU_Usage"
#define CPU_CURVE_EGU		"%"
#define CPU_CURVE_PRECISION	2
#define CPU_CURVE_MIN		0.00
#define CPU_CURVE_MAX		100.00

Strip	strip;
StripDS	strip_ds;		/* the Strip data source */

static int	request_connect		(StripCurve, void *);
static int	request_disconnect	(StripCurve, void *);

static double	get_cpu_usage		(void *);

#ifdef TEST_BAD_CONNECTION
static void	disconnect_curve	(StripCurve);
static void	reconnect_curve		(StripCurve);
#endif

int main (int argc, char *argv[])
{
  int		status;
  FILE		*f;

    /* create and initialize the Strip structure */
  if (!(strip = Strip_init ("StripTool", &argc, argv, tmpfile())))
    {
      fprintf (stderr, "%s: unable to initialize Strip\n", argv[0]);
      exit (1);
    }

  /* fire up the data source  */
  if (!(strip_ds = StripDS_initialize (strip)))
    {
      fprintf
	(stderr, "%s: unable to initialize Strip data source\n", argv[0]);
      status = 1;
      goto done;
    }

  /* set the strip callback functions */
  Strip_setattr
    (strip,
     STRIP_CONNECT_FUNC, 	request_connect,
     STRIP_DISCONNECT_FUNC,	request_disconnect,
     0);

  /* look for and load defaults file */
  if ((f = fopen (STRIP_DEFAULT_FILENAME, "r")) != NULL)
    {
      fprintf
	(stdout, "StripTool: using default file, %s.\n",
	 STRIP_DEFAULT_FILENAME);
      Strip_readconfig (strip, f, STRIPCFGMASK_ALL, STRIP_DEFAULT_FILENAME);/*VTR*/
      fclose (f);
    }

  /* now load a config file if requested */
  if (argc >= 2)
    if (f = fopen (argv[1], "r"))
      {
	fprintf
	  (stdout, "StripTool: using config file, %s.\n", argv[1]);
	Strip_readconfig (strip, f, STRIPCFGMASK_ALL, argv[1]);/*VTR*/
	fclose (f);
      }
    else fprintf
	   (stdout,
	    "StripTool: can't open %s; using default config.\n",
	    argv[1]);
  
  status = 0;
  Strip_go (strip);

  done:
  StripDS_terminate (strip_ds);
  Strip_delete (strip);
  return status;
}


static int	request_connect		(StripCurve curve, void *BOGUS(1))
{
  int ret_val = 0;
  
  if (strcmp
      ((char *)StripCurve_getattr_val (curve, STRIPCURVE_NAME), CPU_CURVE_NAME)
      == 0)
    {
      StripCurve_setattr
	(curve,
	 STRIPCURVE_NAME,	CPU_CURVE_NAME,
	 STRIPCURVE_EGU,	CPU_CURVE_EGU,
	 STRIPCURVE_PRECISION,	CPU_CURVE_PRECISION,
	 STRIPCURVE_MIN,	CPU_CURVE_MIN,
	 STRIPCURVE_MAX,	CPU_CURVE_MAX,
	 STRIPCURVE_SAMPLEFUNC,	get_cpu_usage,
	 0);
      Strip_setconnected (strip, curve);
#ifdef TEST_BAD_CONNECTION
      Strip_addtimeout (strip, 5, disconnect_curve, curve);
#endif
      ret_val = 1;
    }
  else ret_val = StripDS_request_connect (curve, strip_ds);
  return ret_val;
}


static int	request_disconnect	(StripCurve curve, void *BOGUS(1))
{
  int ret_val = 0;
  
  if (strcmp
      ((char *)StripCurve_getattr_val (curve, STRIPCURVE_NAME), CPU_CURVE_NAME)
      == 0)
    ret_val = 1;
  else ret_val = StripDS_request_disconnect (curve, strip_ds);

  return ret_val;
}


static double	get_cpu_usage		(void *BOGUS(1))
{
  static int			initialized = 0;
  static struct timeval		time_a = { 0, 0 };
  static struct timeval		time_b = { 0, 0 };
  static struct timeval		t = { 0, 0 };
  static struct timezone	tz;
  static clock_t		clock_a = (clock_t)-1;
  static clock_t		clock_b = (clock_t)-1;
  static double			cpu_usage = 0;

  if (!initialized)
    {
      gettimeofday (&time_a, &tz);
      clock_a = clock();
      initialized = 1;
    }

  gettimeofday (&time_b, &tz);
  clock_b = clock();

  /* when the time value wraps, just return the previous percentage */
  if (clock_b >= clock_a)
    {
      cpu_usage = (double)(clock_b - clock_a) / (double)CLOCKS_PER_SEC;
      cpu_usage /= subtract_times (&t, &time_a, &time_b);
      cpu_usage *= 100;
    }

  clock_a = clock_b;
  time_a = time_b;
  
  return cpu_usage;
}

#ifdef TEST_BAD_CONNECTION
static void	disconnect_curve	(StripCurve curve)
{
  double	x = drand48();

  x *= 60;
  
  Strip_setwaiting (strip, curve);
  Strip_addtimeout (strip, x, reconnect_curve, curve);
  fprintf (stdout, "disconn: %f\n", x);
}


static void	reconnect_curve		(StripCurve curve)
{
  double	x = drand48();

  x *= 30;
  
  Strip_setconnected (strip, curve);
  Strip_addtimeout (strip, 10, disconnect_curve, curve);
  fprintf (stdout, "reconn: %f\n", x);
}
#endif

