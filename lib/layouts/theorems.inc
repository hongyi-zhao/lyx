# Original Author : David L. Johnson <dlj0@lehigh.edu>
# Probably broken by Jean-Marc Lasgouttes <Jean-Marc.Lasgouttes@inria.fr>
# modified and modularized by Emmanuel GUREGHIAN <gureghia@boston.bertin.fr>
# Tinkered with Sep. '07 by Paul Rubin <rubin@msu.edu>
# Modularized Jan 08 by Richard Kimberly Heck <rikiheck@lyx.org>

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

Format 107

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
	Preamble
		\theoremstyle{plain}
		\newtheorem{thm}{\protect\theoremname}
	EndPreamble
	LangPreamble
		\providecommand{\theoremname}{_(Theorem)}
	EndLangPreamble
	BabelPreamble
		\addto\captions$$lang{\renewcommand{\theoremname}{_(Theorem)}}
	EndBabelPreamble
	Requires              amsthm
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='theorem'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Corollary
	CopyStyle             Theorem
	DependsOn             Theorem
	LatexName             cor
	LabelString           "Corollary \thetheorem."
	Preamble
		\theoremstyle{plain}
		\newtheorem{cor}[thm]{\protect\corollaryname}
	EndPreamble
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
	DependsOn             Theorem
	LatexName             lem
	LabelString           "Lemma \thetheorem."
	Preamble
		\theoremstyle{plain}
		\newtheorem{lem}[thm]{\protect\lemmaname}
	EndPreamble
	LangPreamble
		\providecommand{\lemmaname}{_(Lemma)}
	EndLangPreamble
	BabelPreamble
		\addto\captions$$lang{\renewcommand{\lemmaname}{_(Lemma)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='lemma'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Proposition
	CopyStyle             Theorem
	DependsOn             Theorem
	LatexName             prop
	LabelString           "Proposition \thetheorem."
	Preamble
		\theoremstyle{plain}
		\newtheorem{prop}[thm]{\protect\propositionname}
	EndPreamble
	LangPreamble
		\providecommand{\propositionname}{_(Proposition)}
	EndLangPreamble
	BabelPreamble
		\addto\captions$$lang{\renewcommand{\propositionname}{_(Proposition)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='proposition'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Conjecture
	CopyStyle             Theorem
	DependsOn             Theorem
	LatexName             conjecture
	LabelString           "Conjecture \thetheorem."
	Preamble
		\theoremstyle{plain}
		\newtheorem{conjecture}[thm]{\protect\conjecturename}
	EndPreamble
	LangPreamble
		\providecommand{\conjecturename}{_(Conjecture)}
	EndLangPreamble
	BabelPreamble
		\addto\captions$$lang{\renewcommand{\conjecturename}{_(Conjecture)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='conjecture'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Fact
	CopyStyle             Theorem
	DependsOn             Theorem
	LatexName             fact
	LabelString           "Fact \thetheorem."
	Preamble
		\theoremstyle{plain}
		\newtheorem{fact}[thm]{\protect\factname}
	EndPreamble
	LangPreamble
		\providecommand{\factname}{_(Fact)}
	EndLangPreamble
	BabelPreamble
		\addto\captions$$lang{\renewcommand{\factname}{_(Fact)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='fact'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Definition
	CopyStyle             Theorem
	DependsOn             Theorem
	LatexName             defn
	LabelString           "Definition \thetheorem."
	Font
	  Shape               Up
	EndFont
	LabelFont
	  Shape               Up
	  Series              Bold
	EndFont
	Preamble
		\theoremstyle{definition}
		\newtheorem{defn}[thm]{\protect\definitionname}
	EndPreamble
	LangPreamble
		\providecommand{\definitionname}{_(Definition)}
	EndLangPreamble
	BabelPreamble
		\addto\captions$$lang{\renewcommand{\definitionname}{_(Definition)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='definition'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Example
	CopyStyle             Definition
	LatexName             example
	LabelString           "Example \thetheorem."
	Preamble
		\theoremstyle{definition}
		\newtheorem{example}[thm]{\protect\examplename}
	EndPreamble
	LangPreamble
		\providecommand{\examplename}{_(Example)}
	EndLangPreamble
	BabelPreamble
		\addto\captions$$lang{\renewcommand{\examplename}{_(Example)}}
	EndBabelPreamble
	DocBookWrapperTag     example
	DocBookTag            para
	DocBookAttr           ""
End


Style Problem
	CopyStyle             Definition
	LatexName             problem
	LabelString           "Problem \thetheorem."
	Preamble
		\theoremstyle{definition}
		\newtheorem{problem}[thm]{\protect\problemname}
	EndPreamble
	LangPreamble
		\providecommand{\problemname}{_(Problem)}
	EndLangPreamble
	BabelPreamble
		\addto\captions$$lang{\renewcommand{\problemname}{_(Problem)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='problem'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Exercise
	CopyStyle             Definition
	LatexName             xca
	LabelString           "Exercise \thetheorem."
	Preamble
		\theoremstyle{definition}
		\newtheorem{xca}[thm]{\protect\exercisename}
	EndPreamble
	LangPreamble
		\providecommand{\exercisename}{_(Exercise)}
	EndLangPreamble
	BabelPreamble
		\addto\captions$$lang{\renewcommand{\exercisename}{_(Exercise)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='exercise'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Solution
	CopyStyle             Definition
	LatexName             sol
	LabelString           "Solution \thetheorem."
	Preamble
		\theoremstyle{definition}
		\newtheorem{sol}[thm]{\protect\solutionname}
	EndPreamble
	LangPreamble
		\providecommand{\solutionname}{_(Solution)}
	EndLangPreamble
	BabelPreamble
		\addto\captions$$lang{\renewcommand{\solutionname}{_(Solution)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='solution'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Remark
	CopyStyle             Theorem
	DependsOn             Theorem
	LatexName             rem
	LabelString           "Remark \thetheorem."
	Font
	  Shape               Up
	  Size                Normal
	EndFont
	LabelFont
	  Series              Medium
	  Shape               Italic
	EndFont
	Preamble
		\theoremstyle{remark}
		\newtheorem{rem}[thm]{\protect\remarkname}
	EndPreamble
	LangPreamble
		\providecommand{\remarkname}{_(Remark)}
	EndLangPreamble
	BabelPreamble
		\addto\captions$$lang{\renewcommand{\remarkname}{_(Remark)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='remark'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End


Style Claim
	CopyStyle             Remark
	LatexName             claim
	LabelString           "Claim \thetheorem."
	Preamble
		\theoremstyle{remark}
		\newtheorem{claim}[thm]{\protect\claimname}
	EndPreamble
	LangPreamble
		\providecommand{\claimname}{_(Claim)}
	EndLangPreamble
	BabelPreamble
		\addto\captions$$lang{\renewcommand{\claimname}{_(Claim)}}
	EndBabelPreamble
	DocBookWrapperTag     figure
	DocBookWrapperAttr    role='claim'
	DocBookTag            para
	DocBookGenerateTitle  true
	DocBookWrapperMergeWithPrevious  true
End

Input theorems-case.inc
Input theorems-refprefix.inc
