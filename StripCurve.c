/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#include "StripCurve.h"

StripCurve	StripCurve_init (StripConfig *scfg)
{
  StripCurveInfo	*sc;

  if ((sc = (StripCurveInfo *)malloc (sizeof (StripCurveInfo))) != NULL)
    {
      sc->scfg		= scfg;
      sc->id		= 0;
      sc->details 	= NULL;
      sc->func_data	= NULL;
      sc->get_value	= NULL;
      sc->status	= 0;
    }

  return (StripCurve)sc;
}


void		StripCurve_delete	(StripCurve the_sc)
{
  free (the_sc);
}


int		StripCurve_setattr	(StripCurve the_sc, ...)
{
  va_list		ap;
  int			attrib;
  StripCurveInfo	*sc = (StripCurveInfo *)the_sc;
  int			ret_val = 1;

  va_start (ap, the_sc);
  for (attrib = va_arg (ap, StripCurveAttribute);
       (attrib != 0);
       attrib = va_arg (ap, StripCurveAttribute))
    {
      if ((ret_val = ((attrib > 0) && (attrib < STRIPCURVE_LAST_ATTRIBUTE))))
	switch (attrib)
	  {
	  case STRIPCURVE_NAME:
	    strcpy (sc->details->name, va_arg (ap, char *));
	    sc->details->update_mask |= STRIPCFGMASK_CURVE_NAME;
	    sc->scfg->UpdateInfo.update_mask |= STRIPCFGMASK_CURVE_NAME;
	    break;
	  case STRIPCURVE_EGU:
	    strcpy (sc->details->egu, va_arg (ap, char *));
	    sc->details->update_mask |= STRIPCFGMASK_CURVE_EGU;
	    sc->scfg->UpdateInfo.update_mask |= STRIPCFGMASK_CURVE_EGU;
	    StripCurve_setstat (sc, STRIPCURVE_EGU_SET);
	    break;
	  case STRIPCURVE_PRECISION:
	    sc->details->precision = va_arg (ap, int);
	    sc->details->precision =
	      max ( sc->details->precision, STRIPMIN_CURVE_PRECISION);
	    sc->details->precision =
	      min ( sc->details->precision, STRIPMAX_CURVE_PRECISION);
	    sc->details->update_mask |= STRIPCFGMASK_CURVE_PRECISION;
	    sc->scfg->UpdateInfo.update_mask |= STRIPCFGMASK_CURVE_PRECISION;
	    StripCurve_setstat (sc, STRIPCURVE_PRECISION_SET);
	    break;
	  case STRIPCURVE_MIN:
	    sc->details->min = va_arg (ap, double);
	    sc->details->update_mask |= STRIPCFGMASK_CURVE_MIN;
	    sc->scfg->UpdateInfo.update_mask |= STRIPCFGMASK_CURVE_MIN;
	    StripCurve_setstat (sc, STRIPCURVE_MIN_SET);
	    break;
	  case STRIPCURVE_MAX:
	    sc->details->max = va_arg (ap, double);
	    sc->details->update_mask |= STRIPCFGMASK_CURVE_MAX;
	    sc->scfg->UpdateInfo.update_mask |= STRIPCFGMASK_CURVE_MAX;
	    StripCurve_setstat (sc, STRIPCURVE_MAX_SET);
	    break;
	  case STRIPCURVE_PENSTAT:
	    sc->details->penstat = va_arg (ap, int);
	    sc->details->update_mask |= STRIPCFGMASK_CURVE_PENSTAT;
	    sc->scfg->UpdateInfo.update_mask |= STRIPCFGMASK_CURVE_PENSTAT;
	    break;
	  case STRIPCURVE_PLOTSTAT:
	    sc->details->plotstat = va_arg (ap, int);
	    sc->details->update_mask |= STRIPCFGMASK_CURVE_PLOTSTAT;
	    sc->scfg->UpdateInfo.update_mask |= STRIPCFGMASK_CURVE_PLOTSTAT;
	    break;
	  case STRIPCURVE_SAMPLEFUNC:
	    sc->get_value = va_arg (ap, StripCurveSampleFunc);
	    break;
	  case STRIPCURVE_SAMPLEDATA:
	    sc->func_data = va_arg (ap, void *);
	    break;
	  }
      else break;
    }

  va_end (ap);
  return ret_val;
}


int		StripCurve_getattr	(StripCurve the_sc, ...)
{
  va_list		ap;
  int			attrib;
  StripCurveInfo	*sc = (StripCurveInfo *)the_sc;
  int			ret_val = 1;

  va_start (ap, the_sc);
  for (attrib = va_arg (ap, StripCurveAttribute);
       (attrib != 0);
       attrib = va_arg (ap, StripCurveAttribute))
    {
      if ((ret_val = ((attrib > 0) && (attrib < STRIPCURVE_LAST_ATTRIBUTE))))
	switch (attrib)
	  {
	  case STRIPCURVE_NAME:
	    *(va_arg (ap, char **)) = sc->details->name;
	    break;
	  case STRIPCURVE_EGU:
	    *(va_arg (ap, char **)) = sc->details->egu;
	    break;
	  case STRIPCURVE_PRECISION:
	    *(va_arg (ap, int *)) = sc->details->precision;
	    break;
	  case STRIPCURVE_MIN:
	    *(va_arg (ap, double *)) = sc->details->min;
	    break;
	  case STRIPCURVE_MAX:
	    *(va_arg (ap, double *)) = sc->details->max;
	    break;
	  case STRIPCURVE_PENSTAT:
	    *(va_arg (ap, int *)) = sc->details->penstat;
	    break;
	  case STRIPCURVE_PLOTSTAT:
	    *(va_arg (ap, int *)) = sc->details->plotstat;
	    break;
	  case STRIPCURVE_COLOR:
	    *(va_arg (ap, Pixel*)) = sc->details->pixel;
	    break;
	  case STRIPCURVE_SAMPLEFUNC:
	    *(va_arg (ap, StripCurveSampleFunc *)) = sc->get_value;
	    break;
	  case STRIPCURVE_SAMPLEDATA:
	    *(va_arg (ap, void **)) = sc->func_data;
	    break;
	  }
      else break;
    }
  va_end (ap);
  return ret_val;
}


/*
 * StripCurve_set/getattr_val
 */
void	*StripCurve_getattr_val	(StripCurve the_sc, StripCurveAttribute attrib)
{
  StripCurveInfo	*sc = (StripCurveInfo *)the_sc;
  
  switch (attrib)
    {
    case STRIPCURVE_NAME:
      return (void *)sc->details->name;
      break;
    case STRIPCURVE_EGU:
      return (void *)sc->details->egu;
      break;
    case STRIPCURVE_PRECISION:
      return (void *)&sc->details->precision;
      break;
    case STRIPCURVE_MIN:
      return (void *)&sc->details->min;
      break;
    case STRIPCURVE_MAX:
      return (void *)&sc->details->max;
      break;
    case STRIPCURVE_PENSTAT:
      return (void *)&sc->details->penstat;
      break;
    case STRIPCURVE_PLOTSTAT:
      return (void *)&sc->details->plotstat;
      break;
    case STRIPCURVE_COLOR:
      return (void *)&sc->details->pixel;
      break;
    case STRIPCURVE_SAMPLEFUNC:
      return (void *)sc->get_value;
      break;
    case STRIPCURVE_SAMPLEDATA:
      return (void *)sc->func_data;
      break;
    }
}


/*
 * StripCurve_setstat
 */
StripCurveStatus	StripCurve_setstat	(StripCurve       the_sc,
						 StripCurveStatus stat)
{
  StripCurveInfo	*sc = (StripCurveInfo *)the_sc;

  sc->details->set_mask |= (stat & STRIPCFGMASK_CURVE);
  sc->status |= stat;
  return (((sc->details->set_mask & STRIPCFGMASK_CURVE) | sc->status) &
	  stat);
}

/*
 * StripCurve_getstat
 */
StripCurveStatus	StripCurve_getstat	(StripCurve       the_sc,
						 StripCurveStatus stat)
{
  StripCurveInfo	*sc = (StripCurveInfo *)the_sc;

  return (((sc->details->set_mask & STRIPCFGMASK_CURVE) | sc->status) & stat);
}

/*
 * StripCurve_clearstat
 */
StripCurveStatus	StripCurve_clearstat	(StripCurve       the_sc,
						 StripCurveStatus stat)
{
  StripCurveInfo	*sc = (StripCurveInfo *)the_sc;

  sc->details->set_mask &= ~(stat & STRIPCFGMASK_CURVE);
  sc->status &= ~stat;
  return (((sc->details->set_mask & STRIPCFGMASK_CURVE) | sc->status) &
	  stat);
}
