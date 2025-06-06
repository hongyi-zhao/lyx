# Literate Chunk inset definitions.
# This defines the Chunk inset used in the literate modules.
#
# Author: Liviu Andronic <landronimirc@gmail.com>
#
# Note that this file is included in sweave.module,
# knitr.module and noweb.module.

Format 109

Counter chunk
	GuiName "Knitr Chunk"
	PrettyFormat "Chunk|Chunks ##"
End

OutlinerName literate "Literate programming"

InsetLayout "Flex:Chunk"
    LabelString          "Chunk"
    LatexType            none
    LyXType              Custom
    Counter              chunk
    RightDelim           <br/>@
    Decoration           Classic
    Font
      Color              latex
      Family             typewriter
    EndFont
#   LabelFont
#      Color              latex
#      Size               Small
#   EndFont
    MultiPar              true
    CustomPars            false
    ForcePlain            true
    PassThru              1
    ParbreakIsNewline     1
    KeepEmpty             true
    Spellcheck            0
    FreeSpacing           true
    ForceLTR              true
    AddToToc              literate
    IsTocCaption          0
    Argument 1
      Mandatory           1
      LabelString         "Options"
      Tooltip             "Options"
      LeftDelim           <<
      RightDelim          >>=<br/>
      IsTocCaption        1
      AutoInsert          1
    EndArgument
    ResetsFont            false
    ForceOwnlines         true
    NeedCProtect          true
    EditExternal          true
	DocBookTag            programlisting
	DocBookAttr           role='literate-programming'
End
