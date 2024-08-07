# This file is part of lyx2lyx
# Copyright (C) 2024 The LyX team
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

"""Convert files to the file format generated by lyx 2.5"""

import re

# Uncomment only what you need to import, please (lyx2lyx_tools):
#    convert_info_insets, get_ert, hex2ratio, insert_to_preamble,
#    length_in_bp, lyx2latex, lyx2verbatim,
#    revert_flex_inset, revert_flex_inset, revert_font_attrs,
#    revert_language, str2bool
from lyx2lyx_tools import (
    add_to_preamble,
    latex_length,
    put_cmd_in_ert
)

# Uncomment only what you need to import, please (parser_tools):
#    check_token, count_pars_in_inset, del_complete_lines, 
#    del_value, find_complete_lines, find_end_of, find_end_of_layout,
#    find_re, find_substring, find_token_backwards, find_token_exact,
#    find_tokens, get_bool_value, get_containing_inset,
#    get_containing_layout, get_option_value,
#    is_in_inset, set_bool_value
from parser_tools import (
    del_token,
    find_end_of_inset,
    find_re,
    find_token,
    get_quoted_value,
    get_value
)

####################################################################
# Private helper functions


###############################################################################
###
### Conversion and reversion routines
###
###############################################################################


def convert_url_escapes(document):
    """Unescape # and % in URLs with hyperref."""

    hyperref = find_token(document.header, "\\use_hyperref true", 0) != -1
    beamer = document.textclass in [
        "beamer",
        "scrarticle-beamer",
        "beamerposter",
        "article-beamer",
    ]

    if not hyperref and not beamer:
        return

    rurl = re.compile(r"^[%#].*")
    i = 0
    while True:
        i = find_token(document.body, "\\begin_inset Flex URL", i)
        if i == -1:
            return
        j = find_end_of_inset(document.body, i)
        if j == -1:
            document.warning("Malformed LyX document: Could not find end of URL inset.")
            i += 1
            continue
        while True:
            surl = find_re(document.body, rurl, i, j)
            if surl == -1:
                i = j
                break
            if document.body[surl - 1] == "\\backslash":
                del document.body[surl - 1]
            i = surl


def revert_url_escapes(document):
    """Unescape # and % in URLs with hyperref."""

    hyperref = find_token(document.header, "\\use_hyperref true", 0) != -1
    beamer = document.textclass in [
        "beamer",
        "scrarticle-beamer",
        "beamerposter",
        "article-beamer",
    ]

    if not hyperref and not beamer:
        return

    rurl = re.compile(r"^(.*)([%#].*)")
    i = 0
    while True:
        i = find_token(document.body, "\\begin_inset Flex URL", i)
        if i == -1:
            return
        j = find_end_of_inset(document.body, i)
        if j == -1:
            document.warning("Malformed LyX document: Could not find end of URL inset.")
            i += 1
            continue
        while True:
            surl = find_re(document.body, rurl, i, j)
            if surl == -1:
                i = j
                break
            m = rurl.match(document.body[surl])
            if m:
                if m.group(1) == "" and document.body[surl - 1] == "\\backslash":
                    break
                document.body[surl : surl + 1] = [m.group(1), "\\backslash", m.group(2)]
            i = surl


def convert_url_escapes2(document):
    """Unescape backslashes in URLs with hyperref."""

    i = find_token(document.header, "\\use_hyperref true", 0)

    if i == -1 and document.textclass not in [
        "beamer",
        "scrarticle-beamer",
        "beamerposter",
        "article-beamer",
    ]:
        return

    i = 0
    while True:
        i = find_token(document.body, "\\begin_inset Flex URL", i + 1)
        if i == -1:
            return
        j = find_end_of_inset(document.body, i)
        if j == -1:
            document.warning("Malformed LyX document: Could not find end of URL inset.")
            i += 1
            continue
        while True:
            bs = find_token(document.body, "\\backslash", i, j)
            if bs == -1:
                break
            if document.body[bs + 2] == "\\backslash":
                del document.body[bs + 2]
            i = bs + 1


def revert_url_escapes2(document):
    """Escape backslashes in URLs with hyperref."""

    i = find_token(document.header, "\\use_hyperref true", 0)

    if i == -1 and document.textclass not in [
        "beamer",
        "scrarticle-beamer",
        "beamerposter",
        "article-beamer",
    ]:
        return

    i = 0
    while True:
        i = find_token(document.body, "\\begin_inset Flex URL", i + 1)
        if i == -1:
            return
        j = find_end_of_inset(document.body, i)
        if j == -1:
            document.warning("Malformed LyX document: Could not find end of URL inset.")
            i += 1
            continue
        while True:
            bs = find_token(document.body, "\\backslash", i, j)
            if bs == -1:
                break
            document.body[bs] = "\\backslash\\backslash"
            i = bs + 1


def revert_glue_parskip(document):
    """Revert parskip with glue length to user preamble."""

    i = find_token(document.header, "\\paragraph_separation skip", 0)
    if i == -1:
        return

    j = find_token(document.header, "\\defskip", 0)
    if j == -1:
        document.warning("Malformed LyX document! Missing \\defskip.")
        return

    val = get_value(document.header, "\\defskip", j)

    if val.find("+") == -1 and val.find("-", 1) == -1:
        # not a glue length
        return

    add_to_preamble(document, ["\\usepackage[skip={" + latex_length(val)[1] + "}]{parskip}"])

    document.header[i] = "\\paragraph_separation indent"
    document.header[j] = "\\paragraph_indentation default"


def convert_he_letter(document):
    """Convert hebrew letter to letter document class"""

    if document.textclass == "heb-letter":
        document.textclass = "letter"


chicago_local_layout = [
    "### Inserted by lyx2lyx (biblatex-chicago) ###",
    "Requires biblatex-chicago",
    "### End of insertion by lyx2lyx (biblatex-chicago) ###" "",
]

def convert_biblatex_chicago(document):
    """Convert biblatex-chicago documents"""
    
    chicago = document.del_local_layout(chicago_local_layout)
    if not chicago:
        chicago = document.del_from_header(["Requires biblatex-chicago"])
    if not chicago:
        return

    # 1. Get cite engine
    engine = "basic"
    i = find_token(document.header, "\\cite_engine", 0)
    if i == -1:
        document.warning("Malformed document! Missing \\cite_engine")
    else:
        engine = get_value(document.header, "\\cite_engine", i)

    # 2. If biblatex set to chicago
    biblatex = False
    if engine not in ["biblatex", "biblatex-natbib"]:
        return

    document.header[i] = "\\cite_engine biblatex-chicago"

    i = find_token(document.header, "\\biblio_options", 0)
    bibopts = ""
    if i == -1:
        val = get_value(document.header, "\\biblio_options", i)

    cetype = "authoryear"
    if bibopts.find("authordate") == -1:
        cetype = "notes"

    # 2. Set cite type
    i = find_token(document.header, "\\cite_engine_type", 0)
    if i == -1:
        document.warning("Malformed document! Missing \\cite_engine_type")
    else:
        document.header[i] = "\\cite_engine_type %s" % cetype


def revert_biblatex_chicago(document):
    """Revert biblatex-chicago to ERT where necessary"""

    # 1. Get cite engine
    engine = "basic"
    i = find_token(document.header, "\\cite_engine", 0)
    if i == -1:
        document.warning("Malformed document! Missing \\cite_engine")
    else:
        engine = get_value(document.header, "\\cite_engine", i)

    # 2. Do we use biblatex-chicago?
    if engine != "biblatex-chicago":
        return
    
    # 3. Reset cite engine
    document.header[i] = "\\cite_engine biblatex"

    # 4. Set cite type
    cetype = "authoryear"
    i = find_token(document.header, "\\cite_engine_type", 0)
    if i == -1:
        document.warning("Malformed document! Missing \\cite_engine_type")
    else:
        cetype = get_value(document.header, "\\cite_engine_type", i)
        document.header[i] = "\\cite_engine_type authoryear"

    # 5. Add authordate option if needed
    if cetype == "authoryear":
        i = find_token(document.header, "\\biblio_options", 0)
        if i != -1:
            bibopts = get_value(document.header, "\\biblio_options", i)
            if bibopts.find("authordate") != -1:
                document.header[i] = "\\biblio_options %s" % bibopts + ", authordate"
        else:
            i = find_token(document.header, "\\biblio_style", 0)
            if i == -1:
                document.warning("Malformed document! Missing \\biblio_style")
            else:
                document.header[i+1:i+1] = ["\\biblio_options authordate"]

    # 6. Set local layout
    document.append_local_layout(chicago_local_layout)

    # 7. Handle special citation commands
    # Specific citation insets used in biblatex that need to be reverted to ERT
    new_citations = {
        "atcite": "atcite",
        "atpcite": "atpcite",
        "gentextcite": "gentextcite",
        "Gentextcite": "Gentextcite",
    }
    if cetype == "notes":
        new_citations = {
            "citeyear": "citeyear*",
            "Citetitle": "Citetitle",
            "Citetitle*": "Citetitle*",
            "gentextcite": "gentextcite",
            "Gentextcite": "Gentextcite",
            "shortcite": "shortcite",
            "shortcite*": "shortcite*",
            "shortrefcite": "shortrefcite",
            "shorthandcite": "shorthandcite",
            "shorthandcite*": "shorthandcite*",
            "shorthandrefcite": "shorthandrefcite",
            "citejournal": "citejournal",
            "headlesscite": "headlesscite",
            "Headlesscite": "Headlesscite",
            "headlessfullcite": "headlessfullcite",
            "surnamecite": "surnamecite",
        }

    i = 0
    while True:
        i = find_token(document.body, "\\begin_inset CommandInset citation", i)
        if i == -1:
            break
        j = find_end_of_inset(document.body, i)
        if j == -1:
            document.warning("Can't find end of citation inset at line %d!!" % (i))
            i += 1
            continue
        k = find_token(document.body, "LatexCommand", i, j)
        if k == -1:
            document.warning("Can't find LatexCommand for citation inset at line %d!" % (i))
            i = j + 1
            continue
        cmd = get_value(document.body, "LatexCommand", k)
        if cmd in list(new_citations.keys()):
            pre = get_quoted_value(document.body, "before", i, j)
            post = get_quoted_value(document.body, "after", i, j)
            key = get_quoted_value(document.body, "key", i, j)
            if not key:
                document.warning("Citation inset at line %d does not have a key!" % (i))
                key = "???"
            # Replace known new commands with ERT
            res = "\\" + new_citations[cmd]
            if pre:
                res += "[" + pre + "]"
            if post:
                res += "[" + post + "]"
            elif pre:
                res += "[]"
            res += "{" + key + "}"
            document.body[i : j + 1] = put_cmd_in_ert([res])
        i = j + 1


def revert_nptextcite(document):
    """Revert \\nptextcite and MLA's autocite variants to ERT"""

    # 1. Get cite engine
    engine = "basic"
    i = find_token(document.header, "\\cite_engine", 0)
    if i == -1:
        document.warning("Malformed document! Missing \\cite_engine")
    else:
        engine = get_value(document.header, "\\cite_engine", i)

    # 2. Do we use biblatex?
    if engine != "biblatex" and engine != "biblatex-natbib":
        return
    
    # 3. and APA?
    cetype = "authoryear"
    i = find_token(document.header, "\\biblatex_citestyle", 0)
    if i == -1:
        return

    # 4. Convert \nptextcite to ERT
    new_citations = {
        "nptextcite": "nptextcite",
        "mlaautocite": "autocite",
        "Mlaautocite": "Autocite",
        "mlaautocite*": "autocite*",
        "Mlaautocite*": "Autocite*",
    }
    i = 0
    while True:
        i = find_token(document.body, "\\begin_inset CommandInset citation", i)
        if i == -1:
            break
        j = find_end_of_inset(document.body, i)
        if j == -1:
            document.warning("Can't find end of citation inset at line %d!!" % (i))
            i += 1
            continue
        k = find_token(document.body, "LatexCommand", i, j)
        if k == -1:
            document.warning("Can't find LatexCommand for citation inset at line %d!" % (i))
            i = j + 1
            continue
        cmd = get_value(document.body, "LatexCommand", k)
        if cmd in list(new_citations.keys()):
            pre = get_quoted_value(document.body, "before", i, j)
            post = get_quoted_value(document.body, "after", i, j)
            key = get_quoted_value(document.body, "key", i, j)
            if not key:
                document.warning("Citation inset at line %d does not have a key!" % (i))
                key = "???"
            # Replace known new commands with ERT
            res = "\\" + new_citations[cmd]
            if pre:
                res += "[" + pre + "]"
            if post:
                res += "[" + post + "]"
            elif pre:
                res += "[]"
            res += "{" + key + "}"
            document.body[i : j + 1] = put_cmd_in_ert([res])
        i = j + 1


def revert_nomencl_textwidth(document):
    """Revert nomencl textwidth parameter to ERT."""

    i = 0
    while True:
        i = find_token(document.body, "\\begin_inset CommandInset nomencl_print", i)
        if i == -1:
            return

        j = find_end_of_inset(document.body, i)
        if j == -1:
            document.warning(
                "Malformed LyX document: Can't find end of command inset at line %d" % i
            )
            i += 1
            continue

        sw = get_quoted_value(document.body, "set_width", i, j)
        if sw != "textwidth":
            i += 1
            continue

        # change set_width to "none"
        k = find_token(document.body, "set_width", i, j)
        if k != -1:
            document.body[k] = "set_width \"none\""
        tw = get_quoted_value(document.body, "width", i, j)
        # delete width
        del_token(document.body, "width", i, j)
        # Insert ERT
        res = "\\settowidth{\\nomlabelwidth}{" + tw + "}"
        document.body[i : i] = put_cmd_in_ert([res])
        i = j


##
# Conversion hub
#

supported_versions = ["2.5.0", "2.5"]
convert = [
    [621, [convert_url_escapes, convert_url_escapes2]],
    [622, []],
    [623, [convert_he_letter]],
    [624, [convert_biblatex_chicago]],
    [625, []],
    [626, []]
]


revert = [
    [625, [revert_nomencl_textwidth]],
    [624, [revert_nptextcite]],
    [623, [revert_biblatex_chicago]],
    [622, []],
    [621, [revert_glue_parskip]],
    [620, [revert_url_escapes2, revert_url_escapes]],
]


if __name__ == "__main__":
    pass
