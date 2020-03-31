#ifndef FIRBINDING_H
#define FIRBINDING_H


class FirBinding
{
public:
	FirBinding();
public:
	/// Index of FIR to bind
	unsigned		m_FirIndex;
	/// Index of First FirMac to use
	unsigned		m_FirstFirMacIndex;
	/// Updates will occur at TimeSliceOrigin + (n * TimeSliceInterval)
	unsigned		m_TimeSliceOrigin;
	unsigned		m_TimeSliceInterval;
};


#endif
