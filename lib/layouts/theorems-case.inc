# Original Author : Paul Rubin <rubin@msu.edu>
# Modified by Richard Kimberly Heck to use enumitem

# Definition of case environment as an enumeration

# Define Case as an enumeration environment
# with its own counters

Format 109

Counter casei
	GuiName "Case (Level 1)" 
	PrettyFormat "Case|Cases \arabic{casei}"
	LabelString "Case \arabic{casei}."
End
Counter caseii
	GuiName "Case (Level 2)"
	Within casei
	LabelString "Case \roman{caseii}."
	PrettyFormat "Case|Cases \roman{caseii}"
End
Counter caseiii
	GuiName "Case (Level 3)"
	Within caseii
	LabelString "Case \alph{caseiii}."
	PrettyFormat "Case|Cases \alph{caseiii}"
End
Counter caseiv
	GuiName "Case (Level 4)"
	Within caseiii
	PrettyFormat "Case|Cases \arabic{caseiv}"
	LabelString "Case \arabic{caseiv}."
End

Style Case
  Category              Reasoning
  Margin                Static
  LatexType             Item_Environment
  LatexName             casenv
  LabelType             Enumerate
  LeftMargin            "Case ###."
  LabelSep              xx
  ParSkip               0.0
  ItemSep               0.2
  TopSep                0.7
  BottomSep             0.7
  ParSep                0.3
  Align                 Block
  AlignPossible         Block, Left
  LabelCounter          case
  LabelFont
    Series              Medium
    Shape               Italic
  EndFont
  HTMLTag               ol
  HTMLItem              li
  HTMLLabel             NONE
  HTMLClass             lyxcase
  RefPrefix             enu
  Argument listpreamble:1
    LabelString    "List preamble"
    MenuString     "List Preamble"
    Tooltip        "LaTeX code to be inserted before the first item"
    PassThru       1
    Font
      Family typewriter
      Color latex
    EndFont
  EndArgument
  Argument item:1
    LabelString   "Custom Item|s"
    Tooltip       "A customized item string"
  EndArgument
  NextNoIndent          1
	Requires enumitem
	# It'd be easier to use \setlist[caseenv] here, but that causes problems
	# if you nest an ordinary list inside a case environment. See discussion
	# at bug #7611.
  Preamble
    \newlist{casenv}{enumerate}{4}
    \setlist[casenv]{leftmargin=*,align=left,widest={iiii}}
    \setlist[casenv,1]{label={{\itshape\ \casename} \arabic*.},ref=\arabic*}
    \setlist[casenv,2]{label={{\itshape\ \casename} \roman*.},ref=\roman*}
    \setlist[casenv,3]{label={{\itshape\ \casename\ \alph*.}},ref=\alph*}
    \setlist[casenv,4]{label={{\itshape\ \casename} \arabic*.},ref=\arabic*}
  EndPreamble
  LangPreamble
    \providecommand{\casename}{_(Case)}
  EndLangPreamble
  BabelPreamble
    \addto\captions$$lang{\renewcommand{\casename}{_(Case)}}
  EndBabelPreamble
  RefPrefix OFF
  DocBookTag            para
  DocBookAttr           role='cases'
End
