/* This file is a modified version of a file included in the NetFPGA
distribution, which has the following license notification:
 
Copyright (c) 2006 The Board of Trustees of The Leland Stanford
Junior University

We are making the NetFPGA tools and associated documentation (Software)
available for public use and benefit with the expectation that others will
use, modify and enhance the Software and contribute those enhancements back
to the community. However, since we would like to make the Software
available for broadest use, with as few restrictions as possible permission
is hereby granted, free of charge, to any person obtaining a copy of this
Software) to deal in the Software under the copyrights without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to the
following conditions:  
  
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software. 

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE. 

The name and trademarks of copyright holder(s) may NOT be used in
advertising or publicity pertaining to the Software or any derivatives
without specific, written prior permission. 
*/
 
`timescale 1ns / 1ns

// `define FAKE_PHY_TEST

module fake_phy
(// MAC interfaces
 input       gmii,
 input [7:0] TXD,
 input       TXEN,
 input       GTXCLK,
 input       TXER,
 output      TXCLK,
 output      RXCLK,
 output reg  [7:0] RXD,
 output      RXER,
 output reg  RXDV,  
 input       RESET_N,

 input MDC,
 inout MDIO,

 // test interface
 input force_rxer
 );

parameter input_file_name="packets_in.dat";
parameter input_file_maxsize=10000;
parameter input_pkt_seperator=32'heeeeffff;
parameter max_pkt_size=9100;

integer seed = 10;

reg [7:0] rx_packet_buffer [0:max_pkt_size]; // data of receive packet.
reg [7:0] tx_packet_buffer [0:max_pkt_size];   
reg [31:0] input_file[0:input_file_maxsize];
reg [31:0] crc_table[0:255];

fake_mii mii(MDC, MDIO);

wire rx_clk_125;
wire rx_clk_25;
fake_clk #(125) rxc_125(rx_clk_125);
fake_clk #(10) rxc_25(rx_clk_25);
assign RXCLK = gmii ? rx_clk_125 : rx_clk_25;

wire tx_clk_25;
fake_clk #(25) txc(tx_clk_25);
assign TXCLK = gmii ? 1'b0 : tx_clk_25;

assign RXER = force_rxer;

initial begin
   RXD = 8'hx;
   RXDV = 0;

   // read input
   // TODO take filename from command line       
   $display($time, " reading input file %s.", input_file_name);
   $readmemh(input_file_name, input_file);
   check_integrity;
   
   gen_crc_table;

   fork
      handle_inputs;
      handle_outputs;
   join
   $finish;
end

task handle_inputs;
integer packet_index;       // pointer to next word in input memory
integer words, i;
reg [31:0] len, tmp, crc;
time time2send;
   begin

      packet_index = 0; 

      // send while there are any packets left to send!
      while ((packet_index < max_pkt_size) && (input_file[packet_index] !== 32'hxxxxxxxx))
        begin
	   // get next packet and put in rx_packet_buffer
	   len = input_file[packet_index];

	   // time2send is EARLIEST we can send this packet.
	   time2send = {input_file[packet_index+1],input_file[packet_index+2]};

	   if (time2send > $time) begin
	      $display($time, " %m Info: Waiting until time %t to send packet (length %0d)",
		       time2send, len);
	      
	      #(time2send - $time);
	   end
	   
	   $display($time, " Sending next input packet (len %0d) to MAC.", len);

	   // Build the packet in rx_packet_buffer.
	   packet_index = packet_index + 3;	//now points at DA
	   words = ((len-1)>>2)+1;           // number of 32 bit words in pkt
	   i = 0;                            // index into rx_packet_buffer
	   while (words) begin
	      tmp = input_file[packet_index];
	      rx_packet_buffer[i]   = tmp[31:24];
	      rx_packet_buffer[i+1] = tmp[23:16];
	      rx_packet_buffer[i+2] = tmp[15:8];
	      rx_packet_buffer[i+3] = tmp[7:0];
	      words = words - 1;
	      i = i + 4;
	      packet_index = packet_index + 1;
	   end

	   // might have gone too far so set byte index to correct position,
	   i = len;

	   // clear out buffer ready for CRC
	   rx_packet_buffer[i]   = 8'h0;
	   rx_packet_buffer[i+1] = 8'h0;
	   rx_packet_buffer[i+2] = 8'h0;
	   rx_packet_buffer[i+3] = 8'h0;

	   crc = update_crc(32'hffffffff,len)^32'hffffffff;

	   //$display("%t %m Info: CRC is %x", $time, crc);
	   
	   rx_packet_buffer[i+3] = crc[31:24];
	   rx_packet_buffer[i+2] = crc[23:16];
	   rx_packet_buffer[i+1] = crc[15:8];
	   rx_packet_buffer[i]   = crc[7:0];

	   send_rx_pkt(len+4);  // data + CRC

	   if (input_file[packet_index] !== input_pkt_seperator) begin
	      $display($time," %m Error: expected to point at packet separator %x but saw %x",
		       input_pkt_seperator, input_file[packet_index]);
	      $fflush; $finish;
	   end
	   
	   packet_index = packet_index + 1;	      
        end
      $display($time," rx packets finished at index %d.", packet_index);
   end
endtask

task send_rx_pkt;
input [31:0] pkt_len_w_crc;
   begin
      if (gmii) send_gmii_rx_pkt(pkt_len_w_crc);
      else send_mii_rx_pkt(pkt_len_w_crc);
   end
endtask

task send_gmii_rx_pkt;
input [31:0] pkt_len_w_crc;
integer i;
   begin
      @(posedge RXCLK) #1 RXDV = 1;
      RXD = 8'h55;
      for (i=0; i<7; i=i+1) @(posedge RXCLK) begin end
      #1 RXD = 8'hd5;
      @(posedge RXCLK) begin end

      for (i=0; i<pkt_len_w_crc; i=i+1) begin
         #1;
         RXD = rx_packet_buffer[i];
         @(posedge RXCLK) begin end
      end
      #1 RXDV = 0;
      RXD = 8'hx;
      // IFG
      for (i=0; i<12; i=i+1) begin
         @(posedge RXCLK) begin end
      end
   end
endtask

task send_mii_rx_pkt;
input [31:0] pkt_len_w_crc;
integer i;
   begin
      @(posedge RXCLK) #1 RXDV = 1;
      RXD = 8'hx5;
      for (i=0;i<15;i=i+1) @(posedge RXCLK) begin end
      #1 RXD = 8'hxd;
      @(posedge RXCLK) begin end
      for (i=0; i<pkt_len_w_crc; i=i+1) begin
         #1;
         RXD[3:0] = rx_packet_buffer[i][3:0];
         @(posedge RXCLK) begin end

         #1;
         RXD[3:0] = rx_packet_buffer[i][7:4];
         @(posedge RXCLK) begin end
      end
      #1 RXDV = 0;
      RXD = 8'hx;
      // IFG
      for (i=0; i<12; i=i+1) begin
         @(posedge RXCLK) begin end
      end
   end
endtask         

task handle_outputs;
   begin
      if (gmii) while (1) recv_gmii_tx_pkt;
      else while (1) recv_mii_tx_pkt;
   end

endtask // handle_egress

task recv_gmii_tx_pkt;
reg [7:0] data;
integer   i;
reg       seeing_data;  // as opposed to preamble      
   begin
      // Wait until we see Transmit Enable go active
      wait (TXEN == 1'b1);
      seeing_data = 0;
      i = 0;
      $display($time, " Starting packet transmission.");
      while (TXEN) 
        begin
           @(posedge GTXCLK)
             data[7:0] = TXD[7:0];
           if (seeing_data) 
             begin
                if (TXEN) begin
                   tx_packet_buffer[i] = data;
                   i = i + 1;
                end
             end
           else begin
              if (data == 8'hd5) seeing_data = 1;
              else if (data != 8'h55)
                $display("%t ERROR %m : expected preamble but saw %2x", $time,data);
           end
        end
      
      handle_tx_packet(i);
   end
endtask

task recv_mii_tx_pkt;
reg [7:0] data;
integer   i;
reg       seeing_data;  // as opposed to preamble      
   begin
      // Wait until we see Transmit Enable go active
      wait (TXEN == 1'b1);
      seeing_data = 0;
      i = 0;
      $display($time, " Starting packet transmission.");
      while (TXEN) 
        begin
           @(posedge TXCLK)
             data[3:0] = TXD[3:0];
           @(posedge TXCLK)
             data[7:4] = TXD[3:0];
           if (seeing_data) 
             begin
                if (TXEN) begin
                   tx_packet_buffer[i] = data;
                   i = i + 1;
                end
             end
           else begin
              if (data == 8'hd5) seeing_data = 1;
              else if (data != 8'h55)
                $display("%t ERROR %m : expected preamble but saw %2x", $time,data);
           end
        end
      
      handle_tx_packet(i);
   end
endtask      

///////////////////////////////////////////////////////////////////
// Board just transmitted a packet - we need to write it out to the file.
// Egress packet is in nibbles in tx_packet_buffer[]
// Number of nibbles is in counter.

task handle_tx_packet;

input [31:0] byte_len;  // includes CRC
integer i,j;
integer byte_len_no_crc;
reg [31:0] tmp, calc_crc,pkt_crc;

   
   begin

      // We're not going to put the transmitted CRC in the packet, but tell the user what
      // it was in case they need to know. (So put it in an XML comment)

      pkt_crc = {tx_packet_buffer[byte_len-4],
                 tx_packet_buffer[byte_len-3],
                 tx_packet_buffer[byte_len-2],
                 tx_packet_buffer[byte_len-1]};
      
      $display($time, " Transmitted packet: Full length = %0d. CRC was %2x %2x %2x %2x",
               byte_len, 
               tx_packet_buffer[byte_len-4],
               tx_packet_buffer[byte_len-3],
               tx_packet_buffer[byte_len-2],
               tx_packet_buffer[byte_len-1]);


      // Now drop the CRC
      byte_len_no_crc = byte_len - 4;

      // Now check that the CRC was correct
      for (i=byte_len_no_crc; i<byte_len; i=i+1) tx_packet_buffer[i] = 'h0;
      
      tmp = tx_update_crc(32'hffffffff,byte_len_no_crc)^32'hffffffff;
      calc_crc = {tmp[7:0],tmp[15:8],tmp[23:16],tmp[31:24]};
      
      if (calc_crc !== pkt_crc) 
        $display("%t ERROR: Observed CRC was %8x but calculated CRC was %8x",
                 $time, pkt_crc, calc_crc);

      // OK write the packet out.
      $write("\t\t\t");
      for (i=0;i<byte_len_no_crc;i=i+1) begin
         $write("%2x ", tx_packet_buffer[i]);
         if ((i % 16 == 15)) begin
            $write("\n");
            if (i != byte_len_no_crc-1) $write("\t\t\t");
         end
      end
      if (byte_len_no_crc % 16 != 0) $write("\n");         
   end
endtask // handle_tx_packet


////////////////////////////////////////////////////////////////
// Check integrity of input file read
//    Format of memory data is:
//   0000003c // len = 60 (not including CRC)
//   00000000 // earliest send time MSB (ns)
//   00000001 // earliest send time LSB
//   aa000000 // packet data starting at DA
//   ...
//   24252627 // end of data
//   eeeeffff // token indicates end of packet
////////////////////////////////////////////////////////////////
task check_integrity;
integer pkt_count, i, words;
reg [31:0] len;
time time2send;
   begin      
      #1 pkt_count = 0; // #1 is done so that time format is set.
      i = 0;
      while ((input_file[i] !== 32'hxxxxxxxx) && (input_file[i] !== 32'h0)) 
        begin
           pkt_count = pkt_count + 1;
           
           len = input_file[i];
           if (len <14) $display("%m Warning: packet length %0d is < 14", len);
           if (len >1518) $display("%m Warning: packet length %0d is > 1518", len);
           
           time2send ={input_file[i+1],input_file[i+2]};
           
           words = (len-1)/4+1;
           i=i+words+3;
           if (input_file[i] !== input_pkt_seperator) begin
              $display("%m Error : expected to see %x at word %0d but saw %x",
                       input_pkt_seperator, i, input_file[i]);
           end
           i=i+1;
        end 
      $display($time," There will be %0d input packets.", pkt_count);
   end
endtask 

task gen_crc_table;
reg [31:0] c;
integer n, k;
   begin
      for (n = 0; n < 256; n = n + 1) begin
         c = n;
         for (k = 0; k < 8; k = k + 1) begin
            if (c & 1)
	      c = 32'hedb88320 ^ (c >> 1);
            else
              c = c >> 1;
         end
         crc_table[n] = c;
      end
   end
endtask 

function [31:0] update_crc;
input [31:0]crc;
input [31:0] len;
reg [31:0] c;
integer i, n;
   begin         
      c = crc;
      for (n = 0; n < len; n = n + 1) begin
         i = ( c ^ rx_packet_buffer[n] ) & 8'hff;
         c = crc_table[i] ^ (c >> 8);
      end
      update_crc = c;
      
   end
endfunction
////////////////////////////////////////////////////////////
// CRC generation function. Invert CRC when finished.
// REPEATED HERE FOR USE ON THE TX BUFFER!!! Gee pointers would be nice....
function [31:0]  tx_update_crc;
input [31:0] crc;
input [31:0] len;
reg [31:0] c;
integer i, n;
   begin

      c = crc;
      for (n = 0; n < len; n = n + 1) begin
         i = ( c ^ tx_packet_buffer[n] ) & 8'hff;
         c = crc_table[i] ^ (c >> 8);
      end
      tx_update_crc = c;
      
   end
endfunction // tx_update_crc 

endmodule

`ifdef FAKE_PHY_TEST
module fake_phy_tb;

// PHY interface
reg [7:0] txd;
reg tx_en;
reg gtx_clk;
reg tx_er;
wire tx_clk;
wire rx_clk;
wire [7:0] rxd;
wire rx_er;
wire rx_dv;

// we don't need to generate a clock here because the phy does
// but we do need a reset
reg reset_n;
initial begin
   $dumpfile("fake_phy_tb.vcd");
   $dumpvars;
   reset_n = 1'b0;
   #6 reset_n = 1'b1;
end

integer i;
initial begin
   txd = 0;
   tx_en = 0;
   tx_er = 0;      
   #100;
   @(posedge tx_clk) begin end
   tx_en = 1;
   for(i=0; i<15; i=i+1) begin
      txd[3:0] = 4'h5;
      @(posedge tx_clk) begin #1; end
   end
   txd[3:0] = 4'hd;
   @(posedge tx_clk) begin #1; end
   for(i=0; i<6; i=i+1) begin
      txd[3:0] = i;
      @(posedge tx_clk) begin #1; end
      txd[3:0] = i + 10;
      @(posedge tx_clk) begin #1; end
   end
   for(i=5; i>=0; i=i-1) begin
      txd[3:0] = i;
      @(posedge tx_clk) begin #1; end
      txd[3:0] = i + 10;
      @(posedge tx_clk) begin #1; end
   end
   txd[3:0] = 4'h8;
   @(posedge tx_clk) begin #1; end
   txd[3:0] = 4'h0;
   @(posedge tx_clk) begin #1; end
   txd[3:0] = 4'h0;
   @(posedge tx_clk) begin #1; end
   txd[3:0] = 4'h0;
   @(posedge tx_clk) begin #1; end
   for(i=0; i<46; i=i+1) begin
      txd[3:0] = i[3:0];
      @(posedge tx_clk) begin #1; end
      txd[3:0] = i[7:4];
      @(posedge tx_clk) begin #1; end
   end
   txd[3:0] = 4'h1;
   @(posedge tx_clk) begin #1; end
   txd[3:0] = 4'hb;
   @(posedge tx_clk) begin #1; end
   txd[3:0] = 4'hb;
   @(posedge tx_clk) begin #1; end
   txd[3:0] = 4'he;
   @(posedge tx_clk) begin #1; end
   txd[3:0] = 4'h2;
   @(posedge tx_clk) begin #1; end
   txd[3:0] = 4'hf;
   @(posedge tx_clk) begin #1; end
   txd[3:0] = 4'h5;
   @(posedge tx_clk) begin #1; end
   txd[3:0] = 4'h7;
   @(posedge tx_clk) begin #1; end
   tx_en = 0;
end

fake_phy 
  #(.gmii(0))
U_fake_phy 
  (.TXD(txd), .TXEN(tx_en), .GTXCLK(gtx_clk), .TXER(tx_er), .TXCLK(tx_clk),
   .RXCLK(rx_clk), .RXD(rxd), .RXER(rx_er), .RXDV(rx_dv), 
   .RESET_N(reset_n));

endmodule
`endif
