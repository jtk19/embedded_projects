`timescale 1ns / 1ps

module tb_top();

reg clk;
reg rst;

// Generate the clock
initial
begin
	clk = 0;
	forever
	begin
	  #(10) clk = ~clk;
	end
end

// generate the reset
initial
begin
	rst = 1;
	#(35) rst = 0;
end


wire         coefBuff_wren = 0;
wire [7:0]   coefBuff_wraddr = 0;
wire [17:0]  coefBuff_wrdata = 0;

// Clocks for each input stream
reg clk0;
reg clk1;
reg clk2;
reg clk3;
reg clk4;

initial begin #(40) clk0 = 0; forever begin #(320) clk0 = ~clk0; end end
initial begin #(100) clk1 = 0; forever begin #(320) clk1 = ~clk1; end end
initial begin #(60) clk2 = 0; forever begin #(160) clk2 = ~clk2; end end
initial begin #(80) clk3 = 0; forever begin #(160) clk3 = ~clk3; end end
initial begin #(70) clk4 = 0; forever begin #(100) clk4 = ~clk4; end end

reg [17:0]  dataIn0 = 18'h10000;        // +1
reg [17:0]  dataIn1 = 18'h10000;
reg [17:0]  dataIn2 = 0;
reg [17:0]  dataIn3 = 0;
reg [17:0]  dataIn4 = 0;

// Setup inputs
reg [18:0] dataInState2 = 0;
reg [17:0] dataInState3 = 0;
reg [17:0] dataInState4 = 0;
always @(posedge clk0) begin dataIn0 <= dataIn0 + 1; end                  // Saw tooth
always @(posedge clk1) begin dataIn1 <= dataIn1 ^ 18'h20000; end          // Square Wave (+1, -1)
always @(posedge clk2) begin dataInState2 <= dataInState2 + 1; dataIn2 <= dataInState2[17:0] ^ {18{dataInState2[18]}};  end        // Triangle Wave
always @(posedge clk3) begin dataInState3 <= dataInState3 + 1; dataIn3 <= (dataInState3 == 0) ? 18'h10000 : 18'h0; end        // Pulse Wave
always @(posedge clk4) begin dataInState4 <= dataInState4 + 1; if (dataInState4[2:0]==0) dataIn4 <= dataIn4 - 2; else dataIn4 <= dataIn4 + 1; end              // SawTooth Wave


wire [17:0]  dataOut0;
wire [17:0]  dataOut1;
wire [17:0]  dataOut2;
wire [17:0]  dataOut3;
wire [17:0]  dataOut4;

reg  data0changed = 0;
reg  data1changed = 0;
reg  data2changed = 0;
reg  data3changed = 0;
reg  data4changed = 0;

// ensure dataNchanged toggles after dataInN is stable
always @(negedge clk0) begin data0changed = ~data0changed; end
always @(negedge clk1) begin data1changed = ~data1changed; end
always @(negedge clk2) begin data2changed = ~data2changed; end
always @(negedge clk3) begin data3changed = ~data3changed; end
always @(negedge clk4) begin data4changed = ~data4changed; end

wire  dataOut0changed;
wire  dataOut1changed;
wire  dataOut2changed;
wire  dataOut3changed;
wire  dataOut4changed;


firengine #(
    .LOG2BUFFERDEPTH        (10),
    .BUFFERDEPTH            (1024),

    .LOG2NUMFIFOS           (8),
    .NUMFIFOS               (256),

    .LOG2TIMESLICES         (5),
    .NUM_TIMESLICES         (32),

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // FirEngine Configuration
    //   CHANNEL_SELECT            4 bits    - Selects which Input channel to use in this timeSlot (0 = None) This should only be non-zero during Fir Update-Write cycle
    //   FIRST_ENGINE            1 bit    - '1' when updating the first FirEngine in a chain. Valid on DOUPDATE cycle
    //   LAST_ENGINE            1 bit    - '1' when updating the last FirEngine in a chain. Valid on DOUPDATE cycle
    //   FIRST_TAP                 1 bit    - '1' for first tap of the section of FIR in this FirEngine
    //   PREADD_MODE            4 bits    - 0=NoPreadder(B*A), 1=PreAdd((D+B)*A), 2=PreSub((D-B)*A)
    //   MUL_MODE                4 bits    - 0=MUL, 1=MADD, 2=MSUB
    //   ADDPREVENGINEACCUM     1 bit   - Use value of previous Fir Engine's Accumulator from 2-cycles ago
    //   RDFIFONUM              8 bits    - Selects which Data Fifo to use
    //   UPDATEFIFONUM            8 bits    - Selects which Data Fifo to update (valid on doUpdate cycle and 3 cycles earlier)
    //   DOUPDATE                  1 bits    - High at the same time on all FirEngines involved in a firfilter for the last tap of the fir-filter
    //
    //   FIFOSIZES              16 bits    - Foreach Fifo [FifoOffset : 10 bits, Len-1: 6 bits]
    //
    //   COEFF_VALUES           24 bits for every slot in Coeff-Buffer
    //
    //                  Slot:    31 30 29 28  27 26 25 24  23 22 21 20  19 18 17 16  15 14 13 12  11 10  9  8   7  6  5  4   3  2  1  0
    .CHANNEL_SELECT		   (128'h_0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___2__0__0__0___1__0__0__0),
    .FIRST_ENGINE	 	   (32'b__0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___1__1__1__1___1__1__1__1),
    .LAST_ENGINE	       (32'b__0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___1__1__1__1___1__1__1__1),
    .FIRST_TAP  		   (32'b__0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__1___0__0__0__1),
    .PREADD_MODE	       (128'h_0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0),
    .MUL_MODE			   (128'h_0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___1__1__1__0___1__1__1__0),
    .ADDPREVENGINEACCUM	   (32'b__0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0),
    .RDFIFONUM   		   (512'h00_00_00_00__00_00_00_00__00_00_00_00__00_00_00_00__00_00_00_00__00_00_00_00__01_01_01_01__00_00_00_00),
    .UPDATEFIFONUM         (512'h00_00_00_00__00_00_00_00__00_00_00_00__00_00_00_00__00_00_00_00__00_00_00_00__01_01_01_01__00_00_00_00),
    .DOUPDATE		       (32'b__0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___0__0__0__0___1__0__0__0___1__0__0__0),

    .FIFOSIZES 		       ({{254{16'h0}}, 16'h0000_03, 16'h0001_03}),
    .COEFF_VALUES 		   ({{1016{24'h0}}, 24'h008000, 24'h000000, 24'h000000, 24'h000000, 24'h000000, 24'h004000, 24'h008000, 24'h004000})

) i_firengine (
    // Common Timeslice-Clock and Reset for all FIRs
    .iClk               (clk),
    .iRst               (rst),

    // Each Channel has a seperate (data-input, data-changed) pair
    //   Data-Changed is flipped every time a new sample arrives
    .iData0Changed		(data0changed),
    .iData0				(dataIn0),
    .iData1Changed		(data1changed),
    .iData1				(dataIn1),
    .iData2Changed		(data2changed),
    .iData2				(dataIn2),
    .iData3Changed		(data3changed),
    .iData3				(dataIn3),
    .iData4Changed		(data4changed),
    .iData4				(dataIn4),

    // Each Channel has a seperate (data-output, data-changed) pair
    //   Data-Changed is flipped every time a new sample appears
    .oData0Changed      (dataOut0Changed),
    .oData0				(dataOut0),
    .oData1Changed      (dataOut1Changed),
    .oData1				(dataOut1),
    .oData2Changed      (dataOut2Changed),
    .oData2				(dataOut2),
    .oData3Changed      (dataOut3Changed),
    .oData3				(dataOut3),
    .oData4Changed      (dataOut4Changed),
    .oData4				(dataOut4),

     /// The following signals are used to chain FirEngines together
    .iChainD                    (18'b0),
    .oChainD                    (),
    .iChainR                    (18'b0),
    .oChainR                    (),
    .iChainS                    (36'b0),
    .oChainS                    (),
    .iInputChangeChain          (1'b0),
    .oInputChangeChain          (),

 	// Interface to write to Coefficient Memory
 	.iCoefBuff_wren          (coefBuff_wren),
   	.iCoefBuff_wraddr        (coefBuff_wraddr),
   	.iCoefBuff_wrdata        (coefBuff_wrdata)
    );


//wire [17:0] dataBuffer0_rddata = i_firengine.dataBuffer0_rddata;
//wire [17:0] dataBuffer1_rddata = i_firengine.dataBuffer1_rddata;



endmodule
