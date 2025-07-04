// -*- C++ -*-
/**
 * \file BiblioInfo.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Angus Leeming
 * \author Herbert Voß
 * \author Richard Kimberly Heck
 * \author Julien Rioux
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef BIBLIOINFO_H
#define BIBLIOINFO_H

#include "support/docstring.h"

#include <map>
#include <set>
#include <vector>


namespace lyx {

class Buffer;
class BufferParams;
class CitationStyle;
class CiteItem;
class XMLStream;

/// \param latex_str a LaTeX command, "cite", "Citep*", etc
CitationStyle citationStyleFromString(std::string const & latex_str,
				      BufferParams const &);
/// the other way round
std::string citationStyleToString(CitationStyle const &, bool const latex = false);

/// Transforms the information about authors into a <authorgroup> (directly written to a XMLStream).
/// Type: "author" or empty means author of the entry (article, book, etc.); "book" means author of the book
/// (but not necessarily of this entry in particular).
void authorsToDocBookAuthorGroup(docstring const & authorsString, XMLStream & xs,
                                 Buffer const & buf, std::string const & type);


/// Class to represent information about a BibTeX or
/// bibliography entry.
/// This class basically wraps a std::map, and many of its
/// methods simply delegate to the corresponding methods of
/// std::map.
class BibTeXInfo {
public:
	/// The keys are BibTeX fields (e.g., author, title, etc),
	/// and the values are the associated field values.
	typedef std::map<docstring, docstring>::const_iterator const_iterator;
	///
	typedef std::vector<BibTeXInfo const *> const BibTeXInfoList;
	///
	BibTeXInfo() : is_bibtex_(true), num_bib_key_(0), modifier_(0) {}
	/// argument sets isBibTeX_, so should be false only if it's coming
	/// from a bibliography environment
	BibTeXInfo(bool ib) : is_bibtex_(ib), num_bib_key_(0), modifier_(0) {}
	/// constructor that sets the entryType
	BibTeXInfo(docstring const & key, docstring const & type);
	/// \return an author or editor list (short form by default),
	/// used for sorting.
	/// This will be translated to the UI language if buf is null
	/// otherwise, it will be translated to the buffer language.
	docstring const getAuthorOrEditorList(Buffer const * buf = nullptr,
					      size_t const max_key_size = 128,
					      bool amp = false, bool full = false, bool forceshort = false) const;
	/// Same for a specific author role (editor, author etc.)
	docstring const getAuthorList(Buffer const * buf, docstring const & author, size_t const max_key_size,
				      bool const amp = false, bool const full = false, bool const forceshort = false,
				      bool const allnames = false, bool const beginning = true) const;
	///
	docstring const getYear() const;
	///
	void getLocators(docstring & doi, docstring & url, docstring & file) const;
	/// \return formatted BibTeX data suitable for framing.
	/// \param vector of pointers to crossref/xdata information
	docstring const & getInfo(BibTeXInfoList const & xrefs,
				  Buffer const & buf, CiteItem const & ci,
				  docstring const & format = docstring(),
				  bool const for_xhtml = false) const;
	/// \return formatted BibTeX data for a citation label
	docstring const getLabel(BibTeXInfoList const & xrefs,
		Buffer const & buf, docstring const & format,
		CiteItem const & ci, bool next = false, bool second = false) const;
	///
	const_iterator find(docstring const & f) const { return bimap_.find(f); }
	///
	const_iterator end() const { return bimap_.end(); }
	/// \return value for field f
	/// note that this will create an empty field if it does not exist
	docstring & operator[](docstring const & f)
		{ return bimap_[f]; }
	/// \return value for field f
	/// this one, since it is const, will simply return docstring() if
	/// we don't have the field and will NOT create an empty field
	docstring const & operator[](docstring const & field) const;
	///
	docstring const & operator[](std::string const & field) const;
	///
	docstring const & allData() const { return all_data_; }
	///
	void setAllData(docstring const & d) { all_data_ = d; }
	///
	void label(docstring const & d) { label_= d; }
	///
	void key(docstring const & d) { bib_key_= d; }
	/// Record the number of occurences of the same key
	/// (duplicates are allowed with qualified citation lists)
	void numKey(int const i) { num_bib_key_ = i; }
	///
	docstring const & label() const { return label_; }
	///
	docstring const & key() const { return bib_key_; }
	/// numerical key for citing this entry. currently used only
	/// by XHTML output routines.
	docstring const & citeNumber() const { return cite_number_; }
	///
	void setCiteNumber(docstring const & num) { cite_number_ = num; }
	/// a,b,c, etc, for author-year. currently used only by XHTML
	/// output routines.
	char modifier() const { return modifier_; }
	///
	void setModifier(char c) { modifier_ = c; }
	///
	docstring const & entryType() const { return entry_type_; }
	///
	bool isBibTeX() const { return is_bibtex_; }
private:
	/// like operator[], except, if the field is empty, it will attempt
	/// to get the data from xref BibTeXInfo objects, which would normally
	/// be the one referenced in the crossref or xdata field.
	docstring getValueForKey(std::string const & key, Buffer const & buf,
		CiteItem const & ci, BibTeXInfoList const & xrefs, size_t maxsize = 4096) const;
	/// replace %keys% in a format string with their values
	/// called from getInfo()
	/// format strings may contain:
	///   %key%, which represents a key
	///   {%key%[[format]]}, which prints format if key is non-empty
	/// the latter may optionally contain an `else' clause as well:
	///   {%key%[[if format]][[else format]]}
	/// Material intended only for rich text (HTML) output should be
	/// wrapped in "{!" and "!}". These markers are to be postprocessed
	/// by processRichtext(); this step is left as a separate routine since
	/// expandFormat() is recursive while postprocessing should be done
	/// only once on the final string (just like convertLaTeXCommands).
	/// a simple macro facility is also available. keys that look like
	/// "%!key%" are substituted with their definition.
	/// moreover, keys that look like "%_key%" are treated as translatable
	/// so that things like "pp." and "vol." can be translated.
	docstring expandFormat(docstring const & fmt,
		BibTeXInfoList const & xrefs, int & counter,
		Buffer const & buf, CiteItem const & ci,
		bool next = false, bool second = false) const;
	/// true if from BibTeX; false if from bibliography environment
	bool is_bibtex_;
	/// the BibTeX key for this entry
	docstring bib_key_;
	/// Number of occurences of the same key
	int num_bib_key_;
	/// the label that will appear in citations
	/// this is easily set from bibliography environments, but has
	/// to be calculated for entries we get from BibTeX
	docstring label_;
	/// a single string containing all BibTeX data associated with this key
	docstring all_data_;
	/// the BibTeX entry type (article, book, incollection, ...)
	docstring entry_type_;
	/// a cache for getInfo()
	mutable docstring info_;
	/// a cache for getInfo(richtext = true)
	mutable docstring info_richtext_;
	/// cache for last format pattern
	mutable docstring format_;
	///
	docstring cite_number_;
	///
	char modifier_;
	/// our map: <field, value>
	std::map <docstring, docstring> bimap_;
};


/// Class to represent a collection of bibliographical data, whether
/// from BibTeX or from bibliography environments.
class BiblioInfo {
public:
	///
	typedef std::vector<BibTeXInfo const *> BibTeXInfoList;
	/// bibliography key --> data for that key
	typedef std::map<docstring, BibTeXInfo>::const_iterator const_iterator;
	/// Get a vector with all external data (crossref, xdata)
	std::vector<docstring> const getXRefs(BibTeXInfo const & data,
					      bool const nested = false) const;
	/// \return a sorted vector of bibliography keys
	std::vector<docstring> const getKeys() const;
	/// \return a sorted vector of present BibTeX fields
	std::vector<docstring> const getFields() const;
	/// \return a sorted vector of BibTeX entry types in use
	std::vector<docstring> const getEntries() const;
	/// \return author or editor list (abbreviated form by default)
	docstring const getAuthorOrEditorList(docstring const & key, Buffer const & buf,
					      size_t const max_key_size) const;
	/// \return the year from the bibtex data record for \param key
	/// if \param use_modifier is true, then we will also append any
	/// modifier for this entry (e.g., 1998b).
	/// If no legacy year field is present, check for date (common in
	/// biblatex) and extract the year from there.
	/// Note further that this will get the year from the crossref or xdata
	/// if it's not present in the record itself.
	docstring const getYear(docstring const & key,
			bool use_modifier = false) const;
	/// \return the year from the bibtex data record for \param key
	/// if \param use_modifier is true, then we will also append any
	/// modifier for this entry (e.g., 1998b).
	/// If no legacy year field is present, check for date (common in
	/// biblatex) and extract the year from there.
	/// Note further that this will get the year from the crossref or xdata
	/// if it's not present in the record itself.
	/// If no year is found, \return "No year" translated to the buffer
	/// language.
	docstring const getYear(docstring const & key, Buffer const & buf,
			bool use_modifier = false) const;
	/// get either local pdf or web location of the citation referenced by key.
	/// DOI/file are prefixed so they form proper URL for generic qt handler
	void getLocators(docstring const & key, docstring & doi, docstring & url, docstring & file) const;
	///
	docstring const getCiteNumber(docstring const & key) const;
	/// \return formatted BibTeX data associated with a given key.
	/// Empty if no info exists.
	/// Note that this will retrieve data from the crossref or xdata as needed.
	/// \param ci contains further context information, such as if it should
	/// output any richtext tags marked in the citation format and escape < and >
	/// elsewhere, and the general output context.
	docstring const getInfo(docstring const & key, Buffer const & buf,
				CiteItem const & ci, docstring const & format = docstring(),
				bool const for_xhtml = false) const;
	/// \return formatted BibTeX data for citation labels.
	/// Citation labels can have more than one key.
	docstring const getLabel(std::vector<docstring> const & keys, Buffer const & buf,
				 std::string const & style, CiteItem const & ci) const;
	/// Is this a reference from a bibtex database
	/// or from a bibliography environment?
	bool isBibtex(docstring const & key) const;
	/// A vector holding a pair of lyx cite command and the respective
	/// output for a given (list of) key(s).
	typedef std::vector<std::pair<docstring,docstring>> CiteStringMap;
	/// Translates the available citation styles into strings for a given
	/// list of keys, using either numerical or author-year style depending
	/// upon the active engine. The function returns a CiteStringMap with the first
	/// element being the lyx cite command, the second being the formatted
	/// citation reference.
	CiteStringMap const getCiteStrings(
		std::vector<docstring> const & keys,
		std::vector<CitationStyle> const & styles, Buffer const & buf,
		CiteItem const & ci) const;
	/// A list of BibTeX keys cited in the current document, sorted by
	/// the last name of the author.
	/// Make sure you have called collectCitedEntries() before you try to
	/// use this. You should probably call it just before you use this.
	std::vector<docstring> const & citedEntries() const
		{ return cited_entries_; }
	///
	void makeCitationLabels(Buffer const & buf);
	///
	const_iterator begin() const { return bimap_.begin(); }
	///
	void clear() { bimap_.clear(); }
	///
	bool empty() const { return bimap_.empty(); }
	///
	const_iterator end() const { return bimap_.end(); }
	///
	const_iterator find(docstring const & f) const { return bimap_.find(f); }
	///
	void mergeBiblioInfo(BiblioInfo const & info);
	///
	BibTeXInfo & operator[](docstring const & f) { return bimap_[f]; }
	///
	void addFieldName(docstring const & f) { field_names_.insert(f); }
	///
	void addEntryType(docstring const & f) { entry_types_.insert(f); }
private:
	/// Collects the cited entries from buf.
	void collectCitedEntries(Buffer const & buf);
	///
	std::set<docstring> field_names_;
	///
	std::set<docstring> entry_types_;
	/// our map: keys --> BibTeXInfo
	std::map<docstring, BibTeXInfo> bimap_;
	/// a possibly sorted list of entries cited in our Buffer.
	/// do not try to make this a vector<BibTeXInfo *> or anything of
	/// the sort, because reloads will invalidate those pointers.
	std::vector<docstring> cited_entries_;
};

} // namespace lyx

#endif // BIBLIOINFO_H
