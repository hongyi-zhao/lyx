/**
 * \file MathExtern.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

// This file contains most of the magic that extracts "context
// information" from the unstructered layout-oriented stuff in
// MathData.

#include <config.h>

#include "MathExtern.h"

#include "InsetMathAMSArray.h"
#include "InsetMathArray.h"
#include "InsetMathChar.h"
#include "InsetMathDelim.h"
#include "InsetMathDiff.h"
#include "InsetMathExFunc.h"
#include "InsetMathExInt.h"
#include "InsetMathFont.h"
#include "InsetMathFrac.h"
#include "InsetMathLim.h"
#include "InsetMathMatrix.h"
#include "InsetMathNumber.h"
#include "InsetMathScript.h"
#include "InsetMathString.h"
#include "InsetMathSymbol.h"
#include "MathData.h"
#include "MathParser.h"
#include "MathStream.h"

#include "Encoding.h"

#include "support/debug.h"
#include "support/docstream.h"
#include "support/FileName.h"
#include "support/filetools.h"
#include "support/gettext.h"
#include "support/lstrings.h"
#include "support/TempFile.h"
#include "support/textutils.h"

#include <algorithm>
#include <sstream>
#include <fstream>
#include <memory>

using namespace std;
using namespace lyx::support;

namespace lyx {

namespace {

enum ExternalMath {
	HTML,
	MAPLE,
	MAXIMA,
	MATHEMATICA,
	MATHML,
	OCTAVE
};


static char const * function_names[] = {
	"arccos", "arcsin", "arctan", "arg", "bmod",
	"cos", "cosh", "cot", "coth", "csc", "deg",
	"det", "dim", "exp", "gcd", "hom", "inf", "ker",
	"lg", "lim", "liminf", "limsup", "ln", "log",
	"max", "min", "sec", "sin", "sinh", "sup",
	"tan", "tanh", "Pr", nullptr
};

// define a function for tests
typedef bool TestItemFunc(MathAtom const &);

// define a function for replacing subexpressions
typedef MathAtom ReplaceArgumentFunc(const MathData & md);



// try to extract a super/subscript
// modify iterator position to point behind the thing
bool extractScript(MathData & md,
	MathData::iterator & pos, MathData::iterator last, bool superscript)
{
	// nothing to get here
	if (pos == last)
		return false;

	// is this a scriptinset?
	if (!(*pos)->asScriptInset())
		return false;

	// do we want superscripts only?
	if (superscript && !(*pos)->asScriptInset()->hasUp())
		return false;

	// it is a scriptinset, use it.
	md.push_back(*pos);
	++pos;
	return true;
}


// try to extract an "argument" to some function.
// returns position behind the argument
MathData::iterator extractArgument(MathData & md,
	MathData::iterator pos, MathData::iterator last,
	ExternalMath kind, bool function = false)
{
	// nothing to get here
	if (pos == last)
		return pos;

	// something delimited _is_ an argument
	if ((*pos)->asDelimInset()) {
		// leave out delimiters if this is a function argument
		// unless we are doing MathML, in which case we do want
		// the delimiters
		if (function && kind != MATHML && kind != HTML) {
			MathData const & arg = (*pos)->asDelimInset()->cell(0);
			MathData::const_iterator cur = arg.begin();
			MathData::const_iterator end = arg.end();
			while (cur != end)
				md.push_back(*cur++);
		} else
			md.push_back(*pos);
		++pos;
		if (pos == last)
			return pos;
		// if there's one, get following superscript only if this
		// isn't a function argument
		if (!function)
			extractScript(md, pos, last, true);
		return pos;
	}

	// always take the first thing, no matter what it is
	md.push_back(*pos);

	// go ahead if possible
	++pos;
	if (pos == last)
		return pos;

	// if the next item is a super/subscript, it most certainly belongs
	// to the thing we have
	extractScript(md, pos, last, false);
	if (pos == last)
		return pos;

	// but it might be more than that.
	// FIXME: not implemented
	//for (MathData::iterator it = pos + 1; it != last; ++it) {
	//	// always take the first thing, no matter
	//	if (it == pos) {
	//		md.push_back(*it);
	//		continue;
	//	}
	//}
	return pos;
}


// returns sequence of char with same code starting at it up to end
// it might be less, though...
docstring charSequence
	(MathData::const_iterator it, MathData::const_iterator end)
{
	docstring s;
	for (; it != end && (*it)->asCharInset(); ++it)
		s += (*it)->getChar();
	return s;
}


void extractStrings(MathData & md)
{
	//lyxerr << "\nStrings from: " << md << endl;
	for (size_t i = 0; i < md.size(); ++i) {
		if (!md[i]->asCharInset())
			continue;
		docstring s = charSequence(md.begin() + i, md.end());
		md[i] = MathAtom(new InsetMathString(md.buffer(), s));
		md.erase(i + 1, i + s.size());
	}
	//lyxerr << "\nStrings to: " << md << endl;
}


void extractMatrices(MathData & md)
{
	//lyxerr << "\nMatrices from: " << md << endl;
	// first pass for explicitly delimited stuff
	for (size_t i = 0; i < md.size(); ++i) {
		InsetMathDelim const * const inset = md[i]->asDelimInset();
		if (!inset)
			continue;
		MathData const & mdi = inset->cell(0);
		if (mdi.size() != 1)
			continue;
		if (!mdi.front()->asGridInset())
			continue;
		md[i] = MathAtom(new InsetMathMatrix(*(mdi.front()->asGridInset()),
		                 inset->left_, inset->right_));
	}

	// second pass for AMS "pmatrix" etc
	for (size_t i = 0; i < md.size(); ++i) {
		InsetMathAMSArray const * const inset = md[i]->asAMSArrayInset();
		if (inset) {
			string left = inset->name_left();
			if (left == "Vert")
				left = "[";
			string right = inset->name_right();
			if (right == "Vert")
				right = "]";
			md[i] = MathAtom(new InsetMathMatrix(*inset, from_ascii(left), from_ascii(right)));
		}
	}
	//lyxerr << "\nMatrices to: " << md << endl;
}


// convert this inset somehow to a string
bool extractString(MathAtom const & at, docstring & str)
{
	if (at->getChar()) {
		str = docstring(1, at->getChar());
		return true;
	}
	if (at->asStringInset()) {
		str = at->asStringInset()->str();
		return true;
	}
	return false;
}


// is this a known function?
bool isKnownFunction(docstring const & str)
{
	for (int i = 0; function_names[i]; ++i) {
		if (str == function_names[i])
			return true;
	}
	return false;
}


// extract a function name from this inset
bool extractFunctionName(MathAtom const & at, docstring & str)
{
	if (at->asSymbolInset()) {
		str = at->asSymbolInset()->name();
		return isKnownFunction(str);
	}
	if (at->asUnknownInset()) {
		// assume it is well known...
		str = at->name();
		return true;
	}
	if (at->asFontInset() && at->name() == "mathrm") {
		// assume it is well known...
		MathData const & md = at->asFontInset()->cell(0);
		str = charSequence(md.begin(), md.end());
		return md.size() == str.size();
	}
	return false;
}


bool testString(MathAtom const & at, docstring const & str)
{
	docstring s;
	return extractString(at, s) && str == s;
}


bool testString(MathAtom const & at, char const * const str)
{
	return testString(at, from_ascii(str));
}


bool testSymbol(MathAtom const & at, docstring const & name)
{
	return (at->asSymbolInset() || at->asMacro()) && at->name() == name;
}


bool testSymbol(MathAtom const & at, char const * const name)
{
	return testSymbol(at, from_ascii(name));
}


// search end of nested sequence
MathData::iterator endNestSearch(
	MathData::iterator it,
	const MathData::iterator& last,
	TestItemFunc testOpen,
	TestItemFunc testClose
)
{
	for (int level = 0; it != last; ++it) {
		if (testOpen(*it))
			++level;
		if (testClose(*it))
			--level;
		if (level == 0)
			break;
	}
	return it;
}


// replace nested sequences by a real Insets
void replaceNested(
	MathData & md,
	TestItemFunc testOpen,
	TestItemFunc testClose,
	ReplaceArgumentFunc replaceArg)
{
	Buffer * buf = md.buffer();
	// use indices rather than iterators for the loop  because we are going
	// to modify the data.
	for (size_t i = 0; i < md.size(); ++i) {
		// check whether this is the begin of the sequence
		if (!testOpen(md[i]))
			continue;

		// search end of sequence
		MathData::iterator it = md.begin() + i;
		MathData::iterator jt = endNestSearch(it, md.end(), testOpen, testClose);
		if (jt == md.end())
			continue;

		// replace the original stuff by the new inset
		md[i] = replaceArg(MathData(buf, it + 1, jt));
		md.erase(it + 1, jt + 1);
	}
}



//
// split scripts into separate super- and subscript insets. sub goes in
// front of super...
//

void splitScripts(MathData & md)
{
	Buffer * buf = md.buffer();
	//lyxerr << "\nScripts from: " << md << endl;
	for (size_t i = 0; i < md.size(); ++i) {
		InsetMathScript const * script = md[i]->asScriptInset();

		// is this a script inset and do we also have a superscript?
		if (!script || !script->hasUp())
			continue;

		// we must have a nucleus if we only have a superscript
		if (!script->hasDown() && script->nuc().empty())
			continue;

		if (script->nuc().size() == 1) {
			// leave alone sums and integrals
			MathAtom const & atom = script->nuc().front();
			if (testSymbol(atom, "sum") || testSymbol(atom, "int"))
				continue;
		}

		// create extra script inset and move superscript over
		InsetMathScript * p = md[i].nucleus()->asScriptInset();
		auto q = make_unique<InsetMathScript>(buf, true);
		swap(q->up(), p->up());
		p->removeScript(true);

		// if we don't have a subscript, get rid of the ScriptInset
		if (!script->hasDown()) {
			MathData arg(p->nuc());
			MathData::const_iterator it = arg.begin();
			MathData::const_iterator et = arg.end();
			md.erase(i);
			while (it != et)
				md.insert(i++, *it++);
		} else
			++i;

		// insert new inset behind
		md.insert(i, MathAtom(q.release()));
	}
	//lyxerr << "\nScripts to: " << md << endl;
}


//
// extract exp(...)
//

void extractExps(MathData & md)
{
	Buffer * buf = md.buffer();
	//lyxerr << "\nExps from: " << md << endl;
	for (size_t i = 0; i + 1 < md.size(); ++i) {
		// is this 'e'?
		if (md[i]->getChar() != 'e')
			continue;

		// we need an exponent but no subscript
		InsetMathScript const * sup = md[i + 1]->asScriptInset();
		if (!sup || sup->hasDown())
			continue;

		// create a proper exp-inset as replacement
		md[i] = MathAtom(new InsetMathExFunc(buf, from_ascii("exp"), sup->cell(1)));
		md.erase(i + 1);
	}
	//lyxerr << "\nExps to: " << md << endl;
}


//
// extract det(...)  from |matrix|
//
void extractDets(MathData & md)
{
	Buffer * buf = md.buffer();
	//lyxerr << "\ndet from: " << md << endl;
	for (MathData::iterator it = md.begin(); it != md.end(); ++it) {
		InsetMathDelim const * del = (*it)->asDelimInset();
		if (!del)
			continue;
		if (!del->isAbs())
			continue;
		*it = MathAtom(new InsetMathExFunc(buf, from_ascii("det"), del->cell(0)));
	}
	//lyxerr << "\ndet to: " << md << endl;
}


//
// search numbers
//

bool isDigitOrSimilar(char_type c)
{
	return ('0' <= c && c <= '9') || c == '.';
}


// returns sequence of digits
docstring digitSequence
	(MathData::const_iterator it, MathData::const_iterator end)
{
	docstring s;
	for (; it != end && (*it)->asCharInset(); ++it) {
		if (!isDigitOrSimilar((*it)->getChar()))
			break;
		s += (*it)->getChar();
	}
	return s;
}


void extractNumbers(MathData & md)
{
	//lyxerr << "\nNumbers from: " << md << endl;
	for (size_t i = 0; i < md.size(); ++i) {
		if (!md[i]->asCharInset())
			continue;
		if (!isDigitOrSimilar(md[i]->asCharInset()->getChar()))
			continue;

		docstring s = digitSequence(md.begin() + i, md.end());

		md[i] = MathAtom(new InsetMathNumber(md.buffer(), s));
		md.erase(i + 1, i + s.size());
	}
	//lyxerr << "\nNumbers to: " << md << endl;
}



//
// search delimiters
//

bool testOpenParen(MathAtom const & at)
{
	return testString(at, "(");
}


bool testCloseParen(MathAtom const & at)
{
	return testString(at, ")");
}


MathAtom replaceParenDelims(const MathData & md)
{
	return MathAtom(new InsetMathDelim(const_cast<Buffer *>(md.buffer()),
		from_ascii("("), from_ascii(")"), md, true));
}


bool testOpenBracket(MathAtom const & at)
{
	return testString(at, "[");
}


bool testCloseBracket(MathAtom const & at)
{
	return testString(at, "]");
}


MathAtom replaceBracketDelims(const MathData & md)
{
	return MathAtom(new InsetMathDelim(const_cast<Buffer *>(md.buffer()),
		from_ascii("["), from_ascii("]"), md, true));
}


bool testOpenVert(MathAtom const & at)
{
	return testSymbol(at, "lvert");
}


bool testCloseVert(MathAtom const & at)
{
	return testSymbol(at, "rvert");
}


MathAtom replaceVertDelims(const MathData & md)
{
	return MathAtom(new InsetMathDelim(const_cast<Buffer *>(md.buffer()),
		from_ascii("lvert"), from_ascii("rvert"), md, true));
}


bool testOpenAngled(MathAtom const & at)
{
	return testSymbol(at, "langle");
}


bool testCloseAngled(MathAtom const & at)
{
	return testSymbol(at, "rangle");
}


MathAtom replaceAngledDelims(const MathData & md)
{
	return MathAtom(new InsetMathDelim(const_cast<Buffer *>(md.buffer()),
		from_ascii("langle"), from_ascii("rangle"), md, true));
}


// replace '('...')', '['...']', '|'...'|', and '<'...'>' sequences by a real InsetMathDelim
void extractDelims(MathData & md)
{
	//lyxerr << "\nDelims from: " << md << endl;
	replaceNested(md, testOpenParen, testCloseParen, replaceParenDelims);
	replaceNested(md, testOpenBracket, testCloseBracket, replaceBracketDelims);
	replaceNested(md, testOpenVert, testCloseVert, replaceVertDelims);
	replaceNested(md, testOpenAngled, testCloseAngled, replaceAngledDelims);
	//lyxerr << "\nDelims to: " << md << endl;
}



//
// search well-known functions
//


// replace 'f' '(...)' and 'f' '^n' '(...)' sequences by a real InsetMathExFunc
// assume 'extractDelims' ran before
void extractFunctions(MathData & md, ExternalMath kind)
{
	// FIXME From what I can see, this is quite broken right now, for reasons
	// I will note below. (RGH)

	// we need at least two items...
	if (md.size() < 2)
		return;

	Buffer * buf = md.buffer();

	//lyxerr << "\nFunctions from: " << md << endl;
	for (size_t i = 0; i + 1 < md.size(); ++i) {
		MathData::iterator it = md.begin() + i;
		MathData::iterator jt = it + 1;

		docstring name;
		// is it a function?
		// it certainly is if it is well known...

		// FIXME This will never give us anything. When we get here, *it will
		// never point at a string, but only at a character. I.e., if we are
		// working on "sin(x)", then we are seeing:
		// [char s mathalpha][char i mathalpha][char n mathalpha][delim ( ) [char x mathalpha]]
		// and of course we will not find the function name "sin" in there, but
		// rather "n(x)".
		//
		// It appears that we original ran extractStrings() before we ran
		// extractFunctions(), but Andre changed this at f200be55, I think
		// because this messed up what he was trying to do with "dx" in the
		// context of integrals.
		//
		// This could be fixed by looking at a charSequence instead of just at
		// the various characters, one by one. But I am not sure I understand
		// exactly what we are trying to do here. And it involves a lot of
		// guessing.
		if (!extractFunctionName(*it, name)) {
			// is this a user defined function?
			// probably not, if it doesn't have a name.
			if (!extractString(*it, name))
				continue;
			// it is not if it has no argument
			if (jt == md.end())
				continue;
			// guess so, if this is followed by
			// a DelimInset with a single item in the cell
			InsetMathDelim const * del = (*jt)->asDelimInset();
			if (!del || del->cell(0).size() != 1)
				continue;
			// fall through into main branch
		}

		// do we have an exponent like in
		// 'sin' '^2' 'x' -> 'sin(x)' '^2'
		MathData exp(buf);
		extractScript(exp, jt, md.end(), true);

		// create a proper inset as replacement
		auto p = make_unique<InsetMathExFunc>(buf, name);

		// jt points to the "argument". Get hold of this.
		MathData::iterator st =
				extractArgument(p->cell(0), jt, md.end(), kind, true);

		// replace the function name by a real function inset
		*it = MathAtom(p.release());

		// remove the source of the argument from the data
		md.erase(it + 1, st);

		// re-insert exponent
		md.insert(i + 1, exp);
		//lyxerr << "\nFunctions to: " << md << endl;
	}
}


//
// search integrals
//

bool testIntSymbol(MathAtom const & at)
{
	return testSymbol(at, from_ascii("int"));
}


bool testIntegral(MathAtom const & at)
{
	return
	 testIntSymbol(at) ||
		( at->asScriptInset()
		  && !at->asScriptInset()->nuc().empty()
			&& testIntSymbol(at->asScriptInset()->nuc().back()) );
}



bool testIntDiff(MathAtom const & at)
{
	return testString(at, "d");
}


// replace '\int' ['_^'] x 'd''x'(...)' sequences by a real InsetMathExInt
// assume 'extractDelims' ran before
void extractIntegrals(MathData & md, ExternalMath kind)
{
	// we need at least three items...
	if (md.size() < 3)
		return;

	Buffer * buf = md.buffer();

	//lyxerr << "\nIntegrals from: " << md << endl;
	for (size_t i = 0; i + 1 < md.size(); ++i) {
		MathData::iterator it = md.begin() + i;

		// search 'd'
		MathData::iterator jt =
			endNestSearch(it, md.end(), testIntegral, testIntDiff);

		// something sensible found?
		if (jt == md.end())
			continue;

		// is this a integral name?
		if (!testIntegral(*it))
			continue;

		// \int is a macro nowadays (like in LaTeX), the symbol is \intop.
		auto p = make_unique<InsetMathExInt>(buf, from_ascii("intop"));

		// handle scripts if available
		if (!testIntSymbol(*it)) {
			p->cell(2) = (*it)->asScriptInset()->down();
			p->cell(3) = (*it)->asScriptInset()->up();
		}
		// core is part from behind the scripts to the 'd'
		p->cell(0) = MathData(buf, it + 1, jt);

		// use the "thing" behind the 'd' as differential
		MathData::iterator tt = extractArgument(p->cell(1), jt + 1, md.end(), kind);

		// remove used parts
		md.erase(it + 1, tt);
		*it = MathAtom(p.release());
	}
	//lyxerr << "\nIntegrals to: " << md << endl;
}


bool testTermDelimiter(MathAtom const & at)
{
	return testString(at, "+") || testString(at, "-");
}


// try to extract a "term", i.e., something delimited by '+' or '-'.
// returns position behind the term
MathData::iterator extractTerm(MathData & md,
	MathData::iterator pos, MathData::iterator last)
{
	while (pos != last && !testTermDelimiter(*pos)) {
		md.push_back(*pos);
		++pos;
	}
	return pos;
}


//
// search sums
//


bool testEqualSign(MathAtom const & at)
{
	return testString(at, "=");
}


bool testSumSymbol(MathAtom const & p)
{
	return testSymbol(p, from_ascii("sum"));
}


bool testSum(MathAtom const & at)
{
	return
	 testSumSymbol(at) ||
		( at->asScriptInset()
		  && !at->asScriptInset()->nuc().empty()
			&& testSumSymbol(at->asScriptInset()->nuc().back()) );
}


// replace '\sum' ['_^'] f(x) sequences by a real InsetMathExInt
// assume 'extractDelims' ran before
void extractSums(MathData & md)
{
	// we need at least two items...
	if (md.size() < 2)
		return;

	Buffer * buf = md.buffer();

	//lyxerr << "\nSums from: " << md << endl;
	for (size_t i = 0; i + 1 < md.size(); ++i) {
		MathData::iterator it = md.begin() + i;

		// is this a sum name?
		if (!testSum(md[i]))
			continue;

		// create a proper inset as replacement
		auto p = make_unique<InsetMathExInt>(buf, from_ascii("sum"));

		// collect lower bound and summation index
		InsetMathScript const * sub = md[i]->asScriptInset();
		if (sub && sub->hasDown()) {
			// try to figure out the summation index from the subscript
			MathData const & md = sub->down();
			MathData::const_iterator xt =
				find_if(md.begin(), md.end(), &testEqualSign);
			if (xt != md.end()) {
				// we found a '=', use everything in front of that as index,
				// and everything behind as lower index
				p->cell(1) = MathData(buf, md.begin(), xt);
				p->cell(2) = MathData(buf, xt + 1, md.end());
			} else {
				// use everything as summation index, don't use scripts.
				p->cell(1) = md;
			}
		}

		// collect upper bound
		if (sub && sub->hasUp())
			p->cell(3) = sub->up();

		// use something  behind the script as core
		MathData::iterator tt = extractTerm(p->cell(0), it + 1, md.end());

		// cleanup
		md.erase(it + 1, tt);
		*it = MathAtom(p.release());
	}
	//lyxerr << "\nSums to: " << md << endl;
}


//
// search differential stuff
//

// tests for 'd' or '\partial'
bool testDiffItem(MathAtom const & at)
{
	if (testString(at, "d") || testSymbol(at, "partial"))
		return true;

	// we may have d^n .../d and splitScripts() has not yet seen it
	InsetMathScript const * sup = at->asScriptInset();
	if (sup && !sup->hasDown() && sup->hasUp() && sup->nuc().size() == 1) {
		MathAtom const & ma = sup->nuc().front();
		return testString(ma, "d") || testSymbol(ma, "partial");
	}
	return false;
}


bool testDiffCell(MathData const & md)
{
	return !md.empty() && testDiffItem(md.front());
}


bool testDiffFrac(MathAtom const & at)
{
	return
		at->asFracInset()
			&& testDiffCell(at->asFracInset()->cell(0))
			&& testDiffCell(at->asFracInset()->cell(1));
}


void extractDiff(MathData & md)
{
	Buffer * buf = md.buffer();
	//lyxerr << "\nDiffs from: " << md << endl;
	for (size_t i = 0; i < md.size(); ++i) {
		MathData::iterator it = md.begin() + i;

		// is this a "differential fraction"?
		if (!testDiffFrac(*it))
			continue;

		InsetMathFrac const * f = (*it)->asFracInset();
		if (!f) {
			lyxerr << "should not happen" << endl;
			continue;
		}

		// create a proper diff inset
		auto diff = make_unique<InsetMathDiff>(buf);

		// collect function, let jt point behind last used item
		MathData::iterator jt = it + 1;
		//int n = 1;
		MathData numer(f->cell(0));
		splitScripts(numer);
		if (numer.size() > 1 && numer[1]->asScriptInset()) {
			// this is something like  d^n f(x) / d... or  d^n / d...
			// FIXME
			//n = 1;
			if (numer.size() > 2)
				diff->cell(0) = MathData(buf, numer.begin() + 2, numer.end());
			else
				jt = extractTerm(diff->cell(0), jt, md.end());
		} else {
			// simply d f(x) / d... or  d/d...
			if (numer.size() > 1)
				diff->cell(0) = MathData(buf, numer.begin() + 1, numer.end());
			else
				jt = extractTerm(diff->cell(0), jt, md.end());
		}

		// collect denominator parts
		MathData denom(f->cell(1));
		splitScripts(denom);
		for (MathData::iterator dt = denom.begin(); dt != denom.end();) {
			// find the next 'd'
			MathData::iterator et
				= find_if(dt + 1, denom.end(), &testDiffItem);

			// point before this
			MathData::iterator st = et - 1;
			InsetMathScript const * script = (*st)->asScriptInset();
			if (script && script->hasUp()) {
				// things like   d.../dx^n
				int mult = 1;
				if (extractNumber(script->up(), mult)) {
					//lyxerr << "mult: " << mult << endl;
					if (mult < 0 || mult > 1000) {
						lyxerr << "Cannot differentiate less than 0 or more than 1000 times !" << endl;
						continue;
					}
					for (int ii = 0; ii < mult; ++ii)
						diff->addDer(MathData(buf, dt + 1, st));
				}
			} else {
				// just  d.../dx
				diff->addDer(MathData(buf, dt + 1, et));
			}
			dt = et;
		}

		// cleanup
		md.erase(it + 1, jt);
		*it = MathAtom(diff.release());
	}
	//lyxerr << "\nDiffs to: " << md << endl;
}


//
// search limits
//


bool testRightArrow(MathAtom const & at)
{
	return testSymbol(at, "to") || testSymbol(at, "rightarrow");
}



// replace '\lim_{x->x0} f(x)' sequences by a real InsetMathLim
// assume 'extractDelims' ran before
void extractLims(MathData & md)
{
	Buffer * buf = md.buffer();
	//lyxerr << "\nLimits from: " << md << endl;
	for (size_t i = 0; i < md.size(); ++i) {
		MathData::iterator it = md.begin() + i;

		// must be a script inset with a subscript (without superscript)
		InsetMathScript const * sub = (*it)->asScriptInset();
		if (!sub || !sub->hasDown() || sub->hasUp() || sub->nuc().size() != 1)
			continue;

		// is this a limit function?
		if (!testSymbol(sub->nuc().front(), "lim"))
			continue;

		// subscript must contain a -> symbol
		MathData const & s = sub->down();
		MathData::const_iterator st = find_if(s.begin(), s.end(), &testRightArrow);
		if (st == s.end())
			continue;

		// the -> splits the subscript int x and x0
		MathData x  = MathData(buf, s.begin(), st);
		MathData x0 = MathData(buf, st + 1, s.end());

		// use something behind the script as core
		MathData f(buf);
		MathData::iterator tt = extractTerm(f, it + 1, md.end());

		// cleanup
		md.erase(it + 1, tt);

		// create a proper inset as replacement
		*it = MathAtom(new InsetMathLim(buf, f, x, x0));
	}
	//lyxerr << "\nLimits to: " << md << endl;
}


//
// combine searches
//

void extractStructure(MathData & md, ExternalMath kind)
{
	//lyxerr << "\nStructure from: " << md << endl;
	if (kind != MATHML && kind != HTML)
		splitScripts(md);
	extractDelims(md);
	extractIntegrals(md, kind);
	if (kind != MATHML && kind != HTML)
		extractSums(md);
	extractNumbers(md);
	extractMatrices(md);
	if (kind != MATHML && kind != HTML) {
		extractFunctions(md, kind);
		extractDets(md);
		extractDiff(md);
		extractExps(md);
		extractLims(md);
		extractStrings(md);
	}
	//lyxerr << "\nStructure to: " << md << endl;
}


namespace {

	string captureOutput(string const & cmd, string const & data)
	{
		// In order to avoid parsing problems with command interpreters
		// we pass input data through a file
		// Since the CAS is supposed to read the temp file we need
		// to unlock it on windows (bug 10262).
		unique_ptr<TempFile> tempfile(new TempFile("casinput"));
		tempfile->setAutoRemove(false);
		FileName const cas_tmpfile = tempfile->name();
		tempfile.reset();

		if (cas_tmpfile.empty()) {
			lyxerr << "Warning: cannot create temporary file."
			       << endl;
			return string();
		}
		ofstream os(cas_tmpfile.toFilesystemEncoding().c_str());
		os << data << endl;
		os.close();
		string command =  cmd + " < "
			+ quoteName(cas_tmpfile.toFilesystemEncoding());
		lyxerr << "calling: " << cmd
		       << "\ninput: '" << data << "'" << endl;
		cmd_ret const ret = runCommand(command);
		cas_tmpfile.removeFile();
		return ret.result;
	}

	size_t get_matching_brace(string const & str, size_t i)
	{
		int count = 1;
		size_t n = str.size();
		while (i < n) {
			i = str.find_first_of("{}", i+1);
			if (i == string::npos)
				return i;
			if (str[i] == '{')
				++count;
			else
				--count;
			if (count == 0)
				return i;
		}
		return string::npos;
	}

	size_t get_matching_brace_back(string const & str, size_t i)
	{
		int count = 1;
		while (i > 0) {
			i = str.find_last_of("{}", i-1);
			if (i == string::npos)
				return i;
			if (str[i] == '}')
				++count;
			else
				--count;
			if (count == 0)
				return i;
		}
		return string::npos;
	}

	MathData pipeThroughMaxima(docstring const &command, MathData const & md)
	{
		odocstringstream os;
		MaximaStream ms(os);
		ms << md;
		docstring expr = os.str();
		docstring const header = from_ascii("simpsum:true;");

		docstring comm_left = from_ascii("tex(");
		docstring comm_right = from_ascii(");");
		// "simpsum:true;tex(COMMAND(EXPR));" if command (e.g. "factor") is present
		if (command != "noextra") {
			comm_left = comm_left + command + "(";
			comm_right = ")" + comm_right;
		}
		int preplen = comm_left.length();
		int headlen = header.length();

		// remove spaces after '%' sign
		while (expr.find(from_ascii("% ")) != docstring::npos)
		    expr = subst(expr, from_ascii("% "), from_ascii("%"));

		string out;
		for (int i = 0; i < 100; ++i) { // at most 100 attempts
			// try to fix missing '*' the hard way
			//
			// > echo "2x;" | maxima
			// ...
			// (C1) Incorrect syntax: x is not an infix operator
			// 2x;
			//  ^
			//
			docstring full = header + comm_left + expr + comm_right;
			lyxerr << "checking input: '" << to_utf8(full) << "'" << endl;
			out = captureOutput("maxima", to_utf8(full));

			// leave loop if expression syntax is probably ok
			if (ascii_lowercase(out).find("incorrect syntax") == string::npos)
				break;

			// search line with "Incorrect syntax"
			istringstream is(out);
			string line;
			while (is) {
				getline(is, line);
				if (ascii_lowercase(line).find("incorrect syntax") != string::npos)
					break;
			}

			// 2nd next line is the one with caret
			getline(is, line);
			getline(is, line);
			size_t pos = line.find('^');
			//we print with header at lyxerr, but maxima won't show it in its error
			lyxerr << "found caret at pos: '" << pos + headlen << "'" << endl;
			if (pos == string::npos || pos < 4)
				break; // caret position not found
			pos -= preplen; // skip the "tex(command(" part (header is not printed by maxima)
			if (expr[pos] == '*')
				break; // two '*' in a row are definitely bad
			expr.insert(pos, from_ascii("*"));
		}

		vector<string> tmp = getVectorFromString(out, "$$");
		if (tmp.size() < 2)
			return MathData(nullptr);

		out = subst(subst(tmp[1], "\\>", string()), "{\\it ", "\\mathit{");

		// When returning a matrix, maxima produces a latex snippet
		// for using one of the two constructs "\pmatrix{...}" or
		// "\begin{pmatrix}...\end{pmatrix}" depending on whether
		// the macro \endpmatrix is defined (e.g. by amsmath) or not.
		// However, our math parser cannot cope with this latex
		// snippet. So, simply retain the \begin{}...\end{} version.
		if (out.find("\\ifx\\endpmatrix\\undefined") != string::npos) {
			out = subst(subst(subst(subst(out,
				"\\ifx\\endpmatrix\\undefined", string()),
				"\\pmatrix{\\else\\begin{pmatrix}\\fi", "\\begin{pmatrix}"),
				"}\\else\\end{pmatrix}\\fi", "\\end{pmatrix}"),
				"\\cr", "\\\\");
		}

		lyxerr << "output: '" << out << "'" << endl;

		// Ugly code that tries to make the result prettier
		size_t i = out.find("\\mathchoice");
		while (i != string::npos) {
			size_t j = get_matching_brace(out, i + 12);
			size_t k = get_matching_brace(out, j + 1);
			k = get_matching_brace(out, k + 1);
			k = get_matching_brace(out, k + 1);
			string mid = out.substr(i + 13, j - i - 13);
			if (mid.find("\\over") != string::npos)
				mid = '{' + mid + '}';
			out = out.substr(0, i)
				+ mid
				+ out.substr(k + 1);
			//lyxerr << "output: " << out << endl;
			i = out.find("\\mathchoice", i);
		}

		i = out.find("\\over");
		while (i != string::npos) {
			size_t j = get_matching_brace_back(out, i - 1);
			if (j == string::npos || j == 0)
				break;
			size_t k = get_matching_brace(out, i + 5);
			if (k == string::npos || k + 1 == out.size())
				break;
			out = out.substr(0, j - 1)
				+ "\\frac"
				+ out.substr(j, i - j)
				+ out.substr(i + 5, k - i - 4)
				+ out.substr(k + 2);
			//lyxerr << "output: " << out << endl;
			i = out.find("\\over", i + 4);
		}
		MathData res(nullptr);
		mathed_parse_cell(res, from_utf8(out));
		return res;
	}


	MathData pipeThroughMaple(docstring const & extra, MathData const & md)
	{
		string header = "readlib(latex):\n";

		// remove the \\it for variable names
		//"#`latex/csname_font` := `\\it `:"
		header +=
			"`latex/csname_font` := ``:\n";

		// export matrices in (...) instead of [...]
		header +=
			"`latex/latex/matrix` := "
				"subs(`[`=`(`, `]`=`)`,"
					"eval(`latex/latex/matrix`)):\n";

		// replace \\cdots with proper '*'
		header +=
			"`latex/latex/*` := "
				"subs(`\\,`=`\\cdot `,"
					"eval(`latex/latex/*`)):\n";

		// remove spurious \\noalign{\\medskip} in matrix output
		header +=
			"`latex/latex/matrix`:= "
				"subs(`\\\\\\\\\\\\noalign{\\\\medskip}` = `\\\\\\\\`,"
					"eval(`latex/latex/matrix`)):\n";

		//"#`latex/latex/symbol` "
		//	" := subs((\\'_\\' = \\'`\\_`\\',eval(`latex/latex/symbol`)): ";

		string trailer = "quit;";
		odocstringstream os;
		MapleStream ms(os);
		ms << md;
		string expr = to_utf8(os.str());
		lyxerr << "md: '" << md << "'\n"
		       << "ms: '" << expr << "'" << endl;

		for (int i = 0; i < 100; ++i) { // at most 100 attempts
			// try to fix missing '*' the hard way by using mint
			//
			// ... > echo "1A;" | mint -i 1 -S -s -q
			// on line     1: 1A;
			//                 ^ syntax error -
			//                   Probably missing an operator such as * p
			//
			lyxerr << "checking expr: '" << expr << "'" << endl;
			string out = captureOutput("mint -i 1 -S -s -q -q", expr + ';');
			if (out.empty())
				break; // expression syntax is ok
			istringstream is(out);
			string line;
			getline(is, line);
			if (!prefixIs(line, "on line"))
				break; // error message not identified
			getline(is, line);
			size_t pos = line.find('^');
			if (pos == string::npos || pos < 15)
				break; // caret position not found
			pos -= 15; // skip the "on line ..." part
			if (expr[pos] == '*' || (pos > 0 && expr[pos - 1] == '*'))
				break; // two '*' in a row are definitely bad
			expr.insert(pos, 1, '*');
		}

		// FIXME UNICODE Is utf8 encoding correct?
		string full = "latex(" + to_utf8(extra) + '(' + expr + "));";
		string out = captureOutput("maple -q", header + full + trailer);

		// change \_ into _

		//
		MathData res(nullptr);
		mathed_parse_cell(res, from_utf8(out));
		return res;
	}


	MathData pipeThroughOctave(docstring const &command, MathData const & md)
	{
		odocstringstream os;
		OctaveStream vs(os);
		vs << md;
		string expr = to_utf8(os.str());
		string out;
		// FIXME const cast
		Buffer * buf = const_cast<Buffer *>(md.buffer());
		lyxerr << "pipe: md: '" << md << "'\n"
		       << "pipe: expr: '" << expr << "'" << endl;

		string comm_left;
		string comm_right;
		if (command != "noextra") {
			comm_left = to_utf8(command) + "(";
			comm_right = ")" + comm_right;
		}
		int preplen = comm_left.length();

		for (int i = 0; i < 100; ++i) { // at most 100 attempts
			//
			// try to fix missing '*' the hard way
			// parse error:
			// >>> ([[1 2 3 ];[2 3 1 ];[3 1 2 ]])([[1 2 3 ];[2 3 1 ];[3 1 2 ]])
			//                                   ^
			//
			string full = comm_left + expr + comm_right;
			lyxerr << "checking input: '" << full << "'" << endl;
			out = captureOutput("octave -q 2>&1", full);
			lyxerr << "output: '" << out << "'" << endl;

			// leave loop if expression syntax is probably ok
			if (out.find("parse error:") == string::npos)
				break;

			// search line with single caret
			istringstream is(out);
			string line;
			while (is) {
				getline(is, line);
				lyxerr << "skipping line: '" << line << "'" << endl;
				if (line.find(">>> ") != string::npos)
					break;
			}

			// found line with error, next line is the one with caret
			getline(is, line);
			size_t pos = line.find('^');
			lyxerr << "caret line   : '" << line << "'" << endl;
			lyxerr << "found caret at pos: '" << pos << "'" << endl;
			if (pos == string::npos || pos < 4)
				break; // caret position not found
			pos -= 4 + preplen; // skip the ">>> " part and command prefix
			if (expr[pos] == '*')
				break; // two '*' in a row are definitely bad
			expr.insert(pos, 1, '*');
		}

		// remove 'ans = ' taking into account that there may be an
		// ansi control sequence before, such as '\033[?1034hans = '
		size_t i = out.find("ans = ");
		if (i == string::npos)
			i = out.find("ans =\n");
		if (i == string::npos)
			return MathData(nullptr);
		out = out.substr(i + 6);

		// parse output as matrix or single number
		MathAtom at(new InsetMathArray(buf, from_ascii("array"), from_utf8(out)));
		InsetMathArray const * mat = at->asArrayInset();
		MathData res(buf);
		if (mat->ncols() == 1 && mat->nrows() == 1)
			res.append(mat->cell(0));
		else {
			res.emplace_back(new InsetMathDelim(buf, from_ascii("("), from_ascii(")")));
			res.back().nucleus()->cell(0).push_back(at);
		}
		return res;
	}


	string fromMathematicaName(string const & name)
	{
		if (name == "Sin")    return "sin";
		if (name == "Sinh")   return "sinh";
		if (name == "ArcSin") return "arcsin";
		if (name == "Cos")    return "cos";
		if (name == "Cosh")   return "cosh";
		if (name == "ArcCos") return "arccos";
		if (name == "Tan")    return "tan";
		if (name == "Tanh")   return "tanh";
		if (name == "ArcTan") return "arctan";
		if (name == "Cot")    return "cot";
		if (name == "Coth")   return "coth";
		if (name == "Csc")    return "csc";
		if (name == "Sec")    return "sec";
		if (name == "Exp")    return "exp";
		if (name == "Log")    return "log";
		if (name == "Arg" )   return "arg";
		if (name == "Det" )   return "det";
		if (name == "GCD" )   return "gcd";
		if (name == "Max" )   return "max";
		if (name == "Min" )   return "min";
		if (name == "Erf" )   return "erf";
		if (name == "Erfc" )  return "erfc";
		return name;
	}


	void prettifyMathematicaOutput(string & out, string const & macroName,
			bool roman, bool translate)
	{
		string const macro = "\\" + macroName + "{";
		size_t const len = macro.length();
		size_t i = out.find(macro);

		while (i != string::npos) {
			size_t const j = get_matching_brace(out, i + len);
			string const name = out.substr(i + len, j - i - len);
			out = out.substr(0, i)
				+ (roman ? "\\mathrm{" : "")
				+ (translate ? fromMathematicaName(name) : name)
				+ out.substr(roman ? j : j + 1);
			//lyxerr << "output: " << out << endl;
			i = out.find(macro, i);
		}
	}


	MathData pipeThroughMathematica(docstring const &, MathData const & md)
	{
		odocstringstream os;
		MathematicaStream ms(os);
		ms << md;
		// FIXME UNICODE Is utf8 encoding correct?
		string const expr = to_utf8(os.str());
		string out;

		lyxerr << "expr: '" << expr << "'" << endl;

		string const full = "TeXForm[" + expr + "]";
		out = captureOutput("math", full);
		lyxerr << "output: '" << out << "'" << endl;

		size_t pos1 = out.find("Out[1]//TeXForm= ");
		size_t pos2 = out.find("In[2]:=");

		if (pos1 == string::npos || pos2 == string::npos)
			return MathData(nullptr);

		// get everything from pos1+17 to pos2
		out = out.substr(pos1 + 17, pos2 - pos1 - 17);
		out = subst(subst(out, '\r', ' '), '\n', ' ');

		// tries to make the result prettier
		prettifyMathematicaOutput(out, "Mfunction", true, true);
		prettifyMathematicaOutput(out, "Muserfunction", true, false);
		prettifyMathematicaOutput(out, "Mvariable", false, false);

		MathData res(nullptr);
		mathed_parse_cell(res, from_utf8(out));
		return res;
	}

} // namespace

} // namespace

void write(MathData const & dat, TeXMathStream & wi)
{
	wi.firstitem() = true;
	docstring s;
	for (MathData::const_iterator it = dat.begin(); it != dat.end(); ++it) {
		InsetMathChar const * const c = (*it)->asCharInset();
		if (c)
			s += c->getChar();
		else {
			if (!s.empty()) {
				writeString(s, wi);
				s.clear();
			}
			(*it)->write(wi);
			wi.firstitem() = false;
		}
	}
	if (!s.empty()) {
		writeString(s, wi);
		wi.firstitem() = false;
	}
}


void writeString(docstring const & s, TeXMathStream & os)
{
	if (!os.latex()) {
		os << s;
		return;
	}

	docstring str = s;
	if (os.asciiOnly())
		str = escape(s);

	if (os.output() == TeXMathStream::wsSearchAdv) {
		os << str;
		return;
	}

	if (os.lockedMode()) {
		bool space;
		docstring cmd;
		for (char_type c : str) {
			try {
				Encodings::latexMathChar(c, true, os.encoding(), cmd, space);
				os << cmd;
				os.pendingSpace(space);
			} catch (EncodingException const & e) {
				switch (os.output()) {
				case TeXMathStream::wsDryrun: {
					os << "<" << _("LyX Warning: ")
					   << _("uncodable character") << " '";
					os << docstring(1, e.failed_char);
					os << "'>";
					break;
				}
				case TeXMathStream::wsPreview: {
					// indicate the encoding error by a boxed '?'
					os << "{\\fboxsep=1pt\\fbox{?}}";
					LYXERR0("Uncodable character" << " '"
						<< docstring(1, e.failed_char)
						<< "'");
					break;
				}
				case TeXMathStream::wsDefault:
				default:
					// throw again
					throw;
				}
			}
		}
		return;
	}

	// We may already be inside an \ensuremath command.
	bool in_forced_mode = os.pendingBrace();

	// We will take care of matching braces.
	os.pendingBrace(false);

	for (char_type const c : str) {
		bool mathmode = in_forced_mode ? os.textMode() : !os.textMode();
		docstring command(1, c);
		try {
			bool termination = false;
			if (isASCII(c) ||
			    Encodings::latexMathChar(c, mathmode, os.encoding(), command, termination)) {
				if (os.textMode()) {
					if (in_forced_mode) {
						// we were inside \lyxmathsym
						os << '}';
						os.textMode(false);
						in_forced_mode = false;
					}
					if (!isASCII(c) && os.textMode()) {
						os << "\\ensuremath{";
						os.textMode(false);
						in_forced_mode = true;
					}
				} else if (isASCII(c) && in_forced_mode) {
					// we were inside \ensuremath
					os << '}';
					os.textMode(true);
					in_forced_mode = false;
				}
			} else if (!os.textMode()) {
					if (in_forced_mode) {
						// we were inside \ensuremath
						os << '}';
						in_forced_mode = false;
					} else {
						os << "\\lyxmathsym{";
						in_forced_mode = true;
					}
					os.textMode(true);
			}
			os << command;
			// We may need a space if the command contains a macro
			// and the last char is ASCII.
			if (termination)
				os.pendingSpace(true);
		} catch (EncodingException const & e) {
			switch (os.output()) {
			case TeXMathStream::wsDryrun: {
				os << "<" << _("LyX Warning: ")
				   << _("uncodable character") << " '";
				os << docstring(1, e.failed_char);
				os << "'>";
				break;
			}
			case TeXMathStream::wsPreview: {
				// indicate the encoding error by a boxed '?'
				os << "{\\fboxsep=1pt\\fbox{?}}";
				LYXERR0("Uncodable character" << " '"
					<< docstring(1, e.failed_char)
					<< "'");
				break;
			}
			case TeXMathStream::wsDefault:
			default:
				// throw again
				throw;
			}
		}
	}

	if (in_forced_mode && os.textMode()) {
		// We have to care for closing \lyxmathsym
		os << '}';
		os.textMode(false);
	} else {
		os.pendingBrace(in_forced_mode);
	}
}


void normalize(MathData const & md, NormalStream & os)
{
	for (MathData::const_iterator it = md.begin(); it != md.end(); ++it)
		(*it)->normalize(os);
}


void octave(MathData const & dat, OctaveStream & os)
{
	MathData md = dat;
	extractStructure(md, OCTAVE);
	for (MathData::const_iterator it = md.begin(); it != md.end(); ++it)
		(*it)->octave(os);
}


void maple(MathData const & dat, MapleStream & os)
{
	MathData md = dat;
	extractStructure(md, MAPLE);
	for (MathData::const_iterator it = md.begin(); it != md.end(); ++it)
		(*it)->maple(os);
}


void maxima(MathData const & dat, MaximaStream & os)
{
	MathData md = dat;
	extractStructure(md, MAXIMA);
	for (MathData::const_iterator it = md.begin(); it != md.end(); ++it)
		(*it)->maxima(os);
}


void mathematica(MathData const & dat, MathematicaStream & os)
{
	MathData md = dat;
	extractStructure(md, MATHEMATICA);
	for (MathData::const_iterator it = md.begin(); it != md.end(); ++it)
		(*it)->mathematica(os);
}


void mathmlize(MathData const & dat, MathMLStream & ms)
{
	MathData md = dat;
	extractStructure(md, MATHML);
	if (md.empty()) {
		if (!ms.inText())
			ms << CTag("mrow");
	} else if (md.size() == 1) {
		ms << md.front();
	} else {
		// protect against the value changing in the second test.
		bool const intext = ms.inText();
		if (!intext)
			ms << MTag("mrow");
		for (MathData::const_iterator it = md.begin(); it != md.end(); ++it)
			(*it)->mathmlize(ms);
		if (!intext)
			ms << ETag("mrow");
	}
}


void htmlize(MathData const & dat, HtmlStream & os)
{
	MathData md = dat;
	extractStructure(md, HTML);
	if (md.empty())
		return;
	if (md.size() == 1) {
		os << md.front();
		return;
	}
	for (MathData::const_iterator it = md.begin(); it != md.end(); ++it)
		(*it)->htmlize(os);
}


// convert this inset somehow to a number
bool extractNumber(MathData const & md, int & i)
{
	idocstringstream is(charSequence(md.begin(), md.end()));
	is >> i;
	// Do not convert is implicitly to bool, since that is forbidden in C++11.
	return !is.fail();
}


bool extractNumber(MathData const & md, double & d)
{
	idocstringstream is(charSequence(md.begin(), md.end()));
	is >> d;
	// Do not convert is implicitly to bool, since that is forbidden in C++11.
	return !is.fail();
}


MathData pipeThroughExtern(string const & lang, docstring const & extra,
	MathData const & md)
{
	if (lang == "octave")
		return pipeThroughOctave(extra, md);

	if (lang == "maxima")
		return pipeThroughMaxima(extra, md);

	if (lang == "maple")
		return pipeThroughMaple(extra, md);

	if (lang == "mathematica")
		return pipeThroughMathematica(extra, md);

	// create normalized expression
	odocstringstream os;
	NormalStream ns(os);
	os << '[' << extra << ' ';
	ns << md;
	os << ']';
	// FIXME UNICODE Is utf8 encoding correct?
	string data = to_utf8(os.str());

	// search external script
	FileName const file = libFileSearch("mathed", "extern_" + lang);
	if (file.empty()) {
		lyxerr << "converter to '" << lang << "' not found" << endl;
		return MathData(nullptr);
	}

	// run external sript
	string out = captureOutput(file.absFileName(), data);
	MathData res(nullptr);
	mathed_parse_cell(res, from_utf8(out));
	return res;
}


} // namespace lyx
