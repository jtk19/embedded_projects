#ifndef FIRCOEFFREF_H
#define FIRCOEFFREF_H


/////////////////////////////////////////////////////////////
/// A Reference to a FIR coefficient in the FirEngineSpec
/////////////////////////////////////////////////////////////

class FirCoeffRef
{
public:
	FirCoeffRef();
public:
	bool isNull() const		{ return m_FirIndex == -1; }
public:
	/// Which FIR is this coefficient in (-1 for a Null Ref)
	unsigned			m_FirIndex;
	/// Index of Coefficient within the FIR
	unsigned			m_CoeffIndex;
};


#endif
