/**
 * \file filetools.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * parts Copyright 1985, 1990, 1993 Free Software Foundation, Inc.
 *
 * \author Ivan Schreter
 * \author Dirk Niggemann
 * \author Asger Alstrup
 * \author Lars Gullik Bjønnes
 * \author Jean-Marc Lasgouttes
 * \author Angus Leeming
 * \author John Levon
 * \author Herbert Voß
 *
 * Full author contact details are available in file CREDITS.
 *
 * General path-mangling functions
 */

#include <config.h>

#include "support/filetools.h"

#include "LyXRC.h"

#include "support/convert.h"
#include "support/debug.h"
#include "support/environment.h"
#include "support/lstrings.h"
#include "support/os.h"
#include "support/Messages.h"
#include "support/Package.h"
#include "support/Systemcall.h"
#include "support/qstring_helpers.h"
#include "support/TempFile.h"
#include "support/textutils.h"

#include <QDir>
#include <QUrl>

#include "support/lassert.h"

#include <fcntl.h>
#ifdef HAVE_MAGIC_H
#include <magic.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstdio>

#include <utility>
#include <fstream>
#include <regex>
#include <sstream>
#include <vector>

#include <QCryptographicHash>

#if defined (_WIN32)
#include <io.h>
#include <windows.h>
#endif

using namespace std;

#define USE_QPROCESS

namespace lyx {
namespace support {

bool isLyXFileName(string const & filename)
{
	return suffixIs(ascii_lowercase(filename), ".lyx");
}


bool isSGMLFileName(string const & filename)
{
	return suffixIs(ascii_lowercase(filename), ".sgml");
}


bool isValidLaTeXFileName(string const & filename)
{
	string const invalid_chars("#%\"");
	return filename.find_first_of(invalid_chars) == string::npos;
}


bool isValidDVIFileName(string const & filename)
{
	string const invalid_chars("${}()[]^");
	return filename.find_first_of(invalid_chars) == string::npos;
}


bool isBinaryFile(FileName const & filename)
{
	bool isbinary = false;
	if (filename.empty() || !filename.exists())
		return isbinary;

#ifdef HAVE_MAGIC_H
	magic_t magic_cookie = magic_open(MAGIC_MIME_ENCODING);
	if (magic_cookie) {
		bool detected = true;
		if (magic_load(magic_cookie, nullptr) != 0) {
			LYXERR(Debug::FILES, "isBinaryFile: "
				"Could not load magic database - "
				<< magic_error(magic_cookie));
			detected = false;
		} else {
			char const *charset = magic_file(magic_cookie,
					filename.toFilesystemEncoding().c_str());
			isbinary = contains(charset, "binary");
		}
		magic_close(magic_cookie);
		if (detected)
			return isbinary;
	}
#endif
	// Try by looking for binary chars at the beginning of the file.
	// Note that this is formally not correct, since count_bin_chars
	// expects utf8, and the passed string can be anything: plain text
	// in any encoding, or really binary data. In practice it works,
	// since QString::fromUtf8() drops invalid utf8 sequences, and
	// while the exact number may not be correct, we still get a high
	// number for truly binary files.

	ifstream ifs(filename.toFilesystemEncoding().c_str());
	if (!ifs)
		return isbinary;

	// Maximum strings to read
	int const max_count = 50;

	// Maximum number of binary chars allowed
	int const max_bin = 5;

	int count = 0;
	int binchars = 0;
	string str;
	while (count++ < max_count && !ifs.eof()) {
		getline(ifs, str);
		binchars += count_bin_chars(str);
	}
	return binchars > max_bin;
}


string const latex_path(string const & original_path,
		latex_path_extension extension,
		latex_path_dots dots)
{
	// On cygwin, we may need windows or posix style paths.
	string path = os::latex_path(original_path);
	path = subst(path, "~", "\\string~");
	if (path.find(' ') != string::npos) {
		// We can't use '"' because " is sometimes active (e.g. if
		// babel is loaded with the "german" option)
		if (extension == EXCLUDE_EXTENSION) {
			// changeExtension calls os::internal_path internally
			// so don't use it to remove the extension.
			string const ext = getExtension(path);
			string const base = ext.empty() ?
				path :
				path.substr(0, path.length() - ext.length() - 1);
			// changeExtension calls os::internal_path internally
			// so don't use it to re-add the extension.
			path = "\\string\"" + base + "\\string\"." + ext;
		} else {
			path = "\\string\"" + path + "\\string\"";
		}
	}

	if (dots != ESCAPE_DOTS)
		return path;

	// Replace dots with the lyxdot macro, but only in the file name,
	// not the directory part.
	// addName etc call os::internal_path internally
	// so don't use them for path manipulation
	// The directory separator is always '/' for LaTeX.
	string::size_type pos = path.rfind('/');
	if (pos == string::npos)
		return subst(path, ".", "\\lyxdot ");
	return path.substr(0, pos) + subst(path.substr(pos), ".", "\\lyxdot ");
}


// Substitutes spaces with underscores in filename (and path)
FileName const makeLatexName(FileName const & file)
{
	string name = file.onlyFileName();
	string const path = file.onlyPath().absFileName() + "/";

	// ok so we scan through the string twice, but who cares.
	// FIXME: in Unicode time this will break for sure! There is
	// a non-latin world out there...
	string const keep = "abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"@!'()*+,-./0123456789:;<=>?[]`|";

	string::size_type pos = 0;
	while ((pos = name.find_first_not_of(keep, pos)) != string::npos)
		name[pos++] = '_';

	FileName latex_name(path + name);
	latex_name.changeExtension(".tex");
	return latex_name;
}


string const quoteName(string const & name, quote_style style)
{
	switch(style) {
	case quote_shell:
		// This does not work on native Windows for filenames
		// containing the following characters < > : " / \ | ? *
		// Moreover, it can't be made to work, as, according to
		// http://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx
		// those are reserved characters, and thus are forbidden.
		// Please, also note that the command-line parser in
		// ForkedCall::generateChild cannot deal with filenames
		// containing " or ', therefore we don't pass user filenames
		// to child processes if possible. We store them in a python
		// script instead, where we don't have these limitations.
#ifndef USE_QPROCESS
		return (os::shell() == os::UNIX) ?
			'\'' + subst(name, "'", "\'\\\'\'") + '\'' :
			'"' + name + '"';
#else
		// According to the QProcess parser, a single double
		// quote is represented by three consecutive ones.
		// Here we simply escape the double quote and let our
		// simple parser in Systemcall.cpp do the substitution.
		return '"' + subst(name, "\"", "\\\"") + '"';
#endif
	case quote_shell_filename:
		return quoteName(os::external_path(name), quote_shell);
	case quote_python:
		return "\"" + subst(subst(name, "\\", "\\\\"), "\"", "\\\"")
		     + "\"";
	}
	// shut up stupid compiler
	return string();
}


#if 0
// Uses a string of paths separated by ";"s to find a file to open.
// Can't cope with pathnames with a ';' in them. Returns full path to file.
// If path entry begins with $$LyX/, use system_lyxdir
// If path entry begins with $$User/, use user_lyxdir
// Example: "$$User/doc;$$LyX/doc"
FileName const fileOpenSearch(string const & path, string const & name,
			     string const & ext)
{
	FileName real_file;
	string path_element;
	bool notfound = true;
	string tmppath = split(path, path_element, ';');

	while (notfound && !path_element.empty()) {
		path_element = os::internal_path(path_element);
		if (!suffixIs(path_element, '/'))
			path_element += '/';
		path_element = subst(path_element, "$$LyX",
				     package().system_support().absFileName());
		path_element = subst(path_element, "$$User",
				     package().user_support().absFileName());

		real_file = fileSearch(path_element, name, ext);

		if (real_file.empty()) {
			do {
				tmppath = split(tmppath, path_element, ';');
			} while (!tmppath.empty() && path_element.empty());
		} else {
			notfound = false;
		}
	}
	return real_file;
}
#endif


// Returns the real name of file name in directory path, with optional
// extension ext.
FileName const fileSearch(string const & path, string const & name,
			  string const & exts, search_mode mode)
{
	// if `name' is an absolute path, we ignore the setting of `path'
	// Expand Environmentvariables in 'name'
	string const tmpname = replaceEnvironmentPath(name);
	FileName fullname(makeAbsPath(tmpname, path));
	// search first without extension, then with it.
	if (fullname.isReadableFile())
		return fullname;
	if (exts.empty())
		// We are done.
		return mode == may_not_exist ? fullname : FileName();
	int n = 0;
	string ext = token(exts, ',', n);
	while (!ext.empty()) {
		// Only add the extension if it is not already the extension of
		// fullname.
		bool addext = getExtension(fullname.absFileName()) != ext;
		if (addext) {
			if (mode == check_hidpi) {
				FileName fullname2x = FileName(addExtension(fullname.absFileName() + "@2x", ext));
				if (fullname2x.isReadableFile())
					return fullname2x;
			}
			fullname = FileName(addExtension(fullname.absFileName(), ext));
		}
		if (fullname.isReadableFile() || mode == may_not_exist)
			return fullname;
		if (addext)
			fullname.changeExtension("");
		ext = token(exts, ',', ++n);
	}
	return FileName();
}


// Search the file name.ext in the subdirectory dir of
//   1) user_lyxdir
//   2) build_lyxdir (if not empty)
//   3) system_lyxdir
FileName const libFileSearch(string const & dir, string const & name,
			   string const & ext, search_mode mode,
			   bool const only_global)
{
	FileName fullname;
	if (!only_global) {
		fullname = fileSearch(addPath(package().user_support().absFileName(), dir),
					     name, ext, mode);
		if (!fullname.empty())
			return fullname;
	}

	if (!package().build_support().empty())
		fullname = fileSearch(addPath(package().build_support().absFileName(), dir),
				      name, ext, mode);
	if (!fullname.empty())
		return fullname;

	return fileSearch(addPath(package().system_support().absFileName(), dir),
				      name, ext, mode);
}


FileName const i18nLibFileSearch(string const & dir, string const & name,
		  string const & ext)
{
	// if the LANGUAGE variable is set, use it as a fallback for searching for files.
	string lang = getGuiMessages().language();
	string const language = getEnv("LANGUAGE");
	if (!language.empty())
		lang += ":" + language;

	for (auto const & l : getVectorFromString(lang, ":")) {
		FileName tmp;
		// First try with the full name
		// `en' files are not in a subdirectory
		if (l == "en")
			tmp = libFileSearch(dir, name, ext);
		else
			tmp = libFileSearch(addPath(dir, l), name, ext);
		if (!tmp.empty())
			return tmp;

		// Then the name without country code
		string const shortl = token(l, '_', 0);
		if (shortl != l) {
			tmp = libFileSearch(addPath(dir, shortl), name, ext);
			if (!tmp.empty())
				return tmp;
		}
	}

	return libFileSearch(dir, name, ext);
}


FileName const imageLibFileSearch(string & dir, string const & name,
		  string const & ext, search_mode mode, bool const dark_mode)
{
	if (!lyx::lyxrc.icon_set.empty()) {
		string const imagedir = addPath(dir, lyx::lyxrc.icon_set);
		// In dark mode, try "darkmode" subdirectory first
		string const darkimagedir = addPath(imagedir, "darkmode");
		FileName fn = dark_mode ? libFileSearch(darkimagedir, name, ext, mode)
					: libFileSearch(imagedir, name, ext, mode);
		if (!fn.exists() && dark_mode)
			fn = libFileSearch(imagedir, name, ext, mode);
		if (fn.exists()) {
			dir = imagedir;
			return fn;
		}
	}
	return libFileSearch(dir, name, ext, mode);
}


string const commandPrep(string const & command_in)
{
	static string const token_scriptpath = "$$s/";
	string const python_call = os::python();

	string command = command_in;
	if (prefixIs(command_in, python_call))
		command = os::python() + command_in.substr(python_call.length());

	// Find the starting position of "$$s/"
	string::size_type const pos1 = command.find(token_scriptpath);
	if (pos1 == string::npos)
		return command;
	// Find the end of the "$$s/some_subdir/some_script" word within
	// command. Assumes that the script name does not contain spaces.
	string::size_type const start_script = pos1 + 4;
	string::size_type const pos2 = command.find(' ', start_script);
	string::size_type const size_script = pos2 == string::npos?
		(command.size() - start_script) : pos2 - start_script;

	// Does this script file exist?
	string const script =
		libFileSearch(".", command.substr(start_script, size_script)).absFileName();

	if (script.empty()) {
		// Replace "$$s/" with ""
		command.erase(pos1, 4);
	} else {
		quote_style style = quote_shell;
		if (prefixIs(command, os::python()))
			style = quote_python;

		// Replace "$$s/foo/some_script" with "<path to>/some_script".
		string::size_type const size_replace = size_script + 4;
		command.replace(pos1, size_replace, quoteName(script, style));
	}

	return command;
}


FileName const tempFileName(FileName const & tempdir, string const & mask, bool const dir)
{
	return tempFileName(TempFile(tempdir, mask).name(), dir);
}


FileName const tempFileName(string const & mask, bool const dir)
{
	return tempFileName(TempFile(mask).name(), dir);
}


FileName const tempFileName(FileName tempfile, bool const dir)
{
	// Since the QTemporaryFile object is destroyed at function return
	// (which is what is intended here), the next call to this function
	// may return the same file name again.
	// Thus, in order to prevent race conditions, we track returned names
	// and create our own unique names if QTemporaryFile returns a name again.
	if (tmp_names_.find(tempfile.absFileName()) == tmp_names_.end()) {
		tmp_names_.insert(tempfile.absFileName());
		return tempfile;
	}

	// OK, we need another name. Simply append digits.
	FileName tmp = tempfile;
	string ext;
	if (!dir) {
		// Store and remove extensions
		ext = "." + tempfile.extension();
		tmp.changeExtension("");
	}
	for (int i = 1; i < INT_MAX ;++i) {
		// Append digit to filename and re-add extension
		string const new_fn =
			tmp.absFileName() + convert<string>(i) + ext;
		if (tmp_names_.find(new_fn) == tmp_names_.end()) {
			tmp_names_.insert(new_fn);
			tempfile.set(new_fn);
			return tempfile;
		}
	}

	// This should not happen!
	LYXERR0("tempFileName(): Could not create unique temp file name!");
	return tempfile;
}


void removeTempFile(FileName const & fn)
{
	if (!fn.exists())
		return;

	string const abs = fn.absFileName();
	tmp_names_.erase(abs);
	fn.removeFile();
}


static FileName createTmpDir(FileName const & tempdir, string const & mask)
{
	LYXERR(Debug::FILES, "createTmpDir: tempdir=`" << tempdir << "'\n"
		<< "createTmpDir:    mask=`" << mask << '\'');

	QFileInfo tmp_fi(QDir(toqstr(tempdir.absFileName())), toqstr(mask));
	FileName const tmpfl =
		tempFileName(FileName(fromqstr(tmp_fi.absolutePath())),
			     fromqstr(tmp_fi.fileName()) + ".XXXXXXXXXXXX", true);

	if (tmpfl.empty() || !tmpfl.createDirectory(0700)) {
		LYXERR0("LyX could not create temporary directory in " << tempdir
			<< "'");
		return FileName();
	}

	return tmpfl;
}


FileName const createLyXTmpDir(FileName const & deflt)
{
	if (deflt.empty() || deflt == package().system_temp_dir())
		return createTmpDir(package().system_temp_dir(), "lyx_tmpdir");

	if (deflt.createDirectory(0777))
		return deflt;

	if (deflt.isDirWritable()) {
		// deflt could not be created because it
		// did exist already, so let's create our own
		// dir inside deflt.
		return createTmpDir(deflt, "lyx_tmpdir");
	} else {
		// some other error occurred.
		return createTmpDir(package().system_temp_dir(), "lyx_tmpdir");
	}
}


// Strip filename from path name
string const onlyPath(string const & filename)
{
	// If empty filename, return empty
	if (filename.empty())
		return filename;

	// Find last / or start of filename
	size_t j = filename.rfind('/');
	return j == string::npos ? "./" : filename.substr(0, j + 1);
}


// Convert relative path into absolute path based on a basepath.
// If relpath is absolute, just use that.
// If basepath is empty, use CWD as base.
// Note that basePath can be a relative path, in the sense that it may
// not begin with "/" (e.g.), but it should NOT contain such constructs
// as "/../".
// FIXME It might be nice if the code didn't simply assume that.
FileName const makeAbsPath(string const & relPath, string const & basePath)
{
	// checks for already absolute path
	if (FileName::isAbsolute(relPath))
		return FileName(relPath);

	// Copies given paths
	string tempRel = os::internal_path(relPath);
	// Since TempRel is NOT absolute, we can safely replace "//" with "/"
	tempRel = subst(tempRel, "//", "/");

	string tempBase;

	if (FileName::isAbsolute(basePath))
		tempBase = basePath;
	else
		tempBase = addPath(FileName::getcwd().absFileName(), basePath);

	// Handle /./ at the end of the path
	while (suffixIs(tempBase, "/./"))
		tempBase.erase(tempBase.length() - 2);

	// processes relative path
	string rTemp = tempRel;
	string temp;

	// Check for a leading "~"
	// Split by first /
	rTemp = split(rTemp, temp, '/');
	if (temp == "~") {
		tempBase = Package::get_home_dir().absFileName();
		tempRel = rTemp;
	}

	rTemp = tempRel;
	while (!rTemp.empty()) {
		// Split by next /
		rTemp = split(rTemp, temp, '/');

		if (temp == ".") continue;
		if (temp == "..") {
			// Remove one level of TempBase
			if (tempBase.length() <= 1) {
				//this is supposed to be an absolute path, so...
				tempBase = "/";
				continue;
			}
			//erase a trailing slash if there is one
			if (suffixIs(tempBase, "/"))
				tempBase.erase(tempBase.length() - 1, string::npos);

			string::size_type i = tempBase.length() - 1;
			while (i > 0 && tempBase[i] != '/')
				--i;
			if (i > 0)
				tempBase.erase(i, string::npos);
			else
				tempBase = '/';
		} else if (temp.empty() && !rTemp.empty()) {
				tempBase = os::current_root() + rTemp;
				rTemp.erase();
		} else {
			// Add this piece to TempBase
			if (!suffixIs(tempBase, '/'))
				tempBase += '/';
			tempBase += temp;
		}
	}

	// returns absolute path
	return FileName(tempBase);
}


// Correctly append filename to the pathname.
// If pathname is '.', then don't use pathname.
// Chops any path of filename.
string const addName(string const & path, string const & fname)
{
	string const basename = onlyFileName(fname);
	string buf;

	if (path != "." && path != "./" && !path.empty()) {
		buf = os::internal_path(path);
		if (!suffixIs(buf, '/'))
			buf += '/';
	}

	return buf + basename;
}


string const addPathName(std::string const & path, std::string const & fname)
{
	string const pathpart = onlyPath(fname);
	string const namepart = onlyFileName(fname);
	string newpath = path;
	if (!pathpart.empty())
		newpath = addPath(newpath, pathpart);
	if (!namepart.empty())
		newpath = addName(newpath, namepart);
	return newpath;
}


// Strips path from filename
string const onlyFileName(string const & fname)
{
	if (fname.empty())
		return fname;

	string::size_type j = fname.rfind('/');
	if (j == string::npos) // no '/' in fname
		return fname;

	// Strip to basename
	return fname.substr(j + 1);
}


// Search the string for ${VAR} and $VAR and replace VAR using getenv.
// If VAR does not exist, ${VAR} and $VAR are left as is in the string.
string const replaceEnvironmentPath(string const & path)
{
	if (!contains(path, '$'))
		return path;

	// ${VAR} is defined as
	// $\{[A-Za-z_][A-Za-z_0-9]*\}
	static string const envvar_br = "[$]\\{([A-Za-z_][A-Za-z_0-9]*)\\}";

	// $VAR is defined as:
	// $[A-Za-z_][A-Za-z_0-9]*
	static string const envvar = "[$]([A-Za-z_][A-Za-z_0-9]*)";

	// Coverity thinks that the regex constructor can return an
	// exception. We know that it is not true since our regex are
	// hardcoded, but we have to protect against that nevertheless.
	try {
		static regex const envvar_br_re("(.*)" + envvar_br + "(.*)");
		static regex const envvar_re("(.*)" + envvar + "(.*)");
		string result = path;
		while (1) {
			smatch what;
			bool brackets = true;
			if (!regex_match(result, what, envvar_br_re)) {
				brackets = false;
				if (!regex_match(result, what, envvar_re))
					break;
			}
			string env_var = getEnv(what.str(2));
			if (env_var.empty()) {
				// temporarily use alert/bell (0x07) in place of $
				if (brackets)
					env_var = "\a{" + what.str(2) + '}';
				else
					env_var = "\a" + what.str(2);
			}
			result = what.str(1) + env_var + what.str(3);
		}
		return subst(result, '\a', '$');
	} catch (exception const & e) {
		LYXERR0("Something is very wrong: " << e.what());
		return path;
	}
}


// Return a command prefix for setting the environment of the TeX engine.
string latexEnvCmdPrefix(string const & path, string const & lpath)
{
	bool use_lpath = !(lpath.empty() || lpath == "." || lpath == "./");

	if (path.empty() || (lyxrc.texinputs_prefix.empty() && !use_lpath))
		return string();

	string texinputs_prefix = lyxrc.texinputs_prefix.empty() ? string()
		: os::latex_path_list(
			replaceCurdirPath(path, lyxrc.texinputs_prefix));
	string const allother_prefix = os::latex_path_list(path);
	string const sep = string(1, os::path_separator(os::TEXENGINE));
	string const texinputs = getEnv("TEXINPUTS");
	string const bibinputs = getEnv("BIBINPUTS");
	string const bstinputs = getEnv("BSTINPUTS");
	string const texfonts = getEnv("TEXFONTS");

	if (use_lpath) {
		string const abslpath = FileName::isAbsolute(lpath)
			? os::latex_path(lpath)
			: os::latex_path(FileName(path + "/" + lpath).realPath());
		if (texinputs_prefix.empty())
			texinputs_prefix = abslpath;
		else if (suffixIs(texinputs_prefix, sep))
			texinputs_prefix.append(abslpath + sep);
		else
			texinputs_prefix.append(sep + abslpath);
	}

	if (os::shell() == os::UNIX)
		return "env TEXINPUTS=\"." + sep + texinputs_prefix
		                           + sep + texinputs + "\" "
		         + "BIBINPUTS=\"." + sep + allother_prefix
		                           + sep + bibinputs + "\" "
		         + "BSTINPUTS=\"." + sep + allother_prefix
		                           + sep + bstinputs + "\" "
		         + "TEXFONTS=\"."  + sep + allother_prefix
		                           + sep + texfonts + "\" ";
	else
		// NOTE: the dummy blank dirs are necessary to force the
		//       QProcess parser to quote the argument (see bug 9453)
		return "cmd /d /c set \"TEXINPUTS=." + sep + " "
		                                + sep + texinputs_prefix
		                                + sep + texinputs + "\" & "
		               + "set \"BIBINPUTS=." + sep + " "
		                                + sep + allother_prefix
		                                + sep + bibinputs + "\" & "
		               + "set \"BSTINPUTS=." + sep + " "
		                                + sep + allother_prefix
		                                + sep + bstinputs + "\" & "
		               + "set \"TEXFONTS=."  + sep + " "
		                                + sep + allother_prefix
		                                + sep + texfonts + "\" & ";
}


// Replace current directory in all elements of a path list with a given path.
string const replaceCurdirPath(string const & path, string const & pathlist)
{
	string const oldpathlist = replaceEnvironmentPath(pathlist);
	char const sep = os::path_separator();
	string newpathlist;

	for (size_t i = 0, k = 0; i != string::npos; k = i) {
		i = oldpathlist.find(sep, i);
		string p = oldpathlist.substr(k, i - k);
		if (FileName::isAbsolute(p)) {
			newpathlist += p;
		} else if (i > k) {
			size_t offset = 0;
			if (p == ".") {
				offset = 1;
			} else if (prefixIs(p, "./")) {
				offset = 2;
				while (p[offset] == '/')
					++offset;
			}
			newpathlist += addPath(path, p.substr(offset));
			if (suffixIs(p, "//"))
				newpathlist += '/';
		}
		if (i != string::npos) {
			newpathlist += sep;
			// Stop here if the last element is empty
			if (++i == oldpathlist.length())
				break;
		}
	}
	return newpathlist;
}


// Make relative path out of two absolute paths
docstring const makeRelPath(docstring const & abspath, docstring const & basepath)
// Makes relative path out of absolute path. If it is deeper than basepath,
// it's easy. If basepath and abspath share something (they are all deeper
// than some directory), it'll be rendered using ..'s. If they are completely
// different, then the absolute path will be used as relative path.
{
	docstring::size_type const abslen = abspath.length();
	docstring::size_type const baselen = basepath.length();

	docstring::size_type i = os::common_path(abspath, basepath);

	if (i == 0) {
		// actually no match - cannot make it relative
		return abspath;
	}

	// Count how many dirs there are in basepath above match
	// and append as many '..''s into relpath
	docstring buf;
	docstring::size_type j = i;
	while (j < baselen) {
		if (basepath[j] == '/') {
			if (j + 1 == baselen)
				break;
			buf += "../";
		}
		++j;
	}

	// Append relative stuff from common directory to abspath
	if (abspath[i] == '/')
		++i;
	for (; i < abslen; ++i)
		buf += abspath[i];
	// Remove trailing /
	if (suffixIs(buf, '/'))
		buf.erase(buf.length() - 1);
	// Substitute empty with .
	if (buf.empty())
		buf = '.';
	return buf;
}


// Append sub-directory(ies) to a path in an intelligent way
string const addPath(string const & path, string const & path_2)
{
	string buf;
	string const path2 = os::internal_path(path_2);

	if (!path.empty() && path != "." && path != "./") {
		buf = os::internal_path(path);
		if (path[path.length() - 1] != '/')
			buf += '/';
	}

	if (!path2.empty()) {
		string::size_type const p2start = path2.find_first_not_of('/');
		string::size_type const p2end = path2.find_last_not_of('/');
		string const tmp = path2.substr(p2start, p2end - p2start + 1);
		buf += tmp + '/';
	}
	return buf;
}


string const changeExtension(string const & oldname, string const & extension)
{
	string::size_type const last_slash = oldname.rfind('/');
	string::size_type last_dot = oldname.rfind('.');
	if (last_dot < last_slash && last_slash != string::npos)
		last_dot = string::npos;

	string ext;
	// Make sure the extension starts with a dot
	if (!extension.empty() && extension[0] != '.')
		ext= '.' + extension;
	else
		ext = extension;

	return os::internal_path(oldname.substr(0, last_dot) + ext);
}


string const removeExtension(string const & name)
{
	return changeExtension(name, string());
}


string const addExtension(string const & name, string const & extension)
{
	if (!extension.empty() && extension[0] != '.')
		return name + '.' + extension;
	return name + extension;
}


/// Return the extension of the file (not including the .)
string const getExtension(string const & name)
{
	string::size_type const last_slash = name.rfind('/');
	string::size_type const last_dot = name.rfind('.');
	if (last_dot != string::npos &&
	    (last_slash == string::npos || last_dot > last_slash))
		return name.substr(last_dot + 1,
				   name.length() - (last_dot + 1));
	else
		return string();
}


docstring const provideScheme(docstring const & name, docstring const & scheme)
{
	if (prefixIs(name, scheme + "://"))
		return name;
	QUrl url(toqstr(name));
	if (!url.scheme().isEmpty())
		// Has a scheme. Return as is.
		return name;
	if (scheme == from_ascii("doi")) {
		// check if it is the pure DOI (without URL)
		if (isDigitASCII(name[1]))
			return from_ascii("https://doi.org/") + name;
	}
	url.setScheme(toqstr(scheme));
	return qstring_to_ucs4(url.toString());
}


string const unzippedFileName(string const & zipped_file)
{
	string const ext = getExtension(zipped_file);
	if (ext == "gz" || ext == "z" || ext == "Z")
		return changeExtension(zipped_file, string());
	else if (ext == "svgz")
		return changeExtension(zipped_file, "svg");
	return onlyPath(zipped_file) + "unzipped_" + onlyFileName(zipped_file);
}


FileName const unzipFile(FileName const & zipped_file, string const & unzipped_file)
{
	FileName const tempfile = FileName(unzipped_file.empty() ?
		unzippedFileName(zipped_file.toFilesystemEncoding()) :
		unzipped_file);
	// Run gunzip
	string const command = "gunzip -c \"" +
		zipped_file.toFilesystemEncoding() + "\" > \"" +
		tempfile.toFilesystemEncoding() + "\"";
	Systemcall one;
	one.startscript(Systemcall::Wait, command);
	// test that command was executed successfully (anon)
	// yes, please do. (Lgb)
	return tempfile;
}


docstring const makeDisplayPath(string const & path, unsigned int threshold)
{
	string str = path;

	// Recode URL encoded chars.
	str = from_percent_encoding(str);

	// If file is from LyXDir, display it as if it were relative.
	string const system = package().system_support().absFileName();
	if (prefixIs(str, system) && str != system)
		return from_utf8("[" + str.erase(0, system.length()) + "]");

	// replace /home/blah with ~/
	string const home = Package::get_home_dir().absFileName();
	if (!home.empty() && prefixIs(str, home))
		str = subst(str, home, "~");

	if (str.length() <= threshold)
		return from_utf8(os::external_path(str));

	string const prefix = ".../";
	docstring dstr = from_utf8(str);
	docstring temp;

	while (dstr.length() > threshold)
		dstr = split(dstr, temp, '/');

	// Did we shorten everything away?
	if (dstr.empty()) {
		// Yes, filename itself is too long.
		// Pick the start and the end of the filename.
		docstring fstr = from_utf8(onlyFileName(path));
		dstr = fstr;
		if (support::truncateWithEllipsis(dstr, threshold / 2))
			dstr += fstr.substr(fstr.length() - threshold / 2 - 2,
								docstring::npos);
	}

	return from_utf8(os::external_path(prefix + to_utf8(dstr)));
}


#ifdef HAVE_READLINK
bool readLink(FileName const & file, FileName & link)
{
	string const encoded = file.toFilesystemEncoding();
#ifdef HAVE_DEF_PATH_MAX
	char linkbuffer[PATH_MAX + 1];
	ssize_t const nRead = ::readlink(encoded.c_str(),
				     linkbuffer, sizeof(linkbuffer) - 1);
	if (nRead <= 0)
		return false;
	linkbuffer[nRead] = '\0'; // terminator
#else
	vector<char> buf(1024);
	int nRead = -1;

	while (true) {
		nRead = ::readlink(encoded.c_str(), &buf[0], buf.size() - 1);
		if (nRead < 0) {
			return false;
		}
		if (static_cast<size_t>(nRead) < buf.size() - 1) {
			break;
		}
		buf.resize(buf.size() * 2);
	}
	buf[nRead] = '\0'; // terminator
	const char * linkbuffer = &buf[0];
#endif
	link = makeAbsPath(linkbuffer, onlyPath(file.absFileName()));
	return true;
}
#else
bool readLink(FileName const &, FileName &)
{
	return false;
}
#endif


cmd_ret const runCommand(string const & cmd)
{
	// FIXME: replace all calls to RunCommand with ForkedCall
	// (if the output is not needed) or the code in ISpell.cpp
	// (if the output is needed).

	// One question is if we should use popen or
	// create our own popen based on fork, exec, pipe
	// of course the best would be to have a
	// pstream (process stream), with the
	// variants ipstream, opstream

	if (verbose)
		lyxerr << "\nRunning: " << cmd << endl;
	else
		LYXERR(Debug::INFO,"Running: " << cmd);

#if defined (_WIN32)
	STARTUPINFO startup;
	PROCESS_INFORMATION process;
	SECURITY_ATTRIBUTES security;
	HANDLE in, out;
	FILE * inf = 0;
	bool err2out = false;
	string command;
	string const infile = trim(split(cmd, command, '<'), " \"");
	command = rtrim(command);
	if (suffixIs(command, "2>&1")) {
		command = rtrim(command, "2>&1");
		err2out = true;
	}
	string const cmdarg = "/d /c \"" + command + "\"";
	string const comspec = getEnv("COMSPEC");

	security.nLength = sizeof(SECURITY_ATTRIBUTES);
	security.bInheritHandle = TRUE;
	security.lpSecurityDescriptor = NULL;

	if (CreatePipe(&in, &out, &security, 0)) {
		memset(&startup, 0, sizeof(STARTUPINFO));
		memset(&process, 0, sizeof(PROCESS_INFORMATION));

		startup.cb = sizeof(STARTUPINFO);
		startup.dwFlags = STARTF_USESTDHANDLES;

		startup.hStdError = err2out ? out : GetStdHandle(STD_ERROR_HANDLE);
		startup.hStdInput = infile.empty()
			? GetStdHandle(STD_INPUT_HANDLE)
			: CreateFile(infile.c_str(), GENERIC_READ,
				FILE_SHARE_READ, &security, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL, NULL);
		startup.hStdOutput = out;

		if (startup.hStdInput != INVALID_HANDLE_VALUE &&
			CreateProcess(comspec.c_str(), (LPTSTR)cmdarg.c_str(),
				&security, &security, TRUE, CREATE_NO_WINDOW,
				0, 0, &startup, &process)) {

			CloseHandle(process.hThread);
			int fno = _open_osfhandle((intptr_t)in, _O_RDONLY);
			CloseHandle(out);
			inf = _fdopen(fno, "r");
		}
	}
#elif defined (HAVE_POPEN)
	FILE * inf = ::popen(cmd.c_str(), os::popen_read_mode());
#elif defined (HAVE__POPEN)
	FILE * inf = ::_popen(cmd.c_str(), os::popen_read_mode());
#else
#error No popen() function.
#endif

	// (Claus Hentschel) Check if popen was successful ;-)
	if (!inf) {
		lyxerr << "RunCommand: could not start child process" << endl;
		return { false, string() };
	}

	string result;
	int c = fgetc(inf);
	while (c != EOF) {
		result += static_cast<char>(c);
		c = fgetc(inf);
	}

#if defined (_WIN32)
	WaitForSingleObject(process.hProcess, INFINITE);
	DWORD pret;
	BOOL success = GetExitCodeProcess(process.hProcess, &pret);
	bool valid = (pret == 0) && success;
	if (!success)
		pret = -1;
	if (!infile.empty())
		CloseHandle(startup.hStdInput);
	CloseHandle(process.hProcess);
	if (fclose(inf) != 0)
		pret = -1;
#elif defined (HAVE_PCLOSE)
	int const pret = pclose(inf);
	bool const valid = (WEXITSTATUS(pret) == 0);
#elif defined (HAVE__PCLOSE)
	int const pret = _pclose(inf);
	bool const valid = (WEXITSTATUS(pret) == 0);
#else
#error No pclose() function.
#endif

	if (pret == -1)
		perror("RunCommand: could not terminate child process");

	return { valid, result };
}


FileName const findtexfile(string const & fil, string const & /*format*/,
						   bool const onlykpse)
{
	/* There is no problem to extend this function to use other
	   methods to look for files. It could be setup to look
	   in environment paths and also if wanted as a last resort
	   to a recursive find. One of the easier extensions would
	   perhaps be to use the LyX file lookup methods. But! I am
	   going to implement this until I see some demand for it.
	   Lgb
	*/

	// If the file can be found directly, we just return a
	// absolute path version of it.
	if (!onlykpse) {
		FileName const absfile(makeAbsPath(fil));
		if (absfile.exists())
			return absfile;
	}

	// Now we try to find it using kpsewhich.
	// It seems from the kpsewhich manual page that it is safe to use
	// kpsewhich without --format: "When the --format option is not
	// given, the search path used when looking for a file is inferred
	// from the name given, by looking for a known extension. If no
	// known extension is found, the search path for TeX source files
	// is used."
	// However, we want to take advantage of the format sine almost all
	// the different formats has environment variables that can be used
	// to control which paths to search. f.ex. bib looks in
	// BIBINPUTS and TEXBIB. Small list follows:
	// bib - BIBINPUTS, TEXBIB
	// bst - BSTINPUTS
	// graphic/figure - TEXPICTS, TEXINPUTS
	// ist - TEXINDEXSTYLE, INDEXSTYLE
	// pk - PROGRAMFONTS, PKFONTS, TEXPKS, GLYPHFONTS, TEXFONTS
	// tex - TEXINPUTS
	// tfm - TFMFONTS, TEXFONTS
	// This means that to use kpsewhich in the best possible way we
	// should help it by setting additional path in the approp. envir.var.
	string const kpsecmd = "kpsewhich " + fil;

	cmd_ret const c = runCommand(kpsecmd);

	LYXERR(Debug::OUTFILE, "kpse status = " << c.valid << '\n'
	                                        << "kpse result = `" << rtrim(c.result, "\n\r") << '\'');
	if (c.valid)
		return FileName(rtrim(to_utf8(from_filesystem8bit(c.result)), "\n\r"));
	else
		return FileName();
}


int compare_timestamps(FileName const & file1, FileName const & file2)
{
	// If the original is newer than the copy, then copy the original
	// to the new directory.

	int cmp = 0;
	if (file1.exists() && file2.exists()) {
		double const tmp = difftime(file1.lastModified(), file2.lastModified());
		if (tmp != 0)
			cmp = tmp > 0 ? 1 : -1;

	} else if (file1.exists()) {
		cmp = 1;
	} else if (file2.exists()) {
		cmp = -1;
	}

	return cmp;
}


bool prefs2prefs(FileName const & filename, FileName const & tempfile, bool lfuns)
{
	FileName const script = libFileSearch("scripts", "prefs2prefs.py");
	if (script.empty()) {
		LYXERR0("Could not find bind file conversion "
				"script prefs2prefs.py.");
		return false;
	}

	ostringstream command;
	command << os::python() << ' ' << quoteName(script.toFilesystemEncoding())
	  << ' ' << (lfuns ? "-l" : "-p") << ' '
		<< quoteName(filename.toFilesystemEncoding())
		<< ' ' << quoteName(tempfile.toFilesystemEncoding());
	string const command_str = command.str();

	LYXERR(Debug::FILES, "Running `" << command_str << '\'');

	cmd_ret const ret = runCommand(command_str);
	if (!ret.valid) {
		LYXERR0("Could not run file conversion script prefs2prefs.py.");
		return false;
	}
	return true;
}


bool configFileNeedsUpdate(string const & file)
{
	// We cannot initialize configure_script directly because the package
	// is not initialized yet when static objects are constructed.
	static FileName configure_script;
	static bool firstrun = true;
	if (firstrun) {
		configure_script =
			FileName(addName(package().system_support().absFileName(),
				"configure.py"));
		firstrun = false;
	}

	FileName absfile =
		FileName(addName(package().user_support().absFileName(), file));
	return !absfile.exists()
		|| configure_script.lastModified() > absfile.lastModified();
}


int fileLock(const char * lock_file)
{
	int fd = -1;
#if defined(HAVE_LOCKF)
	fd = open(lock_file, O_CREAT|O_APPEND|O_SYNC|O_RDWR, 0666);
	if (fd == -1)
		return -1;
	if (lockf(fd, F_LOCK, 0) != 0) {
		close(fd);
		return -1;
	}
#endif
	return fd;
}


void fileUnlock(int fd, const char * /* lock_file*/)
{
#if defined(HAVE_LOCKF)
	if (fd >= 0) {
		if (lockf(fd, F_ULOCK, 0))
			LYXERR0("Can't unlock the file.");
		close(fd);
	}
#endif
}


std::string toHexHash(const std::string & str, bool shorten)
{
	auto hashAlgo = QCryptographicHash::Sha256;

	QByteArray hash = QCryptographicHash::hash(toqstr(str).toLocal8Bit(), hashAlgo);
	QString qshash=QString(hash.toHex());

	/* For shortened case we take 12 leftmost chars (6 bytes encoded).
	 * Random experiment shows:
	 *  8 chars: 16 collisions for 10^5 graphic filenames
	 * 12 chars:  0 collisions for 10^5 graphic filenames
	 */
	if (shorten)
		qshash=qshash.left(12);

	return fromqstr(qshash);
}


std::string sanitizeFileName(const std::string & str)
{
	// The list of characters to keep is probably over-restrictive,
	// but it is not really a problem.
	// Apart from non-ASCII characters, at least the following characters
	// are forbidden: '/', '.', ' ', and ':'.
	// On windows it is not possible to create files with '<', '>' or '?'
	// in the name.
	static std::string const keep = "abcdefghijklmnopqrstuvwxyz"
	                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	                           "+-0123456789;=";

	std::string name = str;
	string::size_type pos = 0;
	while ((pos = name.find_first_not_of(keep, pos)) != string::npos)
		name[pos++] = '_';

	return name;
}

} // namespace support
} // namespace lyx
