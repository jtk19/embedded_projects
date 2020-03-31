// firenginebuilder.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <fstream>
#include "firenginespec.h"
#include "firenginedesc.h"
#include "firengineglobals.h"


static void buildFirEngine(int argc, char* argv[])
{
	FirEngineGlobals firEngineGlobals;
	firEngineGlobals.parseArgs(argc, argv);

	FirEngineSpec firEngineSpec(firEngineGlobals.m_ClockFreq);
	{
		string fname(firEngineGlobals.m_FirEngineName + ".fsp");
		ifstream fstream(fname);
		if (!fstream.is_open())
		{
			printf("Unable to open FirEngine-Specification file '%s'\n", fname.c_str());
			exit(1);
		}
		firEngineSpec.readFromFile(fstream);
	}

	FirEngineDesc firEngineDesc(firEngineGlobals.m_NumTimeSlices);

	// Bind all FIRs to the firEngineDesc (creating new MACs as needed)
	for (unsigned firIdx = 0; firIdx < firEngineSpec.m_vFirSpec.size(); ++firIdx)
	{
		firEngineDesc.bindFir(firEngineSpec, firIdx);
	}

	firEngineDesc.generateRtl(firEngineGlobals.m_FirEngineName, firEngineSpec);

	{
		string fname(firEngineGlobals.m_FirEngineName + ".html");
		ofstream fstream(fname);

		firEngineGlobals.renderHtmlHeader(fstream);
		firEngineGlobals.generateHtmlReport(fstream);
		firEngineSpec.generateHtmlReport(fstream);
		firEngineDesc.generateHtmlReport(fstream);
		firEngineGlobals.renderHtmlFooter(fstream);
	}
}


int main(int argc, char* argv[])
{
	printf("///////////////////////////////////////////////////\n");
	printf("FirEngine Builder version 1.0.0\n");
	printf("///////////////////////////////////////////////////\n");

	try
	{
		buildFirEngine(argc, argv);
	}
	catch (const string& errMsg)
	{
		printf("Error: %s\n", errMsg.c_str());
		return 1;
	}

	return 0;
}

