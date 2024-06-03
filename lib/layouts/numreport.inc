# Author : André Pönitz
# This include file contains label definitions for a report-like numbering.

Format 105

Counter chapter
	GuiName Chapter
	Within                ""
	LabelString	      "\arabic{chapter}"
	LabelStringAppendix   "\Alph{chapter}"
	PrettyFormat "Chapter ##"
End

Counter section
	GuiName Section
	Within                chapter
	LabelString	      ""
	PrettyFormat "Section ##"
End

Float
	Type                  table
	NumberWithin          chapter
End


Float
	Type                  figure
	NumberWithin          chapter
End


Float
	Type                  algorithm
	NumberWithin          chapter
End

Counter listing
	GuiName Listing
	Within		      chapter
End

Counter footnote
	GuiName Footnote
	Within		      chapter
	PrettyFormat 	"footnote \arabic{footnote}"
End
