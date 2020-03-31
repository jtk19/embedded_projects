#ifndef FIRENGINESPEC_H
#define FIRENGINESPEC_H


#include <fstream>
#include "firspec.h"
using namespace std;

class FirCoeffRef;			// forward declaration


/////////////////////////////////////////////////////////////
/// Top-level class for the Specification for a FirEngine
///   A FirEngine is a single block with a single clock that
///   can implement a number of independent FIR filters
/////////////////////////////////////////////////////////////

class FirEngineSpec
{
public:
	explicit FirEngineSpec(double clockFreq);
public:
	double lookupCoeff(const FirCoeffRef&) const;
public:
	void readFromFile(istream&);
public:
	void generateHtmlReport(ostream&) const;
public:
	/// Clock frequency that the FirEngine will run at
	const double 		m_ClockFreq;
	/// List of all FIRs that will be implemented
	vector<FirSpec>		m_vFirSpec;
};


#endif
