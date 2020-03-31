#ifndef FIRENGINEMACDESC_H
#define FIRENGINEMACDESC_H


#include <vector>
#include "fircoeffref.h"
#include "firupdateslot.h"
#include "firenginemacfifodesc.h"
using namespace std;

class FirEngineSpec;		// forward declaration


/////////////////////////////////////////////////////////////
/// Describes a FirMac block
///   (And how FIR-Coefficients map to TimeSlots)
/////////////////////////////////////////////////////////////

class FirEngineMacDesc
{
public:
	explicit FirEngineMacDesc(unsigned numTimeSlots);
public:
	unsigned getNumTimeSlots() const			{ return m_vFirCoeffRef.size(); }
	unsigned getNumFifos() const				{ return m_vFirEngineMacFifoDesc.size(); }
public:
	/// Lookup which Input corresponds to a particular FIR
	unsigned findInputIndexForFirIndex(unsigned firIndex) const;
	/// Lookup which Fifo is used for a particular FIR
	unsigned findFifoIndexForFirIndex(unsigned firIndex) const;
public:
	/// Each Control is 4 bits	- Selects which Input channel to use in this timeSlot (0 = None) This should only be non-zero during Fir Update-Write cycle\n";
	void establishChannelSelectCtrl(vector<unsigned>* pOut) const;
	/// Each Control is 1 bit	- '1' when updating the first FirEngine in a chain. Valid on DOUPDATE cycle\n";
	void establishFirstEngineCtrl(vector<unsigned>* pOut) const;
	/// Each Control is 1 bit	- '1' when updating the last FirEngine in a chain. Valid on DOUPDATE cycle\n";
	void establishLastEngineCtrl(vector<unsigned>* pOut) const;
	/// Each Control is 1 bit	- '1' for first tap of the section of FIR in this FirEngine\n";
	void establishFirstTapCtrl(vector<unsigned>* pOut) const;
	/// Each Control is 4 bits	- 0=NoPreadder(B*A), 1=PreAdd((D+B)*A), 2=PreSub((D-B)*A)\n";
	void establishPreAddModeCtrl(vector<unsigned>* pOut) const;
	/// Each Control is 4 bits	- 0=MUL, 1=MADD, 2=MSUB\n";
	void establishMulModeCtrl(vector<unsigned>* pOut) const;
	/// Each Control is 1 bit   - Use value of previous Fir Engine's Accumulator from 2-cycles ago\n";
	void establishAddPrevEngineAccumCtrl(vector<unsigned>* pOut) const;
	/// Each Control is 8 bits	- Selects which Data Fifo to use\n";
	void establishRdFifoNumCtrl(vector<unsigned>* pOut) const;
	/// Each Control is 8 bits	- Selects which Data Fifo to update (valid on doUpdate cycle and 3 cycles earlier)\n";
	void establishUpdateFifoNumCtrl(vector<unsigned>* pOut) const;
	/// Each Control is 1 bits	- High at the same time on all FirEngines involved in a firfilter for the last tap of the fir-filter\n";
	void establishDoUpdateCtrl(vector<unsigned>* pOut) const;
public:
	// Sort Fifos in descending size order
	void sortFifosInDescendingSizeOrder();
	// 16 bits Foreach Fifo: 	- [FifoOffset : 10 bits, Len-1: 6 bits]		(sortFifosInDescendingSizeOrder must be called first)
	void establishFifoSizes(vector<unsigned>* pOut) const;
	// 24 bits for every slot in Coeff-Buffer - (1.17 representation)		(sortFifosInDescendingSizeOrder must be called first)
	void establishCoeffValues(vector<unsigned>* pOut, const FirEngineSpec&) const;
public:
	void generateRtl(const string& firEngineMacName, const FirEngineSpec&) const;
public:
	/// list of FIRs needed as Inputs to this MAC (limited to 15)
	vector<unsigned>				m_vInputFirs; 
	/// list of FIRs whose Outputs are computed by this MAC (limited to 15)
	///   (currently: FIRs are not split across MACs so this is the same as the Inputs)
	vector<unsigned>				m_vOutputFirs; 
	/// Reference to which Coefficients map to each TimeSlot
	vector<FirCoeffRef>				m_vFirCoeffRef;
	/// TimeSlots -> Which FIR's data inputs are being updated
	vector<FirUpdateSlot>			m_vFirUpdateSlot;
	/// Description of Fifos required for this MAC (in Address order)
	///  (TODO: must be aligned to size!!)
	vector<FirEngineMacFifoDesc>	m_vFirEngineMacFifoDesc;
};


#endif
