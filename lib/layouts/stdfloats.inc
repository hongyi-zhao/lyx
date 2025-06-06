# Author : Lars Gullik Bjønnes <larsbj@lyx.org>

# This include file contains all the floats that are defined as standard
# in most LyX layouts.


Format 109

Input stdfloatnames.inc

Float
	Type                  table
	GuiName               Table
	Placement             tbp
	Extension             lot
	NumberWithin          none
	Style                 plain
	ListName              "List of Tables"
	IsPredefined          true
	UsesFloatPkg          false
	ListCommand           listoftables
	RefPrefix             tab
	PrettyFormat          "Table|Tables ##"
End


Float
	Type                  figure
	GuiName               Figure
	Placement             tbp
	Extension             lof
	NumberWithin          none
	Style                 plain
	ListName              "List of Figures"
	IsPredefined          true
	UsesFloatPkg          false
	ListCommand           listoffigures
	RefPrefix             fig
	PrettyFormat          "Figure|Figures ##"
End


Float
	Type                  algorithm
	GuiName               Algorithm
	Placement             tbp
	Extension             loa
	NumberWithin          none
	Style                 ruled
	ListName              "List of Algorithms"
	IsPredefined          false
	UsesFloatPkg          true
	RefPrefix             alg
	PrettyFormat          "Algorithm|Algorithms ##"
End


InsetLayout FloatList
  HTMLTag h2
  HTMLStyle
    a.lyxtoc-floats { 
      text-decoration: none; 
      color: black;
    }
    a.lyxtoc-floats:visited { color: black; }
    div.lyxtoc-floats { 
      margin: 0em 0em 0.1em 1em;
      font-size: large;
      font-weight: normal;
    }
  EndHTMLStyle
End
