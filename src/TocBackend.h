// -*- C++ -*-
/**
 * \file TocBackend.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Jean-Marc Lasgouttes
 * \author Angus Leeming
 * \author Abdelrazak Younes
 * \author Guillaume Munch
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef TOC_BACKEND_H
#define TOC_BACKEND_H

#include "DocIterator.h"
#include "FuncRequest.h"
#include "OutputEnums.h"
#include "Toc.h"
#include "TocBuilder.h"

namespace lyx {

class Buffer;

/* Toc types are described by strings. They cannot be converted into an enum
 * because of the user-configurable categories for index and the user-definable
 * toc types provided in layout files.
 *
 * Here is a summary of built-in toc types
 *
 * Non-customizable (used without TocBuilder): "tableofcontents", "change",
 * "citation", "label", "senseless".
 *
 * Built-in but customizable (used with TocBuilder): "child", "graphics",
 * "equation", "index", "index:<user-str>", "nomencl", "listings", "math-macro",
 * "external", any float type (as defined in the layouts).
 *
 * The following are used for XHTML output: "tableofcontents" (InsetText),
 * "citation" (InsetCitation), any float type.
 *
 * Other types are defined in the layouts.
 */

///
/**
*/
class TocItem
{
public:
	/// Default constructor for STL containers.
	TocItem() : dit_(0), depth_(0), output_(false), missing_(false) {}
	///
	TocItem(DocIterator const & dit,
		int depth,
		docstring const & s,
		bool output_active,
		bool missing = false,
		FuncRequest const & action = FuncRequest(LFUN_UNKNOWN_ACTION)
		);
	///
	DocIterator const & dit() const { return dit_; }
	///
	int depth() const { return depth_; }
	///
	docstring const & str() const { return str_; }
	///
	void str(docstring const & s) { str_ = s; }
	///
	docstring const & prettyStr() const { return pretty_str_; }
	///
	void prettyStr(docstring const & s) { pretty_str_ = s; }
	///
	bool isOutput() const { return output_; }
	///
	bool isMissing() const { return missing_; }
	///
	void setAction(FuncRequest const & a) { action_ = a; }
	/// return comma-separated list of all par IDs (including nested insets)
	/// this is used by captioned elements
	docstring const & parIDs() const { return par_ids_; }
	///
	void setParIDs(docstring const & ids) { par_ids_ = ids; }

	/// custom action, or the default one (paragraph-goto) if not customised
	FuncRequest action() const;
	/// return only main par ID
	int id() const;
	/// String for display, e.g. it has a mark if output is inactive
	docstring const asString() const;

private:
	/// Current position of item.
	DocIterator dit_;
	/// nesting depth
	int depth_;
	/// Full item string
	docstring str_;
	/// Dereferenced name, for labels (e.g. Label 5.2 instead of lem:foobar)
	docstring pretty_str_;
	/// Is this item in a note, inactive branch, etc?
	bool output_;
	/// Is this item missing, e.g. missing label?
	bool missing_;
	/// Custom action
	FuncRequest action_;
	/// Paragraph IDs including nested insets (comma-separated).
	docstring par_ids_;
};


/// Class to build and access the Tocs of a particular buffer.
class TocBackend
{
public:
	static Toc::const_iterator findItem(Toc const & toc,
	                                    DocIterator const & dit);
	/// Look for a TocItem given its depth and string.
	/// \return The first matching item.
	/// \retval end() if no item was found.
	static Toc::iterator findItem(Toc & toc, int depth, docstring const & str);
	///
	TocBackend(Buffer const * buffer) : buffer_(buffer) {}
	///
	void setBuffer(Buffer const * buffer) { buffer_ = buffer; }
	///
	void update(bool output_active, UpdateType utype);
	///
	void reset();
	/// \return true if the item was updated.
	bool updateItem(DocIterator const & pit) const;
	///
	TocList const & tocs() const { return tocs_; }
	/// never null
	std::shared_ptr<Toc const> toc(std::string const & type) const;
	/// never null
	std::shared_ptr<Toc> toc(std::string const & type);
	/// \return the current TocBuilder for the Toc of type \param type, or
	/// creates one if it does not already exist.
	TocBuilder & builder(std::string const & type);
	/// \return the first Toc Item before the cursor.
	/// \param type: Type of Toc.
	/// \param dit: The cursor location in the document.
	Toc::const_iterator
	item(std::string const & type, DocIterator const & dit) const;

	///
	void writePlaintextTocList(std::string const & type,
	        odocstringstream & os, size_t max_length) const;
	/// Localised name for type
	docstring outlinerName(std::string const & type) const;
	/// Add a new (localised) name if yet unknown
	void addName(std::string const & type, docstring const & name);
	/// Whether a toc type is less important and appears in the "Other lists"
	/// submenu
	static bool isOther(std::string const & type);

private:
	///
	void resetOutlinerNames();
	///
	TocList tocs_;
	///
	std::map<std::string, std::unique_ptr<TocBuilder>> builders_;
	/// Stores localised outliner names from this buffer and its children
	std::map<std::string, docstring> outliner_names_;
	///
	Buffer const * buffer_;
}; // TocBackend


} // namespace lyx

#endif // TOC_BACKEND_H
