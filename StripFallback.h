#ifndef _StripFallback
#define _StripFallback

#if !defined (STRIP_APP_CLASS)
#define STRIP_APP_CLASS		"StripTool"
#endif

String	fallback_resources[] =
{
#ifdef STRIP_DEFINE_GUICOLORS
  STRIP_APP_CLASS "*background:			Grey75",
  STRIP_APP_CLASS "*foreground:			Black",
#endif
  STRIP_APP_CLASS "*font:			*helvetica-bold-r-normal--10*",
  STRIP_APP_CLASS "*fontList:			*helvetica-bold-r-normal--10*",
  STRIP_APP_CLASS "*graphPanel.shadowType:	XmSHADOW_ETCHED_OUT",
  STRIP_APP_CLASS "*hintShell.background:	White",
  STRIP_APP_CLASS "*hintShell.foreground:	Black",
  STRIP_APP_CLASS "*hintShell.cancelWaitPeriod:	200",
  STRIP_APP_CLASS "*hintShell.fontSet:		*-helvetica-medium-r-normal--12-*",
  STRIP_APP_CLASS "*MenuBar*Help.sensitive:	False",
  STRIP_APP_CLASS "*ColorDialog*colorPaletteDrawingArea.height:	80",
  STRIP_APP_CLASS "*ColorDialog*redSlider.titleString:		Red",
  STRIP_APP_CLASS "*ColorDialog*greenSlider.titleString:	Green",
  STRIP_APP_CLASS "*ColorDialog*blueSlider.titleString:		Blue",
  STRIP_APP_CLASS "*ColorDialog*cellStatusLabel*FontList: 	*-helvetica-medium-o-normal--10-*",
  STRIP_APP_CLASS "*ColorDialog*buttonRowColumn.orientation: 	XmHORIZONTAL",
  STRIP_APP_CLASS "*ColorDialog*buttonRowColumn.entryAlignment: XmALIGNMENT_CENTER",
  STRIP_APP_CLASS "*ColorDialog*buttonRowColumn.isAligned: 	True",
  STRIP_APP_CLASS "*ColorDialog*buttonRowColumn.packing: 	XmPACK_COLUMN",
  0
};

#endif
