/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "Strip.h"
#include "StripMisc.h"
#include "StripDAQ.h"

#define CPU_CURVE_NAME          "CPU_Usage"
#define CPU_CURVE_EGU           "%"
#define CPU_CURVE_PRECISION     2
#define CPU_CURVE_MIN           0.00
#define CPU_CURVE_MAX           100.00

Strip   strip;
StripDAQ        strip_daq;              /* the Strip data source */

static FILE     *StripTool_open_file    (char *, char *, int, char *, char *);

static int      request_connect         (StripCurve, void *);
static int      request_disconnect      (StripCurve, void *);

static double   get_cpu_usage           (void *);

int StripTool_main (int argc, char *argv[])
{
  int           status;
  FILE          *f;
  char          path_used[1024];

    /* create and initialize the Strip structure */
  if (!(strip = Strip_init (&argc, argv, tmpfile())))
    {
      fprintf (stderr, "%s: unable to initialize Strip\n", argv[0]);
      exit (1);
    }

  /* fire up the data source  */
  if (!(strip_daq = StripDAQ_initialize (strip)))
    {
      fprintf
        (stderr, "%s: unable to initialize Strip data source\n", argv[0]);
      status = 1;
      goto done;
    }

  /* set the strip callback functions */
  Strip_setattr
    (strip,
     STRIP_CONNECT_FUNC,        request_connect,
     STRIP_DISCONNECT_FUNC,     request_disconnect,
     0);

  /* look for and load defaults file */
  if ((f = fopen (STRIP_DEFAULT_FILENAME, "r")) != NULL)
    {
      fprintf
        (stdout, "StripTool: using default file, %s.\n",
         STRIP_DEFAULT_FILENAME);
      Strip_readconfig (strip, f, SCFGMASK_ALL, STRIP_DEFAULT_FILENAME);
      fclose (f);
    }

  /* now load a config file if requested */
  if (argc >= 2)
  {
    f = StripTool_open_file
      (getenv(STRIP_FILE_SEARCH_PATH_ENV), path_used, sizeof(path_used),
       argv[1], "r");
       
    if (f)
      {
        fprintf
          (stdout, "StripTool: using config file, %s.\n", argv[1]);
        Strip_readconfig (strip, f, SCFGMASK_ALL, argv[1]);
        fclose (f);
      }
    else fprintf
           (stdout,
            "StripTool: can't open %s; using default config.\n",
            argv[1]);
  }
  
  status = 0;
  Strip_go (strip);

  done:
  StripDAQ_terminate (strip_daq);
  Strip_delete (strip);
  return status;
}


/*
 *      StripOpenFile()
 *      (attempt to open a file using a path string of the same format 
 *      as the "PATH" environment variable)
 *
 *      2nd and 3rd arguments can be omitted by setting them to NULL and 0
 *      respectively.
 *
 *      ppath_used is left unmodified if no file could be opened.
 *      Otherwise, ppath_used will contain the actual path used 
 *      to open a file.
 *
 *      Copied from open_display.c written by Jeff Hill for edd/dm.
 *
 *      Nick Rees 16 June 1999
 */

#ifdef UNIX
#define errnoSet(E) (errno = (E))
extern errno;
#endif

static FILE * StripTool_open_file    ( char * pld_display_path,
                                       char * ppath_used,
                                       int    length_path_used,
                                       char * pfile_name,
                                       char * mode )
{     
  FILE *  fd;
  char    *pcolon = NULL;
  char    *pslash;
  int     name_length;
  int     path_length;
  char    pname[128];

  if(pfile_name == NULL){
    errnoSet(EINVAL);
    return NULL;
  }

  name_length = strlen(pfile_name);
  if(name_length+1 > sizeof(pname)){
    errnoSet(EINVAL);
    return NULL;
  }

  if(name_length == 0){
    errnoSet(EINVAL);
    return NULL;
  }

  pslash = strrchr(pfile_name, '/');
  if(pslash!=NULL || pld_display_path==NULL){
    fd = fopen(pfile_name, mode);
    if(ppath_used && fd != NULL){
      if(pslash){
        path_length = pslash-pfile_name;
      }
      else{
        path_length = 0;
      }

      if(path_length+1>length_path_used){
        fclose(fd);
        errnoSet(EINVAL);
        return NULL;
      }
      memcpy(ppath_used, pfile_name, path_length);
      ppath_used[path_length] = 0;
    }
    /*
     * errno is set by open
     *
     */
    return fd;
  }

  while(1){

    if(pcolon){
      pld_display_path = pcolon+1;
    }
                        
    pcolon = strchr(pld_display_path, ':');
    if(pcolon){     
      path_length = pcolon-pld_display_path;
    }
    else{
      path_length = strlen(pld_display_path);
    }


    if(path_length+name_length+2 > sizeof(pname))
      continue;
    strncpy(
      pname, 
      pld_display_path, 
      path_length);

    if(pld_display_path[path_length-1] != '/'){
      pname[path_length] = '/';
      path_length++;
    }
                        
    strncpy(
      &pname[path_length], 
      pfile_name,
      name_length+1);

#ifdef DEBUG
    printf("Attempting to open [%s]\n", pname);
#endif
    fd = fopen(pname, mode );
    if(fd != NULL){
      if(ppath_used){
        if(path_length+1>length_path_used){
          fclose(fd);
          errnoSet(EINVAL);
          return NULL;
        }
        memcpy(ppath_used, pname, path_length);
        ppath_used[path_length] = 0;
      }
      return fd;
    }

    /*
     * errno is set by open
     *
     */
    if(pcolon == NULL){
      return NULL;
    }
  }
}


static int      request_connect         (StripCurve curve, void *BOGUS(1))
{
  int ret_val = 0;
  
  if (strcmp
      ((char *)StripCurve_getattr_val (curve, STRIPCURVE_NAME), CPU_CURVE_NAME)
      == 0)
  {
    StripCurve_setattr
      (curve,
       STRIPCURVE_NAME,       CPU_CURVE_NAME,
       STRIPCURVE_EGU,        CPU_CURVE_EGU,
       STRIPCURVE_PRECISION,  CPU_CURVE_PRECISION,
       STRIPCURVE_MIN,        CPU_CURVE_MIN,
       STRIPCURVE_MAX,        CPU_CURVE_MAX,
       STRIPCURVE_SAMPLEFUNC, get_cpu_usage,
       0);
    Strip_setconnected (strip, curve);
    ret_val = 1;
  }
  else ret_val = StripDAQ_request_connect (curve, strip_daq);
  return ret_val;
}


static int      request_disconnect      (StripCurve curve, void *BOGUS(1))
{
  int ret_val = 0;
  
  if (strcmp
      ((char *)StripCurve_getattr_val (curve, STRIPCURVE_NAME), CPU_CURVE_NAME)
      == 0)
    ret_val = 1;
  else ret_val = StripDAQ_request_disconnect (curve, strip_daq);

  return ret_val;
}


static double   get_cpu_usage           (void *BOGUS(1))
{
  static int                    initialized = 0;
  static struct timeval         time_a = { 0, 0 };
  static struct timeval         time_b = { 0, 0 };
  static struct timeval         t = { 0, 0 };
  static clock_t                clock_a = (clock_t)-1;
  static clock_t                clock_b = (clock_t)-1;
  static double                 cpu_usage = 0;
  
  if (!initialized)
  {
    get_current_time (&time_a);
    clock_a = clock();
    initialized = 1;
  }

  get_current_time (&time_b);
  clock_b = clock();

  /* when the time value wraps, just return the previous percentage */
  if (clock_b >= clock_a)
  {
    cpu_usage = subtract_times (&t, &time_a, &time_b);
    cpu_usage = (double)(clock_b - clock_a) / (double)CLOCKS_PER_SEC;
    cpu_usage /= subtract_times (&t, &time_a, &time_b);
    cpu_usage *= 100;
  }

  clock_a = clock_b;
  time_a = time_b;
  
  return cpu_usage;
}

