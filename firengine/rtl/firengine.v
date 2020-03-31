`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Design Name: Fir Engine
// Module Name: firengine
// Project Name:
// Target Devices:
// Tool Versions:
// Description:
//
// Dependencies:
//
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
//
//////////////////////////////////////////////////////////////////////////////////


//
//  All 18-bit data is in the 2.16 format:
//    -(2^1) 2^0 2^-1 2^-2 2^-3... 2^-16
//     Sign      <- 16-fractional bits ->
//
//    0x00000 - represents 0
//    0x10000 - represents +1
//    0x30000 - represents -1
//    0x08000 - represents +1/2
//    0x38000 - represents -1/2
//
// To avoid filter-saturation, ensure that:
//    Sum(abs(coeff)) <= 1.0
//
module firengine (
    // Common Timeslice-Clock and Reset for all FIRs
    input             iClk,
    input             iRst,

    // Each Channel has a seperate (data-input, data-changed) pair
    //   Data-Changed is flipped every time a new sample arrives
    input             iData0Changed,
    input [17:0]      iData0,
    input             iData1Changed,
    input [17:0]      iData1,
    input             iData2Changed,
    input [17:0]      iData2,
    input             iData3Changed,
    input [17:0]      iData3,
    input             iData4Changed,
    input [17:0]      iData4,

    // Each Channel has a seperate (data-output, data-changed) pair
    //   Data-Changed is flipped every time a new sample appears
    output reg        				oData0Changed,
    output reg [17:0]   			oData0,
    output reg          			oData1Changed,
    output reg [17:0]   			oData1,
    output reg          			oData2Changed,
    output reg [17:0]   			oData2,
    output reg          			oData3Changed,
    output reg [17:0]   			oData3,
    output reg          			oData4Changed,
    output reg [17:0]   			oData4,

    /// The following signals are used to chain FirEngines together
    input [17:0]      				iChainD,
    output [17:0]     				oChainD,
    input [17:0]      				iChainR,
    output [17:0]     				oChainR,
    input [35:0]      				iChainS,
    output [35:0]     				oChainS,
    input                           iInputChangeChain,
    output                          oInputChangeChain,

 	// Interface to write to Coefficient Memory
 	input							iCoefBuff_wren,
   	input [LOG2BUFFERDEPTH-1:0]		iCoefBuff_wraddr,
   	input [17:0]	  				iCoefBuff_wrdata
    );


parameter LOG2BUFFERDEPTH = 10;
parameter BUFFERDEPTH = 1024;

parameter LOG2NUMFIFOS = 8;
parameter NUMFIFOS = 256;

parameter LOG2TIMESLICES = 5;      
parameter TIMESLICES = 32;          // Maximum of 64 currently supported


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FirEngine Configuration
//   CHANNEL_SELECT			4 bits	- Selects which Input channel to use in this timeSlot (0 = None) This should only be non-zero during Fir Update-Write cycle
//   FIRST_ENGINE			1 bit	- '1' when updating the first FirEngine in a chain. Valid on DOUPDATE cycle
//   LAST_ENGINE			1 bit	- '1' when updating the last FirEngine in a chain. Valid on DOUPDATE cycle
//   FIRST_TAP     			1 bit	- '1' for first tap of the section of FIR in this FirEngine 
//   PREADD_MODE			4 bits	- 0=NoPreadder(B*A), 1=PreAdd((D+B)*A), 2=PreSub((D-B)*A)
//   MUL_MODE				4 bits	- 0=MUL, 1=MADD, 2=MSUB
//   ADDPREVENGINEACCUM 	1 bit   - Use value of previous Fir Engine's Accumulator from 2-cycles ago
//   RDFIFONUM  			8 bits	- Selects which Data Fifo to use
//   UPDATEFIFONUM			8 bits	- Selects which Data Fifo to update (valid on doUpdate cycle and 3 cycles earlier)
//   DOUPDATE	      		1 bits	- High at the same time on all FirEngines involved in a firfilter for the last tap of the fir-filter
//
//   FIFOSIZES      		16 bits	- Foreach Fifo [FifoOffset : 10 bits, Len-1: 6 bits]
//
//   COEFF_VALUES           24 bits for every slot in Coeff-Buffer
//
//                        	  Slot:    31 30 29 28  27 26 25 24  23 22 21 20  19 18 17 16  15 14 13 12  11 10  9  8   7  6  5  4   3  2  1  0
parameter CHANNEL_SELECT		= 128'h_0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0;
parameter FIRST_ENGINE	 		=  32'b_0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0;
parameter LAST_ENGINE 		    =  32'b_0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0;
parameter FIRST_TAP		        =  32'b_0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0;
parameter PREADD_MODE			= 128'h_0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0;
parameter MUL_MODE				= 128'h_0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0;
parameter ADDPREVENGINEACCUM    =  32'b_0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0;
parameter RDFIFONUM 			= 512'h00_00_00_00__00_00_00_00__00_00_00_00__00_00_00_00__00_00_00_00__00_00_00_00__00_00_00_00__00_00_00_00;
parameter UPDATEFIFONUM      	= 512'h00_00_00_00__00_00_00_00__00_00_00_00__00_00_00_00__00_00_00_00__00_00_00_00__00_00_00_00__00_00_00_00;
parameter DOUPDATE		        =  32'b_0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0;

parameter FIFOSIZES 			= {NUMFIFOS{16'h0}};
parameter COEFF_VALUES 			= {BUFFERDEPTH{24'h0}};



integer i;

///////////////////////////////////////////////////////////////
// TimeSliceCounter at different pipeline stages
//   (timeSlice_psN is the time-slice value that can be read in pipeline stage N)
///////////////////////////////////////////////////////////////
reg [LOG2TIMESLICES-1:0] timeSlice_psm1 = 0;
reg [LOG2TIMESLICES-1:0] timeSlice_ps0 = 0;
reg [LOG2TIMESLICES-1:0] timeSlice_ps1 = 0;
reg [LOG2TIMESLICES-1:0] timeSlice_ps2 = 0;
reg [LOG2TIMESLICES-1:0] timeSlice_ps3 = 0;
reg [LOG2TIMESLICES-1:0] timeSlice_ps4 = 0;
reg [LOG2TIMESLICES-1:0] timeSlice_ps5 = 0;
reg [LOG2TIMESLICES-1:0] timeSlice_ps6 = 0;
reg [LOG2TIMESLICES-1:0] timeSlice_ps7 = 0;
reg [LOG2TIMESLICES-1:0] timeSlice_ps8 = 0;
reg [LOG2TIMESLICES-1:0] timeSlice_ps9 = 0;

// Generate a time-slice counters that will be used throughout the design
always @(posedge iClk or posedge iRst)
begin
    if (iRst) begin
        timeSlice_psm1 <= 0;
        timeSlice_ps0 <= 0;
        timeSlice_ps1 <= 0;
        timeSlice_ps2 <= 0;
        timeSlice_ps3 <= 0;
        timeSlice_ps4 <= 0;
        timeSlice_ps5 <= 0;
        timeSlice_ps6 <= 0;
        timeSlice_ps7 <= 0;
        timeSlice_ps8 <= 0;
        timeSlice_ps9 <= 0;
      end else begin
        timeSlice_psm1 <= timeSlice_psm1 + 1;
        timeSlice_ps0 <= timeSlice_psm1;
        timeSlice_ps1 <= timeSlice_ps0;
        timeSlice_ps2 <= timeSlice_ps1;
        timeSlice_ps3 <= timeSlice_ps2;
        timeSlice_ps4 <= timeSlice_ps3;
        timeSlice_ps5 <= timeSlice_ps4;
        timeSlice_ps6 <= timeSlice_ps5;
        timeSlice_ps7 <= timeSlice_ps6;
        timeSlice_ps8 <= timeSlice_ps7;
        timeSlice_ps9 <= timeSlice_ps8;
    end
end



///////////////////////////////////////////////////////////////
// latch all input data as it arrives
//   note that data could arrive from multiple channels during the same clock cycle
///////////////////////////////////////////////////////////////
// These registers are used to latch input samples when they change
reg [17:0] data0_ps1 = 0;
reg [17:0] data1_ps1 = 0;
reg [17:0] data2_ps1 = 0;
reg [17:0] data3_ps1 = 0;
reg [17:0] data4_ps1 = 0;

reg data0Changed_ps1 = 0;
reg data1Changed_ps1 = 0;
reg data2Changed_ps1 = 0;
reg data3Changed_ps1 = 0;
reg data4Changed_ps1 = 0;

always @(posedge iClk or posedge iRst)
begin
    if (iRst) begin
        data0_ps1 <= 0;
        data1_ps1 <= 0;
        data2_ps1 <= 0;
        data3_ps1 <= 0;
        data4_ps1 <= 0;
        data0Changed_ps1 <= 0;
        data1Changed_ps1 <= 0;
        data2Changed_ps1 <= 0;
        data3Changed_ps1 <= 0;
        data4Changed_ps1 <= 0;
    end else begin
        // Only latch new data when it changes
        if (prevData0Changed != iData0Changed)
            data0_ps1 <= iData0;
        if (prevData1Changed != iData1Changed)
            data1_ps1 <= iData1;
        if (prevData2Changed != iData2Changed)
            data2_ps1 <= iData2;
        if (prevData3Changed != iData3Changed)
            data3_ps1 <= iData3;
        if (prevData4Changed != iData4Changed)
            data4_ps1 <= iData4;
        data0Changed_ps1 <= iData0Changed;
        data1Changed_ps1 <= iData1Changed;
        data2Changed_ps1 <= iData2Changed;
        data3Changed_ps1 <= iData3Changed;
        data4Changed_ps1 <= iData4Changed;
   end
end


///////////////////////////////////////////////////////////////
// select one of the channels
//   only when a channel is selected is the prevDataChanged flag updated to reflect the current dataChanged flag
//   this ensures that the same data channel value is not used again until it has changed.
///////////////////////////////////////////////////////////////
reg [17:0] chosenData_ps2;
reg chosenDataChanged_ps2;

reg [2:0] channelSel_ps1;
always @(posedge iClk)
begin
    channelSel_ps1 <= CHANNEL_SELECT >> {timeSlice_ps0, 2'b0};
end

reg prevData0Changed = 0;
reg prevData1Changed = 0;
reg prevData2Changed = 0;
reg prevData3Changed = 0;
reg prevData4Changed = 0;

always @(posedge iClk or posedge iRst)
begin
    if (iRst) begin
        chosenData_ps2 <= 0;
        chosenDataChanged_ps2 <= 0;
        prevData0Changed <= 0;
        prevData1Changed <= 0;
        prevData2Changed <= 0;
        prevData3Changed <= 0;
        prevData4Changed <= 0;
    end else begin
        case (channelSel_ps1)
            4'b0001: begin chosenData_ps2 <= data0_ps1; chosenDataChanged_ps2 <= data0Changed_ps1 ^ prevData0Changed; prevData0Changed <= data0Changed_ps1; end
            4'b0010: begin chosenData_ps2 <= data1_ps1; chosenDataChanged_ps2 <= data1Changed_ps1 ^ prevData1Changed; prevData1Changed <= data1Changed_ps1; end
            4'b0011: begin chosenData_ps2 <= data2_ps1; chosenDataChanged_ps2 <= data2Changed_ps1 ^ prevData2Changed; prevData2Changed <= data2Changed_ps1; end
            4'b0100: begin chosenData_ps2 <= data3_ps1; chosenDataChanged_ps2 <= data3Changed_ps1 ^ prevData3Changed; prevData3Changed <= data3Changed_ps1; end
            4'b0101: begin chosenData_ps2 <= data4_ps1; chosenDataChanged_ps2 <= data4Changed_ps1 ^ prevData4Changed; prevData4Changed <= data4Changed_ps1; end
            default: begin chosenData_ps2 <= 18'hx; chosenDataChanged_ps2 <= 1'b0; end
        endcase
   end
end

// FIR Data-Fifos are only updated when a Channel is selected
//   Note: DoUpdate must be disabled for a few cycles after reset as it can only be enabled 3-cycles after a valid UpdateFifoNum
reg doUpdate_ps2;
reg [1:0] doUpdate_EnableCounter;
always @(posedge iClk or posedge iRst)
begin
    if (iRst) begin
        doUpdate_ps2 <= 1'b0;
        doUpdate_EnableCounter <= 2'b0;
    end else begin
        doUpdate_ps2 <= (DOUPDATE >> timeSlice_ps1);
        if (doUpdate_EnableCounter != 2'b11) begin
            doUpdate_EnableCounter <= doUpdate_EnableCounter + 1;
            doUpdate_ps2 <= 1'b0;           // force doUpdate to be disabled 
        end
    end
end


///////////////////////////////////////////////////////////////
// create the Coefficient RAM
///////////////////////////////////////////////////////////////
reg [LOG2BUFFERDEPTH-1:0] coefBuff_rdaddr;
reg [17:0] coefBuff_rddata = 0;
reg [17:0] coefBuff_rddata_int = 0;
reg [17:0] coefBuff_contents[(1 << LOG2BUFFERDEPTH)-1:0];

initial 
begin
	for (i = 0; i < (1 << LOG2BUFFERDEPTH); i = i + 1)
		coefBuff_contents[i] <= COEFF_VALUES >> (24 * i);
end

always @(posedge iClk) 
begin
    if (iCoefBuff_wren)
    	coefBuff_contents[iCoefBuff_wraddr] <= iCoefBuff_wrdata;
    coefBuff_rddata_int <= coefBuff_contents[coefBuff_rdaddr];
  	coefBuff_rddata <= coefBuff_rddata_int;
end


///////////////////////////////////////////////////////////////
// declare dataBuff RAMs
///////////////////////////////////////////////////////////////
reg [17:0] dataBuffA0_rddata;
reg [17:0] dataBuffA1_rddata;
reg [17:0] dataBuffA1_wrdata;
reg [LOG2BUFFERDEPTH-1:0] dataBuffA0_rdaddr;
wire [LOG2BUFFERDEPTH-1:0] dataBuffA1_rwaddr;
wire dataBuffA1_wren;

reg [17:0] dataBuffA0_rddata_int = 0;
reg [17:0] dataBuffA1_rddata_int = 0;
reg [17:0] dataBuffA_contents[(1 << LOG2BUFFERDEPTH)-1:0];

initial 
begin
	for (i = 0; i < (1 << LOG2BUFFERDEPTH); i = i + 1)
		dataBuffA_contents[i] <= 0;
end

always @(posedge iClk) 
begin
    if (dataBuffA1_wren)
    	dataBuffA_contents[dataBuffA1_rwaddr[LOG2BUFFERDEPTH-1:0]] <= dataBuffA1_wrdata;
    dataBuffA1_rddata_int <= dataBuffA_contents[dataBuffA1_rwaddr[LOG2BUFFERDEPTH-1:0]];
  	dataBuffA1_rddata <= dataBuffA1_rddata_int;
    dataBuffA0_rddata_int <= dataBuffA_contents[dataBuffA0_rdaddr[LOG2BUFFERDEPTH-1:0]];
    dataBuffA0_rddata <= dataBuffA0_rddata_int;
end



reg [17:0] dataBuffB0_rddata;           
reg [17:0] dataBuffB1_rddata;           
reg [17:0] dataBuffB1_wrdata;         
reg [LOG2BUFFERDEPTH-1:0] dataBuffB0_rdaddr;
wire [LOG2BUFFERDEPTH-1:0] dataBuffB1_rwaddr;
wire dataBuffB1_wren;

reg [17:0] dataBuffB0_rddata_int = 0;
reg [17:0] dataBuffB1_rddata_int = 0;
reg [17:0] dataBuffB_contents[(1 << LOG2BUFFERDEPTH)-1:0];

initial 
begin
	for (i = 0; i < (1 << LOG2BUFFERDEPTH); i = i + 1)
		dataBuffB_contents[i] <= 0;
end

always @(posedge iClk) 
begin
    if (dataBuffB1_wren)
    	dataBuffB_contents[dataBuffB1_rwaddr[LOG2BUFFERDEPTH-1:0]] <= dataBuffB1_wrdata;
    dataBuffB1_rddata_int <= dataBuffB_contents[dataBuffB1_rwaddr[LOG2BUFFERDEPTH-1:0]];
  	dataBuffB1_rddata <= dataBuffB1_rddata_int;
    dataBuffB0_rddata_int <= dataBuffB_contents[dataBuffB0_rdaddr[LOG2BUFFERDEPTH-1:0]];
    dataBuffB0_rddata <= dataBuffB0_rddata_int;
end


///////////////////////////////////////////////////////////////
// mux in data to update dataBuffs (using Ram Port 1)
///////////////////////////////////////////////////////////////
reg firstEngine_ps2;
reg lastEngine_ps2;

always @(posedge iClk) begin
    firstEngine_ps2 <= FIRST_ENGINE[timeSlice_ps1];
    lastEngine_ps2 <= LAST_ENGINE[timeSlice_ps1];
end

always @(posedge iClk) begin
    dataBuffA1_wrdata <= firstEngine_ps2 ? chosenData_ps2 : iChainD;
    dataBuffB1_wrdata <= lastEngine_ps2 ? dataBuffA1_rddata : iChainR;
end

assign oChainD = dataBuffA1_rddata;
assign oChainR = dataBuffB1_rddata;



///////////////////////////////////////////////////////////////
// data Commit chain (commit changes)
///////////////////////////////////////////////////////////////
reg commit_ps3;
reg commit_ps4;
reg commit_ps5;
reg commit_ps6;
reg commit_ps7;
reg commit_ps8;
reg commit_ps9;

wire doCommit_ps2;
assign doCommit_ps2 = firstEngine_ps2 ? chosenDataChanged_ps2 : (chosenDataChanged_ps2 | iInputChangeChain);
assign oInputChangeChain = doCommit_ps2;

always @(posedge iClk or posedge iRst)
begin
    if (iRst) begin
        commit_ps3 <= 0;
        commit_ps4 <= 0;
        commit_ps5 <= 0;
        commit_ps6 <= 0;
        commit_ps7 <= 0;
        commit_ps8 <= 0;
        commit_ps9 <= 0;
    end else begin
        commit_ps3 <= 1'b0;
        commit_ps4 <= commit_ps3;
        commit_ps5 <= commit_ps4;
        commit_ps6 <= commit_ps5;
        commit_ps7 <= commit_ps6;
        commit_ps8 <= commit_ps7;
        commit_ps9 <= commit_ps8;
        if (doUpdate_ps2) begin
            commit_ps3 <= doCommit_ps2;
        end
   end
end

assign dataBuffA1_wren = commit_ps3;
assign dataBuffB1_wren = commit_ps3;


///////////////////////////////////////////////////////////////
// instantiate the DSP48E2
///////////////////////////////////////////////////////////////
wire [47:0] dsp48e_result_ps9;
reg [3:0] dsp48e_alumode_ps7;
wire [2:0] dsp48e_carryinsel = 3'b0;		// Carry from CARRYIN
reg [4:0] dsp48e_inmode_ps5;
reg [8:0] dsp48e_opmode_ps7;

DSP48E2 #(
      // Feature Control Attributes: Data Path Selection
      .AMULTSEL("A"),                    // Selects A input to multiplier (A, AD)
      .A_INPUT("DIRECT"),                // Selects A input source, "DIRECT" (A port) or "CASCADE" (ACIN port)
      .BMULTSEL("AD"),                   // Selects B input to multiplier (AD, B)
      .B_INPUT("DIRECT"),                // Selects B input source, "DIRECT" (B port) or "CASCADE" (BCIN port)
      .PREADDINSEL("B"),                 // Selects input to preadder (A, B)
      .RND(48'h000000000000),            // Rounding Constant
      .USE_MULT("MULTIPLY"),             // Select multiplier usage (DYNAMIC, MULTIPLY, NONE)
      .USE_SIMD("ONE48"),                // SIMD selection (FOUR12, ONE48, TWO24)
      .USE_WIDEXOR("FALSE"),             // Use the Wide XOR function (FALSE, TRUE)
      .XORSIMD("XOR24_48_96"),           // Mode of operation for the Wide XOR (XOR12, XOR24_48_96)
      // Pattern Detector Attributes: Pattern Detection Configuration
      .AUTORESET_PATDET("NO_RESET"),     // NO_RESET, RESET_MATCH, RESET_NOT_MATCH
      .AUTORESET_PRIORITY("RESET"),      // Priority of AUTORESET vs.CEP (CEP, RESET).
      .MASK(48'h3fffffffffff),           // 48-bit mask value for pattern detect (1=ignore)
      .PATTERN(48'h000000000000),        // 48-bit pattern match for pattern detect
      .SEL_MASK("MASK"),                 // C, MASK, ROUNDING_MODE1, ROUNDING_MODE2
      .SEL_PATTERN("PATTERN"),           // Select pattern value (C, PATTERN)
      .USE_PATTERN_DETECT("NO_PATDET"),  // Enable pattern detect (NO_PATDET, PATDET)
      // Programmable Inversion Attributes: Specifies built-in programmable inversion on specific pins
      .IS_ALUMODE_INVERTED(4'b0000),     // Optional inversion for ALUMODE
      .IS_CARRYIN_INVERTED(1'b0),        // Optional inversion for CARRYIN
      .IS_CLK_INVERTED(1'b0),            // Optional inversion for CLK
      .IS_INMODE_INVERTED(5'b00000),     // Optional inversion for INMODE
      .IS_OPMODE_INVERTED(9'b000000000), // Optional inversion for OPMODE
      .IS_RSTALLCARRYIN_INVERTED(1'b0),  // Optional inversion for RSTALLCARRYIN
      .IS_RSTALUMODE_INVERTED(1'b0),     // Optional inversion for RSTALUMODE
      .IS_RSTA_INVERTED(1'b0),           // Optional inversion for RSTA
      .IS_RSTB_INVERTED(1'b0),           // Optional inversion for RSTB
      .IS_RSTCTRL_INVERTED(1'b0),        // Optional inversion for RSTCTRL
      .IS_RSTC_INVERTED(1'b0),           // Optional inversion for RSTC
      .IS_RSTD_INVERTED(1'b0),           // Optional inversion for RSTD
      .IS_RSTINMODE_INVERTED(1'b0),      // Optional inversion for RSTINMODE
      .IS_RSTM_INVERTED(1'b0),           // Optional inversion for RSTM
      .IS_RSTP_INVERTED(1'b0),           // Optional inversion for RSTP
      // Register Control Attributes: Pipeline Register Configuration
      .ACASCREG(1),                      // Number of pipeline stages between A/ACIN and ACOUT (0-2)
      .ADREG(1),                         // Pipeline stages for pre-adder (0-1)
      .ALUMODEREG(1),                    // Pipeline stages for ALUMODE (0-1)
      .AREG(1),                          // Pipeline stages for A (0-2)
      .BCASCREG(1),                      // Number of pipeline stages between B/BCIN and BCOUT (0-2)
      .BREG(1),                          // Pipeline stages for B (0-2)
      .CARRYINREG(1),                    // Pipeline stages for CARRYIN (0-1)
      .CARRYINSELREG(1),                 // Pipeline stages for CARRYINSEL (0-1)
      .CREG(1),                          // Pipeline stages for C (0-1)
      .DREG(1),                          // Pipeline stages for D (0-1)
      .INMODEREG(1),                     // Pipeline stages for INMODE (0-1)
      .MREG(1),                          // Multiplier pipeline stages (0-1)
      .OPMODEREG(1),                     // Pipeline stages for OPMODE (0-1)
      .PREG(1)                           // Number of pipeline stages for P (0-1)
   ) i_DSP48E2 (
      // Cascade outputs: Cascade Ports
      .ACOUT(),                         // 30-bit output: A port cascade
      .BCOUT(),                         // 18-bit output: B cascade
      .CARRYCASCOUT(),                  // 1-bit output: Cascade carry
      .MULTSIGNOUT(),                   // 1-bit output: Multiplier sign cascade
      .PCOUT(),                         // 48-bit output: Cascade output
      // Control outputs: Control Inputs/Status Bits
      .OVERFLOW(),                      // 1-bit output: Overflow in add/acc
      .PATTERNBDETECT(),                // 1-bit output: Pattern bar detect
      .PATTERNDETECT(),                 // 1-bit output: Pattern detect
      .UNDERFLOW(),                     // 1-bit output: Underflow in add/acc
      // Data outputs: Data Ports
      .CARRYOUT(),                     	// 4-bit output: Carry
      .P(dsp48e_result_ps9),           	// 48-bit output: Primary data
      .XOROUT(),                     	// 8-bit output: XOR data
      // Cascade inputs: Cascade Ports
      .ACIN(30'b0),                     // 30-bit input: A cascade data
      .BCIN(18'b0),                     // 18-bit input: B cascade
      .CARRYCASCIN(1'b0),               // 1-bit input: Cascade carry
      .MULTSIGNIN(1'b0),                // 1-bit input: Multiplier sign cascade
      .PCIN(48'b0),                     // 48-bit input: P cascade
      // Control inputs: Control Inputs/Status Bits
      .ALUMODE(dsp48e_alumode_ps7),        	// 4-bit input: ALU control
      .CARRYINSEL(dsp48e_carryinsel),  	// 3-bit input: Carry select
      .CLK(iClk),                      	// 1-bit input: Clock
      .INMODE(dsp48e_inmode_ps5),      	// 5-bit input: INMODE control
      .OPMODE(dsp48e_opmode_ps7),      	// 9-bit input: Operation mode
      // Data inputs: Data Ports
      .A({12'b0, coefBuff_rddata}),  	// 30-bit input: A data
      .B(dataBuffA0_rddata),          	// 18-bit input: B data
      .C({12'b0, iChainS}),            	// 48-bit input: C data
      .CARRYIN(1'b0),                  	// 1-bit input: Carry-in
      .D({9'b0, dataBuffB0_rddata}),  	// 27-bit input: D data
      // Reset/Clock Enable inputs: Reset/Clock Enable Inputs
      .CEA1(1'b1),                     	// 1-bit input: Clock enable for 1st stage AREG
      .CEA2(1'b1),            			// 1-bit input: Clock enable for 2nd stage AREG
      .CEAD(1'b1),            			// 1-bit input: Clock enable for ADREG
      .CEALUMODE(1'b1),       			// 1-bit input: Clock enable for ALUMODE
      .CEB1(1'b1),            			// 1-bit input: Clock enable for 1st stage BREG
      .CEB2(1'b1),            			// 1-bit input: Clock enable for 2nd stage BREG
      .CEC(1'b1),             			// 1-bit input: Clock enable for CREG
      .CECARRYIN(1'b1),       			// 1-bit input: Clock enable for CARRYINREG
      .CECTRL(1'b1),          			// 1-bit input: Clock enable for OPMODEREG and CARRYINSELREG
      .CED(1'b1),             			// 1-bit input: Clock enable for DREG
      .CEINMODE(1'b1),        			// 1-bit input: Clock enable for INMODEREG
      .CEM(1'b1),             			// 1-bit input: Clock enable for MREG
      .CEP(1'b1),             			// 1-bit input: Clock enable for PREG
      .RSTA(1'b0),            			// 1-bit input: Reset for AREG
      .RSTALLCARRYIN(1'b0),   			// 1-bit input: Reset for CARRYINREG
      .RSTALUMODE(1'b0),      			// 1-bit input: Reset for ALUMODEREG
      .RSTB(1'b0),            			// 1-bit input: Reset for BREG
      .RSTC(1'b0),            			// 1-bit input: Reset for CREG
      .RSTCTRL(1'b0),         			// 1-bit input: Reset for OPMODEREG and CARRYINSELREG
      .RSTD(1'b0),            			// 1-bit input: Reset for DREG and ADREG
      .RSTINMODE(1'b0),       			// 1-bit input: Reset for INMODEREG
      .RSTM(1'b0),                     	// 1-bit input: Reset for MREG
      .RSTP(1'b0)                      	// 1-bit input: Reset for PREG
   );


// generate controls for DSP48E2
reg [1:0] preadd_mode_ps5;
reg [1:0] mul_mode_ps7;
reg add_prevengine_accum_ps7;

always @(posedge iClk)
begin
	preadd_mode_ps5 <= PREADD_MODE >> {timeSlice_ps4, 2'b0};
    mul_mode_ps7 <= MUL_MODE >> {timeSlice_ps6, 2'b0};
    add_prevengine_accum_ps7 <= ADDPREVENGINEACCUM >> timeSlice_ps6;
end

always @(*)
begin
    case (preadd_mode_ps5)
        4'b00: dsp48e_inmode_ps5 <= 5'b10001;	// +B*A
        4'b01: dsp48e_inmode_ps5 <= 5'b10101;	// (D+B)*A
        4'b10: dsp48e_inmode_ps5 <= 5'b11101;	// (D-B)*A
        default: dsp48e_inmode_ps5 <= 0;
    endcase
    case (mul_mode_ps7)
        4'b00: dsp48e_alumode_ps7 <= 4'b0000;	// Add
        4'b01: dsp48e_alumode_ps7 <= 4'b0000;	// Add
        4'b10: dsp48e_alumode_ps7 <= 4'b0011;	// Subtract
        default: dsp48e_alumode_ps7 <= 0;
   endcase
    case ({add_prevengine_accum_ps7, mul_mode_ps7})
        4'b000: dsp48e_opmode_ps7 <= 9'b000000101;		// W=0 Z=0
        4'b001: dsp48e_opmode_ps7 <= 9'b000100101;		// W=0 Z=P
        4'b010: dsp48e_opmode_ps7 <= 9'b000100101;		// W=0 Z=P
        4'b100: dsp48e_opmode_ps7 <= 9'b110000101;		// W=SUMIN Z=0
        4'b101: dsp48e_opmode_ps7 <= 9'b110100101;		// W=SUMIN Z=P
        4'b110: dsp48e_opmode_ps7 <= 9'b110100101;		// W=SUMIN Z=P
        default: dsp48e_opmode_ps7 <= 0;
   endcase
end


///////////////////////////////////////////////////////////////
// Mac Chain
///////////////////////////////////////////////////////////////
assign oChainS = dsp48e_result_ps9[35:0];


///////////////////////////////////////////////////////////////
// Data Outputs
///////////////////////////////////////////////////////////////
wire [17:0] dspout_ps9 = dsp48e_result_ps9[33:16];
wire dspoutchanged_ps9 = commit_ps9;

reg [2:0] channelSel_ps9;
always @(posedge iClk)
begin
    channelSel_ps9 <= CHANNEL_SELECT >> {timeSlice_ps8, 2'b0};
end

reg dataOut0Changed;
reg dataOut1Changed;
reg dataOut2Changed;
reg dataOut3Changed;
reg dataOut4Changed;

always @(posedge iClk or posedge iRst)
begin
    if (iRst) begin
        oData0 <= 0;
        dataOut0Changed <= 0;
        oData0Changed <= 0;
        oData1 <= 0;
        dataOut1Changed <= 0;
        oData1Changed <= 0;
        oData2 <= 0;
        dataOut2Changed <= 0;
        oData2Changed <= 0;
        oData3 <= 0;
        dataOut3Changed <= 0;
        oData3Changed <= 0;
        oData4 <= 0;
        dataOut4Changed <= 0;
        oData4Changed <= 0;
    end else begin
       dataOut0Changed <= 0;
       dataOut1Changed <= 0;
       dataOut2Changed <= 0;
       dataOut3Changed <= 0;
       dataOut4Changed <= 0;
       case (channelSel_ps9)
            4'b0001: begin oData0 <= dspout_ps9; dataOut0Changed <= dspoutchanged_ps9; end
            4'b0010: begin oData1 <= dspout_ps9; dataOut1Changed <= dspoutchanged_ps9; end
            4'b0011: begin oData2 <= dspout_ps9; dataOut2Changed <= dspoutchanged_ps9; end
            4'b0100: begin oData3 <= dspout_ps9; dataOut3Changed <= dspoutchanged_ps9; end
            4'b0101: begin oData4 <= dspout_ps9; dataOut4Changed <= dspoutchanged_ps9; end
            default: ;
       endcase
       // 'Changed' signal is delayed by 1 cycle from data, and converted from a 'level' to an 'edge'
       // This ensures the edge occurs when the data value is stable, and allows the data signal to safely cross clock domains
       if (dataOut0Changed) 
            oData0Changed <= ~oData0Changed;
       if (dataOut1Changed) 
            oData1Changed <= ~oData1Changed;
       if (dataOut2Changed) 
            oData2Changed <= ~oData2Changed;
       if (dataOut3Changed) 
            oData3Changed <= ~oData3Changed;
       if (dataOut4Changed) 
            oData4Changed <= ~oData4Changed;
   end
end


///////////////////////////////////////////////////////////////
// Fifo Description Rams (A and B hold the same contents)
///////////////////////////////////////////////////////////////
reg [LOG2NUMFIFOS-1:0] fifoDescBuffA_rdaddr;
reg [LOG2NUMFIFOS-1:0] fifoDescBuffB_rdaddr;
reg [LOG2NUMFIFOS-1:0] fifoDescBuff_wraddr;
wire [15:0] fifoDescBuff_wrdata;
wire fifoDescBuff_wren;

reg [15:0] fifoDescBuffA_rddata;
reg [15:0] fifoDescBuffB_rddata;


reg [15:0] fifoDescBuffA_rddata_int = 0;
reg [15:0] fifoDescBuffA_contents[(1 << LOG2NUMFIFOS)-1:0];

initial 
begin
	for (i = 0; i < NUMFIFOS; i = i + 1)
		fifoDescBuffA_contents[i] <= FIFOSIZES >> (16 * i);
end

always @(posedge iClk) 
begin
    if (fifoDescBuff_wren)
    	fifoDescBuffA_contents[fifoDescBuff_wraddr] <= fifoDescBuff_wrdata;
    fifoDescBuffA_rddata_int <= fifoDescBuffA_contents[fifoDescBuffA_rdaddr];
  	fifoDescBuffA_rddata <= fifoDescBuffA_rddata_int;
end


reg [15:0] fifoDescBuffB_rddata_int = 0;
reg [15:0] fifoDescBuffB_contents[(1 << LOG2NUMFIFOS)-1:0];

initial 
begin
	for (i = 0; i < NUMFIFOS; i = i + 1)
		fifoDescBuffB_contents[i] <= FIFOSIZES >> (16 * i);
end

always @(posedge iClk) 
begin
    if (fifoDescBuff_wren)
    	fifoDescBuffB_contents[fifoDescBuff_wraddr] <= fifoDescBuff_wrdata;
    fifoDescBuffB_rddata_int <= fifoDescBuffB_contents[fifoDescBuffB_rdaddr];
  	fifoDescBuffB_rddata <= fifoDescBuffB_rddata_int;
end


///////////////////////////////////////////////////////////////
// Fifo Controls
///////////////////////////////////////////////////////////////
reg firstTap_ps2;

always @(posedge iClk) 
begin
    fifoDescBuffA_rdaddr <= RDFIFONUM >> {timeSlice_psm1, 3'b0};
    fifoDescBuffB_rdaddr <= UPDATEFIFONUM >> {timeSlice_psm1, 3'b0};
    fifoDescBuff_wraddr <= UPDATEFIFONUM >> {timeSlice_ps2, 3'b0};
    firstTap_ps2 <= FIRST_TAP >> timeSlice_ps1;
end

assign fifoDescBuff_wren = commit_ps3;


wire [9:0] currFifoOffset = fifoDescBuffA_rddata[15:6];
wire [9:0] currFifoLengthMinusOne = {4'b0, fifoDescBuffA_rddata[5:0]};
reg  [9:0] currFifoRegion;        // lowest power of 2 >= currFifoLengthMinusOne
wire [9:0] currFifoRegionOrigin = currFifoOffset & ~currFifoRegion;

always @(currFifoLengthMinusOne) begin
    if (currFifoLengthMinusOne[5])
        currFifoRegion = 10'b0000111111;
    else if (currFifoLengthMinusOne[4])
        currFifoRegion = 10'b0000011111;
    else if (currFifoLengthMinusOne[3])
        currFifoRegion = 10'b0000001111;
    else if (currFifoLengthMinusOne[2])
        currFifoRegion = 10'b0000000111;
    else if (currFifoLengthMinusOne[1])
        currFifoRegion = 10'b0000000011;
    else
        currFifoRegion = 10'b0000000001;
end

// The coefficient buffer is always read from the Fifo-origin address
always @(posedge iClk) 
begin
    if (firstTap_ps2) begin
        coefBuff_rdaddr <= currFifoRegionOrigin;            // Start of Fifo Region
        dataBuffA0_rdaddr <= currFifoOffset;
        dataBuffB0_rdaddr <= currFifoRegionOrigin | ((currFifoOffset - currFifoLengthMinusOne) & currFifoRegion);
    end else begin
        coefBuff_rdaddr <= coefBuff_rdaddr + 1;
        dataBuffA0_rdaddr <= currFifoRegionOrigin | ((dataBuffA0_rdaddr - 1) & currFifoRegion);
        dataBuffB0_rdaddr <= currFifoRegionOrigin | ((dataBuffB0_rdaddr + 1) & currFifoRegion);
    end
end


wire [9:0] updateFifoOffset = fifoDescBuffB_rddata[15:6];
wire [9:0] updateFifoLengthMinusOne = {4'b0, fifoDescBuffB_rddata[5:0]};
reg  [9:0] updateFifoRegion;        // lowest power of 2 >= currFifoLengthMinusOne
wire [9:0] updateFifoRegionOrigin = updateFifoOffset & ~updateFifoRegion;

always @(updateFifoLengthMinusOne) begin
    if (updateFifoLengthMinusOne[5])
        updateFifoRegion = 10'b0000111111;
    else if (updateFifoLengthMinusOne[4])
        updateFifoRegion = 10'b0000011111;
    else if (updateFifoLengthMinusOne[3])
        updateFifoRegion = 10'b0000001111;
    else if (updateFifoLengthMinusOne[2])
        updateFifoRegion = 10'b0000000111;
    else if (updateFifoLengthMinusOne[1])
        updateFifoRegion = 10'b0000000011;
    else
        updateFifoRegion = 10'b0000000001;
end

// The coefficient buffer is always read from the Fifo-origin address
reg [9:0] dataBuffAB1UpdateAddr;
reg [9:0] dataBuffAB1NextAddr;
reg [9:0] dataBuffAB1FifoLengthMinusOne;
reg [9:0] dataBuffAB1NextAddr_delay1;
reg [9:0] dataBuffAB1FifoLengthMinusOne_delay1;
reg [9:0] dataBuffAB1NextAddr_delay2;
reg [9:0] dataBuffAB1FifoLengthMinusOne_delay2;

always @(posedge iClk) 
begin
    if (doUpdate_ps2)           // Update Data in DataBuffers
        dataBuffAB1UpdateAddr <= dataBuffAB1NextAddr_delay2;
    else                    // Read Data about to be popped from DataBuffers
        dataBuffAB1UpdateAddr <= updateFifoRegionOrigin | ((updateFifoOffset - updateFifoLengthMinusOne) & updateFifoRegion);
        
    // this is the address in the circular buffer where the next data entry should go (push)
    dataBuffAB1NextAddr <= currFifoRegionOrigin | ((currFifoOffset + 1) & currFifoRegion);
    dataBuffAB1FifoLengthMinusOne <= currFifoLengthMinusOne;
    
    dataBuffAB1NextAddr_delay1 <= dataBuffAB1NextAddr;
    dataBuffAB1FifoLengthMinusOne_delay1 <= dataBuffAB1FifoLengthMinusOne;
    dataBuffAB1NextAddr_delay2 <= dataBuffAB1NextAddr_delay1;
    dataBuffAB1FifoLengthMinusOne_delay2 <= dataBuffAB1FifoLengthMinusOne_delay1;
end

assign dataBuffA1_rwaddr = dataBuffAB1UpdateAddr;
assign dataBuffB1_rwaddr = dataBuffAB1UpdateAddr;
assign fifoDescBuff_wrdata = { dataBuffAB1NextAddr_delay2, dataBuffAB1FifoLengthMinusOne_delay2[5:0] };



endmodule
