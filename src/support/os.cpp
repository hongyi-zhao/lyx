/**
 * \file os.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Ruurd A. Reitsma
 * \author Enrico Forestieri
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#ifdef _WIN32
# define _WIN32_WINNT 0x0600
#endif

#include "support/convert.h"
#include "support/debug.h"
#include "support/filetools.h"
#include "support/qstring_helpers.h"

#include <QDir>

#include <regex>

#if defined(__CYGWIN__)
#include "support/os_cygwin.cpp"
#elif defined(_WIN32)
#include "support/os_win32.cpp"
#else
#include "support/os_unix.cpp"
#endif

// Static assert to break compilation on platforms where
// int/unsigned int is not 4 bytes. Added to make sure that
// e.g., the author hash is always 32-bit.
template<bool Condition> struct static_assert_helper;
template <> struct static_assert_helper<true> {};
enum {
	dummy = sizeof(static_assert_helper<sizeof(int) == 4>)
};

namespace lyx {
namespace support {
namespace os {

int timeout_ms()
{
	// Starting in 2.4.0, we allow the user to cancel the background
	// process at any time with LFUN_EXPORT_CANCEL, so the timeout dialog
	// is no longer useful.
	// "-1" effectively disables the timeout (it is a special case in
	// SystemcallPrivate::waitWhile()).
	return -1;
}


static string const python_call(string const & binary, bool verbose = false)
{
	const string version_info = " -c \"import sys; print(sys.version_info[:2], end='')\"";
	// Default to "python3" if no binary is given.
	if (binary.empty())
		return "python3";

	if (verbose)
		lyxerr << "Examining " << binary << "\n";
	// Check whether this is a python 2 or 3 binary.
	cmd_ret const out = runCommand(binary + version_info);

	smatch sm;
	try {
		static regex const python_reg("\\((\\d*), (\\d*)\\)");
		if (!out.valid || !regex_match(out.result, sm, python_reg))
			return string();
	} catch(regex_error const & /*e*/) {
		LYXERR0("Regex error! This should not happen.");
		return string();
	}

	int major = convert<int>(sm.str(1));
	int minor = convert<int>(sm.str(2));
	if((major < 3) || (major == 3 && minor < 8))
		return string();

	if (verbose)
		lyxerr << "Found Python " << out.result << "\n";
	return binary;
}


static string const find_python_binary()
{
	// This function takes inspiration from PEP 394 and PEP 397
	// PEP 394 -- The "python" Command on Unix-Like Systems
	// https://www.python.org/dev/peps/pep-0394/
	// PEP 397 -- Python launcher for Windows
	// https://www.python.org/dev/peps/pep-0397/

#ifdef _WIN32
	// Check through python launcher whether python 3 is
	// installed on computer.
	string command = python_call("py -3");
#else
	// Check whether python3 in PATH is the right one.
	string command = python_call("python3");
#endif // _WIN32
	if (!command.empty())
		return command;

	// python3 does not exists, let us try to find python3.x in PATH
	// the search is probably broader than required
	// but we are trying hard to find a valid python binary
	vector<string> const path = getEnvPath("PATH");
	lyxerr << "Looking for python 3.x ...\n";
	for (auto const & bin : path) {
		QString const dir = toqstr(bin);
		string const localdir = dir.toLocal8Bit().constData();
		QDir qdir(dir);
		qdir.setFilter(QDir::Files | QDir::Executable);
		QStringList list = qdir.entryList(QStringList("python3*"));
		for (auto const & bin2 : list) {
			string const binary = "\"" + addName(localdir,
				bin2.toLocal8Bit().constData()) + "\"";
			command = python_call(binary, true);
			if (!command.empty())
				return command;
		}
	}

	if (!command.empty())
		return command;

	// If this happens all hope is lost that this is a sane system
	lyxerr << "Warning: No Python 3.x binary found.\n";
	return python_call("");
}


string const python(bool reset)
{
	static string command = find_python_binary();
	// FIXME THREAD
	if (reset) {
		command = find_python_binary();
	}
	return command;
}


bool hasPython()
{
	return !(python_call(python()).empty());
}

string const python_info()
{
	const string info_version = " -c \"import sys; print('{} ({})'.format(sys.version.split()[0],sys.executable), end='')\"";
	if (!hasPython())
		return("None");
	return (runCommand(python() + info_version).result);
}

} // namespace os
} // namespace support
} // namespace lyx
