#ifndef FIRUPDATESLOT_H
#define FIRUPDATESLOT_H


/////////////////////////////////////////////////////////////
/// Describes a TimeSlice-Slot where an Update occurs
///   An Update is the slot when a new FIR-data input value gets pushed,
///   and the data-input fifo gets advanced
/////////////////////////////////////////////////////////////

class FirUpdateSlot
{
public:
	FirUpdateSlot();
public:
	bool isSlotEmpty() const			{ return m_FirIndex == -1; }
public:
	/// Which FIR is being updated (-1 if none)
	unsigned			m_FirIndex;
};


#endif
