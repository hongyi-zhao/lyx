/**
 * \file Undo.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Asger Alstrup
 * \author Lars Gullik Bjønnes
 * \author John Levon
 * \author André Pönitz
 * \author Jürgen Vigna
 * \author Abdelrazak Younes
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "Undo.h"

#include "Buffer.h"
#include "BufferList.h"
#include "BufferParams.h"
#include "Cursor.h"
#include "CutAndPaste.h"
#include "Paragraph.h"
#include "ParagraphList.h"
#include "Text.h"

#include "mathed/InsetMath.h"
#include "mathed/MathData.h"

#include "insets/InsetText.h"

#include "support/debug.h"
#include "support/lassert.h"
#include "support/lyxtime.h"

#include <algorithm>
#include <deque>
#include <set>

using namespace std;
using namespace lyx::support;


namespace lyx {

/**
These are the elements put on the undo stack. Each object contains
complete paragraphs from some cell and sufficient information to
restore the cursor state.

The cell is given by a DocIterator pointing to this cell, the
'interesting' range of paragraphs by counting them from begin and end
of cell, respectively.

The cursor is also given as DocIterator and should point to some place
in the stored paragraph range. In case of math, we simply store the
whole cell, as there usually is just a simple paragraph in a cell.

The idea is to store the contents of 'interesting' paragraphs in some
structure ('Undo') _before_ it is changed in some edit operation.
Obviously, the stored range should be as small as possible. However,
there is a lower limit: The StableDocIterator stored in the undo class
must be valid after the changes, too, as it will used as a pointer
where to insert the stored bits when performining undo.
*/
struct UndoElement
{
	///
	UndoElement(UndoKind kin, CursorData const & cb,
	            StableDocIterator const & cel,
	            pit_type fro, pit_type en, ParagraphList * pl, MathData * md,
	            bool lc, size_t gid) :
		cur_before(cb), cell(cel), from(fro), end(en),
		pars(pl), array(md), bparams(nullptr),
		group_id(gid), time(current_time()), kind(kin), lyx_clean(lc)
		{}
	///
	UndoElement(CursorData const & cb, BufferParams const & bp,
				bool lc, size_t gid) :
		cur_before(cb), cell(), from(0), end(0),
		pars(nullptr), array(nullptr), bparams(new BufferParams(bp)),
		group_id(gid), time(current_time()), kind(ATOMIC_UNDO), lyx_clean(lc)
	{}
	///
	UndoElement(UndoElement const & ue) :
		cur_before(ue.cur_before), cur_after(ue.cur_after),
		cell(ue.cell), from(ue.from), end(ue.end),
		pars(ue.pars), array(ue.array),
		bparams(ue.bparams ? new BufferParams(*ue.bparams) : nullptr),
		group_id(ue.group_id), time(current_time()), kind(ue.kind),
		lyx_clean(ue.lyx_clean)
		{}
	///
	~UndoElement()
	{
		if (bparams)
			delete bparams;
	}
	/// the position of the cursor before recordUndo
	CursorData cur_before;
	/// the position of the cursor at the end of the undo group
	CursorData cur_after;
	/// the position of the cell described
	StableDocIterator cell;
	/// counted from begin of cell
	pit_type from;
	/// complement to end of this cell
	pit_type end;
	/// the contents of the saved Paragraphs (for texted)
	ParagraphList * pars;
	/// the contents of the saved MathData (for mathed)
	MathData * array;
	/// Only used in case of params undo
	BufferParams const * bparams;
	/// the element's group id
	size_t group_id;
	/// timestamp
	time_t time;
	/// Which kind of operation are we recording for?
	UndoKind kind;
	/// Was the buffer clean at this point?
	bool lyx_clean;
private:
	/// Protect construction
	UndoElement();
};


class UndoElementStack
{
public:
	/// limit is the maximum size of the stack
	UndoElementStack(size_t limit = 100) { limit_ = limit; }
	/// limit is the maximum size of the stack
	~UndoElementStack() { clear(); }

	/// Return the top element.
	UndoElement & top() { return c_.front(); }

	/// Pop and throw away the top element.
	void pop() { c_.pop_front(); }

	/// Return true if the stack is empty.
	bool empty() const { return c_.empty(); }

	/// Clear all elements, deleting them.
	void clear() {
		for (size_t i = 0; i != c_.size(); ++i) {
			delete c_[i].array;
			delete c_[i].pars;
		}
		c_.clear();
	}

	/// Push an item on to the stack, deleting the bottom group on
	/// overflow.
	void push(UndoElement const & v) {
		// Remove some entries if the limit has been reached.
		// However, if the only group on the stack is the one
		// we are currently populating, do nothing.
		if (c_.size() >= limit_
		    && c_.front().group_id != v.group_id) {
			// remove a whole group at once.
			const size_t gid = c_.back().group_id;
			while (!c_.empty() && c_.back().group_id == gid)
				c_.pop_back();
		}
		c_.push_front(v);
	}

	/// Mark all the elements of the stack as dirty
	void markDirty() {
		for (size_t i = 0; i != c_.size(); ++i)
			c_[i].lyx_clean = false;
	}

private:
	/// Internal contents.
	std::deque<UndoElement> c_;
	/// The maximum number elements stored.
	size_t limit_;
};


struct Undo::Private
{
	Private(Buffer & buffer) : buffer_(buffer),
		group_id_(0), group_level_(0), undo_finished_(true) {}

	// Do one undo/redo step
	void doUndoRedoAction(CursorData & cur, UndoElementStack & stack,
			      UndoElementStack & otherStack);
	// Apply one undo/redo group. Returns false if no undo possible.
	bool undoRedoAction(CursorData & cur, bool isUndoOperation);

	///
	void doRecordUndo(UndoKind kind,
		DocIterator const & cell,
		pit_type first_pit,
		pit_type last_pit,
		CursorData const & cur,
		UndoElementStack & stack);
	///
	void recordUndo(UndoKind kind,
		DocIterator const & cell,
		pit_type first_pit,
		pit_type last_pit,
		CursorData const & cur);
	///
	void doRecordUndoBufferParams(CursorData const & cur, UndoElementStack & stack);
	///
	void recordUndoBufferParams(CursorData const & cur);

	///
	Buffer & buffer_;
	/// Undo stack.
	UndoElementStack undostack_;
	/// Redo stack.
	UndoElementStack redostack_;

	/// Current group Id.
	size_t group_id_;
	/// Current group nesting nevel.
	size_t group_level_;
	/// the position of cursor before the group was created
	CursorData group_cur_before_;

	/// The flag used by Undo::finishUndo().
	bool undo_finished_;

};


/////////////////////////////////////////////////////////////////////
//
// Undo
//
/////////////////////////////////////////////////////////////////////


Undo::Undo(Buffer & buffer)
	: d(new Undo::Private(buffer))
{}


Undo::~Undo()
{
	delete d;
}


void Undo::clear()
{
	d->undostack_.clear();
	d->redostack_.clear();
	d->undo_finished_ = true;
	// We used to do that, but I believe it is better to keep
	// groups (only used in Buffer::reload for now (JMarc)
	//d->group_id_ = 0;
	//d->group_level_ = 0;
}


bool Undo::hasUndoStack() const
{
	return !d->undostack_.empty();
}


bool Undo::hasRedoStack() const
{
	return !d->redostack_.empty();
}


void Undo::markDirty()
{
	d->undo_finished_ = true;
	d->undostack_.markDirty();
	d->redostack_.markDirty();
}


/////////////////////////////////////////////////////////////////////
//
// Undo::Private
//
///////////////////////////////////////////////////////////////////////

static bool samePar(StableDocIterator const & i1, StableDocIterator const & i2)
{
	StableDocIterator tmpi2 = i2;
	tmpi2.pos() = i1.pos();
	return i1 == tmpi2;
}


void Undo::Private::doRecordUndo(UndoKind kind,
	DocIterator const & cell,
	pit_type first_pit, pit_type last_pit,
	CursorData const & cur_before,
	UndoElementStack & stack)
{
	if (!group_level_) {
		LATTEST(false);
		LYXERR0("There is no group open (creating one)");
		++group_id_;
	}

	if (first_pit > last_pit)
		swap(first_pit, last_pit);
	pit_type const from = first_pit;
	pit_type const end = cell.lastpit() - last_pit;

	/* Undo coalescing: if the undo element we want to add only
	 * changes stuff that was already modified by the previous one on
	 * undo stack (in the same group), then skip it. There is nothing
	 * to gain in adding it to the stack. The code below works for
	 * both texted and mathed.
	*/
	if (!stack.empty()
	    && stack.top().group_id == group_id_
	    && !stack.top().bparams
	    && samePar(stack.top().cell, cell)
	    //&& stack.top().kind == kind // needed?
	    && stack.top().from <= from
	    && stack.top().end >= end) {
		LYXERR(Debug::UNDO, "Undo coalescing: skip entry");
		return;
	}

	// Undo::ATOMIC are always recorded (no overlapping there).
	// As nobody wants all removed character appear one by one when undoing,
	// we want combine 'similar' non-ATOMIC undo recordings to one.
	if (!undo_finished_
	    && kind != ATOMIC_UNDO
	    && !stack.empty()
	    && !stack.top().bparams
	    && samePar(stack.top().cell, cell)
	    && stack.top().kind == kind
	    && stack.top().from == from
	    && stack.top().end == end
	    && stack.top().cur_after == cur_before
	    && current_time() - stack.top().time <= 2) {
		// reset cur_after; it will be filled correctly by endUndoGroup.
		stack.top().cur_after = CursorData();
		// update the timestamp of the undo element
		stack.top().time = current_time();
		return;
	}

	LYXERR(Debug::UNDO, "Create undo element of group " << group_id_);
	// create the position information of the Undo entry
	UndoElement undo(kind,
	      group_cur_before_.empty() ? cur_before : group_cur_before_,
	      cell, from, end, nullptr, nullptr, buffer_.isClean(), group_id_);

	// fill in the real data to be saved
	if (cell.inMathed()) {
		// simply use the whole cell
		MathData & md = cell.cell();
		undo.array = new MathData(md.buffer(), md.begin(), md.end());
	} else {
		// some more effort needed here as 'the whole cell' of the
		// main Text _is_ the whole document.
		// record the relevant paragraphs
		Text const * text = cell.text();
		LBUFERR(text);
		ParagraphList const & plist = text->paragraphs();
		ParagraphList::const_iterator first = plist.begin();
		advance(first, first_pit);
		ParagraphList::const_iterator last = plist.begin();
		advance(last, last_pit + 1);
		undo.pars = new ParagraphList(first, last);
	}

	// push the undo entry to undo stack
	stack.push(undo);
	//lyxerr << "undo record: " << stack.top() << endl;
}


void Undo::Private::recordUndo(UndoKind kind,
			       DocIterator const & cell,
			       pit_type first_pit, pit_type last_pit,
			       CursorData const & cur)
{
	LASSERT(first_pit <= cell.lastpit(), return);
	LASSERT(last_pit <= cell.lastpit(), return);

	if (buffer_.isReadonly())
		return;

	doRecordUndo(kind, cell, first_pit, last_pit, cur,
		undostack_);

	// next time we'll try again to combine entries if possible
	undo_finished_ = false;

	// If we ran recordUndo, it means that we plan to change the buffer
	buffer_.markDirty();

	redostack_.clear();
}


void Undo::Private::doRecordUndoBufferParams(CursorData const & cur_before,
                                             UndoElementStack & stack)
{
	if (!group_level_) {
		LYXERR0("There is no group open (creating one)");
		++group_id_;
	}

	LYXERR(Debug::UNDO, "Create full buffer undo element of group " << group_id_);
	// create the position information of the Undo entry
	UndoElement undo(group_cur_before_.empty() ? cur_before : group_cur_before_,
                     buffer_.params(), buffer_.isClean(),
					 group_id_);

	// push the undo entry to undo stack
	stack.push(undo);
}


void Undo::Private::recordUndoBufferParams(CursorData const & cur)
{
	if (buffer_.isReadonly())
		return;

	doRecordUndoBufferParams(cur, undostack_);

	// next time we'll try again to combine entries if possible
	undo_finished_ = false;

	// If we ran recordUndo, it means that we plan to change the buffer
	buffer_.markDirty();

	redostack_.clear();
}


void Undo::Private::doUndoRedoAction(CursorData & cur, UndoElementStack & stack, UndoElementStack & otherstack)
{
	// Adjust undo stack and get hold of current undo data.
	UndoElement & undo = stack.top();
	LYXERR(Debug::UNDO, "Undo element of group " << undo.group_id);
	// We'll pop the stack only when we're done with this element. So do NOT
	// try to return early.

	// We will store in otherstack the part of the document under 'undo'
	DocIterator cell_dit = undo.cell.asDocIterator(&buffer_);

	if (undo.bparams)
		doRecordUndoBufferParams(undo.cur_after, otherstack);
	else {
		LATTEST(undo.end <= cell_dit.lastpit());
		doRecordUndo(ATOMIC_UNDO, cell_dit,
					 undo.from, cell_dit.lastpit() - undo.end, undo.cur_after,
					 otherstack);
	}
	otherstack.top().cur_after = undo.cur_before;

	// This does the actual undo/redo.
	//LYXERR0("undo, performing: " << undo);
	DocIterator dit = undo.cell.asDocIterator(&buffer_);
	if (undo.bparams) {
		// This is a params undo element
		delete otherstack.top().bparams;
		otherstack.top().bparams = new BufferParams(buffer_.params());
		DocumentClassConstPtr olddc = buffer_.params().documentClassPtr();
		buffer_.params() = *undo.bparams;
		cap::switchBetweenClasses(olddc, buffer_.params().documentClassPtr(),
			static_cast<InsetText &>(buffer_.inset()));
	} else if (dit.inMathed()) {
		// We stored the full cell here as there is not much to be
		// gained by storing just 'a few' paragraphs (most if not
		// all math inset cells have just one paragraph!)
		//LYXERR0("undo.array: " << *undo.array);
		LBUFERR(undo.array);
		dit.cell().swap(*undo.array);
		dit.inset().setBuffer(buffer_);
		delete undo.array;
		undo.array = nullptr;
	} else {
		// Some finer machinery is needed here.
		Text * text = dit.text();
		LBUFERR(text);
		LBUFERR(undo.pars);
		ParagraphList & plist = text->paragraphs();

		// remove new stuff between first and last
		ParagraphList::iterator first = plist.begin();
		advance(first, undo.from);
		ParagraphList::iterator last = plist.begin();
		advance(last, plist.size() - undo.end);
		plist.erase(first, last);

		// re-insert old stuff instead
		first = plist.begin();
		advance(first, undo.from);

		// this ugly stuff is needed until we get rid of the
		// inset_owner backpointer
		ParagraphList::iterator pit = undo.pars->begin();
		ParagraphList::iterator const end = undo.pars->end();
		for (; pit != end; ++pit)
			pit->setInsetOwner(dit.realInset());
		plist.insert(first, undo.pars->begin(), undo.pars->end());

		// set the buffers for insets we created
		ParagraphList::iterator fpit = plist.begin();
		advance(fpit, undo.from);
		ParagraphList::iterator fend = fpit;
		advance(fend, undo.pars->size());
		for (; fpit != fend; ++fpit)
			fpit->setInsetBuffers(buffer_);

		delete undo.pars;
		undo.pars = nullptr;
	}

	// We'll clean up in release mode.
	LASSERT(undo.pars == nullptr, undo.pars = nullptr);
	LASSERT(undo.array == nullptr, undo.array = nullptr);

	if (!undo.cur_before.empty())
		cur = undo.cur_before;
	if (undo.lyx_clean) {
		buffer_.markClean();
		// since we have changed the buffer, update its id.
		buffer_.updateId();
	} else
		buffer_.markDirty();
	// Now that we're done with undo, we pop it off the stack.
	stack.pop();
}


bool Undo::Private::undoRedoAction(CursorData & cur, bool isUndoOperation)
{
	if (buffer_.isReadonly())
		return false;

	undo_finished_ = true;

	UndoElementStack & stack = isUndoOperation ?  undostack_ : redostack_;

	if (stack.empty())
		// Nothing to do.
		return false;

	UndoElementStack & otherstack = isUndoOperation ? redostack_ : undostack_;

	const size_t gid = stack.top().group_id;
	while (!stack.empty() && stack.top().group_id == gid)
		doUndoRedoAction(cur, stack, otherstack);
	return true;
}


void Undo::finishUndo()
{
	// Make sure the next operation will be stored.
	d->undo_finished_ = true;
}


bool Undo::undoAction(CursorData & cur)
{
	return d->undoRedoAction(cur, true);
}


bool Undo::redoAction(CursorData & cur)
{
	return d->undoRedoAction(cur, false);
}


void Undo::beginUndoGroup()
{
	if (d->group_level_ == 0) {
		// create a new group
		++d->group_id_;
		LYXERR(Debug::UNDO, "+++++++ Creating new group " << d->group_id_
		       << " for buffer " << &d->buffer_);
	}
	++d->group_level_;
}


void Undo::beginUndoGroup(CursorData const & cur_before)
{
	beginUndoGroup();
	if (d->group_cur_before_.empty())
		d->group_cur_before_ = cur_before;
}


void Undo::endUndoGroup()
{
	if (d->group_level_ == 0) {
		LYXERR0("There is no undo group to end here");
		return;
	}
	--d->group_level_;
	if (d->group_level_ == 0) {
		// real end of the group
		d->group_cur_before_ = CursorData();
		LYXERR(Debug::UNDO, "------- End of group " << d->group_id_
		       << " of buffer " << &d->buffer_);
	}
}


void Undo::endUndoGroup(CursorData const & cur_after)
{
	endUndoGroup();
	if (!d->undostack_.empty() && d->undostack_.top().cur_after.empty())
		d->undostack_.top().cur_after = cur_after;
}


void Undo::splitUndoGroup(CursorData const & cur)
{
	size_t const level = d->group_level_;
	d->group_level_ = 1;
	endUndoGroup(cur);
	beginUndoGroup(cur);
	d->group_level_ = level;
}


bool Undo::activeUndoGroup() const
{
	return d->group_level_ > 0
		&& !d->undostack_.empty()
		&& d->undostack_.top().group_id == d->group_id_;
}


void Undo::recordUndo(CursorData const & cur, UndoKind kind)
{
	d->recordUndo(kind, cur, cur.pit(), cur.pit(), cur);
}


void Undo::recordUndo(CursorData const & cur, pit_type from, pit_type to)
{
	d->recordUndo(ATOMIC_UNDO, cur, from, to, cur);
}


void Undo::recordUndoInset(CursorData const & cur, Inset const * inset)
{
	if (!inset || inset == &cur.inset()) {
		DocIterator c = cur;
		c.pop_back();
		d->recordUndo(ATOMIC_UNDO, c, c.pit(), c.pit(), cur);
	} else if (inset == cur.nextInset())
		recordUndo(cur);
	else
		LYXERR0("Inset not found, no undo stack added.");
}


void Undo::recordUndoBufferParams(CursorData const & cur)
{
	d->recordUndoBufferParams(cur);
}


void Undo::recordUndoBufferParams()
{
	d->recordUndoBufferParams(CursorData());
}


void Undo::recordUndoFullBuffer()
{
	// This one may happen outside of the main undo group, so we
	// put it in its own subgroup to avoid complaints.
	beginUndoGroup();
	d->recordUndo(ATOMIC_UNDO, doc_iterator_begin(&d->buffer_),
	              0, d->buffer_.paragraphs().size() - 1, CursorData());
	d->recordUndoBufferParams(CursorData());
	endUndoGroup();
}


/// UndoGroupHelper class stuff

class UndoGroupHelper::Impl {
	friend class UndoGroupHelper;
	set<Buffer *> buffers_;
};


UndoGroupHelper::UndoGroupHelper(Buffer * buf) : d(new UndoGroupHelper::Impl)
{
	resetBuffer(buf);
}


UndoGroupHelper::UndoGroupHelper(CursorData & cur) : d(new UndoGroupHelper::Impl)
{
	resetBuffer(cur);
}


UndoGroupHelper::~UndoGroupHelper()
{
	for (Buffer * buf : d->buffers_)
		if (theBufferList().isLoaded(buf) || theBufferList().isInternal(buf))
			buf->undo().endUndoGroup();
	delete d;
}


void UndoGroupHelper::resetBuffer(Buffer * buf)
{
	if (buf && d->buffers_.count(buf) == 0) {
		d->buffers_.insert(buf);
		buf->undo().beginUndoGroup();
	}
}


void UndoGroupHelper::resetBuffer(CursorData & cur)
{
	if (!cur.empty() && d->buffers_.count(cur.buffer()) == 0) {
		d->buffers_.insert(cur.buffer());
		cur.buffer()->undo().beginUndoGroup(cur);
	}
}


} // namespace lyx
