// -*- C++ -*-
/**
 * \file WorkAreaManager.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Abdelrazak Younes
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "WorkAreaManager.h"

#include "BufferView.h"
#include "Cursor.h"

#include "Application.h"
#include "WorkArea.h"


namespace lyx {
namespace frontend {

void WorkAreaManager::add(WorkArea * wa)
{
	work_areas_.push_back(wa);
}


void WorkAreaManager::remove(WorkArea * wa)
{
	work_areas_.remove(wa);
}


void WorkAreaManager::redrawAll(bool update_metrics)
{
	for (WorkArea * wa : work_areas_)
		wa->scheduleRedraw(update_metrics);
}


void WorkAreaManager::closeAll()
{
	while (!work_areas_.empty())
		// WorkArea is de-registering itself.
		(*work_areas_.begin())->close();
}


bool WorkAreaManager::unhide(Buffer * buf) const
{
	if (!work_areas_.empty())
		return true;
	return theApp()->unhide(buf);
}


void WorkAreaManager::updateTitles()
{
	for (WorkArea * wa : work_areas_)
		wa->updateWindowTitle();
}


void WorkAreaManager::scheduleRedraw()
{
	for (WorkArea * wa : work_areas_)
		wa->scheduleRedraw(true);
}


void WorkAreaManager::sanitizeCursors()
{
	for (WorkArea * wa : work_areas_) {
		wa->bufferView().cursor().sanitize();
		wa->bufferView().resetInlineCompletionPos();
	}
}

void WorkAreaManager::updateCursorsAfterInsertion(DocIterator const & dit, int count)
{
	for (WorkArea * wa : work_areas_) {
		wa->bufferView().cursor().updateAfterInsertion(dit, count);
		wa->bufferView().cursor().realAnchor().updateAfterInsertion(dit, count);
	}
}


} // namespace frontend
} // namespace lyx

