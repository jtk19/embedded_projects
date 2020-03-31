#ifndef FIRENGINEMACFIFODESC_H
#define FIRENGINEMACFIFODESC_H


#include <vector>
#include "fircoeffref.h"
using namespace std;


////////////////////////////////////////////////////////////////
/// Describes a section of memory used for a single-FIR's Fifo
////////////////////////////////////////////////////////////////

class FirEngineMacFifoDesc
{
public:
	FirEngineMacFifoDesc();
public:
	static bool memWordsLessThan(const FirEngineMacFifoDesc& d0, const FirEngineMacFifoDesc& d1);
public:
	/// FIR Index that this Fifo is used for
	unsigned				m_FirIndex;
	/// Number of Entries required by this Fifo
	unsigned				m_FifoDepth;
	/// Number of Words needed for this Fifo (must be a power of 2, greater than or equal to FifoDepth)
	unsigned				m_NumFifoMemWords;
	/// FIR Coefficients used in the Coeff-Fifo
	vector<FirCoeffRef>		m_vFirCoeffRef;
};


#endif
