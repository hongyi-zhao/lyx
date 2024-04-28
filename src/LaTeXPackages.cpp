/**
 * \file LaTeXPackages.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author José Matos
 * \author Lars Gullik Bjønnes
 * \author Jean-Marc Lasgouttes
 * \author Jürgen Vigna
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "LaTeXPackages.h"

#include "support/convert.h"
#include "support/debug.h"
#include "support/FileName.h"
#include "support/filetools.h"
#include "support/gettext.h"
#include "support/Lexer.h"
#include "support/lstrings.h"
#include "support/Package.h"

#include "frontends/alert.h"


using namespace std;
using namespace lyx::support;


namespace lyx {

LaTeXPackages::Packages LaTeXPackages::packages_;


void LaTeXPackages::getAvailable(bool retry)
{
	Lexer lex;
	support::FileName const real_file = libFileSearch("", "packages.lst");

	if (real_file.empty())
		return;

	lex.setFile(real_file);

	if (!lex.isOK())
		return;

	// Make sure that we are clean
	packages_.clear();

	bool finished = false;
	string lstformat = "1";
	// Parse config-file
	while (lex.isOK() && !finished) {
		switch (lex.lex()) {
		case Lexer::LEX_FEOF:
			finished = true;
			break;
		default: {
			string const p = lex.getString();
			// Parse optional version info
			lex.eatLine();
			string const v = trim(lex.getString());
			if (p == "!!fileformat") {
				lstformat = v;
				continue;
			}
			packages_.insert(make_pair(p, v));
		}
		}
	}
	// Check if the pkglist has current format.
	// Reconfigure once and re-parse if not.
	if (lstformat != "2") {
		// If we have already reconfigured, check if there is an outdated config file
		// which produces the outdated lstformat
		if (retry) {
			// check if we have an outdated chkconfig.ltx file in user dir
			support::FileName chkconfig = fileSearch(addPath(package().user_support().absFileName(), ""),
								 "chkconfig.ltx", string(), must_exist);
			if (chkconfig.empty()) {
				// nothing found. So we can only warn
				frontend::Alert::warning(_("Invalid package list format!"),
					_("The format of your LaTeX packages list is wrong. Please file a bug report."));
				return;
			}
			// Found. Try to rename and warn.
			support::FileName chkconfig_bak;
			chkconfig_bak.set(chkconfig.absFileName() + ".bak");
			if (chkconfig.renameTo(chkconfig_bak))
				// renaming succeeded
				frontend::Alert::warning(_("Outdated configuration script detected!"),
					_("We have detected an outdated script 'chkconfig.ltx' in your user directory.\n"
					  "The script has been renamed to 'chkconfig.ltx.bak'.\n"
					  "If you did not copy the script there by purpose, you can safely delete it."));
			else {
				// renaming failed
				frontend::Alert::warning(_("Outdated configuration script detected!"),
					bformat(_("We have detected an outdated script 'chkconfig.ltx' in your user directory\n"
					  "(%1$s).\n"
					  "Please delete or update this file!"), from_utf8(chkconfig.absFileName())));
				return;
			}
		}
		package().reconfigureUserLyXDir("");
		getAvailable(true);
	}
}


bool LaTeXPackages::isAvailable(string const & name)
{
	if (packages_.empty())
		getAvailable();
	string n = name;
	if (suffixIs(n, ".sty"))
		n.erase(name.length() - 4);
	for (auto const & package : packages_) {
		if (package.first == n)
			return true;
	}
	return false;
}


bool LaTeXPackages::isAvailableAtLeastFrom(string const & name,
					   int const y, int const m, int const d)
{
	if (packages_.empty())
		getAvailable();

	// required date as int (yyyymmdd)
	int const req_date = (y * 10000) + (m * 100) + d;
	for (auto const & package : packages_) {
		if (package.first == name && !package.second.empty()) {
			if (!isStrInt(package.second)) {
				LYXERR0("Warning: Invalid date of package "
					<< package.first << " (" << package.second << ")");
				continue;
			}
			// required date not newer than available date
			return req_date <= convert<int>(package.second);
		}
	}
	return false;
}

} // namespace lyx
