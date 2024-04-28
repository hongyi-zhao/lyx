// -*- C++ -*-
/**
 * \file InsetIndex.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef INSET_INDEX_H
#define INSET_INDEX_H


#include "InsetCollapsible.h"
#include "InsetCommand.h"


namespace lyx {

class IndexEntry;

class InsetIndexParams {
public:
	enum PageRange {
		None,
		Start,
		End
	};
	///
	explicit InsetIndexParams(docstring const & b = docstring())
		: index(b), range(None), pagefmt("default") {}
	///
	void write(std::ostream & os) const;
	///
	void read(support::Lexer & lex);
	///
	docstring index;
	///
	PageRange range;
	///
	std::string pagefmt;
};


/** Used to insert index labels
  */
class InsetIndex : public InsetCollapsible {
public:
	///
	InsetIndex(Buffer *, InsetIndexParams const &);
	///
	static std::string params2string(InsetIndexParams const &);
	///
	static void string2params(std::string const &, InsetIndexParams &);
	///
	const InsetIndexParams& params() const { return params_; }
	///
	int rowFlags() const override { return CanBreakBefore | CanBreakAfter; }
	///
	InsetIndex const * asInsetIndex() const override { return this; }
private:
	///
	bool hasSettings() const override;
	///
	InsetCode lyxCode() const override { return INDEX_CODE; }
	///
	docstring layoutName() const override { return from_ascii("Index"); }
	///
	ColorCode labelColor() const override;
	///
	void write(std::ostream & os) const override;
	///
	void read(support::Lexer & lex) override;
	///
	void docbook(XMLStream &, OutputParams const &) const override;
	///
	docstring xhtml(XMLStream &, OutputParams const &) const override;
	///
	void latex(otexstream &, OutputParams const &) const override;
	///
	void processLatexSorting(otexstream &, OutputParams const &,
				 docstring const &, docstring const &) const;
	///
	bool showInsetDialog(BufferView *) const override;
	///
	bool getStatus(Cursor &, FuncRequest const &, FuncStatus &) const override;
	///
	void doDispatch(Cursor & cur, FuncRequest & cmd) override;
	/// should paragraph indentation be omitted in any case?
	bool neverIndent() const override { return true; }
	///
	void addToToc(DocIterator const & di, bool output_active,
				  UpdateType utype, TocBackend & backend) const override;
	///
	docstring toolTip(BufferView const & bv, int x, int y) const override;
	///
	docstring const buttonLabel(BufferView const & bv) const override;
	/// Updates needed features for this inset.
	void validate(LaTeXFeatures & features) const override;
	///
	void getSortkey(otexstream &, OutputParams const &) const;
	///
	docstring getSortkeyAsText(OutputParams const &) const;
	///
	void emptySubentriesWarning(docstring const & mainentry) const;
	///
	void getSubentries(otexstream &, OutputParams const &, docstring const &) const;
	///
	std::vector<docstring> getSubentriesAsText(OutputParams const &,
						   bool const asLabel = false) const;
	///
	docstring getMainSubentryAsText(OutputParams const & runparams) const;
	///
	void getSeeRefs(otexstream &, OutputParams const &) const;
	///
	docstring getSeeAsText(OutputParams const & runparams,
			       bool const asLabel = false) const;
	///
	std::vector<docstring> getSeeAlsoesAsText(OutputParams const & runparams,
						  bool const asLabel = false) const;
	///
	bool hasSubentries() const;
	///
	bool hasSeeRef() const;
	///
	bool hasSortKey() const;
	///
	bool macrosPossible(std::string const type) const;
	///
	std::string contextMenuName() const override;
	///
	std::string contextMenu(BufferView const &, int, int) const override;
	///
	Inset * clone() const override { return new InsetIndex(*this); }
	/// Is the content of this inset part of the immediate text sequence?
	bool isPartOfTextSequence() const override { return false; }
	///
	bool insetAllowed(InsetCode code) const override;

	///
	friend class InsetIndexParams;
	///
	friend class IndexEntry;
	///
	InsetIndexParams params_;
};


class InsetPrintIndex : public InsetCommand {
public:
	///
	InsetPrintIndex(Buffer * buf, InsetCommandParams const &);

	/// \name Public functions inherited from Inset class
	//@{
	///
	InsetCode lyxCode() const override { return INDEX_PRINT_CODE; }
	///
	void latex(otexstream &, OutputParams const &) const override;
	///
	docstring xhtml(XMLStream &, OutputParams const &) const override;
	///
	void doDispatch(Cursor & cur, FuncRequest & cmd) override;
	///
	bool getStatus(Cursor &, FuncRequest const &, FuncStatus &) const override;
	///
	void updateBuffer(ParIterator const & it, UpdateType, bool const deleted = false) override;
	///
	std::string contextMenuName() const override;
	/// Updates needed features for this inset.
	void validate(LaTeXFeatures & features) const override;
	///
	bool hasSettings() const override;
	///
	int rowFlags() const override { return Display; }
	//@}

	/// \name Static public methods obligated for InsetCommand derived classes
	//@{
	///
	static ParamInfo const & findInfo(std::string const &);
	///
	static std::string defaultCommand() { return "printindex"; }
	///
	static bool isCompatibleCommand(std::string const & s);
	//@}

private:
	/// \name Private functions inherited from Inset class
	//@{
	///
	Inset * clone() const override { return new InsetPrintIndex(*this); }
	//@}

	/// \name Private functions inherited from InsetCommand class
	//@{
	///
	docstring screenLabel() const override;
	//@}
};


} // namespace lyx

#endif
