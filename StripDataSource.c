/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#include "StripDataSource.h"
#include "StripDefines.h"
#include "StripMisc.h"

#define CURVE_DATA(C)	((CurveData *)((StripCurveInfo *)C)->id)

#define SDS_LTE			0
#define SDS_GTE 		1

#define SDS_DUMP_FIELDWIDTH	30
#define SDS_DUMP_NUMWIDTH	20
#define SDS_DUMP_BADVALUESTR	"???"

typedef struct
{
  StripCurveInfo	*curve;
  double		*val;
  char			*stat;
  size_t		idx_t0, idx_t1;
} CurveData;

typedef struct
{
  size_t		buf_size;
  size_t		cur_idx;
  size_t		idx_t0, idx_t1;
  size_t		count;
  
  struct timeval	*times;
  CurveData		buffers[STRIP_MAX_CURVES];
}
StripDataSourceInfo;


/* ====== Static Function Prototypes ====== */
static int 	StripDataSource_resize 	(StripDataSourceInfo 	*sds,
					 size_t 	     	buf_size);

static long	StripDataSource_find_date_idx 	(StripDataSourceInfo	*sds,
						 struct timeval		*t,
						 int 			mode);
static int	pack_array	(void **, size_t,
				 int, int, int,
				 int, int *, int *);


/* ====== Static Data ====== */
static struct timezone	tz;


/* ====== Functions ====== */

/*
 * StripDataSource_init
 */
StripDataSource	StripDataSource_init	(void)
{
  StripDataSourceInfo	*sds;
  int			i;

  if ((sds = (StripDataSourceInfo *)malloc (sizeof(StripDataSourceInfo)))
      != NULL)
    {
      sds->buf_size 	= 0;
      sds->cur_idx	= 0;
      sds->count	= 0;
      sds->times	= NULL;
      sds->idx_t0	= 0;
      sds->idx_t1	= 0;

      for (i = 0; i < STRIP_MAX_CURVES; i++)
	{
	  sds->buffers[i].curve 	= NULL;
          sds->buffers[i].val		= NULL;
          sds->buffers[i].stat		= NULL;
	  sds->buffers[i].idx_t1	= 0;
	  sds->buffers[i].idx_t0	= 0;
	}
    }

  return sds;
}



/*
 * StripDataSource_delete
 */
void	StripDataSource_delete	(StripDataSource the_sds)
{
  StripDataSourceInfo	*sds = (StripDataSourceInfo *)the_sds;
  int			i;

  for (i = 0; i < STRIP_MAX_CURVES; i++)
  {
    if (sds->buffers[i].val)
      free (sds->buffers[i].val);
    if (sds->buffers[i].stat)
      free (sds->buffers[i].stat);
  }

  free (sds);
}



/*
 * StripDataSource_setattr
 */
int	StripDataSource_setattr	(StripDataSource the_sds, ...)
{
  va_list		ap;
  StripDataSourceInfo	*sds = (StripDataSourceInfo *)the_sds;
  int			attrib;
  size_t		tmp;
  int			ret_val = 1;

  
  va_start (ap, the_sds);
  for (attrib = va_arg (ap, SDSAttribute);
       (attrib != 0);
       attrib = va_arg (ap, SDSAttribute))
    {
      if ((ret_val = ((attrib > 0) && (attrib < SDS_LAST_ATTRIBUTE))))
	switch (attrib)
	  {
	  case SDS_NUMSAMPLES:
	    tmp = va_arg (ap, size_t);
	    /*
	    fprintf (stdout, "StripDataSource_setattr(): sds = %p\n", sds);
	    fprintf (stdout, "StripDataSource_setattr(): tmp = %u\n", tmp);
	    */
	    if (tmp > sds->buf_size) StripDataSource_resize (sds, tmp);
	    break;
	  }
    }

  va_end (ap);
  return ret_val;
}



/*
 * StripDataSource_getattr
 */
int	StripDataSource_getattr	(StripDataSource the_sds, ...)
{
  va_list		ap;
  StripDataSourceInfo	*sds = (StripDataSourceInfo *)the_sds;
  int			attrib;
  int			ret_val = 1;

  
  va_start (ap, the_sds);
  for (attrib = va_arg (ap, SDSAttribute);
       (attrib != 0);
       attrib = va_arg (ap, SDSAttribute))
    {
      if ((ret_val = ((attrib > 0) && (attrib < SDS_LAST_ATTRIBUTE))))
	switch (attrib)
	  {
	  case SDS_NUMSAMPLES:
	    *(va_arg (ap, size_t *)) = sds->buf_size;
	    break;
	  }
    }

  va_end (ap);
  return ret_val;
}



/*
 * StripDataSource_addcurve
 */
int StripDataSource_addcurve	(StripDataSource 	the_sds,
				 StripCurve 		the_curve)
{
  StripDataSourceInfo	*sds = (StripDataSourceInfo *)the_sds;
  int 			i;
  int			ret = 0;
  
  for (i = 0; i < STRIP_MAX_CURVES; i++)
    if (sds->buffers[i].curve == NULL)		/* it's available */
      break;

  if (i < STRIP_MAX_CURVES)
  {
    sds->buffers[i].val = (double *)malloc (sds->buf_size * sizeof (double));
    sds->buffers[i].stat = (char *)calloc (sds->buf_size, sizeof (char));
    if (sds->buffers[i].val && sds->buffers[i].stat)
    {
      sds->buffers[i].curve 	= (StripCurveInfo *)the_curve;
      sds->buffers[i].idx_t0	= 0;
      sds->buffers[i].idx_t1	= 0;
      
      /* use the id field of the strip curve to reference the buffer */
      ((StripCurveInfo *)the_curve)->id = &sds->buffers[i];
      ret = 1;
    }
    else
    {
      if (sds->buffers[i].val) free (sds->buffers[i].val);
      if (sds->buffers[i].stat) free (sds->buffers[i].stat);
    }
  }
  
  return ret;
}



/*
 * StripDataSource_removecurve
 */
int StripDataSource_removecurve	(StripDataSource the_sds, StripCurve the_curve)
{
  StripDataSourceInfo	*sds = (StripDataSourceInfo *)the_sds;
  CurveData		*cd;
  int			ret_val = 1;

  if ((cd = CURVE_DATA(the_curve)) != NULL)
    {
      cd->curve = NULL;
      free (cd->val);
      free (cd->stat);
      cd->val = NULL;
      cd->stat = NULL;
      ((StripCurveInfo *)the_curve)->id = NULL;
    }

  return ret_val;
}


/*
 * StripDataSource_sample
 */
void	StripDataSource_sample	(StripDataSource the_sds)
{
  StripDataSourceInfo	*sds = (StripDataSourceInfo *)the_sds;
  StripCurveInfo	*c;
  int			i;
  int			need_time = 1;

  for (i = 0; i < STRIP_MAX_CURVES; i++)
  {
    if ((c = sds->buffers[i].curve) != NULL)
    {
      if (need_time)
      {
        sds->cur_idx = (sds->cur_idx + 1) % sds->buf_size;
        gettimeofday (&sds->times[sds->cur_idx], &tz);
        sds->count = min ((sds->count+1), sds->buf_size);
        need_time = 0;
      }
      if ((c->status & STRIPCURVE_CONNECTED) &&
          (!(c->status & STRIPCURVE_WAITING) &&
           (c->details->penstat == STRIPCURVE_PENDOWN)))
      {
        sds->buffers[i].val[sds->cur_idx] =
          c->get_value (c->func_data);
        sds->buffers[i].stat[sds->cur_idx] = DATASTAT_PLOTABLE;
      }
      else sds->buffers[i].stat[sds->cur_idx] = ~DATASTAT_PLOTABLE;
    }
  }
}


/*
 * StripDataSource_init_range
 */
size_t		StripDataSource_init_range	(StripDataSource the_sds,
						 struct timeval  *t0,
						 struct timeval  *t1)
{
  StripDataSourceInfo	*sds = (StripDataSourceInfo *)the_sds;
  long			r1, r2;
  int			i;

  r1 = ((sds->count > 0)?
	StripDataSource_find_date_idx (sds, t0, SDS_GTE) : -1);

  if (r1 >= 0)	/* look for last date only if the first one was ok */
    r2 = StripDataSource_find_date_idx (sds, t1, SDS_LTE);

  if ((r1 < 0) || (r2 < 0))
    sds->idx_t0 = sds->idx_t1;
  else {
    sds->idx_t0 = (size_t)r1;
    sds->idx_t1 = (size_t)r2;
  }

  if (sds->idx_t0 == sds->idx_t1)
    r1 = 0;
  else if (sds->idx_t0 < sds->idx_t1)
    r1 = sds->idx_t1 - sds->idx_t0 + 1;
  else r1 = sds->buf_size - sds->idx_t0 + sds->idx_t1 + 1;

  for (i = 0; i < STRIP_MAX_CURVES; i++)
  {
    sds->buffers[i].idx_t0 = sds->idx_t0;
    sds->buffers[i].idx_t1 = sds->idx_t1;
  }

  return (size_t)r1;
}


/*
 * StripDataSource_get_times
 */
size_t	StripDataSource_get_times	(StripDataSource	the_sds,
					 struct timeval		**times)
{
  StripDataSourceInfo	*sds = (StripDataSourceInfo *)the_sds;
  size_t		ret_val = 0;

  if (sds->idx_t0 != sds->idx_t1)
    {
      *times = &sds->times[sds->idx_t0];

      if (sds->idx_t0 < sds->idx_t1)
	{
	  ret_val = sds->idx_t1 - sds->idx_t0 + 1;
	  sds->idx_t0 = sds->idx_t1;
	}
      else
	{
	  ret_val = sds->buf_size - sds->idx_t0;
	  sds->idx_t0 = 0;
	}
    }
  else *times = NULL;
  
  return ret_val;
}

  
/*
 * StripDataSource_get_data
 */
size_t	StripDataSource_get_data	(StripDataSource	the_sds,
					 StripCurve		the_curve,
                                         double			**val,
                                         char			**stat)
{
  StripDataSourceInfo	*sds = (StripDataSourceInfo *)the_sds;
  CurveData		*cd = CURVE_DATA(the_curve);

  size_t		ret_val = 0;
  
  if (cd->idx_t0 != cd->idx_t1)
  {
    *val = &cd->val[cd->idx_t0];
    *stat = &cd->stat[cd->idx_t0];
    
    if (cd->idx_t0 < cd->idx_t1)
    {
      ret_val = cd->idx_t1 - cd->idx_t0 + 1;
      cd->idx_t0 = cd->idx_t1;
    }
    else
    {
      ret_val = sds->buf_size - cd->idx_t0;
      cd->idx_t0 = 0;
    }
  }
  else *val = NULL;
  
  return ret_val;
}


/*
 * StripDataSource_dump
 */
int	StripDataSource_dump	(StripDataSource	the_sds,
				 StripCurve		curves[],
				 struct timeval 	*begin,
				 struct timeval 	*end,	
				 FILE 			*outfile)
{
  StripDataSourceInfo	*sds = (StripDataSourceInfo *)the_sds;
  struct timeval	*times;
  int			n_curves;
  int			n, i, j;
  int			msec;
  time_t		tt;
  char			buf[SDS_DUMP_FIELDWIDTH+1];
  int			ret_val = 0;
  struct		_cv
  {
    CurveData	*cd;
    double	*val;
    char	*stat;
  }
  cv[STRIP_MAX_CURVES];

  /* first build the array of curves to dump */
  if (curves[0])
  {
    /* get curves from array */
    for (n = 0; curves[n]; n++)
      if ((cv[n].cd = CURVE_DATA(curves[n])) == NULL)
      {
        fprintf (stderr, "StripDataSource_dump(): bad curve descriptor\n");
        return 0;
      }
  }
  else for (i = 0, n = 0; i < STRIP_MAX_CURVES; i++)	/* all curves */
  {
    if (sds->buffers[i].curve != NULL)
    {
      cv[n].cd = &sds->buffers[i];
      n++;
    }
  }
  n_curves = n;
  
  /* now set the begin, end times */
  if (!begin) begin = &sds->times[sds->idx_t0];
  if (!end) end = &sds->times[sds->idx_t1];

  /* now get the data */
  if (StripDataSource_init_range (the_sds, begin, end) > 0)
  {
    /* first print out the curve names along the top */
    fprintf (outfile, "%-*s", SDS_DUMP_FIELDWIDTH, "Sample Time");
    for (i = 0; i < n_curves; i++)
      fprintf
        (outfile, "%*s", SDS_DUMP_FIELDWIDTH,
         cv[i].cd->curve->details->name);
    fprintf (outfile, "\n");
    
    while ((n = StripDataSource_get_times (the_sds, &times)) > 0)
    {
      /* get the value array for each curve */
      for (i = 0; i < n_curves; i++)
        if (StripDataSource_get_data
            (the_sds, (StripCurve)cv[i].cd->curve, &cv[i].val, &cv[i].stat)
            != n)
        {
          fprintf
            (stderr, "StripDataSource_dump(): unexpected data count!\n");
          return 0;
        }
      
      /* write out the samples */
      for (i = 0; i < n; i++)
      {
        /* sample time */
        tt = (time_t)times[i].tv_sec;
        msec = (int)(times[i].tv_usec / ONE_THOUSAND);
        strftime
          (buf, SDS_DUMP_FIELDWIDTH, "%m/%d/%Y %H:%M:%S",
           localtime (&tt));
        j = strlen (buf);
        buf[j++] = '.';
        int2str (msec, &buf[j], 3);
        fprintf (outfile, "%-*s", SDS_DUMP_FIELDWIDTH, buf);
        
        /* sampled values */
        for (j = 0; j < n_curves; j++)
        {
          if (cv[j].stat[i] & DATASTAT_PLOTABLE)
            dbl2str
              (cv[j].val[i], cv[j].cd->curve->details->precision,
               buf, SDS_DUMP_NUMWIDTH);
          else strcpy (buf, SDS_DUMP_BADVALUESTR);
          fprintf (outfile, "%*s", SDS_DUMP_FIELDWIDTH, buf);
        }
        
        /* finally, the end-line */
        fprintf (outfile, "\n");
      }
    }
    
    fflush (outfile);
  }
  
  return ret_val;
}



/* ====== Static Functions ====== */
static long	StripDataSource_find_date_idx 	(StripDataSourceInfo	*sds,
						 struct timeval		*t,
						 int 			mode)
{
  long	a, b, i;
  long	x;


  x = ((long)sds->cur_idx - (long)sds->count + 1);
  if (x < 0) x += sds->buf_size;

  a = x;
  b = (long)sds->cur_idx;

  /* first check boundary conditions */
  if (mode == SDS_LTE)
    {
      x = compare_times (&sds->times[a], t);
      if (x > 0)
	return -1;
      else if (x == 0)	/* hey, we found it! */
	return a;
    }
  else if (mode == SDS_GTE)
    {
      x = compare_times (&sds->times[b], t);
      if (x < 0)
	return -1;
      else if (x == 0)	/* hey, we found it! */
	return b;
    }

  /* break the buffer into two parts if the begin index is greater than
   * the end index */
  if (a > b)
    {
      /* the buffer wraps around --determine whether t lies btw a and n,
       * or 0 and b */
      x = compare_times (t, &sds->times[sds->buf_size-1]);
      if (x < 0)
	b = (long)sds->buf_size-1;
      else if (x > 0)
	a = 0;
      else 	/* hey, we found it! */
	return (long)sds->buf_size-1;
    }
/*
  fprintf (stdout, "*** SDS: beginning search ***\n");
*/
  
  /* now do a binary search */
  do {
    i = a + ((b-a)/2);
/*
    fprintf (stdout, "SDS: a = %0.3d, b = %0.3d, i = %0.3d\n", a, b, i);
*/    
    x = compare_times (&sds->times[i], t);

    if (x > 0)
      b = i-1;
    else if (x < 0)
      a = i+1;
    else	/* found it */
      return i;
  } while (a <= b);
/*  
  fprintf (stdout, "*** SDS: finished search ***\n");
*/  
  if (x < 0)
    {
      if (mode == SDS_LTE)
	return i;
      if (mode == SDS_GTE)
	return i+1;
    }
  else if (x > 0)
    {
      if (mode == SDS_LTE)
	return i-1;
      if (mode == SDS_GTE)
	return i;
    }

  return -1;	/* never executed */
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * change data buffer size routine
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static int StripDataSource_resize (StripDataSourceInfo *sds, size_t buf_size)
{
  int			i;
  int			new_cur_idx = sds->cur_idx;
  int			new_count = sds->count;
  int 			ret_val = 0;
  
  int			new_index;

  if (sds->buf_size == 0)
  {
    
    sds->times = (struct timeval *)malloc
      (buf_size * sizeof(struct timeval));
    new_index = new_count = 0;
    ret_val = (sds->times != NULL);
  }
  else
  {
    ret_val = pack_array
      ((void **)&sds->times, sizeof(struct timeval),
       sds->buf_size, sds->cur_idx, sds->count,
       buf_size, &new_index, &new_count);
    if (ret_val)
      for (i = 0; i < STRIP_MAX_CURVES; i++)
        if (sds->buffers[i].val)
        {
          ret_val = pack_array
            ((void **)&sds->buffers[i].val, sizeof(double),
             sds->buf_size, sds->cur_idx, sds->count,
             buf_size, 0, 0);
          if (ret_val)
            ret_val = pack_array
              ((void **)&sds->buffers[i].stat, sizeof (char),
               sds->buf_size, sds->cur_idx, sds->count,
               buf_size, 0, 0);
          if (!ret_val) break;
        }
  }
  if (ret_val)
  {
    sds->buf_size = buf_size;
    sds->cur_idx = new_index;
    sds->count = new_count;
  }
  return ret_val;
}



static int	pack_array	(void 	**p,	/* address of pointer */
				 size_t	nbytes,	/* array element size */
				 int	n0,	/* old size */
				 int	i0,	/* old index */
				 int	s0,	/* old count */
				 int	n1,	/* new size */
				 int	*i1,	/* new index */
				 int	*s1)	/* new count */
{
  int	x, y;
  char	*q;

  if ((q = (char *)*p) != NULL)
    {
      if (n1 > n0)	/* new size is greater than old */
	{
	  if (q = (char *)realloc (q, n1*nbytes))
	    {
	      /* how many to push to the end? */
	      x = s0-i0-1;
	      if (x > 0)
		memmove (q+((n1-x)*nbytes), q+((n0-x)*nbytes), x*nbytes);
	      if (i1) *i1 = i0;
	      if (s1) *s1 = s0;
	    }
	}
      else		/* new size is less than old */
	{
	  x = (i0+1) - n1; /* num at front of old array which won't now fit */

	  if (x > 0)	/* not all elements on [0, i0] will fit on [0, n1] */
	    {
	      memmove (q, q+(x*nbytes), n1*nbytes);
	      if (i1) *i1 = i0 - x;
	      if (s1) *s1 = n1;
	    }
	  else	/* x <= 0 */
	    {
	      y = i0 + 1;		/* number at front of array */
	      x = min (-x, s0-y);	/* x <-- num elem. to pack at end */
	      
	      if (x > 0)
		memmove (q+((n1-x)*nbytes), q+((n0-x)*nbytes), x*nbytes);
	      
	      if (i1) *i1 = i0;
	      if (s1) *s1 = y + x;
	    }
	  
	  q = (char *)realloc (q, n1*nbytes);
	}
    }

  return ((*p = q) != NULL);
}
