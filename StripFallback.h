#ifndef _StripFallback
#define _StripFallback

#define STRIP_APP_CLASS		"StripTool"

/* these values are relative to the application name, which will
 * be appended before these are passed to the X toolkit
 */
String	fallback_resources[] =
{
  STRIP_APP_CLASS "*background:			Grey75",
  STRIP_APP_CLASS "*foreground:			Black",
  STRIP_APP_CLASS "*font:			*helvetica-bold-r-normal--10*",
  STRIP_APP_CLASS "*fontList:			*helvetica-bold-r-normal--10*",
  STRIP_APP_CLASS "*graphPanel.shadowType:	XmSHADOW_ETCHED_OUT",
  STRIP_APP_CLASS "*hintShell.background:	LightYellow",
  STRIP_APP_CLASS "*hintShell.foreground:	Black",
  STRIP_APP_CLASS "*hintShell.cancelWaitPeriod:	200",
  STRIP_APP_CLASS "*hintShell.fontSet:		*-helvetica-medium-r-normal--12-*",
  STRIP_APP_CLASS "*MenuBar*Help.sensitive:	False",
         (String)NULL
};

#endif
