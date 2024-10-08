/**
 * \file Encoding.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 * \author Jean-Marc Lasgouttes
 * \author Dekel Tsur
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "Encoding.h"

#include "support/debug.h"
#include "support/docstring.h"
#include "support/gettext.h"
#include "support/Lexer.h"
#include "support/lstrings.h"
#include "support/mutex.h"
#include "support/textutils.h"
#include "support/unicode.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <regex>
#include <sstream>

using namespace std;
using namespace lyx::support;

namespace lyx {

int const Encoding::any = -1;

Encodings encodings;

Encodings::MathCommandSet Encodings::mathcmd;
Encodings::TextCommandSet Encodings::textcmd;
Encodings::MathSymbolSet  Encodings::mathsym;

namespace {

typedef map<char_type, CharInfo> CharInfoMap;
CharInfoMap unicodesymbols;

typedef set<char_type> CharSet;
typedef map<string, CharSet> CharSetMap;
CharSet forced;
CharSetMap forcedSelected;

typedef set<char_type> MathAlphaSet;
MathAlphaSet mathalpha;


/// The highest code point in UCS4 encoding (1<<20 + 1<<16)
char_type const max_ucs4 = 0x110000;

} // namespace


EncodingException::EncodingException(char_type c)
	: failed_char(c), par_id(0), pos(0)
{
}


const char * EncodingException::what() const noexcept
{
	return "Could not find LaTeX command for a character";
}


CharInfo::CharInfo(
		docstring const & text_command, docstring const & math_command,
		std::string const & text_preamble, std::string const & math_preamble,
		std::string const & tipa_shortcut, unsigned int flags)
	: text_commands_({text_command}), math_commands_({math_command}),
	  text_preamble_(text_preamble), math_preamble_(math_preamble),
	  tipa_shortcut_(tipa_shortcut), flags_(flags)
{
}


CharInfo::CharInfo(
		std::vector<docstring> const & text_commands, std::vector<docstring> const & math_commands,
		std::string const & text_preamble, std::string const & math_preamble,
		std::string const & tipa_shortcut, unsigned int flags)
	: text_commands_(text_commands), math_commands_(math_commands),
	  text_preamble_(text_preamble), math_preamble_(math_preamble),
	  tipa_shortcut_(tipa_shortcut), flags_(flags)
{
}


Encoding::Encoding(string const & n, string const & l, string const & g,
		   string const & i, bool f, bool u, Encoding::Package p)
	: name_(n), latexName_(l), guiName_(g), iconvName_(i), fixedwidth_(f),
	  unsafe_(u), forced_(&forcedSelected[n]), package_(p)
{
	if (n == "ascii") {
		// ASCII can encode 128 code points and nothing else
		start_encodable_ = 128;
		complete_ = true;
	} else if (i == "UTF-8") {
		// UTF8 can encode all UCS4 code points
		start_encodable_ = max_ucs4;
		complete_ = true;
	} else {
		start_encodable_ = 0;
		complete_ = false;
	}
}


void Encoding::init() const
{
	// Since the the constructor is the only method which sets complete_
	// to false the test for complete_ is thread-safe without mutex.
	if (complete_)
		return;

	static Mutex mutex;
	Mutex::Locker lock(&mutex);

	// We need to test again for complete_, since another thread could
	// have set it to true while we were waiting for the lock and we must
	// not modify an encoding which is already complete.
	if (complete_)
		return;

	// We do not make any member mutable  so that it can be easily verified
	// that all const methods are thread-safe: init() is the only const
	// method which changes complete_, encodable_ and start_encodable_, and
	// it uses a mutex to ensure thread-safety.
	CharSet & encodable = const_cast<Encoding *>(this)->encodable_;
	char_type & start_encodable = const_cast<Encoding *>(this)->start_encodable_;

	start_encodable = 0;
	// temporarily switch off lyxerr, since we will generate iconv errors
	lyxerr.disable();
	if (fixedwidth_) {
		// We do not need to check all UCS4 code points, it is enough
		// if we check all 256 code points of this encoding.
		for (unsigned short j = 0; j < 256; ++j) {
			char const c = char(j);
			vector<char_type> const ucs4 = eightbit_to_ucs4(&c, 1, iconvName_);
			if (ucs4.size() != 1)
				continue;
			char_type const uc = ucs4[0];
			CharInfoMap::const_iterator const it = unicodesymbols.find(uc);
			if (it == unicodesymbols.end())
				encodable.insert(uc);
			else if (!it->second.force()) {
				if (forced_->empty() || forced_->find(uc) == forced_->end())
					encodable.insert(uc);
			}
		}
	} else {
		// We do not know how many code points this encoding has, and
		// they do not have a direct representation as a single byte,
		// therefore we need to check all UCS4 code points.
		// This is expensive!
		for (char_type c = 0; c < max_ucs4; ++c) {
			vector<char> const eightbit = ucs4_to_eightbit(&c, 1, iconvName_);
			if (!eightbit.empty()) {
				CharInfoMap::const_iterator const it = unicodesymbols.find(c);
				if (it == unicodesymbols.end())
					encodable.insert(c);
				else if (!it->second.force()) {
					if (forced_->empty() || forced_->find(c) == forced_->end())
						encodable.insert(c);
				}
			}
		}
	}
	lyxerr.enable();
	CharSet::iterator it = encodable.find(start_encodable);
	while (it != encodable.end()) {
		encodable.erase(it);
		++start_encodable;
		it = encodable.find(start_encodable);
	}
	const_cast<Encoding *>(this)->complete_ = true;
}


bool Encoding::isForced(char_type c) const
{
	if (!forced.empty() && forced.find(c) != forced.end())
		return true;
	return !forced_->empty() && forced_->find(c) != forced_->end();
}


bool Encoding::encodable(char_type c) const
{
	// assure the used encoding is properly initialized
	init();
	if (iconvName_ == "UTF-8" && package_ == none)
		return true;
	// platex does not load inputenc: force conversion of supported characters
	if (package_ == Encoding::japanese
	    && ((0xb7 <= c && c <= 0x05ff) // Latin-1 Supplement ... Hebrew
			|| (0x1d00 <= c && c <= 0x218f) // Phonetic Extensions ... Number Forms
			|| (0x2193 <= c && c <= 0x2aff) // Arrows ... Supplemental Mathematical Operators
			|| (0xfb00 <= c && c <= 0xfb4f) // Alphabetic Presentation Forms
			|| (0x1d400 <= c && c <= 0x1d7ff))) // Mathematical Alphanumeric Symbols
		return false;
	if (c < start_encodable_ && !isForced(c))
		return true;
	if (encodable_.find(c) != encodable_.end())
		return true;
	return false;
}


pair<docstring, bool> Encoding::latexChar(char_type c) const
{
	if (encodable(c))
		return make_pair(docstring(1, c), false);

	// c cannot (or should not) be encoded in this encoding
	CharInfoMap::const_iterator const it = unicodesymbols.find(c);
	if (it == unicodesymbols.end())
		throw EncodingException(c);
	// at least one of mathCommand and textCommand is nonempty
	if (it->second.textCommand().empty())
		return make_pair(
				"\\ensuremath{" + it->second.mathCommand() + '}', false);
	return make_pair(it->second.textCommand(), !it->second.textNoTermination());
}


pair<docstring, docstring> Encoding::latexString(docstring const & input, bool dryrun) const
{
	docstring result;
	docstring uncodable;
	bool terminate = false;
	for (char_type const c : input) {
		try {
			pair<docstring, bool> latex_char = latexChar(c);
			docstring const latex = latex_char.first;
			if (terminate && !prefixIs(latex, '\\')
			    && !prefixIs(latex, '{')
			    && !prefixIs(latex, '}')) {
					// Prevent eating of a following
					// space or command corruption by
					// following characters
					if (latex == " ")
						result += "{}";
					else
						result += " ";
				}
			result += latex;
			terminate = latex_char.second;
		} catch (EncodingException & /* e */) {
			LYXERR0("Uncodable character <" << docstring(1, c) 
					<< "> in latexString!");
			if (dryrun) {
				result += "<" + _("LyX Warning: ")
					   + _("uncodable character") + " '";
				result += docstring(1, c);
				result += "'>";
			} else
				uncodable += c;
		}
	}
	return make_pair(result, uncodable);
}


vector<char_type> Encoding::symbolsList() const
{
	// assure the used encoding is properly initialized
	init();

	// first all those below start_encodable_
	vector<char_type> symbols;
	for (char_type c = 0; c < start_encodable_; ++c)
		symbols.push_back(c);
	// add all encodable characters
	copy(encodable_.begin(), encodable_.end(), back_inserter(symbols));
	// now the ones from the unicodesymbols file that are not already there
	for (auto const & elem : unicodesymbols) {
		if (find(symbols.begin(), symbols.end(), elem.first) == symbols.end())
			symbols.push_back(elem.first);
	}
	// finally, sort the vector
	sort(symbols.begin(), symbols.end());
	return symbols;
}


bool Encodings::latexMathChar(char_type c, bool mathmode,
			Encoding const * encoding, docstring & command,
			bool & needsTermination)
{
	command = empty_docstring();
	if (encoding)
		if (encoding->encodable(c))
			command = docstring(1, c);
	needsTermination = false;

	CharInfoMap::const_iterator const it = unicodesymbols.find(c);
	if (it == unicodesymbols.end()) {
		if (!encoding || command.empty())
			throw EncodingException(c);
		if (mathmode)
			addMathSym(c);
		return false;
	}
	// at least one of mathCommand and textCommand is nonempty
	bool use_math = (mathmode && !it->second.mathCommand().empty()) ||
	                (!mathmode && it->second.textCommand().empty());
	if (use_math) {
		command = it->second.mathCommand();
		needsTermination = !it->second.mathNoTermination();
		addMathCmd(c);
	} else {
		if (!encoding || command.empty()) {
			command = it->second.textCommand();
			needsTermination = !it->second.textNoTermination();
		}
		if (mathmode)
			addMathSym(c);
		else
			addTextCmd(c);
	}
	return use_math;
}


char_type Encodings::fromLaTeXCommand(docstring const & cmd, int cmdtype,
		bool & combining, bool & needsTermination, set<string> * req)
{
	CharInfoMap::const_iterator const end = unicodesymbols.end();
	CharInfoMap::const_iterator it = unicodesymbols.begin();
	for (combining = false; it != end; ++it) {
		if (it->second.deprecated())
			continue;

		if (cmdtype & MATH_CMD) {
			for (const docstring& math : it->second.mathCommands()) {
				if ((cmdtype & MATH_CMD) && math == cmd) {
					combining = it->second.combining();
					needsTermination = !it->second.mathNoTermination();
					if (req && it->second.mathFeature() &&
					    !it->second.mathPreamble().empty())
						req->insert(it->second.mathPreamble());
					return it->first;
				}
			}
		}

		if (cmdtype & TEXT_CMD) {
			for (const docstring& text : it->second.textCommands()) {
				if (text == cmd) {
					combining = it->second.combining();
					needsTermination = !it->second.textNoTermination();
					if (req && it->second.textFeature() &&
					    !it->second.textPreamble().empty())
						req->insert(it->second.textPreamble());
					return it->first;
				}
			}
		}
	}
	needsTermination = false;
	return 0;
}


docstring Encodings::fromLaTeXCommand(docstring const & cmd, int cmdtype,
		bool & needsTermination, docstring & rem, set<string> * req)
{
	needsTermination = false;
	rem = empty_docstring();
	bool const mathmode = cmdtype & MATH_CMD;
	bool const textmode = cmdtype & TEXT_CMD;

	// Easy case: the command is a complete entry of unicodesymbols.
	for (const auto & unicodeSymbol : unicodesymbols) {
		if (mathmode) {
			for (const auto & command : unicodeSymbol.second.mathCommands()) {
				if (command == cmd) {
					docstring value;
					value += unicodeSymbol.first;
					needsTermination = !unicodeSymbol.second.mathNoTermination();
					if (req && unicodeSymbol.second.mathFeature()
						&& !unicodeSymbol.second.mathPreamble().empty())
							req->insert(unicodeSymbol.second.mathPreamble());
					return value;
				}
			}
		}
		if (textmode) {
			for (const auto & command : unicodeSymbol.second.textCommands()) {
				if (command == cmd) {
					docstring value;
					value += unicodeSymbol.first;
					needsTermination = !unicodeSymbol.second.textNoTermination();
					if (req && unicodeSymbol.second.textFeature()
						&& !unicodeSymbol.second.textPreamble().empty())
						req->insert(unicodeSymbol.second.textPreamble());
					return value;
				}
			}
		}
	}

	// Otherwise, try to map as many commands as possible, matching prefixes of the command.
	docstring symbols;
	size_t const cmdend = cmd.size();
	size_t prefix = 0;
	CharInfoMap::const_iterator const uniend = unicodesymbols.end();
	for (size_t i = 0, j = 0; j < cmdend; ++j) {
		// Also get the char after a backslash
		if (j + 1 < cmdend && cmd[j] == '\\') {
			++j;
			prefix = 1;
			// Detect things like \=*{e} as well
			if (j + 3 < cmdend && cmd[j+1] == '*' &&
			    cmd[j+2] == '{') {
				++j;
				prefix = 2;
			}
		}
		// position of the last character before a possible macro
		// argument
		size_t m = j;
		// If a macro argument follows, get it, too
		// Do it here only for single character commands. Other
		// combining commands need this too, but they are handled in
		// the loop below for performance reasons.
		if (j + 1 < cmdend && cmd[j + 1] == '{') {
			size_t k = j + 1;
			int count = 1;
			while (k < cmdend && count) {
				k = cmd.find_first_of(from_ascii("{}"), k + 1);
				// braces may not be balanced
				if (k == docstring::npos)
					break;
				if (cmd[k] == '{')
					++count;
				else
					--count;
			}
			if (k != docstring::npos)
				j = k;
		} else if (m + 1 < cmdend && isAlphaASCII(cmd[m])) {
			while (m + 2 < cmdend && isAlphaASCII(cmd[m+1]))
				m++;
		}
		// Start with this substring and try augmenting it when it is
		// the prefix of some command in the unicodesymbols file
		docstring subcmd = cmd.substr(i, j - i + 1);

		// First part of subcmd which might be a combining character
		docstring combcmd = (m == j) ? docstring() : cmd.substr(i, m - i + 1);
		// The combining character of combcmd if it exists
		size_t unicmd_size = 0;
		char_type c = 0;
		CharInfoMap::const_iterator it = unicodesymbols.begin();
		CharInfoMap::const_iterator combining = uniend;
		for (; it != uniend; ++it) {
			if (it->second.deprecated())
				continue;
			docstring const math = mathmode ? it->second.mathCommand()
							: docstring();
			docstring const text = textmode ? it->second.textCommand()
							: docstring();
			if (!combcmd.empty() && it->second.combining() &&
			    (math == combcmd || text == combcmd))
				combining = it;
			size_t cur_size = max(math.size(), text.size());
			// The current math or text unicode command cannot
			// match, or we already matched a longer one
			if (cur_size < subcmd.size() || cur_size <= unicmd_size)
				continue;

			docstring tmp = subcmd;
			size_t k = j;
			while (prefixIs(math, tmp) || prefixIs(text, tmp)) {
				++k;
				if (k >= cmdend || cur_size <= tmp.size())
					break;
				tmp += cmd[k];
			}
			// No match
			if (k == j)
				continue;

			// The last added char caused a mismatch, because
			// we didn't exhaust the chars in cmd and didn't
			// exceed the maximum size of the current unicmd
			if (k < cmdend && cur_size > tmp.size())
				tmp.resize(tmp.size() - 1);

			// If this is an exact match, we found a (longer)
			// matching entry in the unicodesymbols file.
			if (math != tmp && text != tmp)
				continue;
			// If we found a combining command, we need to append
			// the macro argument if this has not been done above.
			if (tmp == combcmd && combining != uniend &&
			    k < cmdend && cmd[k] == '{') {
				size_t l = k;
				int count = 1;
				while (l < cmdend && count) {
					l = cmd.find_first_of(from_ascii("{}"), l + 1);
					// braces may not be balanced
					if (l == docstring::npos)
						break;
					if (cmd[l] == '{')
						++count;
					else
						--count;
				}
				if (l != docstring::npos) {
					j = l;
					subcmd = cmd.substr(i, j - i + 1);
				}
			}
			// If the entry doesn't start with '\', we take note
			// of the match and continue (this is not a ultimate
			// acceptance, as some other entry may match a longer
			// portion of the cmd string). However, if the entry
			// does start with '\', we accept the match only if
			// this is a valid macro, i.e., either it is a single
			// (nonletter) char macro, or nothing else follows,
			// or what follows is a nonletter char, or the last
			// character is a }.
			else if (tmp[0] != '\\'
				   || (tmp.size() == prefix + 1 &&
				       !isAlphaASCII(tmp[1]) &&
				       (prefix == 1 || !isAlphaASCII(tmp[2])))
				   || k == cmdend
				   || !isAlphaASCII(cmd[k])
				   || tmp[tmp.size() - 1] == '}'
				 ) {
				c = it->first;
				j = k - 1;
				i = j + 1;
				unicmd_size = cur_size;
				if (math == tmp)
					needsTermination = !it->second.mathNoTermination();
				else
					needsTermination = !it->second.textNoTermination();
				if (req) {
					if (math == tmp && it->second.mathFeature() &&
					    !it->second.mathPreamble().empty())
						req->insert(it->second.mathPreamble());
					if (text == tmp && it->second.textFeature() &&
					    !it->second.textPreamble().empty())
						req->insert(it->second.textPreamble());
				}
			}
		}
		if (unicmd_size)
			symbols += c;
		else if (combining != uniend &&
		         prefixIs(subcmd, combcmd + '{')) {
			// We know that subcmd starts with combcmd and
			// contains an argument in braces.
			docstring const arg = subcmd.substr(
				combcmd.length() + 1,
				subcmd.length() - combcmd.length() - 2);
			// If arg is a single character we can construct a
			// combining sequence.
			char_type a;
			bool argcomb = false;
			if (arg.size() == 1 && isAlnumASCII(arg[0]))
				a = arg[0];
			else {
				// Use the version of fromLaTeXCommand() that
				// parses only one command, since we cannot
				// use more than one character.
				bool dummy = false;
				set<string> r;
				a = fromLaTeXCommand(arg, cmdtype, argcomb,
				                     dummy, &r);
				if (a && req && !argcomb)
					req->insert(r.begin(), r.end());
			}
			if (a && !argcomb) {
				// In unicode the combining character comes
				// after its base
				symbols += a;
				symbols += combining->first;
				i = j + 1;
				unicmd_size = 2;
			}
		}
		if (j + 1 == cmdend && !unicmd_size) {
			// No luck. Return what remains
			rem = cmd.substr(i);
			if (needsTermination && !rem.empty()) {
				if (rem.substr(0, 2) == "{}") {
					rem = rem.substr(2);
					needsTermination = false;
				} else if (rem[0] == ' ') {
					needsTermination = false;
					// LaTeX would swallow all spaces
					rem = ltrim(rem);
				}
			}
		}
	}
	return symbols;
}


/// text macros we can convert beyond unicodesymbols
char const * const known_text_macros[] = {"LyX", "TeX", "LaTeXe", "LaTeX", ""};
char const * const known_text_macros_out[] = {"LyX", "TeX", "LaTeX2e", "LaTeX", ""};


docstring Encodings::convertLaTeXCommands(docstring const & str, bool const literal_math)
{
	docstring val = str;
	docstring ret;
	docstring mret;
	docstring cret;

	bool scanning_cmd = false;
	bool scanning_math = false;
	bool is_section = false;
	bool escaped = false; // used to catch \$, etc.
	bool skip_space = false;
	while (!val.empty()) {
		char_type const ch = val[0];

		// if we're scanning math, we collect everything until we
		// find an unescaped $, and then try to convert this piecewise.
		if (scanning_math) {
			if (literal_math) {
				// with xhtml, we output everything until we
				// find an unescaped $, at which point we break out.
				if (escaped)
					escaped = false;
				else if (ch == '\\')
					escaped = true;
				else if (ch == '$')
					scanning_math = false;
				ret += ch;
				val = val.substr(1);
				continue;
			}
			
			if (escaped)
				escaped = false;
			else if (ch == '\\')
				escaped = true;
			else if (ch == '$') {
				scanning_math = false;
				bool termination;
				docstring rem;
				ret += fromLaTeXCommand(mret, MATH_CMD, termination, rem);
				// parse remaining math
				while (!rem.empty()) {
					docstring rrem;
					// split command from normal text
					docstring cmd = split(rem, rrem, '\\');
					ret += rrem;
					// done of no command was found
					if (cmd.empty())
						break;
					// go on ...
					docstring oldrem = rem;
					ret += fromLaTeXCommand(from_ascii("\\") + cmd, MATH_CMD, termination, rem);
					if (oldrem == rem) {
						// Result unchanged, output
						ret += rem;
						break;
					}
				}
				mret = docstring();
			}
			mret += ch;
			val = val.substr(1);
			continue;
		}

		// if we're scanning a command name, then we just
		// discard characters until we hit something that
		// isn't alpha.
		if (scanning_cmd) {
			if (!is_section && ch == 'S') {
				is_section = true;
				val = val.substr(1);
				continue;
			}
			if (isAlphaASCII(ch)) {
				cret += ch;
				is_section = false;
				val = val.substr(1);
				escaped = false;
				continue;
			} else if (is_section) {
				ret.push_back(0x00a7);
				is_section = false;
				continue;
			}
			// so we're done with this command.
			// now we fall through and check this character.
			is_section = false;
			scanning_cmd = false;
		}

		// check if it's a know text macro
		// If so, output and skip the following space
		if (!cret.empty()) {
			int const n = findToken(known_text_macros, to_ascii(cret));
			if (n != -1) {
				ret += known_text_macros_out[n];
				skip_space = true;
			}
			cret.clear();
		}

		// was the last character a \? If so, then this is something like:
		// \\ or \$, so we'll just output it. That's probably not always right...
		if (escaped) {
			// exception: output \, as THIN SPACE
			if (ch == ',')
				ret.push_back(0x2009);
			else
				ret += ch;
			val = val.substr(1);
			escaped = false;
			continue;
		}

		if (ch == '~') {
			ret += char_type(0x00a0);
			val = val.substr(1);
			continue;
		}

		if (ch == '$') {
			val = val.substr(1);
			scanning_math = true;
			continue;
		}

		if (isSpace(ch) && skip_space) {
			val = val.substr(1);
			skip_space = false;
			continue;
		}
		skip_space = false;

		// Change text mode accents in the form
		// {\v a} to \v{a} (see #9340).
		// FIXME: This is a sort of mini-tex2lyx.
		//        Use the real tex2lyx instead!
		static regex const tma_reg("^\\{\\\\[bcCdfGhHkrtuUv]\\s\\w\\}");
		if (regex_search(to_utf8(val), tma_reg)) {
			val = val.substr(1);
			val.replace(2, 1, from_ascii("{"));
			continue;
		}

		// Apart from the above, we just ignore braces
		if (ch == '{' || ch == '}') {
			val = val.substr(1);
			continue;
		}

		// we're going to check things that look like commands, so if
		// this doesn't, just output it.
		if (ch != '\\') {
			ret += ch;
			val = val.substr(1);
			continue;
		}

		// ok, could be a command of some sort
		// let's see if it corresponds to some unicode
		// unicodesymbols has things in the form: \"{u},
		// whereas we may see things like: \"u. So we'll
		// look for that and change it, if necessary.
		// FIXME: This is a sort of mini-tex2lyx.
		//        Use the real tex2lyx instead!
		static regex const reg("^\\\\\\W\\w");
		if (regex_search(to_utf8(val), reg)) {
			val.insert(3, from_ascii("}"));
			val.insert(2, from_ascii("{"));
		}
		bool termination;
		docstring rem;
		docstring const cnvtd = fromLaTeXCommand(val,
				TEXT_CMD, termination, rem);
		if (!cnvtd.empty()) {
			// it did, so we'll take that bit and proceed with what's left
			ret += cnvtd;
			val = rem;
			continue;
		}
		// it's a command of some sort
		scanning_cmd = true;
		escaped = true;
		val = val.substr(1);
	}
	// check if it's a know text macro
	// If so, output and skip the following space
	if (!cret.empty()) {
		int const n = findToken(known_text_macros, to_ascii(cret));
		if (n != -1)
			ret += known_text_macros_out[n];
	}

	return ret;
}


CharInfo const & Encodings::unicodeCharInfo(char_type c)
{
	static CharInfo empty;
	CharInfoMap::const_iterator const it = unicodesymbols.find(c);
	return it != unicodesymbols.end() ? it->second : empty;
}


bool Encodings::isCombiningChar(char_type c)
{
	CharInfoMap::const_iterator const it = unicodesymbols.find(c);
	if (it != unicodesymbols.end())
		return it->second.combining();
	return false;
}


string const Encodings::TIPAShortcut(char_type c)
{
	CharInfoMap::const_iterator const it = unicodesymbols.find(c);
	if (it != unicodesymbols.end())
		return it->second.tipaShortcut();
	return string();
}


string const Encodings::isKnownScriptChar(char_type const c)
{
	CharInfoMap::const_iterator const it = unicodesymbols.find(c);

	if (it == unicodesymbols.end())
		return string();
	// FIXME: parse complex textPreamble (may be list or alternatives,
	// 		  e.g., "subscript,textgreek" or "textcomp|textgreek")
	if (it->second.textPreamble() == "textgreek"
		|| it->second.textPreamble() == "textcyrillic")
		return it->second.textPreamble();
	return string();
}


bool Encodings::fontencSupportsScript(string const & fontenc, string const & script)
{
	if (script == "textgreek")
		return (fontenc == "LGR" || fontenc == "TU");
	if (script == "textcyrillic")
		return (fontenc == "T2A" || fontenc == "T2B" || fontenc == "T2C"
				|| fontenc == "X2" || fontenc == "TU");
	return false;
}


bool Encodings::isMathAlpha(char_type c)
{
	return mathalpha.count(c);
}


bool Encodings::isUnicodeTextOnly(char_type c)
{
	if (isASCII(c) || isMathAlpha(c))
		return false;

	CharInfoMap::const_iterator const it = unicodesymbols.find(c);
	return it == unicodesymbols.end() || it->second.mathCommand().empty();
}


Encoding const *
Encodings::fromLyXName(string const & name, bool allowUnsafe) const
{
	EncodingList::const_iterator const it = encodinglist.find(name);
	if (it == encodinglist.end())
		return nullptr;
	if (!allowUnsafe && it->second.unsafe())
		return nullptr;
	return &it->second;
}


Encoding const *
Encodings::fromLaTeXName(string const & n, int p, bool allowUnsafe) const
{
	string name = n;
	// FIXME: if we have to test for too many of these synonyms,
	// we should instead extend the format of lib/encodings
	if (n == "ansinew")
		name = "cp1252";

	// We don't use find_if because it makes copies of the pairs in
	// the map.
	// This linear search is OK since we don't have many encodings.
	// Users could even optimize it by putting the encodings they use
	// most at the top of lib/encodings.
	EncodingList::const_iterator const end = encodinglist.end();
	for (EncodingList::const_iterator it = encodinglist.begin(); it != end; ++it)
		if ((it->second.latexName() == name) && (it->second.package() & p)
				&& (!it->second.unsafe() || allowUnsafe))
			return &it->second;
	return nullptr;
}


Encoding const *
Encodings::fromIconvName(string const & n, int p, bool allowUnsafe) const
{
	EncodingList::const_iterator const end = encodinglist.end();
	for (EncodingList::const_iterator it = encodinglist.begin(); it != end; ++it)
		if ((it->second.iconvName() == n) && (it->second.package() & p)
				&& (!it->second.unsafe() || allowUnsafe))
			return &it->second;
	return nullptr;
}


Encodings::Encodings()
{}


void Encodings::read(FileName const & encfile, FileName const & symbolsfile)
{
	// We must read the symbolsfile first, because the Encoding
	// constructor depends on it.
	CharSetMap forcedNotSelected;
	Lexer symbolsLex;
	symbolsLex.setFile(symbolsfile);
	bool getNextToken = true;
	while (symbolsLex.isOK()) {
		char_type symbol;

		if (getNextToken) {
			if (!symbolsLex.next(true))
				break;
		} else
			getNextToken = true;

		istringstream is(symbolsLex.getString());
		// reading symbol directly does not work if
		// char_type == wchar_t.
		uint32_t tmp;
		if(!(is >> hex >> tmp))
			break;
		symbol = tmp;

		// Special case: more than one entry for one character (to add other LaTeX commands).
		if (unicodesymbols.find(symbol) != unicodesymbols.end()) {
			if (!symbolsLex.next(true))
				break;
			docstring textCommand = symbolsLex.getDocString();
			if (!symbolsLex.next(true))
				break;
			string mathCommand = symbolsLex.getString();

			if (!textCommand.empty())
				unicodesymbols.at(symbol).addTextCommand(textCommand);
			if (!mathCommand.empty())
				unicodesymbols.at(symbol).addMathCommand(textCommand);

			continue;
		}

		// If the symbol is not the same as the previous entry, consider it is a totally new symbol.
		if (!symbolsLex.next(true))
			break;
		docstring textCommand = symbolsLex.getDocString();
		if (!symbolsLex.next(true))
			break;
		string textPreamble = symbolsLex.getString();
		if (!symbolsLex.next(true))
			break;
		string sflags = symbolsLex.getString();

		string tipaShortcut;
		int flags = 0;

		if (suffixIs(textCommand, '}'))
			flags |= CharInfoTextNoTermination;
		while (!sflags.empty()) {
			string flag;
			sflags = split(sflags, flag, ',');
			if (flag == "combining") {
				flags |= CharInfoCombining;
			} else if (flag == "force") {
				flags |= CharInfoForce;
				forced.insert(symbol);
			} else if (prefixIs(flag, "force=")) {
				vector<string> encs =
					getVectorFromString(flag.substr(6), ";");
				for (auto const & enc : encs)
					forcedSelected[enc].insert(symbol);
				flags |= CharInfoForceSelected;
			} else if (prefixIs(flag, "force!=")) {
				vector<string> encs =
					getVectorFromString(flag.substr(7), ";");
				for (auto const & enc : encs)
					forcedNotSelected[enc].insert(symbol);
				flags |= CharInfoForceSelected;
			} else if (flag == "mathalpha") {
				mathalpha.insert(symbol);
			} else if (flag == "notermination=text") {
				flags |= CharInfoTextNoTermination;
			} else if (flag == "notermination=math") {
				flags |= CharInfoMathNoTermination;
			} else if (flag == "notermination=both") {
				flags |= CharInfoTextNoTermination;
				flags |= CharInfoMathNoTermination;
			} else if (flag == "notermination=none") {
				flags &= ~CharInfoTextNoTermination;
				flags &= ~CharInfoMathNoTermination;
			} else if (contains(flag, "tipashortcut=")) {
				tipaShortcut = split(flag, '=');
			} else if (flag == "deprecated") {
				flags |= CharInfoDeprecated;
			} else {
				lyxerr << "Ignoring unknown flag `" << flag
				       << "' for symbol `0x"
				       << hex << symbol << dec
				       << "'." << endl;
			}
		}
		// mathCommand and mathPreamble have been added for 1.6.0.
		// make them optional so that old files still work.
		int const lineNo = symbolsLex.lineNumber();
		bool breakout = false;
		docstring mathCommand;
		string mathPreamble;
		if (symbolsLex.next(true)) {
			if (symbolsLex.lineNumber() != lineNo) {
				// line in old format without mathCommand and mathPreamble
				getNextToken = false;
			} else {
				mathCommand = symbolsLex.getDocString();
				if (suffixIs(mathCommand, '}'))
					flags |= CharInfoMathNoTermination;
				if (symbolsLex.next(true)) {
					if (symbolsLex.lineNumber() != lineNo) {
						// line in new format with mathCommand only
						getNextToken = false;
					} else {
						// line in new format with mathCommand and mathPreamble
						mathPreamble = symbolsLex.getString();
					}
				} else
					breakout = true;
			}
		} else {
			breakout = true;
		}

		// backward compatibility
		if (mathPreamble == "esintoramsmath")
			mathPreamble = "esint|amsmath";

		if (!textPreamble.empty())
			if (textPreamble[0] != '\\')
				flags |= CharInfoTextFeature;
		if (!mathPreamble.empty())
			if (mathPreamble[0] != '\\')
				flags |= CharInfoMathFeature;

		CharInfo info = CharInfo(
				textCommand, mathCommand,
				textPreamble, mathPreamble,
				tipaShortcut, flags);
		LYXERR(Debug::INFO, "Read unicode symbol " << symbol << " '"
		                                           << to_utf8(info.textCommand()) << "' '" << info.textPreamble()
		                                           << " '" << info.textFeature() << ' ' << info.textNoTermination()
		                                           << ' ' << to_utf8(info.mathCommand()) << "' '" << info.mathPreamble()
		                                           << "' " << info.mathFeature() << ' ' << info.mathNoTermination()
			   << ' ' << info.combining() << ' ' << info.force()
			   << ' ' << info.forceSelected());

		// we assume that at least one command is nonempty when using unicodesymbols
		if (info.isUnicodeSymbol()) {
			unicodesymbols[symbol] = info;
		}

		if (breakout)
			break;
	}

	// Now read the encodings
	enum {
		et_encoding = 1,
		et_end
	};

	LexerKeyword encodingTags[] = {
		{ "encoding", et_encoding },
		{ "end", et_end }
	};

	Lexer lex(encodingTags);
	lex.setFile(encfile);
	lex.setContext("Encodings::read");
	while (lex.isOK()) {
		switch (lex.lex()) {
		case et_encoding:
		{
			lex.next();
			string const name = lex.getString();
			lex.next();
			string const latexName = lex.getString();
			lex.next();
			string const guiName = lex.getString();
			lex.next();
			string const iconvName = lex.getString();
			lex.next();
			string const width = lex.getString();
			bool fixedWidth = false;
			bool unsafe = false;
			if (width == "fixed")
				fixedWidth = true;
			else if (width == "variable")
				fixedWidth = false;
			else if (width == "variableunsafe") {
				fixedWidth = false;
				unsafe = true;
			}
			else
				lex.printError("Unknown width");

			lex.next();
			string const p = lex.getString();
			Encoding::Package package = Encoding::none;
			if (p == "none")
				package = Encoding::none;
			else if (p == "inputenc")
				package = Encoding::inputenc;
			else if (p == "CJK")
				package = Encoding::CJK;
			else if (p == "japanese")
				package = Encoding::japanese;
			else
				lex.printError("Unknown package");

			LYXERR(Debug::INFO, "Reading encoding " << name);
			encodinglist[name] = Encoding(name, latexName,
			                              guiName, iconvName, fixedWidth, unsafe,
			                              package);

			if (lex.lex() != et_end)
				lex.printError("Missing end");
			break;
		}
		case et_end:
			lex.printError("Misplaced end");
			break;
		case Lexer::LEX_FEOF:
			break;
		default:
			lex.printError("Unknown tag");
			break;
		}
	}

	// Move all information from forcedNotSelected to forcedSelected
	for (CharSetMap::const_iterator it1 = forcedNotSelected.begin(); it1 != forcedNotSelected.end(); ++it1) {
		for (CharSetMap::iterator it2 = forcedSelected.begin(); it2 != forcedSelected.end(); ++it2) {
			if (it2->first != it1->first)
				it2->second.insert(it1->second.begin(), it1->second.end());
		}
	}

}


} // namespace lyx
