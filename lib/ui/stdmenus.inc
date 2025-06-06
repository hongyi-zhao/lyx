# -*- text -*-

# file stdmenus.inc
# This file is part of LyX, the document processor.
# Licence details can be found in the file COPYING.

# author Jean-Marc Lasgouttes
# author John Levon
# author Michael Gerz

# Full author contact details are available in file CREDITS.

# The interface is designed (partially) following the KDE Human Interface
# Guidelines (https://develop.kde.org/hig/)

# Casing Rules:
# Capitalize all words in the element, with the following exceptions:
# * Articles: a, an, the.
# * Conjunctions: and, but, for, not, so, yet ...
# * Prepositions of three or fewer letters: at, for, by, in, to ...
#   (except when the preposition is part of a verb phrase, such as "Check In")
# (https://developer.gnome.org/hig/guidelines/writing-style.html)

Format 5

Menuset

	Menubar
# Make the first 4 menus be as much as expected as possible
		Submenu "File|F" "file"
		Submenu "Edit|E" "edit"
		Submenu "View|V" "view"
		Submenu "Insert|I" "insert"
		Submenu "Navigate|N" "navigate"
		Submenu "Document|D" "document"
		Submenu "Tools|T" "tools"
		Submenu "Help|H" "help"
	End

#
# FILE MENU
#

	Menu "file"
		Item "New|N" "buffer-new"
		Item "New from Template...|m" "lyxfiles-open templates"
		Item "Open...|O" "file-open"
		Submenu "Open Recent|t" "file_lastfiles"
		Item "Open Example...|p" "lyxfiles-open examples"
		Separator
		Item "Close|C" "buffer-close"
		Item "Close All" "buffer-close-all"
		Item "Save|S" "buffer-write"
		Item "Save As...|A" "buffer-write-as"
		Item "Save As Template..." "buffer-write-as-template"
		Item "Save All|l" "buffer-write-all"
		Item "Revert to Saved|R" "buffer-reload"
		Submenu "Version Control|V" "file_vc"
		Separator
		Submenu "Import|I" "file_import"
		Submenu "Export|E" "file_export"
		OptItem "Fax...|F" "buffer-export fax"
		Separator
		Item "New Window|W" "window-new"
		Item "Close Window|d" "window-close"
		Separator
		Item "Exit|x" "lyx-quit"
	End

	Menu "file_lastfiles"
		Lastfiles
	End

	Menu "file_vc"
		OptItem "Register...|R" "vc-register"
		OptItem "Check In Changes...|I" "vc-check-in"
		OptItem "Check Out for Edit|O" "vc-check-out"
		OptItem "Copy|p" "vc-copy"
		OptItem "Rename|R" "vc-rename"
		OptItem "Update Local Directory From Repository|d" "vc-repo-update"
		OptItem "Revert to Repository Version|v" "vc-revert"
		OptItem "Undo Last Check In|U" "vc-undo-last"
		OptItem "Compare with Older Revision...|C" "vc-compare"
		OptItem "Show History...|H" "dialog-show vclog"
		OptItem "Use Locking Property|L" "vc-locking-toggle"
	End

	Menu "file_import"
		ImportFormats
	End

	Menu "file_export"
		ExportFormat
		Item "Export As...|s" "buffer-export-as"
		Separator
		ExportFormats
		Item "More Formats & Options...|r" "buffer-export custom"
	End

#
# EDIT MENU
#

	Menu "edit"
		Item "Undo|U" "undo"
		Item "Redo|R" "redo"
		Separator
		Item "Cut" "cut"
		Item "Copy" "copy"
		Item "Paste" "paste"
		Submenu "Paste Recent|e" "edit_pasterecent"
		Submenu "Paste Special" "edit_paste"
		Separator
		Item "Select Whole Inset" "inset-select-all"
		Item "Select All" "command-sequence buffer-begin ; buffer-end-select"
		Separator
		Item "Find & Replace (Quick)...|F" "dialog-show findreplace"
		Item "Find & Replace (Advanced)..." "dialog-show findreplaceadv"
		Separator
		Item "Move Paragraph Up|h" "paragraph-move-up"
		Item "Move Paragraph Down|v" "paragraph-move-down"
		Separator
		Item "Paragraph Settings...|P" "layout-paragraph"
		Submenu "Text Properties|x" "edit_textprops"
		OptSubmenu "Custom Text Styles|S" "edit_textstyles"
		Item "Manage Counter Values..." "dialog-show-new-inset counter"
		LanguageSelector
		Separator
# Mathed b0rkage means these don't work properly
		OptSubmenu "Table|T" "edit_tabular"
		OptSubmenu "Math|M" "edit_math"
		OptSubmenu "Rows & Columns|C" "edit_tabular_features"
		Separator
# This is where the context-specific stuff is supposed to go.
# The idea is that everything on here can disappear and should
# only appear when relevant (getStatus). Only stuff that needs
# a right-click to bring up the dialog should appear here (it is not
# obvious what the context is for the others)
		OptItem "Increase List Depth|I" "depth-increment"
		OptItem "Decrease List Depth|D" "depth-decrement"
		Separator
		OptItem "Dissolve Inset" "inset-dissolve"
		OptItem "TeX Code Settings...|C" "inset-settings ert"
# 'a' shortcut to match Insert entry, shouldn't clash with Table Settings
		OptItem "Float Settings...|a" "inset-settings float"
		OptItem "Text Wrap Settings...|W" "inset-settings wrap"
		OptItem "Note Settings...|N" "inset-settings note"
		OptItem "Phantom Settings...|h" "inset-settings phantom"
		OptItem "Branch Settings...|B" "inset-settings branch"
		OptItem "Box Settings...|S" "inset-settings box"
		OptItem "Index Entry Settings...|y" "inset-settings index"
		OptItem "Index Settings...|S" "inset-settings index_print"
		OptItem "Info Settings...|n" "inset-settings info"
		OptItem "Listings Settings...|g" "inset-settings listings"
# Hey, guess what's broken ? Surprise surprise, it's tabular stuff
# This is in the Table submenu instead for now.
#	OptItem "Table Settings...|a" "inset-settings tabular"
		OptItem "Table Settings...|a" "layout-tabular"
	End

	Menu "edit_paste"
		OptItem "Paste from HTML|H" "paste html"
		OptItem "Paste from LaTeX|L" "paste latex"
		OptItem "Paste as LinkBack PDF" "paste linkback"
		OptItem "Paste as PDF" "paste pdf"
		OptItem "Paste as PNG" "paste png"
		OptItem "Paste as JPEG" "paste jpeg"
		OptItem "Paste as EMF" "paste emf"
		Separator
		Item "Plain Text|T" "clipboard-paste"
		Item "Plain Text, Join Lines|J" "clipboard-paste paragraph"
		Separator
		Item "Selection|S" "primary-selection-paste"
		Item "Selection, Join Lines|i" "primary-selection-paste paragraph"
# clipboard-paste-simple is commented because it is irritating (bug 8749)
# until somebody can explain why it is needed in addition to clipboard-paste
#		Separator
#		Item "Unformatted Text|U" "clipboard-paste-simple"
#		Item "Unformatted, Join Lines|o" "clipboard-paste-simple paragraph"
	End

	Menu "edit_pasterecent"
		PasteRecent
	End

	Menu "edit_textprops"
		Item "Customize...|C" "dialog-show character"
		Item "Apply Last Settings|A" "textstyle-apply"
		Separator
		Item "Capitalize|p" "word-capitalize"
		Item "Uppercase|U" "word-upcase"
		Item "Lowercase|L" "word-lowcase"
		Item "Invert Case|I" "word-invertcase"
	End

	Menu "edit_textstyles"
		OptItem "Dissolve Text Style" "inset-dissolve charstyle"
		CharStyles
	End

# not much we can do to help here
	Menu "edit_tabular"
		Item "Multi-page Table|g" "tabular-feature toggle-longtabular"
		Item "Formal Style|F" "tabular-feature toggle-booktabs"
		Separator
		Item "Multicolumn|M" "tabular-feature multicolumn"
		Item "Multirow|u" "tabular-feature multirow"
		Separator
		Item "Top Line|T" "tabular-feature toggle-line-top"
		Item "Bottom Line|B" "tabular-feature toggle-line-bottom"
		Item "Left Line|L" "tabular-feature toggle-line-left"
		Item "Right Line|R" "tabular-feature toggle-line-right"
		Separator
		Item "Top|p" "tabular-feature tabular-valign-top"
		Item "Middle|i" "tabular-feature tabular-valign-middle"
		Item "Bottom|o" "tabular-feature tabular-valign-bottom"
	End

	Menu "edit_tabular_features"
		Item "Left|L" "tabular-feature align-left"
		Item "Center|C" "tabular-feature align-center"
		Item "Right|R" "tabular-feature align-right"
		Separator
		Item "Top|T" "tabular-feature valign-top"
		Item "Middle|M" "tabular-feature valign-middle"
		Item "Bottom|B" "tabular-feature valign-bottom"
		Separator
		Item "Add Row|A" "tabular-feature append-row"
		Item "Delete Row|D" "tabular-feature delete-row"
		Item "Copy Row|o" "tabular-feature copy-row"
		Item "Move Row Up" "tabular-feature move-row-up"
		Item "Move Row Down" "tabular-feature move-row-down"
		# Item "Swap Rows|S" "tabular-feature swap-row" # currently broken
		Separator
		Item "Add Column|u" "tabular-feature append-column"
		Item "Delete Column|e" "tabular-feature delete-column"
		Item "Copy Column|p" "tabular-feature copy-column"
		Item "Move Column Right|v" "tabular-feature move-column-right"
		Item "Move Column Left" "tabular-feature move-column-left"
		# Item "Swap Columns|w" "tabular-feature swap-column" # currently broken
	End

	Menu "edit_math"
		Item "Number Whole Formula|N" "math-number-toggle"
		Item "Number This Line|u" "math-number-line-toggle"
		Submenu "Change Limits Type|L" "edit_math_limits"
		Submenu "Macro Definition" "edit_math_macro_definition"
		Separator
		Submenu "Change Formula Type|F" "edit_math_mutate"
		Separator
		Submenu "Text Properties|T" "edit_math_fontstyles"
		Separator
		Submenu "Use Computer Algebra System|S" "edit_math_extern"
		Separator
		Item "Split Cell|C" "cell-split"
		Separator
		Item "Add Line Above|A" "tabular-feature add-hline-above"
		Item "Add Line Below|B" "tabular-feature add-hline-below"
		Item "Delete Line Above|D" "tabular-feature delete-hline-above"
		Item "Delete Line Below|e" "tabular-feature delete-hline-below"
		Separator
		Item "Add Line to Left" "tabular-feature add-vline-left"
		Item "Add Line to Right" "tabular-feature add-vline-right"
		Item "Delete Line to Left" "tabular-feature delete-vline-left"
		Item "Delete Line to Right" "tabular-feature delete-vline-right"
	End

	Menu "edit_math_macro_definition"
		Item "Append Argument" "math-macro-add-param"
		Item "Remove Last Argument" "math-macro-remove-param"
		Separator
		Item "Transform First Non-Optional to Optional Argument" "math-macro-make-optional"
		Item "Transform Last Optional to Non-Optional Argument" "math-macro-make-nonoptional"
		Item "Insert Optional Argument" "math-macro-add-optional-param"
		Item "Remove Optional Argument" "math-macro-remove-optional-param"
		Separator
		Item "Append Argument Eating From the Right" "math-macro-append-greedy-param"
		Item "Append Optional Argument Eating From the Right" "math-macro-add-greedy-optional-param"
		Item "Remove Last Argument Spitting Out to the Right" "math-macro-remove-greedy-param"
	End

	Menu "edit_math_limits"
		 Item "Default|t"  "math-limits empty"
		 Item "Display|D"  "math-limits limits"
		 Item "Inline|I"  "math-limits nolimits"
	End

	Menu "edit_math_fontstyles"
		Item "Math Normal Font|N" "math-font-style mathnormal"
		Separator
		Item "Math Calligraphic Family|C" "math-font-style mathcal"
		Item "Math Formal Script Family|o" "math-font-style mathscr"
		Item "Math Fraktur Family|F" "math-font-style mathfrak"
		Item "Math Roman Family|R" "math-font-style mathrm"
		Item "Math Sans Serif Family|S" "math-font-style mathsf"
		Separator
		Item "Math Bold Series|B" "math-font-style mathbf"
		Separator
		Item "Text Normal Font|T" "math-font-style textnormal"
		Separator
		Item "Text Roman Family" "math-font-style textrm"
		Item "Text Sans Serif Family" "math-font-style textsf"
		Item "Text Typewriter Family" "math-font-style texttt"
		Separator
		Item "Text Bold Series" "math-font-style textbf"
		Item "Text Medium Series" "math-font-style textmd"
		Separator
		Item "Text Italic Shape" "math-font-style textit"
		Item "Text Small Caps Shape" "math-font-style textsc"
		Item "Text Slanted Shape" "math-font-style textsl"
		Item "Text Upright Shape" "math-font-style textup"
	End

	Menu "edit_math_extern"
		Item "Octave|O" "math-extern octave"
		Item "Maxima|M" "math-extern maxima"
		Item "Mathematica|a" "math-extern mathematica"
		Separator
		Item "Maple, Simplify|S" "math-extern maple simplify"
		Item "Maple, Factor|F" "math-extern maple factor"
		Item "Maple, Evalm|E" "math-extern maple evalm"
		Item "Maple, Evalf|v" "math-extern maple evalf"
	End

	Menu "edit_math_mutate"
		Item "Inline Formula|I" "math-mutate simple"
		Item "Displayed Formula|D" "math-mutate equation"
		Item "Eqnarray Environment|E" "math-mutate eqnarray"
		Item "AMS align Environment|a" "math-mutate align"
		Item "AMS alignat Environment|t" "math-mutate alignat"
		Item "AMS flalign Environment|f" "math-mutate flalign"
		Item "AMS gather Environment|g" "math-mutate gather"
		Item "AMS multline Environment|m" "math-mutate multline"
	End

#
# VIEW MENU
#

	Menu "view"
		Item "Outline Pane|O" "dialog-toggle toc"
		Item "Code Preview Pane|P" "dialog-toggle view-source"
		Item "Messages Pane|M" "dialog-toggle progress"
		Submenu "Toolbars|T" "toolbars"
		Separator
		OptItem "Unfold Math Macro|n" "math-macro-unfold"
		OptItem "Fold Math Macro|d" "math-macro-fold"
		Separator
		Item "Split View Into Left and Right Half|L" "view-split horizontal"
		Item "Split View Into Upper and Lower Half|U" "view-split vertical"
		OptItem "Close Current View|w" "tab-group-close"
		Item "Fullscreen|F" "ui-toggle fullscreen"
		Separator
		Item "Open All Insets|I" "inset-forall * inset-toggle open"
		Item "Close All Insets|C" "inset-forall * inset-toggle close"
		Separator
		Documents
	End

	Menu "toolbars"
		Toolbars
		Separator
		Item "Lock Toolbars|L" "toolbar-movable *"
		Separator
		Item "Small-sized Icons" "icon-size small"
		Item "Normal-sized Icons" "icon-size normal"
		Item "Big-sized Icons" "icon-size big"
		Item "Huge-sized Icons" "icon-size huge"
		Item "Giant-sized Icons" "icon-size giant"
	End
#
# INSERT MENU
#
	Menu "insert"
# Try and group things a little better, and percolate the most
# commonly used to good positions (exception being mathed, but
# most people using mathed learn key shortcuts quite quickly anyway)
		Submenu "Math|h" "insert_math"
		Submenu "Special Character|p" "insert_special"
		Submenu "Formatting|o" "insert_formatting"
		Submenu "Field|i" "insert_fields"
		Submenu "List/Contents/References|/" "insert_toc"
		Submenu "Float|a" "insert_float"
		Submenu "Note|N" "insert_note"
		Submenu "Branch|B" "insert_branches"
		Submenu "Custom Inset" "insert_custom"
		Submenu "File|e" "insert_file"
		Submenu "Box[[Menu]]|x" "insert_box"
		OptSubmenu "Regular Expression" "context-edit-regexp"
		Separator
		Item "Citation...|C" "dialog-show-new-inset citation"
		Item "Cross-Reference...|R" "dialog-show-new-inset ref"
		Item "Label...|L" "label-insert"
		Captions
		Indices
		OptSubmenu "Index Properties" "index_properties"
		Item "Nomenclature Entry|y" "nomencl-insert"
		Separator
		Item "Table...|T" "tabular-insert"
		Item "Graphics...|G" "dialog-show-new-inset graphics"
		Item "URL|U" "flex-insert URL"
		Item "Hyperlink...|k" "href-insert"
		Item "Footnote|F" "footnote-insert"
		Item "Marginal Note|M" "marginalnote-insert"
		Item "Program Listing[[Menu]]" "listing-insert"
		Separator
		EnvironmentSeparators
		Separator
		Arguments
		Separator
		Item "TeX Code" "ert-insert"
		Item "Preview|w" "preview-insert"
	End

	Menu "insert_special"
		Item "Symbols...|b" "dialog-show symbols"
		Item "Ellipsis|i" "specialchar-insert dots"
		Item "End of Sentence|E" "specialchar-insert end-of-sentence"
		Item "Plain Double Quotation Mark|Q" "quote-insert outer auto plain"
		Item "Plain Single Quotation Mark|S" "quote-insert inner auto plain"
		Item "Inner Quotation Mark|n" "quote-insert inner"
		Item "Non-Breaking Hyphen|y" "specialchar-insert nobreakdash"
		Item "Breakable Slash|a" "specialchar-insert slash"
		Item "Visible Space|V" "space-insert visible"
		Item "Menu Separator|M" "specialchar-insert menu-separator"
		Item "Phonetic Symbols|P" "ipa-insert"
		Submenu "Logos|L" "insert_logos"
	End

	Menu "insert_fields"
		Item "Date (Current)|D" "info-insert date"
		Item "Date (Last Modification of Document)|L" "info-insert moddate"
		Item "Date (Fixed)|F" "info-insert fixdate"
		Separator
		Item "Time (Current)|T" "info-insert time"
		Item "Time (Last Modification of Document)|M" "info-insert modtime"
		Item "Time (Fixed)|x" "info-insert fixtime"
		Separator
		Item "File Name (Excl. Extension)|N" "info-insert buffer name-noext"
		OptItem "Version Control Revision|V" "info-insert vcs revision"
		Item "User Name|U" "info-insert lyxrc user_name"
		Item "User Email|E" "info-insert lyxrc user_email"
		Separator
		Item "Other...|O" "info-insert"
	End

	Menu "insert_logos"
		Item "LyX Logo|L" "specialchar-insert lyx"
		Item "TeX Logo|T" "specialchar-insert tex"
		Item "LaTeX Logo|a" "specialchar-insert latex"
		Item "LaTeX2e Logo|e" "specialchar-insert latex2e"
	End

	Menu "insert_formatting"
		Item "Superscript|S" "script-insert superscript"
		Item "Subscript|u" "script-insert subscript"
		Separator
		Item "Normal Space|e" "space-insert normal"
		Item "Non-breaking Normal Space|p" "space-insert protected"
		Item "Non-breaking Thin Space|T" "space-insert thin"
		Item "Horizontal Space...|o" "command-alternatives dialog-show-new-inset space;dialog-show-new-inset mathspace"
		Item "Horizontal Line...|L" "dialog-show-new-inset line"
		Item "Vertical Space...|V" "dialog-show-new-inset vspace"
		Submenu "Phantom|m" "insert_phantom"
		Separator
		Item "Hyphenation Point|H" "specialchar-insert hyphenation"
		Item "Ligature Break|k" "specialchar-insert ligature-break"
		Item "Optional Line Break|B" "specialchar-insert allowbreak"
		Item "Ragged Line Break|R" "newline-insert newline"
		Item "Justified Line Break|J" "newline-insert linebreak"
		Separator
		Item "New Page|N" "newpage-insert newpage"
		Item "Page Break|a" "newpage-insert pagebreak"
		Item "Prevent Page Break|g" "newpage-insert nopagebreak"
		Item "Clear Page|C" "newpage-insert clearpage"
		Item "Clear Double Page|D" "newpage-insert cleardoublepage"
	End

	Menu "insert_math"
# Always use "math-mode on" instead of "math-display" etc. because this disables the commands in math mode
		Item "Inline Formula|I" "math-mode on"
		Item "Display Formula|D" "command-sequence math-mode on; math-mutate equation"
		Item "Numbered Formula|N" "command-sequence math-mode on; math-mutate equation; math-number-toggle;"
		Item "Eqnarray Environment|E" "command-sequence math-mode on; math-mutate eqnarray;"
		Item "AMS align Environment|a" "command-sequence math-mode on; math-mutate align;"
		Item "AMS alignat Environment|t" "command-sequence math-mode on; math-mutate alignat;"
		Item "AMS flalign Environment|f" "command-sequence math-mode on; math-mutate flalign;"
		Item "AMS gather Environment|g" "command-sequence math-mode on; math-mutate gather;"
		Item "AMS multline Environment|m" "command-sequence math-mode on; math-mutate multline;"
		Separator
# "math-matrix" and "math-insert" create an inline formula first if called from text mode
		Item "Array Environment|y" "math-matrix 2 2"
		Item "Cases Environment|C" "math-insert \cases"
		Item "Aligned Environment|l" "command-sequence math-insert \aligned; tabular-feature append-column"
		Item "AlignedAt Environment|v" "command-sequence math-insert \alignedat; tabular-feature append-column"
		Item "Gathered Environment|h" "math-insert \gathered"
		Item "Split Environment|S" "command-sequence math-insert \split; tabular-feature append-column"
		Separator
		Item "Delimiters...|r" "dialog-show mathdelimiter"
		Item "Matrix...|x" "dialog-show mathmatrix"
		Item "Macro|o" "math-macro newmacroname newcommand"
	End

	Menu "insert_float"
		FloatInsert
# This has got to be better than "Wrap", anyway
		Item "Wrapped Figure|F" "wrap-insert figure"
		Item "Wrapped Table|T" "wrap-insert table"
	End

	Menu "insert_toc"
		Item "Table of Contents|C" "inset-insert toc"
		FloatListInsert
		IndicesLists
		Item "List of Listings|L" "inset-insert toc CommandInset toc LatexCommand lstlistoflistings \end_inset"
		Item "Nomenclature|N" "nomencl-print"
		Item "Bib(la)TeX Bibliography...|B" "dialog-show-new-inset bibtex"
	End

	Menu "insert_file"
		Item "LyX Document...|X" "file-insert"
		Item "Plain Text...|T" "file-insert-plaintext"
		Item "Plain Text, Join Lines...|J" "file-insert-plaintext-para"
		Separator
		Item "External Material...|M" "dialog-show-new-inset external"
		Item "Child Document...|d" "dialog-show-new-inset include"
	End

	Menu "insert_box"
		Item "Frameless|l" "box-insert Frameless"
		Item "Simple Frame|F" "box-insert Boxed"
		Item "Oval, Thin|a" "box-insert ovalbox"
		Item "Oval, Thick|v" "box-insert Ovalbox"
		Item "Drop Shadow|w" "box-insert Shadowbox"
		Item "Shaded Background|B" "box-insert Shaded"
		Item "Double Frame|u" "box-insert Doublebox"
	End

	Menu "index_properties"
		OptItem "Subentry|b" "indexmacro-insert subentry"
		OptItem "Sortkey|k" "indexmacro-insert sortkey"
		OptItem "See|e" "indexmacro-insert see"
		OptItem "See also|a" "indexmacro-insert seealso"
	End

	Menu "insert_note"
		Item "LyX Note|N" "note-insert Note"
		Item "Comment|C" "note-insert Comment"
		Item "Greyed Out|G" "note-insert Greyedout"
	End

	Menu "insert_branches"
		Branches
		Separator
		Item "Insert New Branch...|I" "branch-add-insert"
	End

	Menu "insert_phantom"
		Item "Phantom|P" "phantom-insert Phantom"
		Item "Horizontal Phantom|H" "phantom-insert HPhantom"
		Item "Vertical Phantom|V" "phantom-insert VPhantom"
	End

	Menu "insert_custom"
		Custom
	End

#
# DOCUMENT MENU
#

	Menu "document"
		OptItem "Cancel Export|P" "export-cancel"
		Separator
		Submenu "Change Tracking|C" "document_change"
		OptItem "Build Program|B" "build-program"
		Item "LaTeX Log|L" "dialog-show latexlog"
		Item "Processing Errors|r" "errors-show"
		Item "Start Appendix Here|x" "appendix"
		Separator
		ViewFormats
		UpdateFormats
		OptItem "View Master Document|M" "master-buffer-view"
		OptItem "Update Master Document|a" "master-buffer-update"
		Separator
		Item "Compressed|o" "buffer-toggle-compression"
		Item "Disable Editing|E" "buffer-toggle-read-only"
		Item "Settings...|S" "dialog-show document"
	End

	Menu "document_change"
		Item "Track Changes|T" "changes-track"
		Item "Merge Changes...|M" "changes-merge"
		Item "Accept Change|A" "change-accept"
		Item "Reject Change|R" "change-reject"
		Item "Accept All Changes|c" "all-changes-accept"
		Item "Reject All Changes|e" "all-changes-reject"
		Item "Accept All Changes (incl. relatives)|p" "ifrelatives master-buffer-forall all-changes-accept"
		Item "Reject All Changes (incl. relatives)|j" "ifrelatives master-buffer-forall all-changes-reject"
		Item "Show Changes in Output|S" "changes-output"
	End

#
# NAVIGATE MENU
#
	Menu "navigate"
		Submenu "Bookmarks|B" "navigate_bookmarks"
		Item "Next Error|E" "error-next"
		Item "Next Note|N" "note-next"
		Item "Next Change|C" "change-next"
		Item "Next Cross-Reference|R" "reference-next anyref"
		OptItem "Go to Label|L" "label-goto"
		Item "Forward Search|F" "forward-search"
		Separator
		Toc
	End

	Menu "navigate_bookmarks"
		Item "Save Bookmark 1|S" "bookmark-save 1"
		Item "Save Bookmark 2" "bookmark-save 2"
		Item "Save Bookmark 3" "bookmark-save 3"
		Item "Save Bookmark 4" "bookmark-save 4"
		Item "Save Bookmark 5" "bookmark-save 5"
		Item "Clear Bookmarks|C" "bookmark-clear"
		Separator
		Item "Navigate Back|B" "bookmark-goto 0"
		Bookmarks
	End

#
# TOOLS MENU
#

	Menu "tools"
		Item "Spellchecker...|S" "dialog-show spellchecker"
		OptItem "Thesaurus...|T" "thesaurus-entry"
		Item "Statistics...|a" "statistics"
		Item "Check TeX|h" "buffer-chktex"
		Item "TeX Information|I" "dialog-show texinfo"
		Item "Compare...|C" "dialog-show compare"
		Separator
# A LOT of applications have Tools->Prefs. Remember this
# should be rarely used - Edit menu is not a good place to
# have it.
		Item "Reconfigure|R" "reconfigure"
		Item "Preferences...|P" "dialog-show prefs"
	End

#
# HELP MENU
#
	Menu "help"
		Item "Introduction|I" "help-open Intro"
		Item "Tutorial|T" "help-open Tutorial"
		Item "User's Guide|U" "help-open UserGuide"
		Item "Additional Features|F" "help-open Additional"
		Item "Embedded Objects|O" "help-open EmbeddedObjects"
		Item "Math|M" "help-open Math"
		Item "Customization|C" "help-open Customization"
		Item "Shortcuts|S" "help-open Shortcuts"
		Item "LyX Functions|y" "help-open LFUNs"
		Item "LaTeX Configuration|L" "help-open LaTeXConfig"
		Submenu "Specific Manuals|p" "examples"
		Separator
		Item "About LyX|X" "dialog-show aboutlyx"
	End

	Menu "examples"
		Item "Beamer Presentations|B" "help-open Presentations/Beamer"
		Item "Braille|a" "help-open Modules/Braille"
		Item "Colored boxes|r" "help-open Modules/Fancy_Colored_Boxes"
		Item "Feynman-diagram|F" "help-open Graphics_and_Insets/Feynman_Diagrams"
		Item "Knitr|K" "help-open Modules/Rnw_%28knitr%29"
		Item "LilyPond|P" "help-open Modules/LilyPond_Book"
		Item "Linguistics|L" "help-open Modules/Linguistics"
		Item "Multilingual Captions|C" "help-open Modules/Multilingual_Captions"
		Item "Paralist|t" "help-open Modules/Paragraph_Lists_%28paralist%29"
		Item "PDF comments|D" "help-open Modules/PDF_Comments"
		Item "PDF forms|o" "help-open Modules/PDF_Form"
		Item "Hazard and Precautionary Statements|H" "help-open Modules/Hazard_and_Precautionary_Statements"
		Item "Sweave|S" "help-open Modules/Sweave"
		Item "XY-pic|X" "help-open Graphics_and_Insets/XY-Pic"
	End


End

