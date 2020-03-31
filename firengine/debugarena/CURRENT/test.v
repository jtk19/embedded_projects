`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Design Name: Fir Engine
// Module Name: firengine
//////////////////////////////////////////////////////////////////////////////////

module test (
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

	// Each Channel has a seperate (data-output, data-changed) pair
	//   Data-Changed is flipped every time a new sample appears
	output          				oData0Changed,
	output [17:0]	   		 		oData0,
	output          				oData1Changed,
	output [17:0]	   		 		oData1,
	output          				oData2Changed,
	output [17:0]	   		 		oData2,
	output          				oData3Changed,
	output [17:0]	   		 		oData3,

	// Interface to write to Coefficient Memory
	input							iCoefBuff_wren,
	input [31:0]					iCoefBuff_wraddr,
	input [17:0]	  				iCoefBuff_wrdata
);

wire [17:0]		chainD0;
wire [17:0]		chainR0;
wire [35:0]		chainS0;
wire	inputChangeChain0;
wire [17:0]		chainD1;
wire [17:0]		chainR1;
wire [35:0]		chainS1;
wire	inputChangeChain1;
wire [17:0]		chainD2;
wire [17:0]		chainR2;
wire [35:0]		chainS2;
wire	inputChangeChain2;
wire [17:0]		chainD3;
wire [17:0]		chainR3;
wire [35:0]		chainS3;
wire	inputChangeChain3;

// Initialize start of the MAC-Chains
assign chainD0 = 18'b0;
assign chainR0 = 18'b0;
assign chainS0 = 36'b0;
assign inputChangeChain0 = 1'b0;

test_fir i_test_fir (
	.iClk			(iClk),
	.iRst			(iRst),
	.iData0Changed	(iData0Changed),
	.iData0			(iData0),
	.iData1Changed	(iData1Changed),
	.iData1			(iData1),
	.oData0Changed	(oData0Changed),
	.oData0			(oData0),
	.oData1Changed	(oData1Changed),
	.oData1			(oData1),
	.iChainD				(chainD0),
	.oChainD				(chainD1),
	.iChainR				(chainR0),
	.oChainR				(chainR1),
	.iChainS				(chainS0),
	.oChainS				(chainS1),
	.iInputChangeChain	(inputChangeChain0),
	.oInputChangeChain	(inputChangeChain1),
	.iCoefBuff_wren	(1'b0),
	.iCoefBuff_wraddr	(32'b0),
	.iCoefBuff_wrdata	(18'b0)
	);

test_fir i_test_fir (
	.iClk			(iClk),
	.iRst			(iRst),
	.iData0Changed	(iData2Changed),
	.iData0			(iData2),
	.oData0Changed	(oData2Changed),
	.oData0			(oData2),
	.iChainD				(chainD1),
	.oChainD				(chainD2),
	.iChainR				(chainR1),
	.oChainR				(chainR2),
	.iChainS				(chainS1),
	.oChainS				(chainS2),
	.iInputChangeChain	(inputChangeChain1),
	.oInputChangeChain	(inputChangeChain2),
	.iCoefBuff_wren	(1'b0),
	.iCoefBuff_wraddr	(32'b0),
	.iCoefBuff_wrdata	(18'b0)
	);

test_fir i_test_fir (
	.iClk			(iClk),
	.iRst			(iRst),
	.iData0Changed	(iData3Changed),
	.iData0			(iData3),
	.oData0Changed	(oData3Changed),
	.oData0			(oData3),
	.iChainD				(chainD2),
	.oChainD				(chainD3),
	.iChainR				(chainR2),
	.oChainR				(chainR3),
	.iChainS				(chainS2),
	.oChainS				(chainS3),
	.iInputChangeChain	(inputChangeChain2),
	.oInputChangeChain	(inputChangeChain3),
	.iCoefBuff_wren	(1'b0),
	.iCoefBuff_wraddr	(32'b0),
	.iCoefBuff_wrdata	(18'b0)
	);

endmodule
