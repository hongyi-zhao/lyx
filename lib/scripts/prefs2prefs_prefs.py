# file prefs2prefs-prefs.py
# This file is part of LyX, the document processor.
# Licence details can be found in the file COPYING.

# author Richard Kimberly Heck

# Full author contact details are available in file CREDITS

# This file houses conversion information for the preferences file.

# There are two kinds of converter functions.
# 
# Most of them take a line as argument and return a list:
#     (Bool, NewLine),
# where the Bool says if we've modified anything and the NewLine is
# the new line, if so, which will be used to replace the old line.
# This can be used to erase lines (return (True, "")) or to modify 
# existing preference lines.
# 
# It is also possible for conversion routines to accept the whole
# list of lines and process that. This is useful (as in the change
# to format 35) when you need to add a preference if it's not already
# there.


######################################################################
#
# FORMAT CHANGES
#
######################################################################

# Incremented to format 2, r39670 by jrioux
#   Support for multiple file extensions per format.
#   No conversion necessary.

# Incremented to format 3, r39705 by tommaso
#   Support for file formats that are natively (g)zipped.
#   We must add the flag zipped=native to formats that
#   were previously hardcoded in the C++ source: dia.

# Incremented to format 4, r40028 by vfr
#   Remove support for default paper size.

# Incremented to format 5, r40030 by vfr
#   Add a default length unit.
#   No conversion necessary.

# Incremented to format 6, r40515 by younes
#   Add use_qimage option.
#   No conversion necessary.

# Incremented to format 7, r40789 by gb
#   Add mime type to file format

# Incremented to format 8, 288c1e0f by rgh
#   Add "nice" flag for converters
#   No conversion necessary.

# Incremented to format 9, a18af620 by spitz
#  Remove default_language rc.

# Incremented to format 10, 4985015 by tommaso
#  Add close_buffer_with_last_view in preferences.
#  No conversion necessary.

# Incremented to format 11, by gb
#   Split pdf format into pdf and pdf6

# Incremented to format 12, by vfr
#   Add option to use the system's theme icons
#   No conversion necessary.

# Incremented to format 13, by bh
#   Rename mac_like_word_movement to mac_like_cursor_movement

# Incremented to format 14, by spitz
#   New RC default_otf_view_format
#   No conversion necessary.

# Incremented to format 15, by prannoy
#   Add fullscreen_statusbar
#   No conversion necessary.

# Incremented to format 16, by lasgouttes
#  Remove force_paint_single_char rc.

# Incremented to format 17, by lasgouttes
#  Remove rtl_support rc.

# Incremented to format 18, by ef
#   Add option to allow saving the document directory
#   No conversion necessary.

# Incremented to format 19, by rgh
#   remove print support

# Incremented to format 20, by tommaso
#   Add options to forbid/ignore 'needauth' option
#   No conversion necessary.

# Incremented to format 21, by spitz
#   Add jbibtex_alternatives, allow "automatic" value
#   of bibtex_command and jbibtex_command (actually the
#   default now)
#   No conversion necessary.

# Incremented to format 22, by ef
#   Add pygmentize_command for the python pygments syntax highlighter
#   No conversion necessary.

# Incremented to format 23, by spitz
#   Add default_platex_view_format, a default output format for
#   Japanese documents via pLaTeX.
#   No conversion necessary.

# Incremented to format 24, by spitz
#   Rename collapsable to collapsible

# Incremented to format 25, by lasgouttes
#   Remove use_qimage preference

# Incremented to format 26, by spitz
#   Remove font_encoding preference

# Incremented to format 27, by spitz
#   Add optional flavor value to needaux flag

# Incremented to format 28, by spitz
#   Remove date_insert_format

# Incremented to format 29, by lasgouttes
#   Remove use_pixmap_cache

# Incremented to format 30, by lasgouttes
#   Add respect_os_kbd_language.
#   No convergence necessary.

# Incremented to format 31, by spitz
#   Add ct_additions_underlined.
#   No convergence necessary.

# Incremented to format 32, by spitz
#   Add ct_markup_copied.
#   No convergence necessary.

# Incremented to format 33, by sanda
#   Add \citation_search, \citation_search_pattern
#   and \citation_search_view.
#   No conversion necessary.

# Incremented to format 34, by yuriy
#   Rename *.kmap files for Cyrillic languages

# Incremented to format 35, by spitz
#   \set_color now takes three arguments
#   \set_color lyxname x11hexname x11darkhexname

# Incremented to format 36, by rkh
#   Set spellcheck_continuously to FALSE if it is not otherwise set
#   (the new default is true, so this keeps behavior the same for 
#   existing users)

# Incremented to format 37, by chillenb
#  Remove \fullscreen_width
#  Remove \fullscreen_limit
#  Add \screen_width
#  Add \screen_limit

# Incremented to format 38, by ec
#   Add option to configure ui style
#   No conversion necessary.

# Incremented to format 39
#   Add \color_scheme {system|light|dark}, by spitz
#   Add \ui_theme, by koji
#   No conversion necessary.

# Incremented to format 40
#   Rename \experimental:bookmarks_visibility to \bookmarks_visibility

# NOTE: The format should also be updated in LYXRC.cpp and
# in configure.py (search for lyxrc_fileformat).

import re

###########################################################
#
# Conversion chain

def get_format(line):
	entries = []
	i = 0
	while i < len(line):
		if line[i] == '"':
			beg = i + 1
			i = i + 1
			while i < len(line) and line[i] != '"':
				if line[i] == '\\' and i < len(line) - 1 and line[i+1] == '"':
					# convert \" to "
					i = i + 1
				i = i + 1
			end = i
			entries.append(line[beg:end].replace('\\"', '"'))
		elif line[i] == '#':
			return entries
		elif not line[i].isspace():
			beg = i
			while i < len(line) and not line[i].isspace():
				i = i + 1
			end = i
			entries.append(line[beg:end])
		i = i + 1
	return entries


def simple_renaming(line, old, new):
	i = line.lower().find(old.lower())
	if i == -1:
		return no_match
	line = line[:i] + new + line[i+len(old):]
	return (True, line)

no_match = (False, [])

######################################
### Format 1 conversions (for LyX 2.0)

def remove_obsolete(line):
	tags = ("\\use_tempdir", "\\spell_command", "\\personal_dictionary",
		"\\plaintext_roff_command", "\\use_alt_language",
		"\\use_escape_chars", "\\use_input_encoding",
		"\\use_personal_dictionary", "\\use_pspell",
		"\\use_spell_lib")
	line = line.lower().lstrip()
	for tag in tags:
		if line.lower().startswith(tag):
			return (True, "")
	return no_match


def language_use_babel(line):
	if not line.lower().startswith(r"\language_use_babel"):
		return no_match
	re_lub = re.compile(r'^\\language_use_babel\s+"?(true|false)', re.IGNORECASE)
	m = re_lub.match(line)
	val = m.group(1)
	newval = '0'
	if val == 'false':
		newval = '3'
	newline = "\\language_package_selection " + newval
	return (True, newline)


def language_package(line):
	return simple_renaming(line, "\\language_package", "\\language_custom_package")


lfre = re.compile(r'^\\converter\s+"?(\w+)"?\s+"?(\w+)"?\s+"([^"]*?)"\s+"latex"', re.IGNORECASE)
def latex_flavor(line):
	if not line.lower().startswith("\\converter"):
		return no_match
	m = lfre.match(line)
	if not m:
		return no_match
	conv = m.group(1)
	fmat = m.group(2)
	args = m.group(3)
	conv2fl = {
		   "luatex":   "lualatex",
		   "pplatex":  "latex",
		   "xetex":    "xelatex",
		  }
	if conv in conv2fl.keys():
		flavor = conv2fl[conv]
	else:
		flavor = conv
	if flavor == "latex":
		return no_match
	return (True,
		f"\\converter \"{conv}\" \"{fmat}\" \"{args}\" \"latex={flavor}\"")


emre = re.compile(r'^\\format\s+(.*)\s+"(document[^"]*?)"', re.IGNORECASE)
def export_menu(line):
	if not line.lower().startswith("\\format"):
		return no_match
	m = emre.match(line)
	if not m:
		return no_match
	fmat = m.group(1)
	opts = m.group(2)
	return (True,
		f"\\Format {fmat} \"{opts},menu=export\"")

# End format 1 conversions (for LyX 2.0)
########################################

#################################
# Conversions from LyX 2.0 to 2.1
zipre = re.compile(r'^\\format\s+("?dia"?\s+.*)\s+"([^"]*?)"', re.IGNORECASE)
def zipped_native(line):
	if not line.lower().startswith("\\format"):
		return no_match
	m = zipre.match(line)
	if not m:
		return no_match
	fmat = m.group(1)
	opts = m.group(2)
	return (True,
		f"\\Format {fmat} \"{opts},zipped=native\"")

def remove_default_papersize(line):
	if not line.lower().startswith("\\default_papersize"):
		return no_match
	return (True, "")

def add_mime_types(line):
	if not line.lower().startswith("\\format"):
		return no_match
	entries = get_format(line)
	converted = line
	i = len(entries)
	while i < 7:
		converted = converted + '	""'
		i = i + 1
	formats = {'tgif':'application/x-tgif', \
		'fig':'application/x-xfig', \
		'dia':'application/x-dia-diagram', \
		'odg':'application/vnd.oasis.opendocument.graphics', \
		'svg':'image/svg+xml', \
		'bmp':'image/x-bmp', \
		'gif':'image/gif', \
		'jpg':'image/jpeg', \
		'pbm':'image/x-portable-bitmap', \
		'pgm':'image/x-portable-graymap', \
		'png':'image/x-png', \
		'ppm':'image/x-portable-pixmap', \
		'tiff':'image/tiff', \
		'xbm':'image/x-xbitmap', \
		'xpm':'image/x-xpixmap', \
		'docbook-xml':'application/docbook+xml', \
		'dot':'text/vnd.graphviz', \
		'ly':'text/x-lilypond', \
		'latex':'text/x-tex', \
		'text':'text/plain', \
		'gnumeric':'application/x-gnumeric', \
		'excel':'application/vnd.ms-excel', \
		'oocalc':'application/vnd.oasis.opendocument.spreadsheet', \
		'xhtml':'application/xhtml+xml', \
		'bib':'text/x-bibtex', \
		'eps':'image/x-eps', \
		'ps':'application/postscript', \
		'pdf':'application/pdf', \
		'dvi':'application/x-dvi', \
		'html':'text/html', \
		'odt':'application/vnd.oasis.opendocument.text', \
		'sxw':'application/vnd.sun.xml.writer', \
		'rtf':'application/rtf', \
		'doc':'application/msword', \
		'csv':'text/csv', \
		'lyx':'application/x-lyx', \
		'wmf':'image/x-wmf', \
		'emf':'image/x-emf'}
	if entries[1] in formats.keys():
		converted = converted + '	"' + formats[entries[1]] + '"'
	else:
		converted = converted + '       ""'
	return (True, converted)

re_converter = re.compile(r'^\\converter\s+', re.IGNORECASE)

def split_pdf_format(line):
	# strictly speaking, a new format would not require to bump the
	# version number, but the old pdf format was hardcoded at several
	# places in the C++ code, so an update seemed like a good idea.
	if line.lower().startswith("\\format"):
		entries = get_format(line)
		if entries[1] == 'pdf':
			if len(entries) < 6:
				viewer = ''
			else:
				viewer = entries[5]
			converted = line.replace('application/pdf', '') + r'''
\Format pdf6       pdf    "PDF (graphics)"        "" "''' + viewer + '"	""	"vector"	"application/pdf"'
			return (True, converted)
	elif line.lower().startswith("\\viewer_alternatives") or \
	     line.lower().startswith("\\editor_alternatives"):
		entries = get_format(line)
		if entries[1] == 'pdf':
			converted = line + "\n" + entries[0] + ' pdf6 "' + entries[2] + '"'
			return (True, converted)
	elif re_converter.match(line):
		entries = get_format(line)
		# The only converter from pdf that is touched is pdf->eps:
		# All other converters are likely meant for further processing on export.
		# The only converter to pdf that stays untouched is dvi->pdf:
		# All other converters are likely meant for graphics.
		if len(entries) > 2 and \
		   ((entries[1] == 'pdf' and entries[2] == 'eps') or \
		   (entries[1] != 'ps'  and entries[2] == 'pdf')):
			if entries[1] == 'pdf':
				converted = entries[0] + ' pdf6 ' + entries[2]
			else:
				converted = entries[0] + ' ' + entries[1] + ' pdf6'
			i = 3
			while i < len(entries):
				converted = converted + ' "' + entries[i] + '"'
				i = i + 1
			return (True, converted)
	return no_match

def remove_default_language(line):
	if not line.lower().startswith("\\default_language"):
		return no_match
	return (True, "")

def mac_cursor_movement(line):
	return simple_renaming(line, "\\mac_like_word_movement", "\\mac_like_cursor_movement")

# End conversions for LyX 2.0 to 2.1
####################################


#################################
# Conversions from LyX 2.1 to 2.2

def remove_force_paint_single_char(line):
	if not line.lower().startswith("\\force_paint_single_char"):
		return no_match
	return (True, "")

def remove_rtl(line):
	if not line.lower().startswith("\\rtl "):
		return no_match
	return (True, "")

def remove_print_support(line):
	tags = ("\\printer", "\\print_adapt_output", "\\print_command",
		"\\print_evenpage_flag", "\\print_oddpage_flag", "\\print_pagerange_flag",
		"\\print_copies_flag", "\\print_collcopies_flag", "\\print_reverse_flag",
		"\\print_to_printer", "\\print_to_file", "\\print_file_extension")
	line = line.lower().lstrip()
	for tag in tags:
		if line.lower().startswith(tag):
			return (True, "")
	return no_match

# End conversions for LyX 2.1 to 2.2
####################################


#################################
# Conversions from LyX 2.2 to 2.3

def rename_collapsible(line):
	return simple_renaming(line, "\\set_color \"collapsable", "\\set_color \"collapsible")

# End conversions for LyX 2.2 to 2.3
####################################


#################################
# Conversions from LyX 2.3 to 2.4

def remove_use_qimage(line):
	if not line.lower().startswith("\\use_qimage "):
		return no_match
	return (True, "")

def remove_font_encoding(line):
	if not line.lower().startswith("\\font_encoding "):
		return no_match
	return (True, "")

def remove_date_insert_format(line):
	if not line.lower().startswith("\\date_insert_format "):
		return no_match
	return (True, "")

def remove_use_pixmap_cache(line):
	if not line.lower().startswith("\\use_pixmap_cache "):
		return no_match
	return (True, "")

def rename_cyrillic_kmap_files(line):
	line = line.lower()
	if not (line.startswith("\\kbmap_primary ")
			or line.startswith("\\kbmap_secondary ")):
		return no_match
	line = line.replace('"bg-bds-1251"', '"bulgarian"')
	line = line.replace('"koi8-r"', '"russian"')
	line = line.replace('"koi8-u"', '"ukrainian"')
	return (True, line)

def add_dark_color(line):
	if not line.lower().startswith("\\set_color "):
		return no_match
	colre = re.compile(r'^\\set_color\s+("[^"]+")\s+("[^"]+")\s*$', re.IGNORECASE)
	m = colre.match(line)
	if not m:
		return no_match
	line += " " + m.group(2)
	return (True, line)

def add_spellcheck_default(lines):
	for l in lines:
		if l.startswith("\\spellcheck_continuously"):
			return
	lines.append("\\spellcheck_continuously false")

def remove_fullscreen_widthlimit(line):
	lower = line.lower()
	if lower.startswith("\\fullscreen_width") or lower.startswith("\\fullscreen_limit"):
		return (True, "")
	return no_match

# End conversions for LyX 2.3 to 2.4
####################################


#################################
# Conversions from LyX 2.4 to 2.5

def bookmarks_visibility(line):
	return simple_renaming(line, "\\experimental:bookmarks_visibility", "\\bookmarks_visibility")


# End conversions for LyX 2.4 to 2.5
####################################


############################################################
# Format-conversion map. Also add empty format changes here.

conversions = [
	[  1, [ # there were several conversions for format 1
		export_menu,
		latex_flavor,
		remove_obsolete,
		language_use_babel,
		language_package
	]],
	[ 2, []],
	[ 3, [ zipped_native ]],
	[ 4, [ remove_default_papersize ]],
	[ 5, []],
	[ 6, []],
	[ 7, [add_mime_types]],
	[ 8, []],
	[ 9, [ remove_default_language ]],
	[ 10, []],
	[ 11, [split_pdf_format]],
	[ 12, []],
	[ 13, [mac_cursor_movement]],
	[ 14, []],
	[ 15, []],
	[ 16, [remove_force_paint_single_char]],
	[ 17, [remove_rtl]],
	[ 18, []],
	[ 19, [remove_print_support]],
	[ 20, []],
	[ 21, []],
	[ 22, []],
	[ 23, []],
	[ 24, [rename_collapsible]],
	[ 25, [remove_use_qimage]],
	[ 26, [remove_font_encoding]],
	[ 27, []],
	[ 28, [remove_date_insert_format]],
	[ 29, [remove_use_pixmap_cache]],
	[ 30, []],
	[ 31, []],
	[ 32, []],
	[ 33, []],
	[ 34, [rename_cyrillic_kmap_files]],
	[ 35, [add_dark_color]],
	[ 36, [add_spellcheck_default]],
	[ 37, [remove_fullscreen_widthlimit]],
	[ 38, []],
	[ 39, []],
        [ 40, [bookmarks_visibility]],
]
