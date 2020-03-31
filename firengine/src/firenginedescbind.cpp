
#include <assert.h>
#include "intutils.h"
#include "firenginedesc.h"
#include "firenginespec.h"
#include "firengineglobals.h"
#include "firbinding.h"


bool FirEngineDesc::canBind(const FirSpec& firSpec, const FirBinding& firBinding) const
{
	if (firBinding.m_FirstFirMacIndex >= m_vFirEngineMacDesc.size())
		return true;

	const FirEngineMacDesc& firEngineMacDesc = m_vFirEngineMacDesc[firBinding.m_FirstFirMacIndex];

	// Constraint: number of Inputs to a MAC is limited to 15
	if (firEngineMacDesc.m_vInputFirs.size() == 15)
		return false;
	// Constraint: number of Outputs to a MAC is limited to 15
	if (firEngineMacDesc.m_vOutputFirs.size() == 15)
		return false;
	// Constraint: number of Fifos attached a MAC is limited to 256
	if (firEngineMacDesc.getNumFifos() == 256)
		return false;

	// Check that UpdateSlots are available
	unsigned timeSliceOffset;
	for (timeSliceOffset = firBinding.m_TimeSliceOrigin; timeSliceOffset < m_NumTimeSlots; timeSliceOffset += firBinding.m_TimeSliceInterval)
	{
		if (!firEngineMacDesc.m_vFirUpdateSlot[timeSliceOffset].isSlotEmpty())
			return false;
		
		// Constraint: UpdateSlot must be free for 'Read' too
		unsigned readTimeSliceOffset = IntUtils::modulo(timeSliceOffset - m_FirUpdateLatency, m_NumTimeSlots);
		if (!firEngineMacDesc.m_vFirUpdateSlot[readTimeSliceOffset].isSlotEmpty())
			return false;
	}

	for (timeSliceOffset = firBinding.m_TimeSliceOrigin; timeSliceOffset < m_NumTimeSlots; timeSliceOffset += firBinding.m_TimeSliceInterval)
	{
		// all slots needed for Coefficients leading up to (and including) timeSliceOffset must be Empty
		for (unsigned i = 0; i < firSpec.m_vCoeff.size(); ++i)
		{
			unsigned coeffTimeSliceOffset = IntUtils::modulo(timeSliceOffset - i, m_NumTimeSlots);
			if (!firEngineMacDesc.m_vFirCoeffRef[coeffTimeSliceOffset].isNull())
				return false;
		}
	}

	return true;
}

void FirEngineDesc::bind(const FirSpec& firSpec, const FirBinding& firBinding)
{
	if (firBinding.m_FirstFirMacIndex >= m_vFirEngineMacDesc.size())
		m_vFirEngineMacDesc.push_back(FirEngineMacDesc(m_NumTimeSlots));

	FirEngineMacDesc& firEngineMacDesc = m_vFirEngineMacDesc[firBinding.m_FirstFirMacIndex];

	firEngineMacDesc.m_vInputFirs.push_back(firBinding.m_FirIndex);
	firEngineMacDesc.m_vOutputFirs.push_back(firBinding.m_FirIndex);

	// Check that UpdateSlots are available
	unsigned timeSliceOffset;
	for (timeSliceOffset = firBinding.m_TimeSliceOrigin; timeSliceOffset < m_NumTimeSlots; timeSliceOffset += firBinding.m_TimeSliceInterval)
	{
		FirUpdateSlot firUpdateSlot;
		firUpdateSlot.m_FirIndex = firBinding.m_FirIndex;
		firEngineMacDesc.m_vFirUpdateSlot[timeSliceOffset] = firUpdateSlot;
	}

	for (timeSliceOffset = firBinding.m_TimeSliceOrigin; timeSliceOffset < m_NumTimeSlots; timeSliceOffset += firBinding.m_TimeSliceInterval)
	{
		// all slots needed for Coefficients leading up to (and including) timeSliceOffset must be Empty
		for (unsigned i = 0; i < firSpec.m_vCoeff.size(); ++i)
		{
			FirCoeffRef firCoeffRef;
			firCoeffRef.m_FirIndex = firBinding.m_FirIndex;
			firCoeffRef.m_CoeffIndex = (firSpec.m_vCoeff.size() - 1) - i;

			unsigned coeffTimeSliceOffset = IntUtils::modulo(timeSliceOffset - i, m_NumTimeSlots);
			firEngineMacDesc.m_vFirCoeffRef[coeffTimeSliceOffset] = firCoeffRef;
		}
	}

	FirEngineMacFifoDesc firEngineMacFifoDesc;
	firEngineMacFifoDesc.m_FifoDepth = firSpec.m_vCoeff.size();
	firEngineMacFifoDesc.m_NumFifoMemWords = IntUtils::roundUpToPowerOfTwo(firSpec.m_vCoeff.size());
	firEngineMacFifoDesc.m_FirIndex = firBinding.m_FirIndex;
	for (unsigned i = 0; i < firSpec.m_vCoeff.size(); ++i)
	{
		FirCoeffRef firCoeffRef;
		firCoeffRef.m_FirIndex = firBinding.m_FirIndex;
		firCoeffRef.m_CoeffIndex = i;

		firEngineMacFifoDesc.m_vFirCoeffRef.push_back(firCoeffRef);
	}
	firEngineMacDesc.m_vFirEngineMacFifoDesc.push_back(firEngineMacFifoDesc);

	// Finally update the number of FIRs
	++m_NumFirs;
}

FirBinding FirEngineDesc::findValidBinding(const FirSpec& firSpec, unsigned firIndex, unsigned timeSliceInterval) const
{
	// try to start from lowest available firMac (up to a 'new' one, which should always bind)
	for (unsigned firMacIdx = 0; firMacIdx <= m_vFirEngineMacDesc.size(); ++firMacIdx)
	{
		for (unsigned timeSliceOffset = 0; timeSliceOffset < timeSliceInterval; ++timeSliceOffset)
		{
			FirBinding firBinding;
			firBinding.m_FirIndex = firIndex;
			firBinding.m_FirstFirMacIndex = firMacIdx;
			firBinding.m_TimeSliceOrigin = timeSliceOffset;
			firBinding.m_TimeSliceInterval = timeSliceInterval;

			if (canBind(firSpec, firBinding))
				return firBinding;
		}
	}

	assert(false);		// Should always be able to bind!
	return FirBinding();
}

void FirEngineDesc::bindFir(const FirEngineSpec& firEngineSpec, unsigned firIdx)
{
	const FirSpec& firSpec = firEngineSpec.m_vFirSpec[firIdx];

	if (firSpec.m_SampleFreq > (2.0 * firEngineSpec.m_ClockFreq))
		throw string("FIR sample frequencies must be less than half of the clock frequency");

	unsigned timeSliceInterval = unsigned(floor(firEngineSpec.m_ClockFreq / firSpec.m_SampleFreq));
	if (timeSliceInterval > m_NumTimeSlots)
		timeSliceInterval = m_NumTimeSlots;

	// need to round down timeSliceInterval to a even-divisor of numTimeSlots
	unsigned numRepeats = IntUtils::ceilDiv(m_NumTimeSlots, timeSliceInterval);
	timeSliceInterval = (m_NumTimeSlots / numRepeats);

	if (timeSliceInterval < firSpec.m_vCoeff.size())
		throw string("Too many coefficients for sample rate required (currently multi-MAC FIR is not supported)");

	FirBinding firBinding = findValidBinding(firSpec, firIdx, timeSliceInterval);
	bind(firSpec, firBinding);
}
