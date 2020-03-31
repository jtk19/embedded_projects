#ifndef FIRSPEC_H
#define FIRSPEC_H

#include <vector>
using namespace std;


/////////////////////////////////////////////////////////////
/// Represents the Specification for a single FIR
/////////////////////////////////////////////////////////////

class FirSpec
{
public:
	FirSpec();
public:
	/// Rate at which samples will be processed by the FIR
	///   (Currently only single rate FIRs are supported)
	unsigned			m_SampleFreq;
	/// List of all FIR coefficients
	vector<double>		m_vCoeff;
};


#endif
