# This file is part of lyx2lyx
# Copyright (C) 2018 The LyX team
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

"This modules tests the auxiliary functions for lyx2lyx."

import unittest
from lyx2lyx_tools import put_cmd_in_ert, latex_length


class TestParserTools(unittest.TestCase):
    def test_put_cmd_in_ert(self):
        ert = [
            "\\begin_inset ERT",
            "status collapsed",
            "",
            "\\begin_layout Plain Layout",
            "",
            "",
            "\\backslash",
            "texttt{Gr",
            "\\backslash",
            '"{u}',
            "\\backslash",
            "ss{}e}",
            "\\end_layout",
            "",
            "\\end_inset",
        ]
        ert_open = ert[:]
        ert_open[1] = "status open"
        ert_paragraph = [
            "\\begin_layout Standard",
            "\\begin_inset ERT",
            "status collapsed",
            "",
            "\\begin_layout Plain Layout",
            "",
            "",
            "\\backslash",
            "texttt{Gr",
            "\\backslash",
            '"{u}',
            "\\backslash",
            "ss{}e}",
            "\\end_layout",
            "",
            "\\end_inset",
            "",
            "",
            "\\end_layout",
            "",
        ]
        self.assertEqual(put_cmd_in_ert("\\texttt{Grüße}"), ert)
        self.assertEqual(put_cmd_in_ert(["\\texttt{Grüße}"]), ert)
        self.assertEqual(put_cmd_in_ert("\\texttt{Grüße}", is_open=True), ert_open)
        self.assertEqual(put_cmd_in_ert("\\texttt{Grüße}", as_paragraph=True), ert_paragraph)

    def test_latex_length(self):
        self.assertEqual(latex_length("-30.5col%"), (True, "-0.305\\columnwidth"))
        self.assertEqual(latex_length("35baselineskip%"), (True, "0.35\\baselineskip"))
        self.assertEqual(latex_length("11em"), (False, "11em"))
        self.assertEqual(latex_length("-0.4pt"), (False, "-0.4pt"))


if __name__ == "__main__":
    unittest.main()
