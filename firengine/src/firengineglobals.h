#ifndef FIRENGINEGLOBALS_H
#define FIRENGINEGLOBALS_H


#include <string>
using namespace std;


/////////////////////////////////////////////////////////////
/// Global Settings
/////////////////////////////////////////////////////////////

class FirEngineGlobals
{
public:
	FirEngineGlobals();
public:
	void parseArgs(int argc, char* argv[]);
public:
	static string getHtmlRainbowColor(double f);			///< f must be [0..1)
	static void renderHtmlHeader(ostream&);
	static void renderHtmlFooter(ostream&);
public:
	void generateHtmlReport(ostream&) const;
public:
	string				m_FirEngineName;
	double				m_ClockFreq;
	unsigned			m_NumTimeSlices;
};


#endif
