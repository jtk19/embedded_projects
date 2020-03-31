
#include "firenginemacfifodesc.h"


FirEngineMacFifoDesc::FirEngineMacFifoDesc() :
	m_FirIndex			(0),
	m_FifoDepth			(0),
	m_NumFifoMemWords	(1),
	m_vFirCoeffRef		()
{
}
	
bool FirEngineMacFifoDesc::memWordsLessThan(const FirEngineMacFifoDesc& d0, const FirEngineMacFifoDesc& d1)
{
	return d0.m_NumFifoMemWords < d1.m_NumFifoMemWords;
}
