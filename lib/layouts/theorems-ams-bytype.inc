# Original Author : David L. Johnson <dlj0@lehigh.edu>
# Probably broken by Jean-Marc Lasgouttes <Jean-Marc.Lasgouttes@inria.fr>
# modified and modularized by Emmanuel GUREGHIAN <gureghia@boston.bertin.fr>
# Tinkered with Sep. '07 by Paul Rubin <rubin@msu.edu>
# Modularized Jan 08 by Richard Kimberly Heck <rikiheck@lyx.org>
# Hacked June '09 by Paul Rubin <rubin@msu.edu> to use separate counters

# The environments defined are :
# - Theorem
# - Corollary
# - Lemma
# - Proposition
# - Conjecture
# - Fact
# - Definition
# - Example
# - Problem
# - Exercise
# - Solution
# - Remark
# - Claim
# - Case (by inclusion)

Format 108

Input theorems-counters-bytype.inc

OutlinerName thm "Definitions & Theorems"

Style Theorem
	Category              Reasoning
	Margin                First_Dynamic
	LatexType             Environment
	LatexName             thm
	NextNoIndent          1
	ResetArgs             1
	AddToToc              thm
	IsTocCaption          1
	Argument 1
		LabelString    "Additional Theorem Text"
		Tooltip        "Additional text appended to the theorem header"
		IsTocCaption      1
	EndArgument
	LabelSep              xx
	ParIndent             MMM
	ParSkip               0.4
	ItemSep               0.2
	TopSep                0.7
	BottomSep             0.7
	ParSep                0.3
	Align                 Block
	AlignPossible         Left
	LabelType             Static
	LabelCounter          theorem
	LabelString           "Theorem \thetheorem."
	Font
	  Shape               Italic
	  Size                Normal
	EndFont
	LabelFont
	  Shape               Up
	  Series              Bold
	EndFont
	TheoremName           thm
	TheoremLaTeXName      theoremname
	TheoremZRefName       theorem
	TheoremCrossRefName   Theorem
	TheoremCrossRefPluralName Theorems
	LangPreamble
		\providecommand{\theoremname}{_(Theorem)}
	EndLangPreamble
	BabelPreamble
		\addto\captions$$lang{\renewcommand{\theoremname}{_(Theorem)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='theorem'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Corollary
	CopyStyle             Theorem
	LatexName             cor
	LabelString           "Corollary \thecorollary."
	LabelCounter          corollary
	TheoremName           cor
	TheoremLaTeXName      corollaryname
	TheoremZRefName       corollary
	TheoremCrossRefName   Corollary
	TheoremCrossRefPluralName Corollaries
	LangPreamble
		\providecommand{\corollaryname}{_(Corollary)}
	EndLangPreamble
	BabelPreamble
		\addto\captions$$lang{\renewcommand{\corollaryname}{_(Corollary)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='corollary'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Lemma
	CopyStyle             Theorem
	LatexName             lem
	LabelString           "Lemma \thelemma."
	TheoremName           lem
	TheoremLaTeXName      lemmaname
	TheoremZRefName       lemma
	TheoremCrossRefName   Lemma
	TheoremCrossRefPluralName Lemmas
	LangPreamble
	  \providecommand{\lemmaname}{_(Lemma)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\lemmaname}{_(Lemma)}}
	EndBabelPreamble
	LabelCounter          lemma
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='lemma'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Proposition
	CopyStyle             Theorem
	LatexName             prop
	LabelString           "Proposition \theproposition."
	TheoremName           prop
	TheoremLaTeXName      propositionname
	TheoremZRefName       proposition
	TheoremCrossRefName   Proposition
	TheoremCrossRefPluralName Propositions
	LangPreamble
	  \providecommand{\propositionname}{_(Proposition)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\propositionname}{_(Proposition)}}
	EndBabelPreamble
	LabelCounter          proposition
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='proposition'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Conjecture
	CopyStyle             Theorem
	LatexName             conjecture
	LabelString           "Conjecture \theconjecture."
	TheoremName           conjecture
	TheoremLaTeXName      conjecturename
	TheoremCrossRefName   Conjecture
	TheoremCrossRefPluralName Conjectures
	TheoremZRefName       none
	LangPreamble
	  \providecommand{\conjecturename}{_(Conjecture)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\conjecturename}{_(Conjecture)}}
	EndBabelPreamble
	LabelCounter          conjecture
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='conjecture'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Fact
	CopyStyle             Theorem
	LatexName             fact
	LabelString           "Fact \thefact."
	TheoremName           fact
	TheoremLaTeXName      factname
	TheoremCrossRefName   Fact
	TheoremCrossRefPluralName Facts
	TheoremZRefName       none
	LangPreamble
	  \providecommand{\factname}{_(Fact)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\factname}{_(Fact)}}
	EndBabelPreamble
	LabelCounter          fact
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='fact'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Definition
	CopyStyle             Theorem
	LatexName             defn
	LabelString           "Definition \thedefinition."
	Font
	  Shape               Up
	EndFont
	LabelFont
	  Shape               Up
	  Series              Bold
	EndFont
	TheoremName           defn
	TheoremStyle          definition
	TheoremLaTeXName      definitionname
	TheoremZRefName       definition
	TheoremCrossRefName   Definition
	TheoremCrossRefPluralName Definitions
	LangPreamble
	  \providecommand{\definitionname}{_(Definition)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\definitionname}{_(Definition)}}
	EndBabelPreamble
	LabelCounter          definition
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='definition'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Example
	CopyStyle             Definition
	LatexName             example
	LabelString           "Example \theexample."
	TheoremName           example
	TheoremStyle          definition
	TheoremLaTeXName      examplename
	TheoremCrossRefName   Example
	TheoremCrossRefPluralName Examples
	LangPreamble
	  \providecommand{\examplename}{_(Example)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\examplename}{_(Example)}}
	EndBabelPreamble
	LabelCounter          example
	DocBookWrapperTag     informalexample
	DocBookTag            para
	DocBookAttr           ""
End


Style Problem
	CopyStyle             Definition
	LatexName             problem
	LabelString           "Problem \theproblem."
	TheoremName           problem
	TheoremStyle          definition
	TheoremLaTeXName      problemname
	TheoremCrossRefName   Problem
	TheoremCrossRefPluralName Problems
	TheoremZRefName       none
	LangPreamble
	  \providecommand{\problemname}{_(Problem)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\problemname}{_(Problem)}}
	EndBabelPreamble
	LabelCounter          problem
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='problem'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Exercise
	CopyStyle             Definition
	LatexName             xca
	LabelString           "Exercise \theexercise."
	TheoremName           xca
	TheoremStyle          definition
	TheoremLaTeXName      exercisename
	TheoremZRefName       exercise
	TheoremCrossRefName   Exercise
	TheoremCrossRefPluralName Exercises
	LangPreamble
	  \providecommand{\exercisename}{_(Exercise)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\exercisename}{_(Exercise)}}
	EndBabelPreamble
	LabelCounter          exercise
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='exercise'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Solution
	CopyStyle             Definition
	LatexName             sol
	LabelString           "Solution \thesolution."
	TheoremName           sol
	TheoremStyle          definition
	TheoremLaTeXName      solutionname
	TheoremZRefName       solution
	TheoremCrossRefName   Solution
	TheoremCrossRefPluralName Solutions
	LangPreamble
		\providecommand{\solutionname}{_(Solution)}
	EndLangPreamble
	BabelPreamble
		\addto\captions$$lang{\renewcommand{\solutionname}{_(Solution)}}
	EndBabelPreamble
	LabelCounter          solution
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='solution'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Remark
	CopyStyle             Theorem
	LatexName             rem
	LabelString           "Remark \theremark."
	Font
	  Shape               Up
	  Size                Normal
	EndFont
	LabelFont
	  Series              Medium
	  Shape               Italic
	EndFont
	TheoremName           rem
	TheoremStyle          remark
	TheoremLaTeXName      remarkname
	TheoremZRefName       remark
	TheoremCrossRefName   Remark
	TheoremCrossRefPluralName Remarks
	LangPreamble
	  \providecommand{\remarkname}{_(Remark)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\remarkname}{_(Remark)}}
	EndBabelPreamble
	LabelCounter          remark
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='remark'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Claim
	CopyStyle             Remark
	LatexName             claim
	LabelString           "Claim \theclaim."
	TheoremName           claim
	TheoremStyle          remark
	TheoremLaTeXName      claimname
	TheoremCrossRefName   Claim
	TheoremCrossRefPluralName Claims
	TheoremZRefName       none
	LangPreamble
	  \providecommand{\claimname}{_(Claim)}
	EndLangPreamble
	BabelPreamble
	  \addto\captions$$lang{\renewcommand{\claimname}{_(Claim)}}
	EndBabelPreamble
	LabelCounter          claim
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='claim'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End

Input theorems-case.inc
Input theorems-refprefix.inc
