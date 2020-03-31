
#include <fstream>
#include <assert.h>
#include <algorithm>
#include "intutils.h"
#include "datetime.h"
#include "firenginemacdesc.h"
#include "firenginespec.h"


FirEngineMacDesc::FirEngineMacDesc(unsigned numTimeSlots) :
	m_vInputFirs			(),
	m_vOutputFirs			(),
	m_vFirCoeffRef			(numTimeSlots),
	m_vFirUpdateSlot		(numTimeSlots),
	m_vFirEngineMacFifoDesc	()
{
}

unsigned FirEngineMacDesc::findInputIndexForFirIndex(unsigned firIndex) const
{
	for (unsigned i = 0; i < m_vInputFirs.size(); ++i)
	{
		if (m_vInputFirs[i] == firIndex)
			return i;
	}
	assert(false);		// Input not found!
	return 0;
}

unsigned FirEngineMacDesc::findFifoIndexForFirIndex(unsigned firIndex) const
{
	for (unsigned i = 0; i < m_vFirEngineMacFifoDesc.size(); ++i)
	{
		if (m_vFirEngineMacFifoDesc[i].m_FirIndex == firIndex)
			return i;
	}
	assert(false);		// Fifo not found!
	return 0;
}

//////////////////////////////////////////////////////////////////


/// Each Control is 4 bits	- Selects which Input channel to use in this timeSlot (0 = None) This should only be non-zero during Fir Update-Write cycle\n";
void FirEngineMacDesc::establishChannelSelectCtrl(vector<unsigned>* pvValues) const
{
	pvValues->clear();
	pvValues->resize(getNumTimeSlots(), 0xF);

	for (unsigned i = 0; i < m_vFirUpdateSlot.size(); ++i)
	{
		const FirUpdateSlot& firUpdateSlot = m_vFirUpdateSlot[i];
		if (!firUpdateSlot.isSlotEmpty())
		{
			(*pvValues)[i] = findInputIndexForFirIndex(firUpdateSlot.m_FirIndex);
		}
	}
}

/// Each Control is 1 bit	- '1' when updating the first FirEngine in a chain. Valid on DOUPDATE cycle\n";
void FirEngineMacDesc::establishFirstEngineCtrl(vector<unsigned>* pvValues) const
{
	pvValues->clear();
	pvValues->resize(getNumTimeSlots(), 0);

	for (unsigned i = 0; i < m_vFirUpdateSlot.size(); ++i)
	{
		const FirUpdateSlot& firUpdateSlot = m_vFirUpdateSlot[i];
		if (!firUpdateSlot.isSlotEmpty())
		{
			(*pvValues)[i] = 1;
		}
	}
}

/// Each Control is 1 bit	- '1' when updating the last FirEngine in a chain. Valid on DOUPDATE cycle\n";
void FirEngineMacDesc::establishLastEngineCtrl(vector<unsigned>* pvValues) const
{
	pvValues->clear();
	pvValues->resize(getNumTimeSlots(), 0);

	for (unsigned i = 0; i < m_vFirUpdateSlot.size(); ++i)
	{
		const FirUpdateSlot& firUpdateSlot = m_vFirUpdateSlot[i];
		if (!firUpdateSlot.isSlotEmpty())
		{
			(*pvValues)[i] = 1;
		}
	}
}

/// Each Control is 1 bit	- '1' for first tap of the section of FIR in this FirEngine\n";
void FirEngineMacDesc::establishFirstTapCtrl(vector<unsigned>* pvValues) const
{
	pvValues->clear();
	pvValues->resize(getNumTimeSlots(), 0);

	for (unsigned i = 0; i < m_vFirCoeffRef.size(); ++i)
	{
		const FirCoeffRef& firCoeffRef = m_vFirCoeffRef[i];
		if (!firCoeffRef.isNull() && (firCoeffRef.m_CoeffIndex == 0))
		{
			(*pvValues)[i] = 1;
		}
	}
}

/// Each Control is 4 bits	- 0=NoPreadder(B*A), 1=PreAdd((D+B)*A), 2=PreSub((D-B)*A)\n";
void FirEngineMacDesc::establishPreAddModeCtrl(vector<unsigned>* pvValues) const
{
	pvValues->clear();
	pvValues->resize(getNumTimeSlots(), 0);
}

/// Each Control is 4 bits	- 0=MUL, 1=MADD, 2=MSUB\n";
void FirEngineMacDesc::establishMulModeCtrl(vector<unsigned>* pvValues) const
{
	pvValues->clear();
	pvValues->resize(getNumTimeSlots(), 1);

	for (unsigned i = 0; i < m_vFirCoeffRef.size(); ++i)
	{
		const FirCoeffRef& firCoeffRef = m_vFirCoeffRef[i];
		if (!firCoeffRef.isNull() && (firCoeffRef.m_CoeffIndex == 0))
		{
			(*pvValues)[i] = 0;
		}
	}
}

/// Each Control is 1 bit   - Use value of previous Fir Engine's Accumulator from 2-cycles ago\n";
void FirEngineMacDesc::establishAddPrevEngineAccumCtrl(vector<unsigned>* pvValues) const
{
	pvValues->clear();
	pvValues->resize(getNumTimeSlots(), 0);
}

/// Each Control is 8 bits	- Selects which Data Fifo to use\n";
void FirEngineMacDesc::establishRdFifoNumCtrl(vector<unsigned>* pvValues) const
{
	pvValues->clear();
	pvValues->resize(getNumTimeSlots(), 0);

	for (unsigned i = 0; i < m_vFirCoeffRef.size(); ++i)
	{
		const FirCoeffRef& firCoeffRef = m_vFirCoeffRef[i];
		if (!firCoeffRef.isNull())
		{
			(*pvValues)[i] = findFifoIndexForFirIndex(firCoeffRef.m_FirIndex);
		}
	}
}

/// Each Control is 8 bits	- Selects which Data Fifo to update (valid on doUpdate cycle and 3 cycles earlier)\n";
void FirEngineMacDesc::establishUpdateFifoNumCtrl(vector<unsigned>* pvValues) const
{
	pvValues->clear();
	pvValues->resize(getNumTimeSlots(), 0);

	for (unsigned i = 0; i < m_vFirUpdateSlot.size(); ++i)
	{
		const FirUpdateSlot& firUpdateSlot = m_vFirUpdateSlot[i];
		if (!firUpdateSlot.isSlotEmpty())
		{
			(*pvValues)[i] = findFifoIndexForFirIndex(firUpdateSlot.m_FirIndex);
		}
	}
}

/// Each Control is 1 bits	- High at the same time on all FirEngines involved in a firfilter for the last tap of the fir-filter\n";
void FirEngineMacDesc::establishDoUpdateCtrl(vector<unsigned>* pvValues) const
{
	pvValues->clear();
	pvValues->resize(getNumTimeSlots(), 0);

	for (unsigned i = 0; i < m_vFirUpdateSlot.size(); ++i)
	{
		const FirUpdateSlot& firUpdateSlot = m_vFirUpdateSlot[i];
		if (!firUpdateSlot.isSlotEmpty())
		{
			(*pvValues)[i] = 1;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////


void FirEngineMacDesc::sortFifosInDescendingSizeOrder()
{
	std::sort(m_vFirEngineMacFifoDesc.begin(), m_vFirEngineMacFifoDesc.end(), FirEngineMacFifoDesc::memWordsLessThan);
	std::reverse(m_vFirEngineMacFifoDesc.begin(), m_vFirEngineMacFifoDesc.end());
}


// 16 bits Foreach Fifo: 	- [FifoOffset : 10 bits, Len-1: 6 bits]
void FirEngineMacDesc::establishFifoSizes(vector<unsigned>* pvValues) const
{
	pvValues->clear();
	pvValues->resize(getNumFifos(), 0);

	unsigned offset = 0;
	for (unsigned i = 0; i < m_vFirEngineMacFifoDesc.size(); ++i)
	{
		// Align Fifos to their 2^N Size
		offset = IntUtils::alignAddressOnOrAfter(offset, m_vFirEngineMacFifoDesc[i].m_NumFifoMemWords);

		unsigned fifoDepth = m_vFirEngineMacFifoDesc[i].m_FifoDepth;
		assert(fifoDepth <= (1 << 6));		// check range
		(*pvValues)[i] = (offset << 6) | (fifoDepth - 1);

		// advance offset to end of this FIFO
		offset += m_vFirEngineMacFifoDesc[i].m_NumFifoMemWords;	
	}
}

// 24 bits for every slot in Coeff-Buffer - (1.17 representation)
void FirEngineMacDesc::establishCoeffValues(vector<unsigned>* pvValues, const FirEngineSpec& firEngineSpec) const
{
	pvValues->clear();

	unsigned offset = 0;
	for (unsigned i = 0; i < m_vFirEngineMacFifoDesc.size(); ++i)
	{
		// Align Fifos to their 2^N Size
		offset = IntUtils::alignAddressOnOrAfter(offset, m_vFirEngineMacFifoDesc[i].m_NumFifoMemWords);

		pvValues->resize(offset + m_vFirEngineMacFifoDesc[i].m_NumFifoMemWords, 0);

		for (unsigned j = 0; j < m_vFirEngineMacFifoDesc[i].m_vFirCoeffRef.size(); ++j)
		{
			const FirCoeffRef& firCoeffRef = m_vFirEngineMacFifoDesc[i].m_vFirCoeffRef[j];
			double coeffVal = firEngineSpec.lookupCoeff(firCoeffRef);
			assert(coeffVal <= 1.0);
			assert(coeffVal > -1.0);
			unsigned coeffVal1p17 = int(coeffVal * double(1 << 17)) & ((1 << 18) - 1);
			(*pvValues)[offset + j] = coeffVal1p17;
		}

		// advance offset to end of this FIFO
		offset += m_vFirEngineMacFifoDesc[i].m_NumFifoMemWords;	
	}
}

/*
unsigned FirEngineMacDesc::getNumFifoMemWords() const
{
	unsigned numFifoMemWords = 0;
	for (unsigned i = 0; i < m_vFirEngineMacFifoDesc.size(); ++i)
		numFifoMemWords += m_vFirEngineMacFifoDesc[i].m_NumFifoMemWords;
	return numFifoMemWords;
}
*/

