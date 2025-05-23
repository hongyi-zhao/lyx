# This file is part of lyx2lyx
# Copyright (C) 2011 The LyX team
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

"""
This module offers several free functions to help with lyx2lyx'ing.
More documentaton is below, but here is a quick guide to what
they do. Optional arguments are marked by brackets.

add_to_preamble(document, text):
  Here, text can be either a single line or a list of lines. It
  is bad practice to pass something with embedded newlines, but
  we will handle that properly.
  The routine checks to see whether the provided material is
  already in the preamble. If not, it adds it.
  Prepends a comment "% Added by lyx2lyx" to text.

insert_to_preamble(document, text[, index]):
  Here, text can be either a single line or a list of lines. It
  is bad practice to pass something with embedded newlines, but
  we will handle that properly.
  The routine inserts text at document.preamble[index], where by
  default index is 0, so the material is inserted at the beginning.
  Prepends a comment "% Added by lyx2lyx" to text.

put_cmd_in_ert(cmd):
  Here cmd should be a list of strings (lines), which we want to
  wrap in ERT. Returns a list of strings so wrapped.
  A call to this routine will often go something like this:
    i = find_token('\\begin_inset FunkyInset', ...)
    j = find_end_of_inset(document.body, i)
    content = lyx2latex(document[i:j + 1])
    ert = put_cmd_in_ert(content)
    document.body[i:j+1] = ert

get_ert(lines, i[, verbatim]):
  Here, lines is a list of lines of LyX material containing an ERT inset,
  whose content we want to convert to LaTeX. The ERT starts at index i.
  If the optional (by default: False) bool verbatim is True, the content
  of the ERT is returned verbatim, that is in LyX syntax (not LaTeX syntax)
  for the use in verbatim insets.

lyx2latex(document, lines):
  Here, lines is a list of lines of LyX material we want to convert
  to LaTeX. We do the best we can and return a string containing
  the translated material.

lyx2verbatim(document, lines):
  Here, lines is a list of lines of LyX material we want to convert
  to verbatim material (used in ERT an the like). We do the best we
  can and return a string containing the translated material.

latex_length(slen):
  Convert lengths (in LyX form) to their LaTeX representation. Returns
  (bool, length), where the bool tells us if it was a percentage, and
  the length is the LaTeX representation.

convert_info_insets(document, type, func):
  Applies func to the argument of all info insets matching certain types
  type : the type to match. This can be a regular expression.
  func : function from string to string to apply to the "arg" field of
         the info insets.

is_document_option(document, option):
  Find if _option_ is a document option (\\options in the header).

insert_document_option(document, option):
  Insert _option_ as a document option.

remove_document_option(document, option):
  Remove _option_ as a document option.

revert_language(document, lyxname, babelname="", polyglossianame=""):
  Reverts native language support to ERT
  If babelname or polyglossianame is empty, it is assumed
  this language package is not supported for the given language.
"""

import re

from parser_tools import (
    find_end_of_inset,
    find_token,
    get_bool_value,
    get_containing_inset,
    get_containing_layout,
    get_value,
)
from unicode_symbols import unicode_reps


# This will accept either a list of lines or a single line.
# It is bad practice to pass something with embedded newlines,
# though we will handle that.
def add_to_preamble(document, text):
    "Add text to the preamble if it is not already there."

    if not isinstance(text, list):
        # split on \n just in case
        # it'll give us the one element list we want
        # if there's no \n, too
        text = text.split("\n")

    i = 0
    prelen = len(document.preamble)
    while True:
        i = find_token(document.preamble, text[0], i)
        if i == -1:
            break
        # we need a perfect match
        # ignoring surrounding whitespace, though (see #13168)
        matched = True
        for line in text:
            if i >= prelen or line.strip() != document.preamble[i].strip():
                matched = False
                break
            i += 1
        if matched:
            return
        else:
            i += 1

    document.preamble.extend(["% Added by lyx2lyx"])
    document.preamble.extend(text)


# Note that text can be either a list of lines or a single line.
# It should really be a list.
def insert_to_preamble(document, text, index=0):
    """Insert text to the preamble at a given line"""

    if not isinstance(text, list):
        # split on \n just in case
        # it'll give us the one element list we want
        # if there's no \n, too
        text = text.split("\n")

    text.insert(0, "% Added by lyx2lyx")
    document.preamble[index:index] = text


# A dictionary of Unicode->LICR mappings for use in a Unicode string's translate() method
# Created from the reversed list to keep the first of alternative definitions.
licr_table = {ord(ch): cmd for cmd, ch in unicode_reps[::-1]}


def put_cmd_in_ert(cmd, is_open=False, as_paragraph=False):
    """
    Return ERT inset wrapping `cmd` as a list of strings.

    `cmd` can be a string or list of lines. Non-ASCII characters are converted
    to the respective LICR macros if defined in unicodesymbols,
    `is_open` is a boolean setting the inset status to "open",
    `as_paragraph` wraps the ERT inset in a Standard paragraph.
    """

    status = {False: "collapsed", True: "open"}
    ert_inset = [
        "\\begin_inset ERT",
        "status %s" % status[is_open],
        "",
        "\\begin_layout Plain Layout",
        "",
        # content here ([5:5])
        "\\end_layout",
        "",
        "\\end_inset",
    ]

    paragraph = [
        "\\begin_layout Standard",
        # content here ([1:1])
        "",
        "",
        "\\end_layout",
        "",
    ]
    # ensure cmd is an unicode instance and make it "LyX safe".
    if isinstance(cmd, list):
        cmd = "\n".join(cmd)
    cmd = cmd.translate(licr_table)
    cmd = cmd.replace("\\", "\n\\backslash\n")

    ert_inset[5:5] = cmd.splitlines()
    if not as_paragraph:
        return ert_inset
    paragraph[1:1] = ert_inset
    return paragraph


def get_ert(lines, i, verbatim=False):
    "Convert an ERT inset into LaTeX."
    if not lines[i].startswith("\\begin_inset ERT"):
        return ""
    j = find_end_of_inset(lines, i)
    if j == -1:
        return ""
    while i < j and not lines[i].startswith("status"):
        i = i + 1
    i = i + 1
    ret = ""
    first = True
    while i < j:
        if lines[i] == "\\begin_layout Plain Layout":
            if first:
                first = False
            else:
                ret = ret + "\n"
            while i + 1 < j and lines[i + 1] == "":
                i = i + 1
        elif lines[i] == "\\end_layout":
            while i + 1 < j and lines[i + 1] == "":
                i = i + 1
        elif lines[i] == "\\backslash":
            if verbatim:
                ret = ret + "\n" + lines[i] + "\n"
            else:
                ret = ret + "\\"
        else:
            ret = ret + lines[i]
        i = i + 1
    return ret


def lyx2latex(document, lines):
    "Convert some LyX stuff into corresponding LaTeX stuff, as best we can."

    content = ""
    ert_end = 0
    note_end = -1
    hspace = ""

    for curline in range(len(lines)):
        line = lines[curline]
        if line.startswith("\\begin_inset Note Note"):
            # We want to skip LyX notes, so remember where the inset ends
            note_end = find_end_of_inset(lines, curline + 1)
            continue
        elif note_end >= curline:
            # Skip LyX notes
            continue
        elif line.startswith("\\begin_inset ERT"):
            # We don't want to replace things inside ERT, so figure out
            # where the end of the inset is.
            ert_end = find_end_of_inset(lines, curline + 1)
            continue
        elif line.startswith("\\begin_inset Formula"):
            line = line[20:]
        elif line.startswith("\\begin_inset Quotes"):
            # For now, we do a very basic reversion. Someone who understands
            # quotes is welcome to fix it up.
            qtype = line[20:].strip()
            # lang = qtype[0]
            side = qtype[1]
            dbls = qtype[2]
            if side == "l":
                if dbls == "d":
                    line = "``"
                else:
                    line = "`"
            else:
                if dbls == "d":
                    line = "''"
                else:
                    line = "'"
        elif line.startswith("\\begin_inset Newline newline"):
            line = "\\\\ "
        elif line.startswith("\\noindent"):
            line = "\\noindent "  # we need the space behind the command
        elif line.startswith("\\begin_inset space"):
            line = line[18:].strip()
            if line.startswith("\\hspace"):
                # Account for both \hspace and \hspace*
                hspace = line[:-2]
                continue
            elif line == "\\space{}":
                line = "\\ "
            elif line == "\\thinspace{}":
                line = "\\,"
        elif hspace != "":
            # The LyX length is in line[8:], after the \length keyword
            length = latex_length(line[8:])[1]
            line = hspace + "{" + length + "}"
            hspace = ""
        elif (
            line.isspace()
            or line.startswith("\\begin_layout")
            or line.startswith("\\end_layout")
            or line.startswith("\\begin_inset")
            or line.startswith("\\end_inset")
            or line.startswith("\\lang")
            or line.strip() == "status collapsed"
            or line.strip() == "status open"
        ):
            # skip all that stuff
            continue

        # this needs to be added to the preamble because of cases like
        # \textmu, \textbackslash, etc.
        add_to_preamble(
            document,
            [
                "% added by lyx2lyx for converted index entries",
                "\\@ifundefined{textmu}",
                " {\\usepackage{textcomp}}{}",
            ],
        )
        # a lossless reversion is not possible
        # try at least to handle some common insets and settings
        if ert_end >= curline:
            line = line.replace(r"\backslash", "\\")
        else:
            # No need to add "{}" after single-nonletter macros
            line = line.replace("&", "\\&")
            line = line.replace("#", "\\#")
            line = line.replace("^", "\\textasciicircum{}")
            line = line.replace("%", "\\%")
            line = line.replace("_", "\\_")
            line = line.replace("$", "\\$")

            # Do the LyX text --> LaTeX conversion
            for rep in unicode_reps:
                line = line.replace(rep[1], rep[0])
            line = line.replace(r"\backslash", r"\textbackslash{}")
            line = line.replace(r"\series bold", r"\bfseries{}").replace(
                r"\series default", r"\mdseries{}"
            )
            line = line.replace(r"\shape italic", r"\itshape{}").replace(
                r"\shape smallcaps", r"\scshape{}"
            )
            line = line.replace(r"\shape slanted", r"\slshape{}").replace(
                r"\shape default", r"\upshape{}"
            )
            line = line.replace(r"\emph on", r"\em{}").replace(r"\emph default", r"\em{}")
            line = line.replace(r"\noun on", r"\scshape{}").replace(
                r"\noun default", r"\upshape{}"
            )
            line = line.replace(r"\bar under", r"\underbar{").replace(r"\bar default", r"}")
            line = line.replace(r"\family sans", r"\sffamily{}").replace(
                r"\family default", r"\normalfont{}"
            )
            line = line.replace(r"\family typewriter", r"\ttfamily{}").replace(
                r"\family roman", r"\rmfamily{}"
            )
            line = line.replace(r"\InsetSpace ", r"").replace(r"\SpecialChar ", r"")
        content += line
    return content


def lyx2verbatim(document, lines):
    "Convert some LyX stuff into corresponding verbatim stuff, as best we can."

    content = lyx2latex(document, lines)
    content = re.sub(r"\\(?!backslash)", r"\n\\backslash\n", content)

    return content


def latex_length(slen):
    """
    Convert lengths to their LaTeX representation. Returns (bool, length),
    where the bool tells us if it was a percentage, and the length is the
    LaTeX representation.
    """
    i = 0
    percent = False
    # the slen has the form
    # ValueUnit+ValueUnit-ValueUnit or
    # ValueUnit+-ValueUnit
    # the + and - (glue lengths) are optional
    # the + always precedes the -

    # Convert relative lengths to LaTeX units
    units = {
        "col%": "\\columnwidth",
        "text%": "\\textwidth",
        "page%": "\\paperwidth",
        "line%": "\\linewidth",
        "theight%": "\\textheight",
        "pheight%": "\\paperheight",
        "baselineskip%": "\\baselineskip",
    }
    for unit in list(units.keys()):
        i = slen.find(unit)
        if i == -1:
            continue
        percent = True
        minus = slen.rfind("-", 1, i)
        plus = slen.rfind("+", 0, i)
        latex_unit = units[unit]
        if plus == -1 and minus == -1:
            value = slen[:i]
            value = str(float(value) / 100)
            end = slen[i + len(unit) :]
            slen = value + latex_unit + end
        if plus > minus:
            value = slen[plus + 1 : i]
            value = str(float(value) / 100)
            begin = slen[: plus + 1]
            end = slen[i + len(unit) :]
            slen = begin + value + latex_unit + end
        if plus < minus:
            value = slen[minus + 1 : i]
            value = str(float(value) / 100)
            begin = slen[: minus + 1]
            slen = begin + value + latex_unit

    # replace + and -, but only if the - is not the first character
    slen = slen[0] + slen[1:].replace("+", " plus ").replace("-", " minus ")
    # handle the case where "+-1mm" was used, because LaTeX only understands
    # "plus 1mm minus 1mm"
    if slen.find("plus  minus"):
        lastvaluepos = slen.rfind(" ")
        lastvalue = slen[lastvaluepos:]
        slen = slen.replace("  ", lastvalue + " ")
    return (percent, slen)


def length_in_bp(document, length):
    "Convert a length in LyX format to its value in bp units"

    em_width = 10.0 / 72.27  # assume 10pt font size
    text_width = 8.27 / 1.7  # assume A4 with default margins
    # scale factors are taken from Length::inInch()
    scales = {
        "bp": 1.0,
        "cc": (72.0 / (72.27 / (12.0 * 0.376 * 2.845))),
        "cm": (72.0 / 2.54),
        "dd": (72.0 / (72.27 / (0.376 * 2.845))),
        "em": (72.0 * em_width),
        "ex": (72.0 * em_width * 0.4305),
        "in": 72.0,
        "mm": (72.0 / 25.4),
        "mu": (72.0 * em_width / 18.0),
        "pc": (72.0 / (72.27 / 12.0)),
        "pt": (72.0 / (72.27)),
        "sp": (72.0 / (72.27 * 65536.0)),
        "text%": (72.0 * text_width / 100.0),
        "col%": (72.0 * text_width / 100.0),  # assume 1 column
        "page%": (72.0 * text_width * 1.7 / 100.0),
        "line%": (72.0 * text_width / 100.0),
        "theight%": (72.0 * text_width * 1.787 / 100.0),
        "pheight%": (72.0 * text_width * 2.2 / 100.0),
    }

    rx = re.compile(r"^\s*([^a-zA-Z%]+)([a-zA-Z%]+)\s*$")
    m = rx.match(length)
    if not m:
        document.warning("Invalid length value: " + length + ".")
        return 0
    value = m.group(1)
    unit = m.group(2)
    if unit not in scales.keys():
        document.warning("Unknown length unit: " + unit + ".")
        return value
    return "%g" % (float(value) * scales[unit])


def revert_flex_inset(document, name, LaTeXname):
    "Convert flex insets to TeX code"
    lines = document.body
    i = 0
    while True:
        i = find_token(lines, "\\begin_inset Flex " + name, i)
        if i == -1:
            return
        z = find_end_of_inset(lines, i)
        if z == -1:
            document.warning("Can't find end of Flex " + name + " inset.")
            i += 1
            continue
        # remove the \end_inset
        lines[z - 2 : z + 1] = put_cmd_in_ert("}")
        # we need to reset character layouts if necessary
        j = find_token(lines, "\\emph on", i, z)
        k = find_token(lines, "\\noun on", i, z)
        l = find_token(lines, "\\series", i, z)
        m = find_token(lines, "\\family", i, z)
        n = find_token(lines, "\\shape", i, z)
        o = find_token(lines, "\\color", i, z)
        p = find_token(lines, "\\size", i, z)
        q = find_token(lines, "\\bar under", i, z)
        r = find_token(lines, "\\uuline on", i, z)
        s = find_token(lines, "\\uwave on", i, z)
        t = find_token(lines, "\\strikeout on", i, z)
        if j != -1:
            lines.insert(z - 2, "\\emph default")
        if k != -1:
            lines.insert(z - 2, "\\noun default")
        if l != -1:
            lines.insert(z - 2, "\\series default")
        if m != -1:
            lines.insert(z - 2, "\\family default")
        if n != -1:
            lines.insert(z - 2, "\\shape default")
        if o != -1:
            lines.insert(z - 2, "\\color inherit")
        if p != -1:
            lines.insert(z - 2, "\\size default")
        if q != -1:
            lines.insert(z - 2, "\\bar default")
        if r != -1:
            lines.insert(z - 2, "\\uuline default")
        if s != -1:
            lines.insert(z - 2, "\\uwave default")
        if t != -1:
            lines.insert(z - 2, "\\strikeout default")
        lines[i : i + 4] = put_cmd_in_ert(LaTeXname + "{")
        i += 1


def revert_font_attrs(lines, name, LaTeXname):
    "Reverts font changes to TeX code"
    i = 0
    changed = False
    while True:
        i = find_token(lines, name + " on", i)
        if i == -1:
            break
        j = find_token(lines, name + " default", i)
        k = find_token(lines, name + " on", i + 1)
        # if there is no default set, the style ends with the layout
        # assure hereby that we found the correct layout end
        if j != -1 and (j < k or k == -1):
            lines[j : j + 1] = put_cmd_in_ert("}")
        else:
            j = find_token(lines, "\\end_layout", i)
            lines[j:j] = put_cmd_in_ert("}")
        lines[i : i + 1] = put_cmd_in_ert(LaTeXname + "{")
        changed = True
        i += 1

    # now delete all remaining lines that manipulate this attribute
    i = 0
    while True:
        i = find_token(lines, name, i)
        if i == -1:
            break
        del lines[i]

    return changed


def revert_layout_command(lines, name, LaTeXname):
    "Reverts a command from a layout to TeX code"
    i = 0
    while True:
        i = find_token(lines, "\\begin_layout " + name, i)
        if i == -1:
            return
        k = -1
        # find the next layout
        j = i + 1
        while k == -1:
            j = find_token(lines, "\\begin_layout", j)
            l = len(lines)
            # if nothing was found it was the last layout of the document
            if j == -1:
                lines[l - 4 : l - 4] = put_cmd_in_ert("}")
                k = 0
            # exclude plain layout because this can be TeX code or another inset
            elif lines[j] != "\\begin_layout Plain Layout":
                lines[j - 2 : j - 2] = put_cmd_in_ert("}")
                k = 0
            else:
                j += 1
        lines[i] = "\\begin_layout Standard"
        lines[i + 1 : i + 1] = put_cmd_in_ert(LaTeXname + "{")
        i += 1


def hex2ratio(s):
    "Converts an RRGGBB-type hexadecimal string to a float in [0.0,1.0]"
    try:
        val = int(s, 16)
    except ValueError:
        val = 0
    if val != 0:
        val += 1
    return str(val / 256.0)


def str2bool(s):
    "'true' goes to True, case-insensitively, and we strip whitespace."
    s = s.strip().lower()
    return s == "true"


def convert_info_insets(document, type, func):
    "Convert info insets matching type using func."
    i = 0
    type_re = re.compile(r'^type\s+"(%s)"$' % type)
    arg_re = re.compile(r'^arg\s+"(.*)"$')
    while True:
        i = find_token(document.body, "\\begin_inset Info", i)
        if i == -1:
            return
        t = type_re.match(document.body[i + 1])
        if t:
            arg = arg_re.match(document.body[i + 2])
            if arg:
                new_arg = func(arg.group(1))
                document.body[i + 2] = 'arg   "%s"' % new_arg
        i += 3


def insert_document_option(document, option):
    "Insert _option_ as a document option."

    # Find \options in the header
    i = find_token(document.header, "\\options", 0)
    # if the options does not exists add it after the textclass
    if i == -1:
        i = find_token(document.header, "\\textclass", 0) + 1
        document.header.insert(i, r"\options %s" % option)
        return
    # otherwise append to options
    if not is_document_option(document, option):
        document.header[i] += ",%s" % option


def remove_document_option(document, option):
    """Remove _option_ as a document option."""

    i = find_token(document.header, "\\options")
    options = get_value(document.header, "\\options", i)
    options = [op.strip() for op in options.split(",")]

    # Remove `option` from \options
    options = [op for op in options if op != option]

    if options:
        document.header[i] = "\\options " + ",".join(options)
    else:
        del document.header[i]


def is_document_option(document, option):
    "Find if _option_ is a document option"

    options = get_value(document.header, "\\options")
    options = [op.strip() for op in options.split(",")]
    return option in options


singlepar_insets = [
    s.strip()
    for s in "Argument, Caption Above, Caption Below, Caption Bicaption,"
    "Caption Centered, Caption FigCaption, Caption Standard, Caption Table,"
    "Flex Chemistry, Flex Fixme_Note, Flex Latin, Flex ListOfSlides,"
    "Flex Missing_Figure, Flex PDF-Annotation, Flex PDF-Comment-Setup,"
    "Flex Reflectbox, Flex S/R expression, Flex Sweave Input File,"
    "Flex Sweave Options, Flex Thanks_Reference, Flex URL, Foot InTitle,"
    "IPADeco, Index, Info, Phantom, Script".split(",")
]
# print(singlepar_insets)


def revert_language(document, lyxname, babelname="", polyglossianame="", polyglossiaopts="", babelprovide=False):
    "Revert native language support"

    # Does the document use polyglossia?
    use_polyglossia = False
    if get_bool_value(document.header, "\\use_non_tex_fonts"):
        i = find_token(document.header, "\\language_package")
        if i == -1:
            document.warning("Malformed document! Missing \\language_package")
        else:
            pack = get_value(document.header, "\\language_package", i)
            if pack in ("default", "auto"):
                use_polyglossia = True

    # Do we use this language with polyglossia?
    with_polyglossia = use_polyglossia and polyglossianame != ""
    # Do we use this language with babel?
    with_babel = with_polyglossia == False and babelname != ""

    # Are we dealing with a primary or secondary language?
    primary = document.language == lyxname
    secondary = False

    # Main language first
    orig_doc_language = document.language
    if primary:
        # Change LyX document language to English (we will tell LaTeX
        # to use the original language at the end of this function):
        document.language = "english"
        i = find_token(document.header, "\\language %s" % lyxname, 0)
        if i != -1:
            document.header[i] = "\\language english"

    # Now look for occurences in the body
    i = 0
    while True:
        i = find_token(document.body, "\\lang", i + 1)
        if i == -1:
            break
        if document.body[i].startswith("\\lang %s" % lyxname):
            secondary = True
            texname = use_polyglossia and polyglossianame or babelname
        elif primary and document.body[i].startswith("\\lang english"):
            # Since we switched the main language manually, English parts need to be marked
            texname = "english"
        else:
            continue

        parent = get_containing_layout(document.body, i)
        i_e = parent[2]  # end line no,
        # print(i, texname, parent, document.body[i+1], file=sys.stderr)

        # Move leading space to the previous line:
        if document.body[i + 1].startswith(" "):
            document.body[i + 1] = document.body[i + 1][1:]
            document.body.insert(i, " ")
            continue

        # TODO: handle nesting issues with font attributes, e.g.
        # \begin_layout Standard
        #
        # \emph on
        # \lang macedonian
        # Македонски јазик
        # \emph default
        #  — јужнословенски јазик, дел од групата на словенски јазици од јазичното
        #  семејство на индоевропски јазици.
        #  Македонскиот е службен и национален јазик во Македонија.
        # \end_layout

        # Ensure correct handling of list labels
        if parent[0] in ["Labeling", "Description"] and " " not in "\n".join(
            document.body[parent[3] : i]
        ):
            # line `i+1` is first line of a list item,
            # part before a space character is the label
            # TODO: insets or language change before first space character
            labelline = document.body[i + 1].split(" ", 1)
            if len(labelline) > 1:
                # Insert a space in the (original) document language
                # between label and remainder.
                # print("  Label:", labelline, file=sys.stderr)
                lines = [
                    labelline[0],
                    "\\lang %s" % orig_doc_language,
                    " ",
                    "\\lang %s" % (primary and "english" or lyxname),
                    labelline[1],
                ]
                document.body[i + 1 : i + 2] = lines
                i_e += 4

        # Find out where to end the language change.
        langswitch = i
        while True:
            langswitch = find_token(document.body, "\\lang", langswitch + 1, i_e)
            if langswitch == -1:
                break
            # print("  ", langswitch, document.body[langswitch], file=sys.stderr)
            # skip insets
            i_a = parent[3]  # paragraph start line
            container = get_containing_inset(document.body[i_a:i_e], langswitch - i_a)
            if (
                container
                and container[1] < langswitch - i_a
                and container[2] > langswitch - i_a
            ):
                # print("  inset", container, file=sys.stderr)
                continue
            i_e = langswitch
            break

        # use function or environment?
        singlepar = i_e - i < 3
        if not singlepar and parent[0] == "Plain Layout":
            # environment not allowed in some insets
            container = get_containing_inset(document.body, i)
            singlepar = container[0] in singlepar_insets

        # Delete empty language switches:
        if not "".join(document.body[i + 1 : i_e]):
            del document.body[i:i_e]
            i -= 1
            continue


        if polyglossiaopts != "":
            polyglossiaopts = "[" + polyglossiaopts + "]"
        if singlepar:
            if with_polyglossia:
                begin_cmd = "\\text%s%s{" % (texname, polyglossiaopts)
            elif with_babel:
                begin_cmd = "\\foreignlanguage{%s}{" % texname
            end_cmd = "}"
        else:
            if with_polyglossia:
                begin_cmd = "\\begin%s{%s}" % (polyglossiaopts, texname)
                end_cmd = "\\end{%s}" % texname
            elif with_babel:
                begin_cmd = "\\begin{otherlanguage}{%s}" % texname
                end_cmd = "\\end{otherlanguage}"

        if not primary or texname == "english":
            try:
                document.body[i_e:i_e] = put_cmd_in_ert(end_cmd)
                document.body[i + 1 : i + 1] = put_cmd_in_ert(begin_cmd)
            except UnboundLocalError:
                pass
        del document.body[i]

    if not (primary or secondary):
        return

    # Make the language known to Babel/Polyglossia and ensure the correct
    # document language:
    doc_lang_switch = ""
    if with_babel:
        # add as global option
        insert_document_option(document, babelname)
        # babelprovide
        if babelprovide:
           babelprovide = "\\babelprovide[import"
           if primary:
               babelprovide += ", main"
           babelprovide += "]{%s}" % babelname
           add_to_preamble(
            document,
            ["\\AddToHook{package/babel/after}{%s}" % babelprovide],
           )
        # Since user options are appended to the document options,
        # Babel will treat `babelname` as primary language.
        if not primary:
            doc_lang_switch = "\\selectlanguage{%s}" % orig_doc_language
    if with_polyglossia:
        # Define language in the user preamble
        # (don't use \AtBeginDocument, this fails with some languages).
        add_to_preamble(
            document,
            ["\\usepackage{polyglossia}", "\\setotherlanguage{%s}" % polyglossianame],
        )
        if primary:
            # Changing the main language must be done in the document body.
            doc_lang_switch = "\\resetdefaultlanguage{%s}" % polyglossianame

    # Reset LaTeX main language if required and not already done
    if doc_lang_switch and doc_lang_switch[1:] not in document.body[8:20]:
        document.body[2:2] = put_cmd_in_ert(doc_lang_switch, is_open=True, as_paragraph=True)
