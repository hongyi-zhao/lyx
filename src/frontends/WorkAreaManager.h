// -*- C++ -*-
/**
 * \file WorkAreaManager.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Abdelrazak Younes
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef WORKAREA_MANAGER_H
#define WORKAREA_MANAGER_H

#include <list>

namespace lyx {

class Buffer;
class DocIterator;

namespace frontend {

class WorkArea;

/// \c WorkArea Manager.
/**
  * This is a helper class designed to avoid signal/slot connections
  * between a \c Buffer and the potentially multiple \c WorkArea(s)
  * used to visualize this Buffer contents.
  */
class WorkAreaManager
{
public:
	///
	WorkAreaManager() {}
	///
	void add(WorkArea * wa);
	///
	void remove(WorkArea * wa);
	///
	void redrawAll(bool update_metrics);
	///
	void closeAll();
	/// Update window titles of all users and the external modifications
	/// warning.
	void updateTitles();
	/// Schedule redraw of work areas
	void scheduleRedraw();
	/// If there is no work area, create a new one in the current view using the
	/// buffer buf. Returns false if not possible.
	bool unhide(Buffer * buf) const;
	/// Fix cursors in all buffer views held by work areas.
	void sanitizeCursors();
	/// Update all cursors after insertion (or deletion is count < 0)
	void updateCursorsAfterInsertion(DocIterator const & dit, int count);


private:
	typedef std::list<WorkArea *>::iterator iterator;
	///
	std::list<WorkArea *> work_areas_;
};

} // namespace frontend
} // namespace lyx

#endif // BASE_WORKAREA_H
