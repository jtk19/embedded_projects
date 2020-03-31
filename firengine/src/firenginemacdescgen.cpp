
#include <fstream>
#include <assert.h>
#include "datetime.h"
#include "intutils.h"
#include "stringutil.h"
#include "firenginemacdesc.h"



static void _renderVectorAsHexString(ostream& stream, unsigned bitsPerValue, const vector<unsigned>& vValues)
{
	string str = toString(bitsPerValue * vValues.size());
	while (str.size() < 3)
		str = " " + str;
	stream << str;

	unsigned maxAllowedValue = 0;
	unsigned numDigits = 0;
	if (bitsPerValue == 1)
	{
		stream << "'b";
		maxAllowedValue = 1;
		numDigits = 1;
	}
	else if (bitsPerValue == 4)
	{
		stream << "'h";
		maxAllowedValue = 0xF;
		numDigits = 1;
	}
	else if (bitsPerValue == 8)
	{
		stream << "'h";
		maxAllowedValue = 0xFF;
		numDigits = 2;
	}
	else
	{
		assert(false);		// unexpected bitsPerValue
	}

	for (unsigned i = 0; i < vValues.size(); ++i)
	{
		if (i > 0)
			stream << "_";
		if (numDigits == 1)
			stream << "_";
		unsigned val = vValues[vValues.size() - 1 - i];
		assert(val <= maxAllowedValue);
		stream << toHexDigits(val, numDigits);
	}
}

static void _renderVectorAsConcat(ostream& stream, unsigned bitsPerValue, const vector<unsigned>& vValues)
{
	stream << "{";
	for (unsigned i = 0; i < vValues.size(); ++i)
	{
		if (i > 0)
			stream << ", ";
		unsigned val = vValues[vValues.size() - 1 - i];
		assert(val < (1u << bitsPerValue));		// check in range
		stream << bitsPerValue << "'h" << toHexDigits(val, IntUtils::ceilDiv(bitsPerValue, 4));
	}
	stream << "}";
}


void FirEngineMacDesc::generateRtl(const string& firEngineMacName, const FirEngineSpec& firEngineSpec) const
{
	vector<unsigned> vChannelSelectCtrl;
	vector<unsigned> vFirstEngineCtrl;
	vector<unsigned> vLastEngineCtrl;
	vector<unsigned> vFirstTapCtrl;
	vector<unsigned> vPreAddModeCtrl;
	vector<unsigned> vMulModeCtrl;
	vector<unsigned> vAddPrevEngineAccumCtrl;
	vector<unsigned> vRdFifoNumCtrl;
	vector<unsigned> vUpdateFifoNumCtrl;
	vector<unsigned> vDoUpdateCtrl;

	establishChannelSelectCtrl(&vChannelSelectCtrl);
	establishFirstEngineCtrl(&vFirstEngineCtrl);
	establishLastEngineCtrl(&vLastEngineCtrl);
	establishFirstTapCtrl(&vFirstTapCtrl);
	establishPreAddModeCtrl(&vPreAddModeCtrl);
	establishMulModeCtrl(&vMulModeCtrl);
	establishAddPrevEngineAccumCtrl(&vAddPrevEngineAccumCtrl);
	establishRdFifoNumCtrl(&vRdFifoNumCtrl);
	establishUpdateFifoNumCtrl(&vUpdateFifoNumCtrl);
	establishDoUpdateCtrl(&vDoUpdateCtrl);

	const_cast<FirEngineMacDesc*>(this)->sortFifosInDescendingSizeOrder();

	vector<unsigned> vFifoSizes;
	establishFifoSizes(&vFifoSizes);
	
	vector<unsigned> vCoeffValues;
	establishCoeffValues(&vCoeffValues, firEngineSpec);
	

	ofstream fStream(firEngineMacName + ".v");

	fStream << "`timescale 1ns / 1ps\n";
	fStream << "//////////////////////////////////////////////////////////////////////////////////\n";
	fStream << "// Design Name: Fir Engine\n";
	fStream << "// Module Name: firengine\n";
	fStream << "//////////////////////////////////////////////////////////////////////////////////\n";
	fStream << "\n";

	//////////////////////////////////////////////////////////
	// Module Definition
	//////////////////////////////////////////////////////////
	fStream << "module " << firEngineMacName << " (\n";
	fStream << "\tinput             iClk,\n";
	fStream << "\tinput             iRst,\n";
	fStream << "\n";

	fStream << "\t// Each Channel has a seperate (data-input, data-changed) pair\n";
	fStream << "\t//   Data-Changed is flipped every time a new sample arrives\n";
	for (unsigned i = 0; i < m_vInputFirs.size(); ++i)
	{
		fStream << "\tinput             iData" << i << "Changed,\n";
		fStream << "\tinput [17:0]      iData" << i << ",\n";
	}
	fStream << "\n";

	fStream << "\t// Each Channel has a seperate (data-output, data-changed) pair\n";
	fStream << "\t//   Data-Changed is flipped every time a new sample appears\n";
	for (unsigned i = 0; i < m_vOutputFirs.size(); ++i)
	{
		fStream << "\toutput          				oData" << i << "Changed,\n";
		fStream << "\toutput [17:0]	   		 		oData" << i << ",\n";
	}
	fStream << "\n";

	fStream << "\t/// The following signals are used to chain FirEngines together\n";
	fStream << "\tinput [17:0]      			iChainD,\n";
	fStream << "\toutput [17:0]     			oChainD,\n";
	fStream << "\tinput [17:0]      			iChainR,\n";
	fStream << "\toutput [17:0]     			oChainR,\n";
	fStream << "\tinput [35:0]      			iChainS,\n";
	fStream << "\toutput [35:0]     			oChainS,\n";
	fStream << "\tinput                         iInputChangeChain,\n";
	fStream << "\toutput                        oInputChangeChain,\n";
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
	assert(vCoeffValues.size() <= 1024);		// Reason: FIFOSIZES offset bitwidth
	fStream << "parameter LOG2BUFFERDEPTH = " << IntUtils::bitWidthForEncodingValues(vCoeffValues.size()) << ";\n";
	fStream << "parameter BUFFERDEPTH = " << vCoeffValues.size() << ";          // Maximum of 1024 currently supported\n";
	fStream << "\n";
	assert(getNumFifos() <= 256);			// Reason: RDFIFONUM / UPDATEFIFONUM bitwidth
	fStream << "parameter LOG2NUMFIFOS = " << IntUtils::bitWidthForEncodingValues(getNumFifos()) << ";\n";
	fStream << "parameter NUMFIFOS = " << getNumFifos() << ";          // Maximum of 256 currently supported\n";
	fStream << "\n";
	assert(getNumTimeSlots() <= 256);			// Reason: formatting of SlotNumbers limited to 2 hex digits
	fStream << "parameter LOG2TIMESLICES = " << IntUtils::bitWidthForEncodingValues(getNumTimeSlots()) << ";\n";
	fStream << "parameter TIMESLICES = " << getNumTimeSlots() << ";          // Maximum of 256 currently supported\n";
	fStream << "\n";

	fStream << "/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n";
	fStream << "// FirEngine Configuration\n";
	fStream << "//   CHANNEL_SELECT			4 bits	- Selects which Input channel to use in this timeSlot (0 = None) This should only be non-zero during Fir Update-Write cycle\n";
	fStream << "//   FIRST_ENGINE			1 bit	- '1' when updating the first FirEngine in a chain. Valid on DOUPDATE cycle\n";
	fStream << "//   LAST_ENGINE			1 bit	- '1' when updating the last FirEngine in a chain. Valid on DOUPDATE cycle\n";
	fStream << "//   FIRST_TAP     			1 bit	- '1' for first tap of the section of FIR in this FirEngine\n";
	fStream << "//   PREADD_MODE			4 bits	- 0=NoPreadder(B*A), 1=PreAdd((D+B)*A), 2=PreSub((D-B)*A)\n";
	fStream << "//   MUL_MODE				4 bits	- 0=MUL, 1=MADD, 2=MSUB\n";
	fStream << "//   ADDPREVENGINEACCUM 	1 bit   - Use value of previous Fir Engine's Accumulator from 2-cycles ago\n";
	fStream << "//   RDFIFONUM  			8 bits	- Selects which Data Fifo to use\n";
	fStream << "//   UPDATEFIFONUM			8 bits	- Selects which Data Fifo to update (valid on doUpdate cycle and 3 cycles earlier)\n";
	fStream << "//   DOUPDATE	      		1 bits	- High at the same time on all FirEngines involved in a firfilter for the last tap of the fir-filter\n";
	fStream << "//\n";
	fStream << "//   FIFOSIZES      		16 bits	- Foreach Fifo [FifoOffset : 10 bits, Len-1: 6 bits]\n";
	fStream << "//\n";
	fStream << "//   COEFF_VALUES           24 bits for every slot in Coeff-Buffer\n";
	fStream << "//\n";
	// Print Slot Numbers (each Slot is 3 characters wide)
	fStream << "//                        	  Slot:    ";
	for (unsigned i = 0; i < getNumTimeSlots(); ++i)
		fStream << toHexDigits(getNumTimeSlots() - i - 1, 2) << " ";
	fStream << "\n";
	fStream << "parameter CHANNEL_SELECT		= "; _renderVectorAsHexString(fStream, 4, vChannelSelectCtrl); fStream << ";\n";
	fStream << "parameter FIRST_ENGINE	 		= "; _renderVectorAsHexString(fStream, 1, vFirstEngineCtrl); fStream << ";\n";
	fStream << "parameter LAST_ENGINE 		    = "; _renderVectorAsHexString(fStream, 1, vLastEngineCtrl); fStream << ";\n";
	fStream << "parameter FIRST_TAP		        = "; _renderVectorAsHexString(fStream, 1, vFirstTapCtrl); fStream << ";\n";
	fStream << "parameter PREADD_MODE			= "; _renderVectorAsHexString(fStream, 4, vPreAddModeCtrl); fStream << ";\n";
	fStream << "parameter MUL_MODE				= "; _renderVectorAsHexString(fStream, 4, vMulModeCtrl); fStream << ";\n";
	fStream << "parameter ADDPREVENGINEACCUM    = "; _renderVectorAsHexString(fStream, 1, vAddPrevEngineAccumCtrl); fStream << ";\n";
	fStream << "parameter RDFIFONUM 			= "; _renderVectorAsHexString(fStream, 8, vRdFifoNumCtrl); fStream << ";\n";
	fStream << "parameter UPDATEFIFONUM      	= "; _renderVectorAsHexString(fStream, 8, vUpdateFifoNumCtrl); fStream << ";\n";
	fStream << "parameter DOUPDATE		        = "; _renderVectorAsHexString(fStream, 1, vDoUpdateCtrl); fStream << ";\n";
	fStream << "\n";
	fStream << "parameter FIFOSIZES 			= "; _renderVectorAsConcat(fStream, 16, vFifoSizes); fStream << "; \n";
	fStream << "parameter COEFF_VALUES 			= "; _renderVectorAsConcat(fStream, 24, vCoeffValues); fStream << ";\n";
	fStream << "\n";

	fStream << "integer i;\n";
	fStream << "\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "// TimeSliceCounter at different pipeline stages\n";
	fStream << "//   (timeSlice_psN is the time-slice value that can be read in pipeline stage N)\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "reg [LOG2TIMESLICES-1:0] timeSlice_psm1 = 0;\n";
	fStream << "reg [LOG2TIMESLICES-1:0] timeSlice_ps0 = 0;\n";
	fStream << "reg [LOG2TIMESLICES-1:0] timeSlice_ps1 = 0;\n";
	fStream << "reg [LOG2TIMESLICES-1:0] timeSlice_ps2 = 0;\n";
	fStream << "reg [LOG2TIMESLICES-1:0] timeSlice_ps3 = 0;\n";
	fStream << "reg [LOG2TIMESLICES-1:0] timeSlice_ps4 = 0;\n";
	fStream << "reg [LOG2TIMESLICES-1:0] timeSlice_ps5 = 0;\n";
	fStream << "reg [LOG2TIMESLICES-1:0] timeSlice_ps6 = 0;\n";
	fStream << "reg [LOG2TIMESLICES-1:0] timeSlice_ps7 = 0;\n";
	fStream << "reg [LOG2TIMESLICES-1:0] timeSlice_ps8 = 0;\n";
	fStream << "reg [LOG2TIMESLICES-1:0] timeSlice_ps9 = 0;\n";
	fStream << "\n";
	fStream << "// Generate a time-slice counters that will be used throughout the design\n";
	fStream << "always @(posedge iClk or posedge iRst)\n";
	fStream << "begin\n";
	fStream << "    if (iRst) begin\n";
	fStream << "        timeSlice_psm1 <= 0;\n";
	fStream << "        timeSlice_ps0 <= 0;\n";
	fStream << "        timeSlice_ps1 <= 0;\n";
	fStream << "        timeSlice_ps2 <= 0;\n";
	fStream << "        timeSlice_ps3 <= 0;\n";
	fStream << "        timeSlice_ps4 <= 0;\n";
	fStream << "        timeSlice_ps5 <= 0;\n";
	fStream << "        timeSlice_ps6 <= 0;\n";
	fStream << "        timeSlice_ps7 <= 0;\n";
	fStream << "        timeSlice_ps8 <= 0;\n";
	fStream << "        timeSlice_ps9 <= 0;\n";
	fStream << "      end else begin\n";
	fStream << "        timeSlice_psm1 <= timeSlice_psm1 + 1;\n";
	fStream << "        timeSlice_ps0 <= timeSlice_psm1;\n";
	fStream << "        timeSlice_ps1 <= timeSlice_ps0;\n";
	fStream << "        timeSlice_ps2 <= timeSlice_ps1;\n";
	fStream << "        timeSlice_ps3 <= timeSlice_ps2;\n";
	fStream << "        timeSlice_ps4 <= timeSlice_ps3;\n";
	fStream << "        timeSlice_ps5 <= timeSlice_ps4;\n";
	fStream << "        timeSlice_ps6 <= timeSlice_ps5;\n";
	fStream << "        timeSlice_ps7 <= timeSlice_ps6;\n";
	fStream << "        timeSlice_ps8 <= timeSlice_ps7;\n";
	fStream << "        timeSlice_ps9 <= timeSlice_ps8;\n";
	fStream << "    end\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "// latch all input data as it arrives\n";
	fStream << "//   note that data could arrive from multiple channels during the same clock cycle\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "// These registers are used to latch input samples when they change\n";
	for (unsigned i = 0; i < m_vInputFirs.size(); ++i)
	{
		fStream << "reg [17:0] data" << i << "_ps1 = 0;\n";
		fStream << "reg data" << i << "Changed_ps1 = 0;\n";
		fStream << "reg prevData" << i << "Changed = 0;\n";
	}
	fStream << "\n";
	fStream << "always @(posedge iClk or posedge iRst)\n";
	fStream << "begin\n";
	fStream << "    if (iRst) begin\n";
	for (unsigned i = 0; i < m_vInputFirs.size(); ++i)
	{
		fStream << "		data" << i << "_ps1 <= 0;\n";
		fStream << "		data" << i << "Changed_ps1 <= 0;\n";
	}
	fStream << "    end else begin\n";
	fStream << "        // Only latch new data when it changes\n";
	for (unsigned i = 0; i < m_vInputFirs.size(); ++i)
	{
		fStream << "        if (prevData" << i << "Changed != iData" << i << "Changed)\n";
		fStream << "            data" << i << "_ps1 <= iData" << i << ";\n";
		fStream << "        data" << i << "Changed_ps1 <= iData" << i << "Changed;\n";
	}
	fStream << "   end\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "// select one of the channels\n";
	fStream << "//   only when a channel is selected is the prevDataChanged flag updated to reflect the current dataChanged flag\n";
	fStream << "//   this ensures that the same data channel value is not used again until it has changed.\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "reg [17:0] chosenData_ps2;\n";
	fStream << "reg chosenDataChanged_ps2;\n";
	fStream << "\n";
	fStream << "reg [3:0] channelSel_ps1;\n";
	fStream << "always @(posedge iClk)\n";
	fStream << "begin\n";
	fStream << "    channelSel_ps1 <= CHANNEL_SELECT >> {timeSlice_ps0, 2'b0};\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "always @(posedge iClk or posedge iRst)\n";
	fStream << "begin\n";
	fStream << "    if (iRst) begin\n";
	fStream << "        chosenData_ps2 <= 0;\n";
	fStream << "        chosenDataChanged_ps2 <= 0;\n";
	for (unsigned i = 0; i < m_vInputFirs.size(); ++i)
	{
		fStream << "        prevData" << i << "Changed <= 0;\n";
	}
	fStream << "    end else begin\n";
	fStream << "        case (channelSel_ps1)\n";
	for (unsigned i = 0; i < m_vInputFirs.size(); ++i)
	{
		fStream << "            " << i << ": begin ";
		fStream << "chosenData_ps2 <= data" << i << "_ps1; ";
		fStream << "chosenDataChanged_ps2 <= data" << i << "Changed_ps1 ^ prevData" << i << "Changed;";
		fStream << "prevData" << i << "Changed <= data" << i << "Changed_ps1; ";
		fStream << "end\n";
	}
	fStream << "            default: begin chosenData_ps2 <= 18'hx; chosenDataChanged_ps2 <= 1'b0; end\n";
	fStream << "        endcase\n";
	fStream << "   end\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "// FIR Data-Fifos are only updated when a Channel is selected\n";
	fStream << "//   Note: DoUpdate must be disabled for a few cycles after reset as it can only be enabled 3-cycles after a valid UpdateFifoNum\n";
	fStream << "reg doUpdate_ps2;\n";
	fStream << "reg [1:0] doUpdate_EnableCounter;\n";
	fStream << "always @(posedge iClk or posedge iRst)\n";
	fStream << "begin\n";
	fStream << "    if (iRst) begin\n";
	fStream << "        doUpdate_ps2 <= 1'b0;\n";
	fStream << "        doUpdate_EnableCounter <= 2'b0;\n";
	fStream << "    end else begin\n";
	fStream << "        doUpdate_ps2 <= (DOUPDATE >> timeSlice_ps1);\n";
	fStream << "        if (doUpdate_EnableCounter != 2'b11) begin\n";
	fStream << "            doUpdate_EnableCounter <= doUpdate_EnableCounter + 1;\n";
	fStream << "            doUpdate_ps2 <= 1'b0;           // force doUpdate to be disabled \n";
	fStream << "        end\n";
	fStream << "    end\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "// create the Coefficient RAM\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "reg [LOG2BUFFERDEPTH-1:0] coefBuff_rdaddr;\n";
	fStream << "reg [17:0] coefBuff_rddata = 0;\n";
	fStream << "reg [17:0] coefBuff_rddata_int = 0;\n";
	fStream << "reg [17:0] coefBuff_contents[(1 << LOG2BUFFERDEPTH)-1:0];\n";
	fStream << "\n";
	fStream << "initial\n";
	fStream << "begin\n";
	fStream << "	for (i = 0; i < (1 << LOG2BUFFERDEPTH); i = i + 1)\n";
	fStream << "		coefBuff_contents[i] <= COEFF_VALUES >> (24 * i);\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "always @(posedge iClk) \n";
	fStream << "begin\n";
	fStream << "    if (iCoefBuff_wren)\n";
	fStream << "    	coefBuff_contents[iCoefBuff_wraddr] <= iCoefBuff_wrdata;\n";
	fStream << "    coefBuff_rddata_int <= coefBuff_contents[coefBuff_rdaddr];\n";
	fStream << "  	coefBuff_rddata <= coefBuff_rddata_int;\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "// declare dataBuff RAMs\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "reg [17:0] dataBuffA0_rddata;\n";
	fStream << "reg [17:0] dataBuffA1_rddata;\n";
	fStream << "reg [17:0] dataBuffA1_wrdata;\n";
	fStream << "reg [LOG2BUFFERDEPTH-1:0] dataBuffA0_rdaddr;\n";
	fStream << "wire [LOG2BUFFERDEPTH-1:0] dataBuffA1_rwaddr;\n";
	fStream << "wire dataBuffA1_wren;\n";
	fStream << "\n";
	fStream << "reg [17:0] dataBuffA0_rddata_int = 0;\n";
	fStream << "reg [17:0] dataBuffA1_rddata_int = 0;\n";
	fStream << "reg [17:0] dataBuffA_contents[(1 << LOG2BUFFERDEPTH)-1:0];\n";
	fStream << "\n";
	fStream << "initial \n";
	fStream << "begin\n";
	fStream << "	for (i = 0; i < (1 << LOG2BUFFERDEPTH); i = i + 1)\n";
	fStream << "		dataBuffA_contents[i] <= 0;\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "always @(posedge iClk)\n";
	fStream << "begin\n";
	fStream << "    if (dataBuffA1_wren)\n";
	fStream << "    	dataBuffA_contents[dataBuffA1_rwaddr[LOG2BUFFERDEPTH-1:0]] <= dataBuffA1_wrdata;\n";
	fStream << "    dataBuffA1_rddata_int <= dataBuffA_contents[dataBuffA1_rwaddr[LOG2BUFFERDEPTH-1:0]];\n";
	fStream << "  	dataBuffA1_rddata <= dataBuffA1_rddata_int;\n";
	fStream << "    dataBuffA0_rddata_int <= dataBuffA_contents[dataBuffA0_rdaddr[LOG2BUFFERDEPTH-1:0]];\n";
	fStream << "    dataBuffA0_rddata <= dataBuffA0_rddata_int;\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "reg [17:0] dataBuffB0_rddata;\n";
	fStream << "reg [17:0] dataBuffB1_rddata;\n";
	fStream << "reg [17:0] dataBuffB1_wrdata;\n";
	fStream << "reg [LOG2BUFFERDEPTH-1:0] dataBuffB0_rdaddr;\n";
	fStream << "wire [LOG2BUFFERDEPTH-1:0] dataBuffB1_rwaddr;\n";
	fStream << "wire dataBuffB1_wren;\n";
	fStream << "\n";
	fStream << "reg [17:0] dataBuffB0_rddata_int = 0;\n";
	fStream << "reg [17:0] dataBuffB1_rddata_int = 0;\n";
	fStream << "reg [17:0] dataBuffB_contents[(1 << LOG2BUFFERDEPTH)-1:0];\n";
	fStream << "\n";
	fStream << "initial\n";
	fStream << "begin\n";
	fStream << "	for (i = 0; i < (1 << LOG2BUFFERDEPTH); i = i + 1)\n";
	fStream << "		dataBuffB_contents[i] <= 0;\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "always @(posedge iClk)\n";
	fStream << "begin\n";
	fStream << "    if (dataBuffB1_wren)\n";
	fStream << "    	dataBuffB_contents[dataBuffB1_rwaddr[LOG2BUFFERDEPTH-1:0]] <= dataBuffB1_wrdata;\n";
	fStream << "    dataBuffB1_rddata_int <= dataBuffB_contents[dataBuffB1_rwaddr[LOG2BUFFERDEPTH-1:0]];\n";
	fStream << "  	dataBuffB1_rddata <= dataBuffB1_rddata_int;\n";
	fStream << "    dataBuffB0_rddata_int <= dataBuffB_contents[dataBuffB0_rdaddr[LOG2BUFFERDEPTH-1:0]];\n";
	fStream << "    dataBuffB0_rddata <= dataBuffB0_rddata_int;\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "// mux in data to update dataBuffs (using Ram Port 1)\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "reg firstEngine_ps2;\n";
	fStream << "reg lastEngine_ps2;\n";
	fStream << "\n";
	fStream << "always @(posedge iClk) begin\n";
	fStream << "    firstEngine_ps2 <= FIRST_ENGINE[timeSlice_ps1];\n";
	fStream << "    lastEngine_ps2 <= LAST_ENGINE[timeSlice_ps1];\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "always @(posedge iClk) begin\n";
	fStream << "    dataBuffA1_wrdata <= firstEngine_ps2 ? chosenData_ps2 : iChainD;\n";
	fStream << "    dataBuffB1_wrdata <= lastEngine_ps2 ? dataBuffA1_rddata : iChainR;\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "assign oChainD = dataBuffA1_rddata;\n";
	fStream << "assign oChainR = dataBuffB1_rddata;\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "// data Commit chain (commit changes)\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "reg commit_ps3;\n";
	fStream << "reg commit_ps4;\n";
	fStream << "reg commit_ps5;\n";
	fStream << "reg commit_ps6;\n";
	fStream << "reg commit_ps7;\n";
	fStream << "reg commit_ps8;\n";
	fStream << "reg commit_ps9;\n";
	fStream << "\n";
	fStream << "wire doCommit_ps2;\n";
	fStream << "assign doCommit_ps2 = firstEngine_ps2 ? chosenDataChanged_ps2 : (chosenDataChanged_ps2 | iInputChangeChain);\n";
	fStream << "assign oInputChangeChain = doCommit_ps2;\n";
	fStream << "\n";
	fStream << "always @(posedge iClk or posedge iRst)\n";
	fStream << "begin\n";
	fStream << "    if (iRst) begin\n";
	fStream << "        commit_ps3 <= 0;\n";
	fStream << "        commit_ps4 <= 0;\n";
	fStream << "        commit_ps5 <= 0;\n";
	fStream << "        commit_ps6 <= 0;\n";
	fStream << "        commit_ps7 <= 0;\n";
	fStream << "        commit_ps8 <= 0;\n";
	fStream << "        commit_ps9 <= 0;\n";
	fStream << "    end else begin\n";
	fStream << "        commit_ps3 <= 1'b0;\n";
	fStream << "        commit_ps4 <= commit_ps3;\n";
	fStream << "        commit_ps5 <= commit_ps4;\n";
	fStream << "        commit_ps6 <= commit_ps5;\n";
	fStream << "        commit_ps7 <= commit_ps6;\n";
	fStream << "        commit_ps8 <= commit_ps7;\n";
	fStream << "        commit_ps9 <= commit_ps8;\n";
	fStream << "        if (doUpdate_ps2) begin\n";
	fStream << "            commit_ps3 <= doCommit_ps2;\n";
	fStream << "        end\n";
	fStream << "   end\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "assign dataBuffA1_wren = commit_ps3;\n";
	fStream << "assign dataBuffB1_wren = commit_ps3;\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "// instantiate the DSP48E2\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "wire [47:0] dsp48e_result_ps9;\n";
	fStream << "reg [3:0] dsp48e_alumode_ps7;\n";
	fStream << "wire [2:0] dsp48e_carryinsel = 3'b0;		// Carry from CARRYIN\n";
	fStream << "reg [4:0] dsp48e_inmode_ps5;\n";
	fStream << "reg [8:0] dsp48e_opmode_ps7;\n";
	fStream << "\n";
	fStream << "DSP48E2 #(\n";
	fStream << "      // Feature Control Attributes: Data Path Selection\n";
	fStream << "      .AMULTSEL(\"A\"),                    // Selects A input to multiplier (A, AD)\n";
	fStream << "      .A_INPUT(\"DIRECT\"),                // Selects A input source, \"DIRECT\" (A port) or \"CASCADE\" (ACIN port)\n";
	fStream << "      .BMULTSEL(\"AD\"),                   // Selects B input to multiplier (AD, B)\n";
	fStream << "      .B_INPUT(\"DIRECT\"),                // Selects B input source, \"DIRECT\" (B port) or \"CASCADE\" (BCIN port)\n";
	fStream << "      .PREADDINSEL(\"B\"),                 // Selects input to preadder (A, B)\n";
	fStream << "      .RND(48'h000000000000),            // Rounding Constant\n";
	fStream << "      .USE_MULT(\"MULTIPLY\"),             // Select multiplier usage (DYNAMIC, MULTIPLY, NONE)\n";
	fStream << "      .USE_SIMD(\"ONE48\"),                // SIMD selection (FOUR12, ONE48, TWO24)\n";
	fStream << "      .USE_WIDEXOR(\"FALSE\"),             // Use the Wide XOR function (FALSE, TRUE)\n";
	fStream << "      .XORSIMD(\"XOR24_48_96\"),           // Mode of operation for the Wide XOR (XOR12, XOR24_48_96)\n";
	fStream << "      // Pattern Detector Attributes: Pattern Detection Configuration\n";
	fStream << "      .AUTORESET_PATDET(\"NO_RESET\"),     // NO_RESET, RESET_MATCH, RESET_NOT_MATCH\n";
	fStream << "      .AUTORESET_PRIORITY(\"RESET\"),      // Priority of AUTORESET vs.CEP (CEP, RESET).\n";
	fStream << "      .MASK(48'h3fffffffffff),           // 48-bit mask value for pattern detect (1=ignore)\n";
	fStream << "      .PATTERN(48'h000000000000),        // 48-bit pattern match for pattern detect\n";
	fStream << "      .SEL_MASK(\"MASK\"),                 // C, MASK, ROUNDING_MODE1, ROUNDING_MODE2\n";
	fStream << "      .SEL_PATTERN(\"PATTERN\"),           // Select pattern value (C, PATTERN)\n";
	fStream << "      .USE_PATTERN_DETECT(\"NO_PATDET\"),  // Enable pattern detect (NO_PATDET, PATDET)\n";
	fStream << "      // Programmable Inversion Attributes: Specifies built-in programmable inversion on specific pins\n";
	fStream << "      .IS_ALUMODE_INVERTED(4'b0000),     // Optional inversion for ALUMODE\n";
	fStream << "      .IS_CARRYIN_INVERTED(1'b0),        // Optional inversion for CARRYIN\n";
	fStream << "      .IS_CLK_INVERTED(1'b0),            // Optional inversion for CLK\n";
	fStream << "      .IS_INMODE_INVERTED(5'b00000),     // Optional inversion for INMODE\n";
	fStream << "      .IS_OPMODE_INVERTED(9'b000000000), // Optional inversion for OPMODE\n";
	fStream << "      .IS_RSTALLCARRYIN_INVERTED(1'b0),  // Optional inversion for RSTALLCARRYIN\n";
	fStream << "      .IS_RSTALUMODE_INVERTED(1'b0),     // Optional inversion for RSTALUMODE\n";
	fStream << "      .IS_RSTA_INVERTED(1'b0),           // Optional inversion for RSTA\n";
	fStream << "      .IS_RSTB_INVERTED(1'b0),           // Optional inversion for RSTB\n";
	fStream << "      .IS_RSTCTRL_INVERTED(1'b0),        // Optional inversion for RSTCTRL\n";
	fStream << "      .IS_RSTC_INVERTED(1'b0),           // Optional inversion for RSTC\n";
	fStream << "      .IS_RSTD_INVERTED(1'b0),           // Optional inversion for RSTD\n";
	fStream << "      .IS_RSTINMODE_INVERTED(1'b0),      // Optional inversion for RSTINMODE\n";
	fStream << "      .IS_RSTM_INVERTED(1'b0),           // Optional inversion for RSTM\n";
	fStream << "      .IS_RSTP_INVERTED(1'b0),           // Optional inversion for RSTP\n";
	fStream << "      // Register Control Attributes: Pipeline Register Configuration\n";
	fStream << "      .ACASCREG(1),                      // Number of pipeline stages between A/ACIN and ACOUT (0-2)\n";
	fStream << "      .ADREG(1),                         // Pipeline stages for pre-adder (0-1)\n";
	fStream << "      .ALUMODEREG(1),                    // Pipeline stages for ALUMODE (0-1)\n";
	fStream << "      .AREG(1),                          // Pipeline stages for A (0-2)\n";
	fStream << "      .BCASCREG(1),                      // Number of pipeline stages between B/BCIN and BCOUT (0-2)\n";
	fStream << "      .BREG(1),                          // Pipeline stages for B (0-2)\n";
	fStream << "      .CARRYINREG(1),                    // Pipeline stages for CARRYIN (0-1)\n";
	fStream << "      .CARRYINSELREG(1),                 // Pipeline stages for CARRYINSEL (0-1)\n";
	fStream << "      .CREG(1),                          // Pipeline stages for C (0-1)\n";
	fStream << "      .DREG(1),                          // Pipeline stages for D (0-1)\n";
	fStream << "      .INMODEREG(1),                     // Pipeline stages for INMODE (0-1)\n";
	fStream << "      .MREG(1),                          // Multiplier pipeline stages (0-1)\n";
	fStream << "      .OPMODEREG(1),                     // Pipeline stages for OPMODE (0-1)\n";
	fStream << "      .PREG(1)                           // Number of pipeline stages for P (0-1)\n";
	fStream << "   ) i_DSP48E2 (\n";
	fStream << "      // Cascade outputs: Cascade Ports\n";
	fStream << "      .ACOUT(),                         // 30-bit output: A port cascade\n";
	fStream << "      .BCOUT(),                         // 18-bit output: B cascade\n";
	fStream << "      .CARRYCASCOUT(),                  // 1-bit output: Cascade carry\n";
	fStream << "      .MULTSIGNOUT(),                   // 1-bit output: Multiplier sign cascade\n";
	fStream << "      .PCOUT(),                         // 48-bit output: Cascade output\n";
	fStream << "      // Control outputs: Control Inputs/Status Bits\n";
	fStream << "      .OVERFLOW(),                      // 1-bit output: Overflow in add/acc\n";
	fStream << "      .PATTERNBDETECT(),                // 1-bit output: Pattern bar detect\n";
	fStream << "      .PATTERNDETECT(),                 // 1-bit output: Pattern detect\n";
	fStream << "      .UNDERFLOW(),                     // 1-bit output: Underflow in add/acc\n";
	fStream << "      // Data outputs: Data Ports\n";
	fStream << "      .CARRYOUT(),                     	// 4-bit output: Carry\n";
	fStream << "      .P(dsp48e_result_ps9),           	// 48-bit output: Primary data\n";
	fStream << "      .XOROUT(),                     	// 8-bit output: XOR data\n";
	fStream << "      // Cascade inputs: Cascade Ports\n";
	fStream << "      .ACIN(30'b0),                     // 30-bit input: A cascade data\n";
	fStream << "      .BCIN(18'b0),                     // 18-bit input: B cascade\n";
	fStream << "      .CARRYCASCIN(1'b0),               // 1-bit input: Cascade carry\n";
	fStream << "      .MULTSIGNIN(1'b0),                // 1-bit input: Multiplier sign cascade\n";
	fStream << "      .PCIN(48'b0),                     // 48-bit input: P cascade\n";
	fStream << "      // Control inputs: Control Inputs/Status Bits\n";
	fStream << "      .ALUMODE(dsp48e_alumode_ps7),        	// 4-bit input: ALU control\n";
	fStream << "      .CARRYINSEL(dsp48e_carryinsel),  	// 3-bit input: Carry select\n";
	fStream << "      .CLK(iClk),                      	// 1-bit input: Clock\n";
	fStream << "      .INMODE(dsp48e_inmode_ps5),      	// 5-bit input: INMODE control\n";
	fStream << "      .OPMODE(dsp48e_opmode_ps7),      	// 9-bit input: Operation mode\n";
	fStream << "      // Data inputs: Data Ports\n";
	fStream << "      .A({12'b0, coefBuff_rddata}),  	// 30-bit input: A data\n";
	fStream << "      .B(dataBuffA0_rddata),          	// 18-bit input: B data\n";
	fStream << "      .C({12'b0, iChainS}),            	// 48-bit input: C data\n";
	fStream << "      .CARRYIN(1'b0),                  	// 1-bit input: Carry-in\n";
	fStream << "      .D({9'b0, dataBuffB0_rddata}),  	// 27-bit input: D data\n";
	fStream << "      // Reset/Clock Enable inputs: Reset/Clock Enable Inputs\n";
	fStream << "      .CEA1(1'b1),                     	// 1-bit input: Clock enable for 1st stage AREG\n";
	fStream << "      .CEA2(1'b1),            			// 1-bit input: Clock enable for 2nd stage AREG\n";
	fStream << "      .CEAD(1'b1),            			// 1-bit input: Clock enable for ADREG\n";
	fStream << "      .CEALUMODE(1'b1),       			// 1-bit input: Clock enable for ALUMODE\n";
	fStream << "      .CEB1(1'b1),            			// 1-bit input: Clock enable for 1st stage BREG\n";
	fStream << "      .CEB2(1'b1),            			// 1-bit input: Clock enable for 2nd stage BREG\n";
	fStream << "      .CEC(1'b1),             			// 1-bit input: Clock enable for CREG\n";
	fStream << "      .CECARRYIN(1'b1),       			// 1-bit input: Clock enable for CARRYINREG\n";
	fStream << "      .CECTRL(1'b1),          			// 1-bit input: Clock enable for OPMODEREG and CARRYINSELREG\n";
	fStream << "      .CED(1'b1),             			// 1-bit input: Clock enable for DREG\n";
	fStream << "      .CEINMODE(1'b1),        			// 1-bit input: Clock enable for INMODEREG\n";
	fStream << "      .CEM(1'b1),             			// 1-bit input: Clock enable for MREG\n";
	fStream << "      .CEP(1'b1),             			// 1-bit input: Clock enable for PREG\n";
	fStream << "      .RSTA(1'b0),            			// 1-bit input: Reset for AREG\n";
	fStream << "      .RSTALLCARRYIN(1'b0),   			// 1-bit input: Reset for CARRYINREG\n";
	fStream << "      .RSTALUMODE(1'b0),      			// 1-bit input: Reset for ALUMODEREG\n";
	fStream << "      .RSTB(1'b0),            			// 1-bit input: Reset for BREG\n";
	fStream << "      .RSTC(1'b0),            			// 1-bit input: Reset for CREG\n";
	fStream << "      .RSTCTRL(1'b0),         			// 1-bit input: Reset for OPMODEREG and CARRYINSELREG\n";
	fStream << "      .RSTD(1'b0),            			// 1-bit input: Reset for DREG and ADREG\n";
	fStream << "      .RSTINMODE(1'b0),       			// 1-bit input: Reset for INMODEREG\n";
	fStream << "      .RSTM(1'b0),                     	// 1-bit input: Reset for MREG\n";
	fStream << "      .RSTP(1'b0)                      	// 1-bit input: Reset for PREG\n";
	fStream << "   );\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "// generate controls for DSP48E2\n";
	fStream << "reg [1:0] preadd_mode_ps5;\n";
	fStream << "reg [1:0] mul_mode_ps7;\n";
	fStream << "reg add_prevengine_accum_ps7;\n";
	fStream << "\n";
	fStream << "always @(posedge iClk)\n";
	fStream << "begin\n";
	fStream << "	preadd_mode_ps5 <= PREADD_MODE >> {timeSlice_ps4, 2'b0};\n";
	fStream << "    mul_mode_ps7 <= MUL_MODE >> {timeSlice_ps6, 2'b0};\n";
	fStream << "    add_prevengine_accum_ps7 <= ADDPREVENGINEACCUM >> timeSlice_ps6;\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "always @(*)\n";
	fStream << "begin\n";
	fStream << "    case (preadd_mode_ps5)\n";
	fStream << "        4'b00: dsp48e_inmode_ps5 <= 5'b10001;	// +B*A\n";
	fStream << "        4'b01: dsp48e_inmode_ps5 <= 5'b10101;	// (D+B)*A\n";
	fStream << "        4'b10: dsp48e_inmode_ps5 <= 5'b11101;	// (D-B)*A\n";
	fStream << "        default: dsp48e_inmode_ps5 <= 0;\n";
	fStream << "    endcase\n";
	fStream << "    case (mul_mode_ps7)\n";
	fStream << "        4'b00: dsp48e_alumode_ps7 <= 4'b0000;	// Add\n";
	fStream << "        4'b01: dsp48e_alumode_ps7 <= 4'b0000;	// Add\n";
	fStream << "        4'b10: dsp48e_alumode_ps7 <= 4'b0011;	// Subtract\n";
	fStream << "        default: dsp48e_alumode_ps7 <= 0;\n";
	fStream << "   endcase\n";
	fStream << "    case ({add_prevengine_accum_ps7, mul_mode_ps7})\n";
	fStream << "        4'b000: dsp48e_opmode_ps7 <= 9'b000000101;		// W=0 Z=0\n";
	fStream << "        4'b001: dsp48e_opmode_ps7 <= 9'b000100101;		// W=0 Z=P\n";
	fStream << "        4'b010: dsp48e_opmode_ps7 <= 9'b000100101;		// W=0 Z=P\n";
	fStream << "        4'b100: dsp48e_opmode_ps7 <= 9'b110000101;		// W=SUMIN Z=0\n";
	fStream << "        4'b101: dsp48e_opmode_ps7 <= 9'b110100101;		// W=SUMIN Z=P\n";
	fStream << "        4'b110: dsp48e_opmode_ps7 <= 9'b110100101;		// W=SUMIN Z=P\n";
	fStream << "        default: dsp48e_opmode_ps7 <= 0;\n";
	fStream << "   endcase\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "// Mac Chain\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "assign oChainS = dsp48e_result_ps9[35:0];\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "// Data Outputs\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "wire [17:0] dspout_ps9 = dsp48e_result_ps9[33:16];\n";
	fStream << "wire dspoutchanged_ps9 = commit_ps9;\n";
	fStream << "\n";
	fStream << "reg [3:0] channelSel_ps9;\n";
	fStream << "always @(posedge iClk)\n";
	fStream << "begin\n";
	fStream << "    channelSel_ps9 <= CHANNEL_SELECT >> {timeSlice_ps8, 2'b0};\n";
	fStream << "end\n";
	fStream << "\n";
	for (unsigned i = 0; i < m_vOutputFirs.size(); ++i)
		fStream << "reg dataOut" << i << "Changed;\n";
	fStream << "\n";
	fStream << "always @(posedge iClk or posedge iRst)\n";
	fStream << "begin\n";
	fStream << "    if (iRst) begin\n";
	for (unsigned i = 0; i < m_vOutputFirs.size(); ++i)
	{
		fStream << "        oData" << i << " <= 0;\n";
		fStream << "        dataOut" << i << "Changed <= 0;\n";
		fStream << "        oData" << i << "Changed <= 0;\n";
	}
	fStream << "    end else begin\n";
	for (unsigned i = 0; i < m_vOutputFirs.size(); ++i)
	{
		fStream << "       dataOut" << i << "Changed <= 0;\n";
	}
	fStream << "       case (channelSel_ps9)\n";
	for (unsigned i = 0; i < m_vOutputFirs.size(); ++i)
	{
		fStream << "            " << i << ": begin oData" << i << " <= dspout_ps9; dataOut" << i << "Changed <= dspoutchanged_ps9; end\n";
	}
	fStream << "            default: ;\n";
	fStream << "       endcase\n";
	fStream << "       // 'Changed' signal is delayed by 1 cycle from data, and converted from a 'level' to an 'edge'\n";
	fStream << "       // This ensures the edge occurs when the data value is stable, and allows the data signal to safely cross clock domains\n";
	for (unsigned i = 0; i < m_vOutputFirs.size(); ++i)
	{
		fStream << "       if (dataOut" << i << "Changed) \n";
		fStream << "            oData" << i << "Changed <= ~oData" << i << "Changed;\n";
	}
	fStream << "   end\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "// Fifo Description Rams (A and B hold the same contents)\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "reg [LOG2NUMFIFOS-1:0] fifoDescBuffA_rdaddr;\n";
	fStream << "reg [LOG2NUMFIFOS-1:0] fifoDescBuffB_rdaddr;\n";
	fStream << "reg [LOG2NUMFIFOS-1:0] fifoDescBuff_wraddr;\n";
	fStream << "wire [15:0] fifoDescBuff_wrdata;\n";
	fStream << "wire fifoDescBuff_wren;\n";
	fStream << "\n";
	fStream << "reg [15:0] fifoDescBuffA_rddata;\n";
	fStream << "reg [15:0] fifoDescBuffB_rddata;\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "reg [15:0] fifoDescBuffA_rddata_int = 0;\n";
	fStream << "reg [15:0] fifoDescBuffA_contents[(1 << LOG2NUMFIFOS)-1:0];\n";
	fStream << "\n";
	fStream << "initial\n";
	fStream << "begin\n";
	fStream << "	for (i = 0; i < NUMFIFOS; i = i + 1)\n";
	fStream << "		fifoDescBuffA_contents[i] <= FIFOSIZES >> (16 * i);\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "always @(posedge iClk)\n";
	fStream << "begin\n";
	fStream << "    if (fifoDescBuff_wren)\n";
	fStream << "    	fifoDescBuffA_contents[fifoDescBuff_wraddr] <= fifoDescBuff_wrdata;\n";
	fStream << "    fifoDescBuffA_rddata_int <= fifoDescBuffA_contents[fifoDescBuffA_rdaddr];\n";
	fStream << "  	fifoDescBuffA_rddata <= fifoDescBuffA_rddata_int;\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "reg [15:0] fifoDescBuffB_rddata_int = 0;\n";
	fStream << "reg [15:0] fifoDescBuffB_contents[(1 << LOG2NUMFIFOS)-1:0];\n";
	fStream << "\n";
	fStream << "initial\n";
	fStream << "begin\n";
	fStream << "	for (i = 0; i < NUMFIFOS; i = i + 1)\n";
	fStream << "		fifoDescBuffB_contents[i] <= FIFOSIZES >> (16 * i);\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "always @(posedge iClk)\n";
	fStream << "begin\n";
	fStream << "    if (fifoDescBuff_wren)\n";
	fStream << "    	fifoDescBuffB_contents[fifoDescBuff_wraddr] <= fifoDescBuff_wrdata;\n";
	fStream << "    fifoDescBuffB_rddata_int <= fifoDescBuffB_contents[fifoDescBuffB_rdaddr];\n";
	fStream << "  	fifoDescBuffB_rddata <= fifoDescBuffB_rddata_int;\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "// Fifo Controls\n";
	fStream << "///////////////////////////////////////////////////////////////\n";
	fStream << "reg firstTap_ps2;\n";
	fStream << "\n";
	fStream << "always @(posedge iClk) \n";
	fStream << "begin\n";
	fStream << "    fifoDescBuffA_rdaddr <= RDFIFONUM >> {timeSlice_psm1, 3'b0};\n";
	fStream << "    fifoDescBuffB_rdaddr <= UPDATEFIFONUM >> {timeSlice_psm1, 3'b0};\n";
	fStream << "    fifoDescBuff_wraddr <= UPDATEFIFONUM >> {timeSlice_ps2, 3'b0};\n";
	fStream << "    firstTap_ps2 <= FIRST_TAP >> timeSlice_ps1;\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "assign fifoDescBuff_wren = commit_ps3;\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "wire [9:0] currFifoOffset = fifoDescBuffA_rddata[15:6];\n";
	fStream << "wire [9:0] currFifoLengthMinusOne = {4'b0, fifoDescBuffA_rddata[5:0]};\n";
	fStream << "reg  [9:0] currFifoRegion;        // lowest power of 2 >= currFifoLengthMinusOne\n";
	fStream << "wire [9:0] currFifoRegionOrigin = currFifoOffset & ~currFifoRegion;\n";
	fStream << "\n";
	fStream << "always @(currFifoLengthMinusOne) begin\n";
	fStream << "    if (currFifoLengthMinusOne[5])\n";
	fStream << "        currFifoRegion = 10'b0000111111;\n";
	fStream << "    else if (currFifoLengthMinusOne[4])\n";
	fStream << "        currFifoRegion = 10'b0000011111;\n";
	fStream << "    else if (currFifoLengthMinusOne[3])\n";
	fStream << "        currFifoRegion = 10'b0000001111;\n";
	fStream << "    else if (currFifoLengthMinusOne[2])\n";
	fStream << "        currFifoRegion = 10'b0000000111;\n";
	fStream << "    else if (currFifoLengthMinusOne[1])\n";
	fStream << "        currFifoRegion = 10'b0000000011;\n";
	fStream << "    else\n";
	fStream << "        currFifoRegion = 10'b0000000001;\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "// The coefficient buffer is always read from the Fifo-origin address\n";
	fStream << "always @(posedge iClk) \n";
	fStream << "begin\n";
	fStream << "    if (firstTap_ps2) begin\n";
	fStream << "        coefBuff_rdaddr <= currFifoRegionOrigin;            // Start of Fifo Region\n";
	fStream << "        dataBuffA0_rdaddr <= currFifoOffset;\n";
	fStream << "        dataBuffB0_rdaddr <= currFifoRegionOrigin | ((currFifoOffset - currFifoLengthMinusOne) & currFifoRegion);\n";
	fStream << "    end else begin\n";
	fStream << "        coefBuff_rdaddr <= coefBuff_rdaddr + 1;\n";
	fStream << "        dataBuffA0_rdaddr <= currFifoRegionOrigin | ((dataBuffA0_rdaddr - 1) & currFifoRegion);\n";
	fStream << "        dataBuffB0_rdaddr <= currFifoRegionOrigin | ((dataBuffB0_rdaddr + 1) & currFifoRegion);\n";
	fStream << "    end\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "wire [9:0] updateFifoOffset = fifoDescBuffB_rddata[15:6];\n";
	fStream << "wire [9:0] updateFifoLengthMinusOne = {4'b0, fifoDescBuffB_rddata[5:0]};\n";
	fStream << "reg  [9:0] updateFifoRegion;        // lowest power of 2 >= currFifoLengthMinusOne\n";
	fStream << "wire [9:0] updateFifoRegionOrigin = updateFifoOffset & ~updateFifoRegion;\n";
	fStream << "\n";
	fStream << "always @(updateFifoLengthMinusOne) begin\n";
	fStream << "    if (updateFifoLengthMinusOne[5])\n";
	fStream << "        updateFifoRegion = 10'b0000111111;\n";
	fStream << "    else if (updateFifoLengthMinusOne[4])\n";
	fStream << "        updateFifoRegion = 10'b0000011111;\n";
	fStream << "    else if (updateFifoLengthMinusOne[3])\n";
	fStream << "        updateFifoRegion = 10'b0000001111;\n";
	fStream << "    else if (updateFifoLengthMinusOne[2])\n";
	fStream << "        updateFifoRegion = 10'b0000000111;\n";
	fStream << "    else if (updateFifoLengthMinusOne[1])\n";
	fStream << "        updateFifoRegion = 10'b0000000011;\n";
	fStream << "    else\n";
	fStream << "        updateFifoRegion = 10'b0000000001;\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "// The coefficient buffer is always read from the Fifo-origin address\n";
	fStream << "reg [9:0] dataBuffAB1UpdateAddr;\n";
	fStream << "reg [9:0] dataBuffAB1NextAddr;\n";
	fStream << "reg [9:0] dataBuffAB1FifoLengthMinusOne;\n";
	fStream << "reg [9:0] dataBuffAB1NextAddr_delay1;\n";
	fStream << "reg [9:0] dataBuffAB1FifoLengthMinusOne_delay1;\n";
	fStream << "reg [9:0] dataBuffAB1NextAddr_delay2;\n";
	fStream << "reg [9:0] dataBuffAB1FifoLengthMinusOne_delay2;\n";
	fStream << "\n";
	fStream << "always @(posedge iClk)\n";
	fStream << "begin\n";
	fStream << "    if (doUpdate_ps2)           // Update Data in DataBuffers\n";
	fStream << "        dataBuffAB1UpdateAddr <= dataBuffAB1NextAddr_delay2;\n";
	fStream << "    else                    // Read Data about to be popped from DataBuffers\n";
	fStream << "        dataBuffAB1UpdateAddr <= updateFifoRegionOrigin | ((updateFifoOffset - updateFifoLengthMinusOne) & updateFifoRegion);\n";
	fStream << "\n";
	fStream << "    // this is the address in the circular buffer where the next data entry should go (push)\n";
	fStream << "    dataBuffAB1NextAddr <= currFifoRegionOrigin | ((currFifoOffset + 1) & currFifoRegion);\n";
	fStream << "    dataBuffAB1FifoLengthMinusOne <= currFifoLengthMinusOne;\n";
	fStream << "\n";
	fStream << "    dataBuffAB1NextAddr_delay1 <= dataBuffAB1NextAddr;\n";
	fStream << "    dataBuffAB1FifoLengthMinusOne_delay1 <= dataBuffAB1FifoLengthMinusOne;\n";
	fStream << "    dataBuffAB1NextAddr_delay2 <= dataBuffAB1NextAddr_delay1;\n";
	fStream << "    dataBuffAB1FifoLengthMinusOne_delay2 <= dataBuffAB1FifoLengthMinusOne_delay1;\n";
	fStream << "end\n";
	fStream << "\n";
	fStream << "assign dataBuffA1_rwaddr = dataBuffAB1UpdateAddr;\n";
	fStream << "assign dataBuffB1_rwaddr = dataBuffAB1UpdateAddr;\n";
	fStream << "assign fifoDescBuff_wrdata = { dataBuffAB1NextAddr_delay2, dataBuffAB1FifoLengthMinusOne_delay2[5:0] };\n";
	fStream << "\n";
	fStream << "\n";
	fStream << "endmodule\n";
}
