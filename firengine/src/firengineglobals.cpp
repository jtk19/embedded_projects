
#include <stdlib.h>
#include "getopt.h"
#include "firengineglobals.h"


FirEngineGlobals::FirEngineGlobals() :
	m_FirEngineName		("unknown"),
	m_ClockFreq			(400e6),
	m_NumTimeSlices		(16)
{
}

void FirEngineGlobals::parseArgs(int argc, char* argv[])
{
	static char usage[] = "usage: %s [-f ClockFreq] [-t timeSlices] firEngineName";
	char c;
	while ((c = getopt(argc, argv, "-f:-t:")) != -1)
	{
		switch (c)
		{
		case 'f':
			m_ClockFreq = stod(optarg);
			break;
		case 't':
			m_NumTimeSlices = stoi(optarg);
			break;
		case '?':
			fprintf(stderr, usage, argv[0]);
			exit(1);
		}
	}
	if ((optind + 1) > argc)
	{
		fprintf(stderr, "%s has more than one firEngineName\n", argv[0]);
		fprintf(stderr, usage, argv[0]);
		exit(1);
	}
	else if ((optind + 1) < argc)
	{
		fprintf(stderr, "%s missing firEngineName\n", argv[0]);
		fprintf(stderr, usage, argv[0]);
		exit(1);
	}
	m_FirEngineName = argv[optind];
}

void FirEngineGlobals::generateHtmlReport(ostream& stream) const
{
	stream << "<h2>Feature Summary</h2>\n";
	stream << "<table class=\"t1\">\n";
	stream << "<tr><th>FirEngineName</th><td>" << m_FirEngineName << "</td></tr>\n";
	stream << "<tr><th>ClockFreq</th><td>" << unsigned(m_ClockFreq) << "</td></tr>\n";
	stream << "<tr><th>NumTimeSlices</th><td>" << m_NumTimeSlices << "</td></tr>\n";
	stream << "</table>\n\n";
}
