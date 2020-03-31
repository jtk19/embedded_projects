
#include <assert.h>
#include "stringutil.h"
#include "datetime.h"
#include "firenginedesc.h"
#include "firenginespec.h"
#include "firengineglobals.h"


FirEngineDesc::FirEngineDesc(unsigned numTimeSlots) :
	m_FirUpdateLatency		(2),
	m_NumTimeSlots			(numTimeSlots),
	m_NumFirs				(0),
	m_vFirEngineMacDesc		()
{
}

void FirEngineDesc::generateRtl(const string& firEngineName, const FirEngineSpec& firEngineSpec) const
{
	// Generate top-level
	ofstream fStream(firEngineName + ".v");

	fStream << "`timescale 1ns / 1ps\n";
	fStream << "//////////////////////////////////////////////////////////////////////////////////\n";
	fStream << "// Design Name: Fir Engine\n";
	fStream << "// Module Name: firengine\n";
	fStream << "//////////////////////////////////////////////////////////////////////////////////\n";
	fStream << "\n";

	//////////////////////////////////////////////////////////
	// Module Definition
	//////////////////////////////////////////////////////////
	fStream << "module " << firEngineName << " (\n";
	fStream << "\tinput             iClk,\n";
	fStream << "\tinput             iRst,\n";
	fStream << "\n";

	fStream << "\t// Each Channel has a seperate (data-input, data-changed) pair\n";
	fStream << "\t//   Data-Changed is flipped every time a new sample arrives\n";
	for (unsigned i = 0; i < m_NumFirs; ++i)
	{
		fStream << "\tinput             iData" << i << "Changed,\n";
		fStream << "\tinput [17:0]      iData" << i << ",\n";
	}
	fStream << "\n";

    fStream << "\t// Each Channel has a seperate (data-output, data-changed) pair\n";
    fStream << "\t//   Data-Changed is flipped every time a new sample appears\n";
	for (unsigned i = 0; i < m_NumFirs; ++i)
	{
		fStream << "\toutput          				oData" << i << "Changed,\n";
		fStream << "\toutput [17:0]	   		 		oData" << i << ",\n";
	}
	fStream << "\n";

	fStream << "\t// Interface to write to Coefficient Memory\n";
 	fStream << "\tinput							iCoefBuff_wren,\n";
   	fStream << "\tinput [31:0]					iCoefBuff_wraddr,\n";
   	fStream << "\tinput [17:0]	  				iCoefBuff_wrdata\n";
	fStream << ");\n";
	fStream << "\n";

	//////////////////////////////////////////////////////////
	// Module Implementation
	//////////////////////////////////////////////////////////
	for (unsigned macIdx = 0; macIdx < (m_vFirEngineMacDesc.size() + 1); ++macIdx)
	{
		fStream << "wire [17:0]		chainD" << macIdx << ";\n";
		fStream << "wire [17:0]		chainR" << macIdx << ";\n";
		fStream << "wire [35:0]		chainS" << macIdx << ";\n";
		fStream << "wire	inputChangeChain" << macIdx << ";\n";
	}
	fStream << "\n";
	fStream << "// Initialize start of the MAC-Chains\n";
	fStream << "assign chainD0 = 18'b0;\n";
	fStream << "assign chainR0 = 18'b0;\n";
	fStream << "assign chainS0 = 36'b0;\n";
	fStream << "assign inputChangeChain0 = 1'b0;\n";
	fStream << "\n";

	for (unsigned macIdx = 0; macIdx < m_vFirEngineMacDesc.size(); ++macIdx)
	{
		const FirEngineMacDesc& firEngineMacDesc = m_vFirEngineMacDesc[macIdx];
		const vector<unsigned>& vInputFirs = firEngineMacDesc.m_vInputFirs;
		const vector<unsigned>& vOutputFirs = firEngineMacDesc.m_vOutputFirs;

		fStream << firEngineName << "_fir ";
		fStream << "i_" << firEngineName << "_fir ";
		fStream << "(\n";
		fStream << "\t.iClk			(iClk),\n";
		fStream << "\t.iRst			(iRst),\n";
		for (unsigned i = 0; i < vInputFirs.size(); ++i)
		{
			fStream << "\t.iData" << i << "Changed	(iData" << vInputFirs[i] << "Changed),\n";
			fStream << "\t.iData" << i << "			(iData" << vInputFirs[i] << "),\n";
		}
		for (unsigned i = 0; i < vOutputFirs.size(); ++i)
		{
			fStream << "\t.oData" << i << "Changed	(oData" << vOutputFirs[i] << "Changed),\n";
			fStream << "\t.oData" << i << "			(oData" << vOutputFirs[i] << "),\n";
		}
		
		// Chain MACs together
		fStream << "\t.iChainD				(chainD" << macIdx << "),\n";
		fStream << "\t.oChainD				(chainD" << (macIdx + 1) << "),\n";
		fStream << "\t.iChainR				(chainR" << macIdx << "),\n";
		fStream << "\t.oChainR				(chainR" << (macIdx + 1) << "),\n";
		fStream << "\t.iChainS				(chainS" << macIdx << "),\n";
		fStream << "\t.oChainS				(chainS" << (macIdx + 1) << "),\n";
		fStream << "\t.iInputChangeChain	(inputChangeChain" << macIdx << "),\n";
		fStream << "\t.oInputChangeChain	(inputChangeChain" << (macIdx + 1) << "),\n";

		// Coeff Write Interface
 		fStream << "\t.iCoefBuff_wren	(1'b0),\n";
 		fStream << "\t.iCoefBuff_wraddr	(32'b0),\n";
 		fStream << "\t.iCoefBuff_wrdata	(18'b0)\n";

		fStream << "\t);\n";
		fStream << "\n";
	}
	
	fStream << "endmodule\n";

	//////////////////////////////////////////////////////////
	// Generate Sub-Modules
	//////////////////////////////////////////////////////////
	for (unsigned macIdx = 0; macIdx < m_vFirEngineMacDesc.size(); ++macIdx)
	{
		const FirEngineMacDesc& firEngineMacDesc = m_vFirEngineMacDesc[macIdx];
		firEngineMacDesc.generateRtl(firEngineName + "_fir" + toString(macIdx), firEngineSpec);
	}
}

void FirEngineDesc::generateHtmlReport(ostream& stream) const
{
	stream << "<h2>FirEngine Description</h2>\n";

	stream << "<table class=\"t1\">\n";
	stream << "<tr><th>Fir#</th><th>Colour</th></tr>\n";
	for (unsigned firIdx = 0; firIdx < m_NumFirs; ++firIdx)
	{
		stream << "<tr><td>" << firIdx << "</td><td  style=\"background: " << FirEngineGlobals::getHtmlRainbowColor(double(firIdx) / double(m_NumFirs)) << "\"></td></tr>\n";
	}
	stream << "</table>\n\n";

	stream << "<table class=\"t1\">\n";
	stream << "<tr><th>TimeSlot</th>";
	for (unsigned firMac = 0; firMac < m_vFirEngineMacDesc.size(); ++firMac)
	{
		stream << "<th>FirMac" << firMac << "</th>";
	}
	stream << "</tr>\n";

	for (unsigned timeSlot = 0; timeSlot < m_NumTimeSlots; ++timeSlot)
	{
		stream << "<tr>";
		stream << "<td>" << timeSlot << "</td>";
		for (unsigned firMac = 0; firMac < m_vFirEngineMacDesc.size(); ++firMac)
		{
			const FirEngineMacDesc& firEngineMacDesc = m_vFirEngineMacDesc[firMac];
			const FirCoeffRef& firCoeffRef = firEngineMacDesc.m_vFirCoeffRef[timeSlot];
			const FirUpdateSlot& firUpdateSlot = firEngineMacDesc.m_vFirUpdateSlot[timeSlot];
			if (firCoeffRef.isNull())
				stream << "<td></td>";
			else
				stream << "<td style=\"background: " << FirEngineGlobals::getHtmlRainbowColor(double(firCoeffRef.m_FirIndex) / double(m_NumFirs)) << "\">";
			if (!firUpdateSlot.isSlotEmpty())
				stream << "Update";
			stream << "</td>";
		}
		stream << "</tr>\n";
	}
	stream << "</table>\n\n";

	///////////////////////////////////////////////////////////

	stream << "<h2>Coefficient Address Map</h2>\n";
	stream << "TODO";
}
