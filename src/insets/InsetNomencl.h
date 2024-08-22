// -*- C++ -*-
/**
 * \file InsetNomencl.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 * \author O. U. Baran
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef INSET_NOMENCL_H
#define INSET_NOMENCL_H

#include "InsetCollapsible.h"
#include "InsetCommand.h"


namespace lyx {

class LaTeXFeatures;

/** Used to insert nomenclature entries
  */
class InsetNomencl : public InsetCollapsible {
public:
	///
	InsetNomencl(Buffer * buf);

	/// \name Public functions inherited from Inset class
	//@{
	///
	docstring toolTip(BufferView const & bv, int x, int y) const override;
	///
	bool hasSettings() const override { return true; }
	/// Updates needed features for this inset.
	void validate(LaTeXFeatures & features) const override;
	///
	void addToToc(DocIterator const & di, bool output_active,
		      UpdateType utype, TocBackend & backend) const override;
	///
	InsetCode lyxCode() const override { return NOMENCL_CODE; }
	///
	int plaintext(odocstringstream & ods, OutputParams const & op,
	              size_t max_length = INT_MAX) const override;
	///
	void docbook(XMLStream &, OutputParams const &) const override;
	/// Does nothing at the moment.
	docstring xhtml(XMLStream &, OutputParams const &) const override;
	///
	InsetNomencl const * asInsetNomencl() const override { return this; }
	//@}
	///
	docstring getSymbol() const;
	///
	docstring getDescription() const;
	///
	docstring getPrefix() const;

private:
	/// \name Private functions inherited from Inset class
	//@{
	///
	Inset * clone() const override { return new InsetNomencl(*this); }
	//@}

	/// \name Private functions inherited from InsetCollapsible class
	//@{
	///
	docstring layoutName() const override;
	///
	void write(std::ostream & os) const override;
	//@}
};


class InsetPrintNomencl : public InsetCommand {
public:
	///
	InsetPrintNomencl(Buffer * buf, InsetCommandParams const &);

	/// \name Public functions inherited from Inset class
	//@{
	/// Updates needed features for this inset.
	void validate(LaTeXFeatures & features) const override;
	///
	void docbook(XMLStream &, OutputParams const &) const override;
	///
	docstring xhtml(XMLStream &, OutputParams const &) const override;
	///
	InsetCode lyxCode() const override;
	///
	bool hasSettings() const override;
	///
	int rowFlags() const override { return Display; }
	///
	void latex(otexstream &, OutputParams const &) const override;
	///
	std::string contextMenuName() const override;
	//@}

	/// \name Static public methods obligated for InsetCommand derived classes
	//@{
	///
	static ParamInfo const & findInfo(std::string const &);
	///
	static std::string defaultCommand() { return "printnomenclature"; }
	///
	static bool isCompatibleCommand(std::string const & s)
		{ return s == "printnomenclature"; }
	//@}

private:
	/// \name Private functions inherited from Inset class
	//@{
	///
	Inset * clone() const override { return new InsetPrintNomencl(*this); }
	///
	bool getStatus(Cursor & cur, FuncRequest const & cmd, FuncStatus & status) const override;
	///
	void doDispatch(Cursor & cur, FuncRequest & cmd) override;
	///
	docstring layoutName() const override { return from_ascii("PrintNomencl"); }
	//@}

	/// \name Private functions inherited from InsetCommand class
	//@{
	///
	docstring screenLabel() const override;
	//@}
};

} // namespace lyx

#endif
