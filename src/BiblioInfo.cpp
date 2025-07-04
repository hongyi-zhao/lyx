/**
 * \file BiblioInfo.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Angus Leeming
 * \author Herbert Voß
 * \author Richard Kimberly Heck
 * \author Julien Rioux
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "BiblioInfo.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "Citation.h"
#include "Encoding.h"
#include "Language.h"
#include "TextClass.h"
#include "TocBackend.h"
#include "xml.h"

#include "support/convert.h"
#include "support/debug.h"
#include "support/docstream.h"
#include "support/FileName.h"
#include "support/gettext.h"
#include "support/lassert.h"
#include "support/lstrings.h"
#include "support/textutils.h"

#include <map>
#include <regex>
#include <set>

using namespace std;
using namespace lyx::support;


namespace lyx {

namespace {

// Remove placeholders from names
docstring renormalize(docstring const & input)
{
	docstring res = subst(input, from_ascii("$$space!"), from_ascii(" "));
	return subst(res, from_ascii("$$comma!"), from_ascii(","));
}


// Split the surname into prefix ("von-part") and family name
pair<docstring, docstring> parseSurname(docstring const & sname)
{
	// Split the surname into its tokens
	vector<docstring> pieces = getVectorFromString(sname, from_ascii(" "));
	if (pieces.size() < 2)
		return make_pair(docstring(), sname);

	// Now we look for pieces that begin with a lower case letter.
	// All except for the very last token constitute the "von-part".
	docstring prefix;
	vector<docstring>::const_iterator it = pieces.begin();
	vector<docstring>::const_iterator const en = pieces.end();
	bool first = true;
	for (; it != en; ++it) {
		if ((*it).empty())
			continue;
		// If this is the last piece, then what we now have is
		// the family name, notwithstanding the casing.
		if (it + 1 == en)
			break;
		char_type const c = (*it)[0];
		// If the piece starts with a upper case char, we assume
		// this is part of the surname.
		if (!isLower(c))
			break;
		// Nothing of the former, so add this piece to the prename
		if (!first)
			prefix += " ";
		else
			first = false;
		prefix += *it;
	}

	// Reconstruct the family name.
	// Note that if we left the loop with because it + 1 == en,
	// then this will still do the right thing, i.e., make surname
	// just be the last piece.
	docstring surname;
	first = true;
	for (; it != en; ++it) {
		if (!first)
			surname += " ";
		else
			first = false;
		surname += *it;
	}
	return make_pair(prefix, surname);
}


struct name_parts {
	docstring surname;
	docstring prename;
	docstring suffix;
	docstring prefix;
};


// gets the name parts (prename, surname, prefix, suffix) from an author-type string
name_parts nameParts(docstring const & iname)
{
	name_parts res;
	if (iname.empty())
		return res;

	// First we check for goupings (via {...}) and replace blanks and
	// commas inside groups with temporary placeholders
	docstring name;
	int gl = 0;
	docstring::const_iterator p = iname.begin();
	while (p != iname.end()) {
		// count grouping level
		if (*p == '{')
			++gl;
		else if (*p == '}')
			--gl;
		// generate string with probable placeholders
		if (*p == ' ' && gl > 0)
			name += from_ascii("$$space!");
		else if (*p == ',' && gl > 0)
			name += from_ascii("$$comma!");
		else
			name += *p;
		++p;
	}

	// Now we look for a comma, and take the last name to be everything
	// preceding the right-most one, so that we also get the name suffix
	// (aka "jr" part).
	vector<docstring> pieces = getVectorFromString(name);
	if (pieces.size() > 1) {
		// Whether we have a name suffix or not, the prename is
		// always last item
		res.prename = renormalize(pieces.back());
		// The family name, conversely, is always the first item.
		// However, it might contain a prefix (aka "von" part)
		docstring const sname = pieces.front();
		res.prefix = renormalize(parseSurname(sname).first);
		res.surname = renormalize(parseSurname(sname).second);
		// If we have three pieces (the maximum allowed by BibTeX),
		// the second one is the name suffix.
		if (pieces.size() > 2)
			res.suffix = renormalize(pieces.at(1));
		return res;
	}

	// OK, so now we want to look for the last name.
	// Split on spaces, to get various tokens.
	pieces = getVectorFromString(name, from_ascii(" "));
	// No space: Only a family name given
	if (pieces.size() < 2) {
		res.surname = renormalize(pieces.back());
		return res;
	}
	// If we get two pieces, assume "prename surname"
	if (pieces.size() == 2) {
		res.prename = renormalize(pieces.front());
		res.surname = renormalize(pieces.back());
		return res;
	}

	// More than 3 pieces: A name prefix (aka "von" part) might be included.
	// We look for the first piece that begins with a lower case letter
	// (which is the name prefix, if it is not the last token) or the last token.
	docstring prename;
	vector<docstring>::const_iterator it = pieces.begin();
	vector<docstring>::const_iterator const en = pieces.end();
	bool first = true;
	for (; it != en; ++it) {
		if ((*it).empty())
			continue;
		char_type const c = (*it)[0];
		// If the piece starts with a lower case char, we assume
		// this is the name prefix and thus prename is complete.
		if (isLower(c))
			break;
		// Same if this is the last piece, which is always the surname.
		if (it + 1 == en)
			break;
		// Nothing of the former, so add this piece to the prename
		if (!first)
			prename += " ";
		else
			first = false;
		prename += *it;
	}

	// Now reconstruct the family name and strip the prefix.
	// Note that if we left the loop because it + 1 == en,
	// then this will still do the right thing, i.e., make surname
	// just be the last piece.
	docstring surname;
	first = true;
	for (; it != en; ++it) {
		if (!first)
			surname += " ";
		else
			first = false;
		surname += *it;
	}
	res.prename = renormalize(prename);
	res.prefix = renormalize(parseSurname(surname).first);
	res.surname = renormalize(parseSurname(surname).second);
	return res;
}


docstring constructName(docstring const & name, string const & scheme)
{
	// re-constructs a name from name parts according
	// to a given scheme
	docstring const prename = nameParts(name).prename;
	docstring const surname = nameParts(name).surname;
	docstring const prefix = nameParts(name).prefix;
	docstring const suffix = nameParts(name).suffix;
	string res = scheme;
	static regex const reg1("(.*)(\\{%prename%\\[\\[)([^\\]]+)(\\]\\]\\})(.*)");
	static regex const reg2("(.*)(\\{%suffix%\\[\\[)([^\\]]+)(\\]\\]\\})(.*)");
	static regex const reg3("(.*)(\\{%prefix%\\[\\[)([^\\]]+)(\\]\\]\\})(.*)");
	smatch sub;
	// Changing the first parameter of regex_match() may corrupt the
	// second one. In this case we use the temporary string tmp.
	if (regex_match(scheme, sub, reg1)) {
		res = sub.str(1);
		if (!prename.empty())
			res += sub.str(3);
		res += sub.str(5);
	}
	if (regex_match(res, sub, reg2)) {
		string tmp = sub.str(1);
		if (!suffix.empty())
			tmp += sub.str(3);
		res = tmp + sub.str(5);
	}
	if (regex_match(res, sub, reg3)) {
		string tmp = sub.str(1);
		if (!prefix.empty())
			tmp += sub.str(3);
		res = tmp + sub.str(5);
	}
	docstring result = from_ascii(res);
	result = subst(result, from_ascii("%prename%"), prename);
	result = subst(result, from_ascii("%surname%"), surname);
	result = subst(result, from_ascii("%prefix%"), prefix);
	result = subst(result, from_ascii("%suffix%"), suffix);
	return result;
}


vector<docstring> const getAuthors(docstring const & author_in,
				   size_t const max_key_size)
{
	docstring author = author_in;
	// for the GUI (not xhtml output) we cut obscenely long
	// author lists as we won't display all authors anyway,
	// and these long lists impact heavily on performance
	// We take more than max_key_size, as we might have
	// some extra characters in here
	if (max_key_size < UINT_MAX && author.size() > 2 * max_key_size)
		author.resize(2 * max_key_size);

	// We check for goupings (via {...}) and only consider " and "
	// outside groups as author separator. This is to account
	// for cases such as {{Barnes and Noble, Inc.}}, which
	// need to be treated as one single family name.
	// We use temporary placeholders in order to differentiate the
	// diverse " and " cases.

	// First, we temporarily replace all ampersands. It is rather unusual
	// in author names, but can happen (consider cases such as "C \& A Corp.").
	docstring iname = subst(author, from_ascii("&"), from_ascii("$$amp!"));
	// Then, we temporarily make all " and " strings to ampersands in order
	// to handle them later on a per-char level. Note that arbitrary casing
	// ("And", "AND", "aNd", ...) is allowed in bibtex (#10465).
	iname = subst(iname, from_ascii(" and "), from_ascii(" & "), false);
	// Now we traverse through the string and replace the "&" by the proper
	// output in- and outside groups
	docstring name;
	int gl = 0;
	docstring::const_iterator p = iname.begin();
	while (p != iname.end()) {
		// count grouping level
		if (*p == '{')
			++gl;
		else if (*p == '}')
			--gl;
		// generate string with probable placeholders
		if (*p == '&') {
			if (gl > 0)
				// Inside groups, we output "and"
				name += from_ascii("and");
			else
				// Outside groups, we output a separator
				name += from_ascii("$$namesep!");
		}
		else
			name += *p;
		++p;
	}

	// re-insert the literal ampersands
	name = subst(name, from_ascii("$$amp!"), from_ascii("&"));

	// Now construct the actual vector
	return getVectorFromString(name, from_ascii(" $$namesep! "));
}


bool multipleAuthors(docstring const & author)
{
	return getAuthors(author, 128).size() > 1;
}


// Escape '<' and '>' and remove richtext markers (e.g. {!this is richtext!}) from a string.
docstring processRichtext(docstring const & str, bool richtext)
{
	docstring val = str;
	docstring ret;

	bool scanning_rich = false;
	while (!val.empty()) {
		char_type const ch = val[0];
		if (ch == '{' && val.size() > 1 && val[1] == '!') {
			// beginning of rich text
			scanning_rich = true;
			val = val.substr(2);
			continue;
		}
		if (scanning_rich && ch == '!' && val.size() > 1 && val[1] == '}') {
			// end of rich text
			scanning_rich = false;
			val = val.substr(2);
			continue;
		}
		if (richtext) {
			if (scanning_rich)
				ret += ch;
			else {
				// we need to escape '<' and '>'
				if (ch == '<')
					ret += "&lt;";
				else if (ch == '>')
					ret += "&gt;";
				else
					ret += ch;
			}
		} else if (!scanning_rich /* && !richtext */)
			ret += ch;
		// else the character is discarded, which will happen only if
		// richtext == false and we are scanning rich text
		val = val.substr(1);
	}
	return ret;
}

} // namespace


//////////////////////////////////////////////////////////////////////
//
// BibTeXInfo
//
//////////////////////////////////////////////////////////////////////

BibTeXInfo::BibTeXInfo(docstring const & key, docstring const & type)
	: is_bibtex_(true), bib_key_(key), num_bib_key_(0), entry_type_(type),
	  info_(), format_(), modifier_(0)
{}



docstring const BibTeXInfo::getAuthorOrEditorList(Buffer const * buf,
						  size_t const max_key_size,
						  bool amp, bool full, bool forceshort) const
{
	docstring author = operator[]("author");
	if (author.empty())
		author = operator[]("editor");

	return getAuthorList(buf, author, max_key_size, amp, full, forceshort);
}


docstring const BibTeXInfo::getAuthorList(Buffer const * buf,
		docstring const & author, size_t const max_key_size,
		bool const amp, bool const full, bool const forceshort,
		bool const allnames, bool const beginning) const
{
	// Maxnames treshold depend on engine
	size_t maxnames = buf ?
		buf->params().documentClass().max_citenames() : 2;

	if (!is_bibtex_) {
		docstring const opt = label();
		if (opt.empty())
			return docstring();

		docstring authors;
		docstring const remainder = trim(split(opt, authors, '('));
		if (remainder.empty())
			// in this case, we didn't find a "(",
			// so we don't have author (year)
			return docstring();
		if (full) {
			// Natbib syntax is "Jones et al.(1990)Jones, Baker, and Williams"
			docstring const fullauthors = trim(rsplit(remainder, ')'));
			if (!fullauthors.empty())
				return fullauthors;
		}
		return authors;
	}

	if (author.empty())
		return author;

	// OK, we've got some names. Let's format them.
	// Try to split the author list
	vector<docstring> const authors = getAuthors(author, max_key_size);

	docstring retval;

	CiteEngineType const engine_type = buf ? buf->params().citeEngineType()
					       : ENGINE_TYPE_DEFAULT;

	// These are defined in the styles
	string const etal =
		buf ? buf->params().documentClass().getCiteMacro(engine_type, "B_etal")
		    : " et al.";
	string const namesep =
		buf ? buf->params().documentClass().getCiteMacro(engine_type, "B_namesep")
		   : ", ";
	string lastnamesep = ", and ";
	if (buf)
		lastnamesep = amp ? buf->params().documentClass().getCiteMacro(engine_type, "B_lastampnamesep")
				  : buf->params().documentClass().getCiteMacro(engine_type, "B_lastnamesep");
	string pairnamesep = " and ";
	if (buf)
		pairnamesep = amp ? buf->params().documentClass().getCiteMacro(engine_type, "B_amppairnamesep")
				  : buf->params().documentClass().getCiteMacro(engine_type, "B_pairnamesep");
	string firstnameform =
			buf ? buf->params().documentClass().getCiteMacro(engine_type, "!firstnameform")
			     : "{%prefix%[[%prefix% ]]}%surname%{%suffix%[[, %suffix%]]}{%prename%[[, %prename%]]}";
	if (!beginning)
		firstnameform = buf ? buf->params().documentClass().getCiteMacro(engine_type, "!firstbynameform")
					     : "%prename% {%prefix%[[%prefix% ]]}%surname%{%suffix%[[, %suffix%]]}";
	string othernameform = buf ? buf->params().documentClass().getCiteMacro(engine_type, "!othernameform")
			     : "{%prefix%[[%prefix% ]]}%surname%{%suffix%[[, %suffix%]]}{%prename%[[, %prename%]]}";
	if (!beginning)
		othernameform = buf ? buf->params().documentClass().getCiteMacro(engine_type, "!otherbynameform")
					     : "%prename% {%prefix%[[%prefix% ]]}%surname%{%suffix%[[, %suffix%]]}";
	string citenameform = buf ? buf->params().documentClass().getCiteMacro(engine_type, "!citenameform")
			     : "{%prefix%[[%prefix% ]]}%surname%";

	// Shorten the list (with et al.) if forceshort is set
	// and the list can actually be shortened, else if maxcitenames
	// is passed and full is not set.
	bool shorten = forceshort && authors.size() > 1;
	vector<docstring>::const_iterator it = authors.begin();
	vector<docstring>::const_iterator en = authors.end();
	for (size_t i = 0; it != en; ++it, ++i) {
		if (i >= maxnames && !full) {
			shorten = true;
			break;
		}
		if (*it == "others") {
			retval += buf ? buf->B_(etal) : from_ascii(etal);
			break;
		}
		if (i > 0 && i == authors.size() - 1) {
			if (authors.size() == 2)
				retval += buf ? buf->B_(pairnamesep) : from_ascii(pairnamesep);
			else
				retval += buf ? buf->B_(lastnamesep) : from_ascii(lastnamesep);
		} else if (i > 0)
			retval += buf ? buf->B_(namesep) : from_ascii(namesep);
		if (allnames)
			retval += (i == 0) ? constructName(*it, firstnameform)
				: constructName(*it, othernameform);
		else
			retval += constructName(*it, citenameform);
	}
	if (shorten) {
		if (allnames)
			retval = constructName(authors[0], firstnameform) + (buf ? buf->B_(etal) : from_ascii(etal));
		else
			retval = constructName(authors[0], citenameform) + (buf ? buf->B_(etal) : from_ascii(etal));
	}

	return Encodings::convertLaTeXCommands(retval);
}


docstring const BibTeXInfo::getYear() const
{
	if (is_bibtex_) {
		// first try legacy year field
		docstring year = operator[]("year");
		if (!year.empty())
			return year;
		// now try biblatex's date field
		year = operator[]("date");
		// Format is [-]YYYY-MM-DD*/[-]YYYY-MM-DD*
		// We only want the years.
		static regex const yreg("[-]?([\\d]{4}).*");
		static regex const ereg(".*/[-]?([\\d]{4}).*");
		smatch sm;
		string const date = to_utf8(year);
		if (!regex_match(date, sm, yreg))
			// cannot parse year.
			return docstring();
		year = from_ascii(sm[1]);
		// check for an endyear
		if (regex_match(date, sm, ereg))
			year += char_type(0x2013) + from_ascii(sm[1]);
		return year;
	}

	docstring const opt = label();
	if (opt.empty())
		return docstring();

	docstring authors;
	docstring tmp = split(opt, authors, '(');
	if (tmp.empty())
		// we don't have author (year)
		return docstring();
	docstring year;
	tmp = split(tmp, year, ')');
	return year;
}


void BibTeXInfo::getLocators(docstring & doi, docstring & url, docstring & file) const
{
	if (is_bibtex_) {
		// get "doi" entry from citation record
		doi = operator[]("doi");
		if (!doi.empty() && !prefixIs(doi,from_ascii("http")))
			doi = "https://doi.org/" + doi;
		// get "url" entry from citation record
		url = operator[]("url");
		// get "file" entry from citation record
		file = operator[]("file");

		// Jabref case, "file" field has a format (depending on exporter):
		// Description:Location:Filetype;Description:Location:Filetype...
		// or simply:
		// Location;Location;...
		// We will strip out the locations and return an \n-separated list
		if (!file.empty()) {
			docstring filelist;
			vector<docstring> files = getVectorFromString(file, from_ascii(";"));
			for (auto const & f : files) {
				// first try if we have Description:Location:Filetype
				docstring ret, filedest, tmp;
				ret = split(f, tmp, ':');
				tmp = split(ret, filedest, ':');
				if (filedest.empty())
					// we haven't, so use the whole string
					filedest = f;
				// TODO howto deal with relative directories?
				FileName fn(to_utf8(filedest));
				if (fn.exists()) {
					if (!filelist.empty())
						filelist += '\n';
					filelist += "file:///" + filedest;
				}
			}
			if (!filelist.empty())
				file = filelist;
		}

		// kbibtex case, "localfile" field with format:
		// file1.pdf;file2.pdf
		// We will strip out the locations and return an \n-separated list
		docstring kfile;
		if (file.empty())
			kfile = operator[]("localfile");
		if (!kfile.empty()) {
			docstring filelist;
			vector<docstring> files = getVectorFromString(kfile, from_ascii(";"));
			for (auto const & f : files) {
				// TODO howto deal with relative directories?
				FileName fn(to_utf8(f));
				if (fn.exists()) {
					if (!filelist.empty())
						filelist += '\n';
					filelist = "file:///" + f;
				}
			}
			if (!filelist.empty())
				file = filelist;
		}

		if (!url.empty())
			return;

		// try biblatex specific fields, see its manual
		// 3.13.7 "Electronic Publishing Informationl"
		docstring eprinttype = operator[]("eprinttype");
		docstring eprint = operator[]("eprint");
		if (eprint.empty())
			return;

		if (eprinttype == "arxiv")
			url = "https://arxiv.org/abs/" + eprint;
		if (eprinttype == "jstor")
			url = "https://www.jstor.org/stable/" + eprint;
		if (eprinttype == "pubmed")
			url = "http://www.ncbi.nlm.nih.gov/pubmed/" + eprint;
		if (eprinttype == "hdl")
			url = "https://hdl.handle.net/" + eprint;
		if (eprinttype == "googlebooks")
			url = "http://books.google.com/books?id=" + eprint;

		return;
	}

	// Here can be handled the bibliography environment. All one could do
	// here is let LyX scan the entry for URL or HRef insets.
}


namespace {

docstring parseOptions(docstring const & format, string & optkey,
		    docstring & ifpart, docstring & elsepart);

// Calls parseOptions to deal with an embedded option, such as:
//   {%number%[[, no.~%number%]]}
// which must appear at the start of format. ifelsepart gets the
// whole of the option, and we return what's left after the option.
// we return format if there is an error.
docstring parseEmbeddedOption(docstring const & format, docstring & ifelsepart)
{
	LASSERT(format[0] == '{' && format[1] == '%', return format);
	string optkey;
	docstring ifpart;
	docstring elsepart;
	docstring const rest = parseOptions(format, optkey, ifpart, elsepart);
	if (format == rest) { // parse error
		LYXERR0("ERROR! Couldn't parse `" << format <<"'.");
		return format;
	}
	LASSERT(rest.size() <= format.size(),
		{ ifelsepart = docstring(); return format; });
	ifelsepart = format.substr(0, format.size() - rest.size());
	return rest;
}


// Gets a "clause" from a format string, where the clause is
// delimited by '[[' and ']]'. Returns what is left after the
// clause is removed, and returns format if there is an error.
docstring getClause(docstring const & format, docstring & clause)
{
	docstring fmt = format;
	// remove '[['
	fmt = fmt.substr(2);
	// we'll remove characters from the front of fmt as we
	// deal with them
	while (!fmt.empty()) {
		if (fmt[0] == ']' && fmt.size() > 1 && fmt[1] == ']') {
			// that's the end
			fmt = fmt.substr(2);
			break;
		}
		// check for an embedded option
		if (fmt[0] == '{' && fmt.size() > 1 && fmt[1] == '%') {
			docstring part;
			docstring const rest = parseEmbeddedOption(fmt, part);
			if (fmt == rest) {
				LYXERR0("ERROR! Couldn't parse embedded option in `" << format <<"'.");
				return format;
			}
			clause += part;
			fmt = rest;
		} else { // it's just a normal character
				clause += fmt[0];
				fmt = fmt.substr(1);
		}
	}
	return fmt;
}


// parse an options string, which must appear at the start of the
// format parameter. puts the parsed bits in optkey, ifpart, and
// elsepart and returns what's left after the option is removed.
// if there's an error, it returns format itself.
docstring parseOptions(docstring const & format, string & optkey,
		    docstring & ifpart, docstring & elsepart)
{
	LASSERT(format[0] == '{' && format[1] == '%', return format);
	// strip '{%'
	docstring fmt = format.substr(2);
	size_t pos = fmt.find('%'); // end of key
	if (pos == string::npos) {
		LYXERR0("Error parsing  `" << format <<"'. Can't find end of key.");
		return format;
	}
	optkey = to_utf8(fmt.substr(0, pos));
	fmt = fmt.substr(pos + 1);
	// [[format]] should be next
	if (fmt[0] != '[' || fmt[1] != '[') {
		LYXERR0("Error parsing  `" << format <<"'. Can't find '[[' after key.");
		return format;
	}

	docstring curfmt = fmt;
	fmt = getClause(curfmt, ifpart);
	if (fmt == curfmt) {
		LYXERR0("Error parsing  `" << format <<"'. Couldn't get if clause.");
		return format;
	}

	if (fmt[0] == '}') // we're done, no else clause
		return fmt.substr(1);

	// else part should follow
	if (fmt[0] != '[' || fmt[1] != '[') {
		LYXERR0("Error parsing  `" << format <<"'. Can't find else clause.");
		return format;
	}

	curfmt = fmt;
	fmt = getClause(curfmt, elsepart);
	// we should be done
	if (fmt == curfmt || fmt[0] != '}') {
		LYXERR0("Error parsing  `" << format <<"'. Can't find end of option.");
		return format;
	}
	return fmt.substr(1);
}


} // namespace

/* FIXME
Bug #9131 revealed an oddity in how we are generating citation information
when more than one key is given. We end up building a longer and longer format
string as we go, which we then have to re-parse, over and over and over again,
rather than generating the information for the individual keys and then putting
all of that together. We do that to deal with the way separators work, from what
I can tell, but it still feels like a hack. Fixing this would require quite a
bit of work, however.
*/
docstring BibTeXInfo::expandFormat(docstring const & format,
		BibTeXInfoList const & xrefs, int & counter, Buffer const & buf,
		CiteItem const & ci, bool next, bool second) const
{
	// incorrect use of macros could put us in an infinite loop
	static int const max_passes = 5000;
	// the use of overly large keys can lead to performance problems, due
	// to eventual attempts to convert LaTeX macros to unicode. See bug
	// #8944. By default, the size is limited to 128 (in CiteItem), but
	// for specific purposes (such as XHTML export), it needs to be enlarged
	// This is perhaps not the best solution, but it will have to do for now.
	size_t const max_keysize = ci.max_key_size;
	odocstringstream ret; // return value
	string key;
	bool scanning_key = false;
	bool scanning_rich = false;

	CiteEngineType const engine_type = buf.params().citeEngineType();
	docstring fmt = format;
	// we'll remove characters from the front of fmt as we
	// deal with them
	while (!fmt.empty()) {
		if (counter > max_passes) {
			LYXERR0("Recursion limit reached while parsing `"
			        << format << "'.");
			return _("ERROR!");
		}

		char_type thischar = fmt[0];
		if (thischar == '%') {
			// beginning or end of key
			if (scanning_key) {
				// end of key
				scanning_key = false;
				// so we replace the key with its value, which may be empty
				if (key[0] == '!') {
					// macro
					string const val =
						buf.params().documentClass().getCiteMacro(engine_type, key);
					fmt = from_utf8(val) + fmt.substr(1);
					counter += 1;
					continue;
				} else if (prefixIs(key, "B_")) {
					// a translatable bit (to the Buffer language)
					string const val =
						buf.params().documentClass().getCiteMacro(engine_type, key);
					docstring const trans =
						translateIfPossible(from_utf8(val), buf.params().language->code());
					ret << trans;
				} else if (key[0] == '_') {
					// a translatable bit (to the GUI language)
					string const val =
						buf.params().documentClass().getCiteMacro(engine_type, key);
					docstring const trans =
						translateIfPossible(from_utf8(val));
					ret << trans;
				} else {
					docstring const val =
						getValueForKey(key, buf, ci, xrefs, max_keysize);
					if (!scanning_rich)
						ret << from_ascii("{!<span class=\"bib-" + key + "\">!}");
					ret << val;
					if (!scanning_rich)
						ret << from_ascii("{!</span>!}");
				}
			} else {
				// beginning of key
				key.clear();
				scanning_key = true;
			}
		}
		else if (thischar == '{') {
			// beginning of option?
			if (scanning_key) {
				LYXERR0("ERROR: Found `{' when scanning key in `" << format << "'.");
				return _("ERROR!");
			}
			if (fmt.size() > 1) {
				if (fmt[1] == '%') {
					// it is the beginning of an optional format
					string optkey;
					docstring ifpart;
					docstring elsepart;
					docstring const newfmt =
						parseOptions(fmt, optkey, ifpart, elsepart);
					if (newfmt == fmt) // parse error
						return _("ERROR!");
					fmt = newfmt;
					docstring const val =
						getValueForKey(optkey, buf, ci, xrefs);
					if (optkey == "next" && next)
						ret << ifpart; // without expansion
					else if (optkey == "second" && second) {
						int newcounter = 0;
						ret << expandFormat(ifpart, xrefs, newcounter, buf,
							ci, next);
					} else if (!val.empty()) {
						int newcounter = 0;
						ret << expandFormat(ifpart, xrefs, newcounter, buf,
							ci, next);
					} else if (!elsepart.empty()) {
						int newcounter = 0;
						ret << expandFormat(elsepart, xrefs, newcounter, buf,
							ci, next);
					}
					// fmt will have been shortened for us already
					continue;
				}
				if (fmt[1] == '!') {
					// beginning of rich text
					scanning_rich = true;
					fmt = fmt.substr(2);
					ret << from_ascii("{!");
					continue;
				}
			}
			// we are here if '{' was not followed by % or !.
			// So it's just a character.
			ret << thischar;
		}
		else if (scanning_rich && thischar == '!'
		         && fmt.size() > 1 && fmt[1] == '}') {
			// end of rich text
			scanning_rich = false;
			fmt = fmt.substr(2);
			ret << from_ascii("!}");
			continue;
		}
		else if (scanning_key)
			key += char(thischar);
		else {
			try {
				ret.put(thischar);
			} catch (EncodingException & /* e */) {
				LYXERR0("Uncodable character '" << docstring(1, thischar) << " in citation label!");
			}
		}
		fmt = fmt.substr(1);
	} // for loop
	if (scanning_key) {
		LYXERR0("Never found end of key in `" << format << "'!");
		return _("ERROR!");
	}
	if (scanning_rich) {
		LYXERR0("Never found end of rich text in `" << format << "'!");
		return _("ERROR!");
	}
	return ret.str();
}


docstring const & BibTeXInfo::getInfo(BibTeXInfoList const & xrefs,
	Buffer const & buf, CiteItem const & ci, docstring const & format_in,
	bool const for_xhtml) const
{
	bool const richtext = ci.richtext;

	CiteEngineType const engine_type = buf.params().citeEngineType();
	DocumentClass const & dc = buf.params().documentClass();
	docstring const & format = format_in.empty()? 
				from_utf8(dc.getCiteFormat(engine_type, to_utf8(entry_type_)))
			      : format_in;

	if (format != format_) {
		// clear caches since format changed
		info_.clear();
		info_richtext_.clear();
		format_ = format;
	}

	if (!richtext && !info_.empty()) {
		info_ = Encodings::convertLaTeXCommands(processRichtext(info_, false), for_xhtml);
		return info_;
	}
	if (richtext && !info_richtext_.empty())
		return info_richtext_;

	if (!is_bibtex_) {
		BibTeXInfo::const_iterator it = find(from_ascii("ref"));
		info_ = it->second;
		return info_;
	}

	int counter = 0;
	info_ = expandFormat(format, xrefs, counter, buf,
		ci, false, false);

	if (info_.empty()) {
		// this probably shouldn't happen
		return info_;
	}

	if (richtext) {
		info_richtext_ = Encodings::convertLaTeXCommands(processRichtext(info_, true), for_xhtml);
		return info_richtext_;
	}

	info_ = Encodings::convertLaTeXCommands(processRichtext(info_, false), for_xhtml);
	return info_;
}


docstring const BibTeXInfo::getLabel(BibTeXInfoList const & xrefs,
	Buffer const & buf, docstring const & format,
	CiteItem const & ci, bool next, bool second) const
{
	docstring loclabel;

	int counter = 0;
	loclabel = expandFormat(format, xrefs, counter, buf, ci, next, second);

	if (!loclabel.empty() && !next) {
		loclabel = processRichtext(loclabel, ci.richtext);
		loclabel = Encodings::convertLaTeXCommands(loclabel);
	}

	return loclabel;
}


docstring const & BibTeXInfo::operator[](docstring const & field) const
{
	BibTeXInfo::const_iterator it = find(field);
	if (it != end())
		return it->second;
	static docstring const empty_value = docstring();
	return empty_value;
}


docstring const & BibTeXInfo::operator[](string const & field) const
{
	return operator[](from_ascii(field));
}


docstring BibTeXInfo::getValueForKey(string const & oldkey, Buffer const & buf,
	CiteItem const & ci, BibTeXInfoList const & xrefs, size_t maxsize) const
{
	// anything less is pointless
	LASSERT(maxsize >= 16, maxsize = 16);
	string key = oldkey;
	bool cleanit = false;
	if (prefixIs(oldkey, "clean:")) {
		key = oldkey.substr(6);
		cleanit = true;
	}

	docstring ret = operator[](key);
	if (ret.empty()) {
		docstring subtype;
		if (contains(key, ':'))
			subtype = from_ascii(token(key, ':', 1));
		// some special keys
		// FIXME: dialog, textbefore and textafter have nothing to do with this
		if (key == "dialog" && ci.context == CiteItem::Dialog)
			ret = from_ascii("x"); // any non-empty string will do
		else if (key == "export" && ci.context == CiteItem::Export)
			ret = from_ascii("x"); // any non-empty string will do
		else if (key == "ifstar" && ci.Starred)
			ret = from_ascii("x"); // any non-empty string will do
		else if (key == "ifqualified" && ci.isQualified)
			ret = from_ascii("x"); // any non-empty string will do
		else if (key == "entrytype")
			ret = entry_type_;
		else if (prefixIs(key, "ifentrytype:")
			 && from_ascii(key.substr(12)) == entry_type_)
			ret = from_ascii("x"); // any non-empty string will do
		else if (key == "key")
			ret = bib_key_;
		else if (key == "label")
			ret = label_;
		else if (key == "modifier" && modifier_ != 0)
			ret = modifier_;
		else if (key == "numericallabel")
			ret = cite_number_;
		else if (prefixIs(key, "ifstyle:")) {
			// Return whether we use a particular cite style
			vector<string> const stls = getVectorFromString(to_ascii(subtype));
			string const cs = buf.masterParams().biblatex_citestyle;
			for (auto const & s : stls) {
				if (cs == s) {
					ret = from_ascii("x"); // any non-empty string will do
					break;
				}
			}
		}
		else if (prefixIs(key, "ifmultiple:")) {
			// Return whether we have multiple authors
			docstring const kind = operator[](subtype);
			if (multipleAuthors(kind))
				ret = from_ascii("x"); // any non-empty string will do
		}
		else if (prefixIs(key, "abbrvnames:")) {
			// Special key to provide abbreviated name list,
			// with respect to maxcitenames. Suitable for Bibliography
			// beginnings.
			bool const amp = prefixIs(subtype, '&');
			if (amp)
				subtype = subtype.substr(1);
			docstring const kind = operator[](subtype);
			ret = getAuthorList(&buf, kind, ci.max_key_size, amp, false, false, true);
			if (ci.forceUpperCase && isLowerCase(ret[0]))
				ret[0] = uppercase(ret[0]);
		} else if (prefixIs(key, "fullnames:")) {
			// Return a full name list. Suitable for Bibliography
			// beginnings.
			bool const amp = prefixIs(subtype, '&');
			if (amp)
				subtype = subtype.substr(1);
			docstring const kind = operator[](subtype);
			ret = getAuthorList(&buf, kind, ci.max_key_size, amp, true, false, true);
			if (ci.forceUpperCase && isLowerCase(ret[0]))
				ret[0] = uppercase(ret[0]);
		} else if (prefixIs(key, "forceabbrvnames:")) {
			// Special key to provide abbreviated name lists,
			// irrespective of maxcitenames. Suitable for Bibliography
			// beginnings.
			bool const amp = prefixIs(subtype, '&');
			if (amp)
				subtype = subtype.substr(1);
			docstring const kind = operator[](subtype);
			ret = getAuthorList(&buf, kind, ci.max_key_size, amp, false, true, true);
			if (ci.forceUpperCase && isLowerCase(ret[0]))
				ret[0] = uppercase(ret[0]);
		} else if (prefixIs(key, "abbrvbynames:")) {
			// Special key to provide abbreviated name list,
			// with respect to maxcitenames. Suitable for further names inside a
			// bibliography item // (such as "ed. by ...")
			bool const amp = prefixIs(subtype, '&');
			if (amp)
				subtype = subtype.substr(1);
			docstring const kind = operator[](subtype);
			ret = getAuthorList(&buf, kind, ci.max_key_size, amp, false, false, true, false);
			if (ci.forceUpperCase && isLowerCase(ret[0]))
				ret[0] = uppercase(ret[0]);
		} else if (prefixIs(key, "fullbynames:")) {
			// Return a full name list. Suitable for further names inside a
			// bibliography item // (such as "ed. by ...")
			bool const amp = prefixIs(subtype, '&');
			if (amp)
				subtype = subtype.substr(1);
			docstring const kind = operator[](subtype);
			ret = getAuthorList(&buf, kind, ci.max_key_size, amp, true, false, true, false);
			if (ci.forceUpperCase && isLowerCase(ret[0]))
				ret[0] = uppercase(ret[0]);
		} else if (prefixIs(key, "forceabbrvbynames:")) {
			// Special key to provide abbreviated name lists,
			// irrespective of maxcitenames. Suitable for further names inside a
			// bibliography item // (such as "ed. by ...")
			bool const amp = prefixIs(subtype, '&');
			if (amp)
				subtype = subtype.substr(1);
			docstring const kind = operator[](subtype);
			ret = getAuthorList(&buf, kind, ci.max_key_size, amp, false, true, true, false);
			if (ci.forceUpperCase && isLowerCase(ret[0]))
				ret[0] = uppercase(ret[0]);
		} else if (prefixIs(key, "abbrvciteauthor")) {
			// Special key to provide abbreviated author or
			// editor names (suitable for citation labels),
			// with respect to maxcitenames.
			bool const amp = suffixIs(key, "&");
			ret = getAuthorOrEditorList(&buf, ci.max_key_size, amp, false, false);
			if (ci.forceUpperCase && isLowerCase(ret[0]))
				ret[0] = uppercase(ret[0]);
		} else if (prefixIs(key, "fullciteauthor")) {
			// Return a full author or editor list (for citation labels)
			bool const amp = suffixIs(key, "&");
			ret = getAuthorOrEditorList(&buf, ci.max_key_size, amp, true, false);
			if (ci.forceUpperCase && isLowerCase(ret[0]))
				ret[0] = uppercase(ret[0]);
		} else if (prefixIs(key, "forceabbrvciteauthor")) {
			// Special key to provide abbreviated author or
			// editor names (suitable for citation labels),
			// irrespective of maxcitenames.
			bool const amp = suffixIs(key, "&");
			ret = getAuthorOrEditorList(&buf, ci.max_key_size, amp, false, true);
			if (ci.forceUpperCase && isLowerCase(ret[0]))
				ret[0] = uppercase(ret[0]);
		} else if (key == "bibentry") {
			// Special key to provide the full bibliography entry: see getInfo()
			CiteEngineType const engine_type = buf.params().citeEngineType();
			DocumentClass const & dc = buf.params().documentClass();
			docstring const & format =
				from_utf8(dc.getCiteFormat(engine_type, to_utf8(entry_type_), false));
			int counter = 0;
			ret = expandFormat(format, xrefs, counter, buf, ci, false, false);
		} else if (key == "textbefore")
			ret = ci.textBefore;
		else if (key == "textafter")
			ret = ci.textAfter;
		else if (key == "curpretext") {
			vector<pair<docstring, docstring>> pres = ci.getPretexts();
			vector<pair<docstring, docstring>>::iterator it = pres.begin();
			int numkey = 1;
			for (; it != pres.end() ; ++it) {
				if ((*it).first == bib_key_ && numkey == num_bib_key_) {
					ret = (*it).second;
					pres.erase(it);
					break;
				}
				if ((*it).first == bib_key_)
					++numkey;
			}
		} else if (key == "curposttext") {
			vector<pair<docstring, docstring>> posts = ci.getPosttexts();
			vector<pair<docstring, docstring>>::iterator it = posts.begin();
			int numkey = 1;
			for (; it != posts.end() ; ++it) {
				if ((*it).first == bib_key_ && numkey == num_bib_key_) {
					ret = (*it).second;
					posts.erase(it);
					break;
				}
				if ((*it).first == bib_key_)
					++numkey;
			}
		} else if (key == "year")
			ret = getYear();
		else if (key == "elp")
			// ellipsis
			ret = docstring(1, char_type(0x2026));
	}

	// If we have no result, check in the cross-ref'ed entries
	if (ret.empty() && !xrefs.empty()) {
		bool const biblatex =
			buf.params().documentClass().citeFramework() == "biblatex";
		// xr is a (reference to a) BibTeXInfo const *
		for (auto const & xr : xrefs) {
			if (!xr)
				continue;
			// use empty BibTeXInfoList to avoid loops
			BibTeXInfoList xr_dummy;
			ret = xr->getValueForKey(oldkey, buf, ci, xr_dummy, maxsize);
			if (!ret.empty())
				// success!
				break;
			// in biblatex, cross-ref'ed titles are mapped
			// to booktitle. Same for subtitle etc.
			if (biblatex && prefixIs(key, "book"))
				ret = (*xr)[key.substr(4)];
			// likewise, author is maped onto bookauthor
			else if (biblatex && contains(key, ":bookauthor"))
				ret = xr->getValueForKey(subst(key, "bookauthor", "author"),
							 buf, ci, xr_dummy, maxsize);
			if (!ret.empty())
				// success!
				break;
		}
	}

	if (cleanit)
		ret = xml::cleanAttr(ret);

	// make sure it is not too big
	support::truncateWithEllipsis(ret, maxsize);
	return ret;
}


//////////////////////////////////////////////////////////////////////
//
// BiblioInfo
//
//////////////////////////////////////////////////////////////////////

namespace {

// A functor for use with sort, leading to case insensitive sorting
bool compareNoCase(const docstring & a, const docstring & b) {
	return compare_no_case(a, b) < 0;
}

} // namespace


vector<docstring> const BiblioInfo::getXRefs(BibTeXInfo const & data, bool const nested) const
{
	vector<docstring> result;
	if (!data.isBibTeX())
		return result;
	// Legacy crossref field. This is not nestable.
	if (!nested && !data["crossref"].empty()) {
		docstring const xrefkey = data["crossref"];
		result.push_back(xrefkey);
		// However, check for nested xdatas
		BiblioInfo::const_iterator it = find(xrefkey);
		if (it != end()) {
			BibTeXInfo const & xref = it->second;
			vector<docstring> const nxdata = getXRefs(xref, true);
			if (!nxdata.empty())
				result.insert(result.end(), nxdata.begin(), nxdata.end());
		}
	}
	// Biblatex's xdata field. Infinitely nestable.
	// XData field can consist of a comma-separated list of keys
	vector<docstring> const xdatakeys = getVectorFromString(data["xdata"]);
	if (!xdatakeys.empty()) {
		for (auto const & xdatakey : xdatakeys) {
			result.push_back(xdatakey);
			BiblioInfo::const_iterator it = find(xdatakey);
			if (it != end()) {
				BibTeXInfo const & xdata = it->second;
				vector<docstring> const nxdata = getXRefs(xdata, true);
				if (!nxdata.empty())
					result.insert(result.end(), nxdata.begin(), nxdata.end());
			}
		}
	}
	return result;
}


vector<docstring> const BiblioInfo::getKeys() const
{
	vector<docstring> bibkeys;
	for (auto const & bi : *this)
		bibkeys.push_back(bi.first);
	sort(bibkeys.begin(), bibkeys.end(), &compareNoCase);
	return bibkeys;
}


vector<docstring> const BiblioInfo::getFields() const
{
	vector<docstring> bibfields;
	for (auto const & fn : field_names_)
		bibfields.push_back(fn);
	sort(bibfields.begin(), bibfields.end());
	return bibfields;
}


vector<docstring> const BiblioInfo::getEntries() const
{
	vector<docstring> bibentries;
	for (auto const & et : entry_types_)
		bibentries.push_back(et);
	sort(bibentries.begin(), bibentries.end());
	return bibentries;
}


docstring const BiblioInfo::getAuthorOrEditorList(docstring const & key, Buffer const & buf,
						  size_t const max_key_size) const
{
	BiblioInfo::const_iterator it = find(key);
	if (it == end())
		return docstring();
	BibTeXInfo const & data = it->second;
	return data.getAuthorOrEditorList(&buf, max_key_size, false, false);
}


docstring const BiblioInfo::getCiteNumber(docstring const & key) const
{
	BiblioInfo::const_iterator it = find(key);
	if (it == end())
		return docstring();
	BibTeXInfo const & data = it->second;
	return data.citeNumber();
}

void BiblioInfo::getLocators(docstring const & key, docstring & doi, docstring & url, docstring & file) const
{
	BiblioInfo::const_iterator it = find(key);
	 if (it == end())
		return;
	BibTeXInfo const & data = it->second;
	data.getLocators(doi,url,file);
}


docstring const BiblioInfo::getYear(docstring const & key, bool use_modifier) const
{
	BiblioInfo::const_iterator it = find(key);
	if (it == end())
		return docstring();
	BibTeXInfo const & data = it->second;
	docstring year = data.getYear();
	if (year.empty()) {
		// let's try the crossrefs
		vector<docstring> const xrefs = getXRefs(data);
		if (xrefs.empty())
			// no luck
			return docstring();
		for (docstring const & xref : xrefs) {
			BiblioInfo::const_iterator const xrefit = find(xref);
			if (xrefit == end())
				continue;
			BibTeXInfo const & xref_data = xrefit->second;
			year = xref_data.getYear();
			if (!year.empty())
				// success!
				break;
		}
	}
	if (use_modifier && data.modifier() != 0)
		year += data.modifier();
	return year;
}


docstring const BiblioInfo::getYear(docstring const & key, Buffer const & buf, bool use_modifier) const
{
	docstring const year = getYear(key, use_modifier);
	if (year.empty())
		return buf.B_("No year");
	return year;
}


docstring const BiblioInfo::getInfo(docstring const & key,
	Buffer const & buf, CiteItem const & ci, docstring const & format, bool const for_xhtml) const
{
	BiblioInfo::const_iterator it = find(key);
	if (it == end())
		return _("Bibliography entry not found!");
	BibTeXInfo const & data = it->second;
	BibTeXInfoList xrefptrs;
	for (docstring const & xref : getXRefs(data)) {
		BiblioInfo::const_iterator const xrefit = find(xref);
		if (xrefit != end())
			xrefptrs.push_back(&(xrefit->second));
	}
	return data.getInfo(xrefptrs, buf, ci, format, for_xhtml);
}


docstring const BiblioInfo::getLabel(vector<docstring> const & keys,
	Buffer const & buf, string const & style, CiteItem const & ci) const
{
	size_t max_size = ci.max_size;
	// shorter makes no sense
	LASSERT(max_size >= 16, max_size = 16);

	CiteEngineType const engine_type = buf.params().citeEngineType();
	DocumentClass const & dc = buf.params().documentClass();
	docstring const & format = from_utf8(dc.getCiteFormat(engine_type, style, false, "cite"));
	docstring ret = format;
	auto key = keys.begin();
	auto const ken = keys.end();
	vector<docstring> handled_keys;
	for (int i = 0; key != ken; ++key, ++i) {
		// we can't display more than 10 keys anyway, so keep 5 from
		// the start and 5 from the end.
		if (i == 5 && keys.size() > 10) {
			i = keys.size() - 5;
			key = ken - 5;
		}

		handled_keys.push_back(*key);
		int n = 0;
		for (auto const & k : handled_keys) {
			if (k == *key)
				++n;
		}
		BiblioInfo::const_iterator it = find(*key);
		BibTeXInfo empty_data;
		empty_data.key(*key);
		BibTeXInfo & data = empty_data;
		vector<BibTeXInfo const *> xrefptrs;
		if (it != end()) {
			data = it->second;
			for (docstring const & xref : getXRefs(data)) {
				BiblioInfo::const_iterator const xrefit = find(xref);
				if (xrefit != end())
					xrefptrs.push_back(&(xrefit->second));
			}
		}
		data.numKey(n);
		ret = data.getLabel(xrefptrs, buf, ret, ci, key + 1 != ken, i == 1);
	}

	support::truncateWithEllipsis(ret, max_size, true);

	return ret;
}


bool BiblioInfo::isBibtex(docstring const & key) const
{
	docstring key1;
	split(key, key1, ',');
	BiblioInfo::const_iterator it = find(key1);
	if (it == end())
		return false;
	return it->second.isBibTeX();
}


BiblioInfo::CiteStringMap const BiblioInfo::getCiteStrings(
	vector<docstring> const & keys, vector<CitationStyle> const & styles,
	Buffer const & buf, CiteItem const & ci) const
{
	if (empty())
		return vector<pair<docstring,docstring>>();

	vector<CitationStyle> realStyles;
	for (size_t i = 0; i != styles.size(); ++i) {
		// exclude variants that are not supported in the current style
		if (buf.masterParams().isActiveCiteStyle(styles[i]))
			realStyles.push_back(styles[i]);
	}

	string style;
	CiteStringMap csm(realStyles.size());
	for (size_t i = 0; i != csm.size(); ++i) {
		style = realStyles[i].name;
		csm[i] = make_pair(from_ascii(style), getLabel(keys, buf, style, ci));
	}

	return csm;
}


void BiblioInfo::mergeBiblioInfo(BiblioInfo const & info)
{
	bimap_.insert(info.begin(), info.end());
	field_names_.insert(info.field_names_.begin(), info.field_names_.end());
	entry_types_.insert(info.entry_types_.begin(), info.entry_types_.end());
}


namespace {

// used in xhtml to sort a list of BibTeXInfo objects
bool lSorter(BibTeXInfo const * lhs, BibTeXInfo const * rhs)
{
	docstring const lauth = lhs->getAuthorOrEditorList();
	docstring const rauth = rhs->getAuthorOrEditorList();
	docstring const lyear = lhs->getYear();
	docstring const ryear = rhs->getYear();
	docstring const ltitl = lhs->operator[]("title");
	docstring const rtitl = rhs->operator[]("title");
	return  (lauth < rauth)
		|| (lauth == rauth && lyear < ryear)
		|| (lauth == rauth && lyear == ryear && ltitl < rtitl);
}

} // namespace


void BiblioInfo::collectCitedEntries(Buffer const & buf)
{
	cited_entries_.clear();
	// We are going to collect all the citation keys used in the document,
	// getting them from the TOC.
	// FIXME We may want to collect these differently, in the first case,
	// so that we might have them in order of appearance.
	set<docstring> citekeys;
	Toc const & toc = *buf.tocBackend().toc("citation");
	for (auto const & t : toc) {
		if (t.str().empty())
			continue;
		vector<docstring> const keys = getVectorFromString(t.str());
		citekeys.insert(keys.begin(), keys.end());
	}
	if (citekeys.empty())
		return;

	// We have a set of the keys used in this document.
	// We will now convert it to a list of the BibTeXInfo objects used in
	// this document...
	vector<BibTeXInfo const *> bi;
	for (auto const & ck : citekeys) {
		BiblioInfo::const_iterator const bt = find(ck);
		if (bt == end() || !bt->second.isBibTeX())
			continue;
		bi.push_back(&(bt->second));
	}
	// ...and sort it.
	sort(bi.begin(), bi.end(), lSorter);

	// Now we can write the sorted keys
	// b is a BibTeXInfo const *
	for (auto const & b : bi)
		cited_entries_.push_back(b->key());
}


void BiblioInfo::makeCitationLabels(Buffer const & buf)
{
	collectCitedEntries(buf);
	CiteEngineType const engine_type = buf.params().citeEngineType();
	bool const numbers = (engine_type & ENGINE_TYPE_NUMERICAL);

	int keynumber = 0;
	char modifier = 0;
	// used to remember the last one we saw
	// we'll be comparing entries to see if we need to add
	// modifiers, like "1984a"
	map<docstring, BibTeXInfo>::iterator last = bimap_.end();

	// add letters to years
	for (auto const & ce : cited_entries_) {
		map<docstring, BibTeXInfo>::iterator const biit = bimap_.find(ce);
		// this shouldn't happen, but...
		if (biit == bimap_.end())
			// ...fail gracefully, anyway.
			continue;
		BibTeXInfo & entry = biit->second;
		if (numbers) {
			docstring const num = convert<docstring>(++keynumber);
			entry.setCiteNumber(num);
		} else {
			// The first test here is checking whether this is the first
			// time through the loop. If so, then we do not have anything
			// with which to compare.
			if (last != bimap_.end()
			     && entry.getAuthorOrEditorList() == last->second.getAuthorOrEditorList()
			     // we access the year via getYear() so as to get it from the xref,
			     // if we need to do so
			     && getYear(entry.key()) == getYear(last->second.key())) {
				if (modifier == 0) {
					// so the last one should have been 'a'
					last->second.setModifier('a');
					modifier = 'b';
				} else if (modifier == 'z')
					modifier = 'A';
				else
					modifier++;
			} else {
				modifier = 0;
			}
			entry.setModifier(modifier);
			// remember the last one
			last = biit;
		}
	}
	// Set the labels
	for (auto const & ce : cited_entries_) {
		map<docstring, BibTeXInfo>::iterator const biit = bimap_.find(ce);
		// this shouldn't happen, but...
		if (biit == bimap_.end())
			// ...fail gracefully, anyway.
			continue;
		BibTeXInfo & entry = biit->second;
		if (numbers) {
			entry.label(entry.citeNumber());
		} else {
			docstring const auth = entry.getAuthorOrEditorList(&buf, 128, false);
			// we do it this way so as to access the xref, if necessary
			// note that this also gives us the modifier
			docstring const year = getYear(ce, buf, true);
			if (!auth.empty() && !year.empty())
				entry.label(auth + ' ' + year);
			else
				entry.label(entry.key());
		}
	}
}


//////////////////////////////////////////////////////////////////////
//
// CitationStyle
//
//////////////////////////////////////////////////////////////////////


CitationStyle citationStyleFromString(string const & command,
                                      BufferParams const & params)
{
	CitationStyle cs;
	if (command.empty())
		return cs;

	string const alias = params.getCiteAlias(command);
	string cmd = alias.empty() ? command : alias;
	if (isUpperCase(command[0])) {
		cs.forceUpperCase = true;
		cmd[0] = lowercase(cmd[0]);
	}

	size_t const n = command.size() - 1;
	if (command[n] == '*') {
		cs.hasStarredVersion = true;
		if (suffixIs(cmd, '*'))
			cmd = cmd.substr(0, cmd.size() - 1);
	}

	cs.name = cmd;
	return cs;
}


string citationStyleToString(const CitationStyle & cs, bool const latex)
{
	string cmd = latex ? cs.cmd : cs.name;
	if (cs.forceUpperCase)
		cmd[0] = uppercase(cmd[0]);
	if (cs.hasStarredVersion)
		cmd += '*';
	return cmd;
}


void authorsToDocBookAuthorGroup(docstring const & authorsString, XMLStream & xs,
                                 Buffer const & buf, const std::string & type)
{
	// This function closely mimics getAuthorList, but produces DocBook instead of text.
	// It has been greatly simplified, as the complete list of authors is always produced. No separators are required,
	// as the output has a database-like shape.
	// constructName has also been merged within, as it becomes really simple and leads to no copy-paste.

	if (! type.empty() && (type != "author" && type != "book")) {
		LYXERR0("ERROR! Unexpected author contribution `" << type <<"'.");
		return;
	}

	if (authorsString.empty()) {
		return;
	}

	// Split the input list of authors into individual authors.
	vector<docstring> const authors = getAuthors(authorsString, UINT_MAX);

	// Retrieve the "et al." variation.
	string const etal = buf.params().documentClass().getCiteMacro(buf.params().citeEngineType(), "_etal");

	// Output the list of authors.
	xs << xml::StartTag("authorgroup");
	xs << xml::CR();

	auto it = authors.cbegin();
	auto en = authors.cend();
	for (size_t i = 0; it != en; ++it, ++i) {
		const std::string tag = (type.empty() || type == "author") ? "author" : "othercredit";
		const std::string attr = (type == "book") ? R"(class="other" otherclass="bookauthor")" : "";

		xs << xml::StartTag(tag, attr);
		xs << xml::CR();
		xs << xml::StartTag("personname");
		xs << xml::CR();
		const docstring name = *it;

		// All authors go in a <personname>. If more structure is known, use it; otherwise (just "et al."),
		// print it as such.
		if (name == "others") {
			xs << buf.B_(etal);
		} else {
			name_parts parts = nameParts(name);
			if (! parts.prefix.empty()) {
				xs << xml::StartTag("honorific");
				xs << parts.prefix;
				xs << xml::EndTag("honorific");
				xs << xml::CR();
			}
			if (! parts.prename.empty()) {
				xs << xml::StartTag("firstname");
				xs << parts.prename;
				xs << xml::EndTag("firstname");
				xs << xml::CR();
			}
			if (! parts.surname.empty()) {
				xs << xml::StartTag("surname");
				xs << parts.surname;
				xs << xml::EndTag("surname");
				xs << xml::CR();
			}
			if (! parts.suffix.empty()) {
				xs << xml::StartTag("othername", "role=\"suffix\"");
				xs << parts.suffix;
				xs << xml::EndTag("othername");
				xs << xml::CR();
			}
		}

		xs << xml::EndTag("personname");
		xs << xml::CR();
		xs << xml::EndTag(tag);
		xs << xml::CR();

		// Could add an affiliation after <personname>, but not stored in BibTeX.
	}
	xs << xml::EndTag("authorgroup");
	xs << xml::CR();
}

} // namespace lyx
