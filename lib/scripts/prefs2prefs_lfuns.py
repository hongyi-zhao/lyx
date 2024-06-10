# file prefs2prefs-lfuns.py
# This file is part of LyX, the document processor.
# Licence details can be found in the file COPYING.

# author Richard Kimberly Heck

# Full author contact details are available in file CREDITS

# This file houses conversion information for the bind and ui files,
# i.e., for files where we are converting lfuns.

# The converter functions take a line as argument and return a list: 
# 	(Bool, NewLine), 
# where the Bool says if  we've modified anything and the NewLine is 
# the new line, if so, which will be used to replace the old line.


import sys, re

###########################################################
#
# Actual converter functions
#
# These accept a line as argument and should return a list:
#  (bool, newline)
# where the bool indicates whether we changed anything. If not,
# one normally returns: (False, "").

no_match = (False, "")

def simple_renaming(line, old, new):
	if line.find(old) == -1:
		return no_match
	line = line.replace(old, new)
	return (True, line)


def simple_remove(line, old):
	if line.find(old) == -1:
		return no_match
	return (True, "")


def next_inset_modify(line):
	return simple_renaming(line, "next-inset-modify", "inset-modify")


def next_inset_toggle(line):
	return simple_renaming(line, "next-inset-toggle", "inset-toggle")


def optional_insert(line):
	return simple_renaming(line, "optional-insert", "argument-insert")


re_nm = re.compile(r'^(.*)notes-mutate\s+(\w+)\s+(\w+)(.*)$')
def notes_mutate(line):
	m = re_nm.search(line)
	if not m:
		return no_match

	prefix = m.group(1)
	source = m.group(2)
	target = m.group(3)
	suffix = m.group(4)
	newline = prefix + "inset-forall Note:" + source + \
		" inset-modify note Note " + target + suffix
	return (True, newline)


re_ait = re.compile(r'^(.*)all-insets-toggle\s+(\w+)(?:\s+(\w+))?(.*)$')
def all_insets_toggle(line):
	m = re_ait.search(line)
	if not m:
		return no_match

	prefix = m.group(1)
	action = m.group(2)
	target = m.group(3) 
	suffix = m.group(4)

	# we need to transform the target to match the inset layout names
	# this will not be perfect
	if target == "ert":
		target = "ERT"
	elif target == None:
		target = "*"
	elif target == "tabular":
		# There does not seem to be an InsetLayout for tables, so
		# I do not know what to do here. If anyone does, then please
		# fix this. For now, we just have to remove this line.
		return (True, "")
	else:
		target = target.capitalize()
	
	newline = prefix + "inset-forall " + target + " inset-toggle " + \
		action + suffix
	return (True, newline)


re_li = re.compile(r'^(.*)\bline-insert\b(.*)$')
def line_insert(line):
	m = re_li.search(line)
	if not m: 
		return no_match
	newline = m.group(1) + \
		"inset-insert line rule height 0.25ex width 100col% \\end_inset" + \
		m.group(2)
	return (True, newline)


def toc_insert(line):
	return simple_renaming(line, "toc-insert", "inset-insert toc")


re_ps = re.compile(r'^(.*)paragraph-spacing\s+(default|single|onehalf|double)\b(.*)$')
re_psother = re.compile(r'^(.*)paragraph-spacing\s+other\s+(\d+\.\d?|\d?\.\d+|\d+)(.*)$')
def paragraph_spacing(line):
	# possible args: default, single, onehalf, double, other FLOAT
	m = re_ps.search(line)
	if m:
		arg = m.group(2)
		newline = m.group(1) + "paragraph-params \\paragraph-spacing " + arg + \
			m.group(3)
		return (True, newline)

	m = re_psother.search(line)
	if not m:
		return no_match

	arg = m.group(2)
	newline = m.group(1) + "paragraph-params \\paragraph-spacing other " + \
		arg + m.group(3)
	return (True, newline)


def tabular_feature(line):
	return simple_renaming(line, "tabular-feature", "inset-modify tabular")


re_tabular_feature = re.compile(r"inset-modify\s+tabular(\s+from-dialog)?")
def redo_tabular_feature(line):
	# we change as follows:
	# inset-modify tabular -> tabular-feature
	# but:
	# inset-modify tabular from-dialog -> inset-modify tabular
	#
	# "from-dialog" was never used directly but the user might do, if they
	# followed the standard instructions to create a custom shortcut by looking
	# at the message panel. The equivalent functionality is now provided by
	# inset-modify tabular (without from-dialog).
	def change(match):
		if match.group(1):
			return "inset-modify tabular"
		else:
			return "tabular-feature"

	result = re_tabular_feature.subn(change, line)
	if result[1]:
		return (True, result[0])
	else:
		return no_match


re_Bar2bar = re.compile(r'^(\\(?:bind|unbind))\s+"([^"]*)Bar"(\s+"[^"]+")')
def Bar2bar(line):
	m = re_Bar2bar.search(line)
	if not m:
		return no_match

	btype = m.group(1)
	mod = m.group(2)
	rest = m.group(3)
	newline = btype + " \"" + mod + "bar\"" + rest
	return (True, newline)


def paragraph_break(line):
	return simple_renaming(line, "break-paragraph", "paragraph-break")


def tab_group_close(line):
	return simple_renaming(line, "close-tab-group", "tab-group-close")


def view_split(line):
	return simple_renaming(line, "split-view", "view-split")


def label_copy_as_reference(line):
	return simple_renaming(line, "copy-label-as-reference", "label-copy-as-reference")


def remove_print_support(line):
	return simple_remove(line, "dialog-show print")


def info_rename_vcsauthor(line):
	return simple_renaming(line, "info-insert buffer vcs-author", "info-insert vcs author")


def info_rename_vcsdate(line):
	return simple_renaming(line, "info-insert buffer vcs-date", "info-insert vcs date")


def info_rename_vcstime(line):
	return simple_renaming(line, "info-insert buffer vcs-time", "info-insert vcs time")


def info_rename_vcsrevision(line):
	return simple_renaming(line, "info-insert buffer vcs-revision", "info-insert vcs revision")


def info_rename_vcstreerevision(line):
	return simple_renaming(line, "info-insert buffer vcs-tree-revision", "info-insert vcs tree-revision")


def remove_date_insert(line):
	return simple_remove(line, "date-insert")


re_delete_force = re.compile(r"((char|word)-delete-(for|back)ward)(\s+force)?")
def delete_force(line):
	# we change as follows:
	# char-delete-forward -> char-delete-forward confirm
	# but:
	# char-delete-forward force -> char-delete-forward
	#
	def change(match):
		if match.group(4):
			return match.group(1)
		else:
			return match.group(1) + " confirm"

	result = re_delete_force.subn(change, line)
	if result[1]:
		return (True, result[0])
	else:
		return no_match



#
###########################################################


# Conversion chain

conversions = [
	[  1, [ # this will be a long list of conversions to format 1, LyX 2.0
		next_inset_toggle,
		next_inset_modify,
		optional_insert,
		notes_mutate,
		all_insets_toggle,
		line_insert,
		toc_insert,
		paragraph_spacing,
		tabular_feature,
		Bar2bar
	]],
	[  2, [ # list of conversions to format 2, LyX 2.1
		paragraph_break,
		tab_group_close,
		view_split,
		label_copy_as_reference
	]],
	[ 3, [ # list of conversions to format 3
		remove_print_support
	]],
	[ 4, [ # list of conversions to format 4, LyX 2.2
		redo_tabular_feature
	]],
	[ 5, [ # list of conversions to format 5, LyX 2.4
		info_rename_vcsauthor,
		info_rename_vcsdate,
		info_rename_vcstime,
		info_rename_vcsrevision,
		info_rename_vcstreerevision,
		remove_date_insert,
                delete_force
	]]
]
