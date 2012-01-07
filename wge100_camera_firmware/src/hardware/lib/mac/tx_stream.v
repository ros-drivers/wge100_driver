module tx_stream
(input usr_clk,
 input reset_n,

 // header
 input [47:0] src_mac_addr,
 input [47:0] dst_mac_addr,
 input [15:0] ethertype,
 input [9:0] ifg,
 input jumboframes,

 // from user interace
 input [31:0] tx_data,
 input [3:0]  tx_eof,
 input        tx_we,
 output       tx_stop,

 output [31:0] tx_count,
 
 // interface to reconciliation layer
 input        int_tx_clk,
 output [7:0] int_tx_dout,
 output       int_tx_en,
 output       int_tx_er,

 output [7:0] debug);

// mix in eof signals 
wire [35:0] txff_din = {tx_eof[3], tx_data[31:24],
                        tx_eof[2], tx_data[23:16],
                        tx_eof[1], tx_data[15:8],
                        tx_eof[0], tx_data[7:0]};
wire [8:0] txff_dout;
wire       txff_empty;
wire       txff_rden;
txfifo U_txfifo
  (.rst(~reset_n),
   .wr_clk(usr_clk), .din(txff_din), .wr_en(tx_we), .full(tx_stop),
   .rd_clk(int_tx_clk), 
   .dout(txff_dout), .rd_en(txff_rden), .empty(txff_empty));

wire crc_init;
wire crc_rden;
wire [7:0] crc_data;
wire [7:0] crc_crc;
crc_gen U_crc_gen
  (.clk(int_tx_clk), .reset_n(reset_n),
   .init(crc_init), .data(crc_data),
   .rden(crc_rden), .crc(crc_crc));

wire [3:0] txe_debug;
tx_engine_stream U_tx_engine
  (.clk(int_tx_clk), .reset_n(reset_n),
   .src_mac_addr(src_mac_addr), .dst_mac_addr(dst_mac_addr), .ethertype(ethertype),
   .ifg(ifg), .jumboframes(jumboframes),
   .txff_dout(txff_dout), .txff_empty(txff_empty), .txff_rden(txff_rden), 
   .crc_init(crc_init), .crc_rden(crc_rden), .crc_data(crc_data), .crc_crc(crc_crc),
   .int_tx_dout(int_tx_dout), .int_tx_en(int_tx_en), .int_tx_er(int_tx_er),
   .tx_count(tx_count), .debug(txe_debug));

assign debug = {txe_debug, txff_rden, int_tx_er, int_tx_en, int_tx_clk};
endmodule
