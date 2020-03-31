#ifndef FIRENGINEDESC_H
#define FIRENGINEDESC_H


#include "firbinding.h"
#include "firenginespec.h"
#include "firenginemacdesc.h"


/////////////////////////////////////////////////////////////////////////
/// Describes a FirEngine block (which will implement a FirEngineSpec)
/////////////////////////////////////////////////////////////////////////

class FirEngineDesc
{
public:
	explicit FirEngineDesc(unsigned numTimeSlots);
public:
	/// Bind all FIRs to the firEngineDesc (creating new MACs as needed)
	void bindFir(const FirEngineSpec&, unsigned firIdx);
private:
	FirBinding findValidBinding(const FirSpec&, unsigned firIndex, unsigned timeSliceInterval) const;
	bool canBind(const FirSpec&, const FirBinding&) const;
	void bind(const FirSpec&, const FirBinding&);
public:
	void generateRtl(const string& firEngineName, const FirEngineSpec&) const;
public:
	void generateHtmlReport(ostream&) const;
public:
	/// Number of ClockCycles between Fir-Read and Fir-Write in an Update-Cycle
	///   (Note: a FirUpdate occurs on a Fir-write timeslot and no FirUpdates can occur on a Fir-Read timeslot)
	const unsigned				m_FirUpdateLatency;
	/// This FirEngine will be divided into a number of timeslots of the global clock
	const unsigned				m_NumTimeSlots;
public:
	/// Keep track of the number of FIRs mapped to this FirEngine
	unsigned					m_NumFirs;
	/// Describe each MAC-block's assignments
	vector<FirEngineMacDesc>	m_vFirEngineMacDesc;
};


#endif
