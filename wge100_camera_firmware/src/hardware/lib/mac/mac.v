module mac
#(parameter USR_CLK_PERIOD=5,
  parameter RX_CKSM_CHECK=1,
  parameter RAW_TXIF_COUNT=1, // You can currently only have a raw or streaming IF
  parameter RAW_RXIF_COUNT=1, // not both (both tx and rx, indepedently)
  parameter STREAM_TXIF_COUNT=0,
  parameter STREAM_RXIF_COUNT=0) // there currently is no streming rx if
(input usr_clk, input clk_125, input reset_n,

 // config
 input        gmii,
 input        promiscuous,
 input        jumboframes,
 input [9:0]  ifg,

 // raw tx interface(s)
 input  [(RAW_TXIF_COUNT*32)-1:0] tx_raw_data,
 input  [RAW_TXIF_COUNT-1:0]      tx_raw_sof,
 input  [RAW_TXIF_COUNT-1:0]      tx_raw_we,
 output [RAW_TXIF_COUNT-1:0]      tx_raw_stop,
 output [(RAW_TXIF_COUNT*32)-1:0] tx_raw_count,

 // raw rx interface(s)
 input  [(RAW_RXIF_COUNT*48)-1:0] rx_raw_mac_addr,
 output [(RAW_RXIF_COUNT*32)-1:0] rx_raw_data,
 output [RAW_RXIF_COUNT-1:0]      rx_raw_sof,
 output [RAW_RXIF_COUNT-1:0]      rx_raw_dv,
 input  [RAW_RXIF_COUNT-1:0]      rx_raw_ack,
 output [(RAW_RXIF_COUNT*32)-1:0] rx_raw_count,

 //// streaming tx interface(s)
 //input  [(STREAM_TXIF_COUNT*48)-1:0] tx_stream_src_mac_addr,
 //input  [(STREAM_TXIF_COUNT*48)-1:0] tx_stream_dst_mac_addr,
 //input  [(STREAM_TXIF_COUNT*16)-1:0] tx_stream_ethertype,
 //input  [(STREAM_TXIF_COUNT*32)-1:0] tx_stream_data,
 //input  [(STREAM_TXIF_COUNT*4)-1:0]  tx_stream_eof,
 //input  [STREAM_TXIF_COUNT-1:0]      tx_stream_we,
 //output [STREAM_TXIF_COUNT-1:0]      tx_stream_stop,
 //output [(STREAM_TXIF_COUNT*32)-1:0] tx_stream_count,

 //// streaming rx interfaces(s)
 //input  [(STREAM_RXIF_COUNT*48)-1:0] rx_stream_mac_addr,
 //output [(STREAM_RXIF_COUNT*32)-1:0] rx_stream_data,
 //output [(STREAM_RXIF_COUNT*4)-1:0]  rx_stream_eof,
 //output [STREAM_RXIF_COUNT-1:0]      rx_stream_dv,
 //input  [STREAM_RXIF_COUNT-1:0]      rx_stream_ack,
 //output [(STREAM_RXIF_COUNT*32)-1:0] rx_stream_count,
 
 // management interface
 input [0:4]   miim_phyad,
 input [0:4]   miim_addr,
 input [0:15]  miim_wdata,
 input         miim_req,
 input         miim_we,
 output        miim_ack,
 output [15:0] miim_rdata,    

 // PHY interface
 output [7:0] phy_TXD,
 output       phy_TXEN,
 output       phy_GTXCLK,
 output       phy_TXER,
 input        phy_TXCLK,
 input        phy_RXCLK,
 input [7:0]  phy_RXD,
 input        phy_RXER,
 input        phy_RXDV,
 output       phy_RESET_N,

 output       phy_MDC,
 inout        phy_MDIO,

 output [31:0] debug);

assign phy_RESET_N = reset_n;

wire int_tx_clk;
wire [7:0] int_tx_dout;
wire int_tx_en;
wire int_tx_er;

wire int_rx_clk;
wire [7:0] int_rx_din;
wire int_rx_dv;
wire int_rx_er;
reconciliation rec
  (.clk_125(clk_125), .reset_n(reset_n),
   .gmii(gmii), 
   .int_tx_dout(int_tx_dout), .int_tx_en(int_tx_en), .int_tx_er(int_tx_er),
   .int_tx_clk(int_tx_clk),
   .int_rx_din(int_rx_din), .int_rx_dv(int_rx_dv), .int_rx_er(int_rx_er),
   .int_rx_clk(int_rx_clk),
   .phy_TXD(phy_TXD), .phy_TXEN(phy_TXEN), .phy_TXER(phy_TXER),
   .phy_GTXCLK(phy_GTXCLK), .phy_TXCLK(phy_TXCLK), 
   .phy_RXD(phy_RXD), .phy_RXDV(phy_RXDV), .phy_RXER(phy_RXER),
   .phy_RXCLK(phy_RXCLK)
   );

wire [7:0] tx_debug;
genvar i;
generate for(i=0; i<RAW_TXIF_COUNT; i=i+1) begin:tx_raw
  tx_raw U_tx
     (.usr_clk(usr_clk), .reset_n(reset_n), 
      .ifg(ifg), .jumboframes(jumboframes),
      .tx_data(tx_raw_data[32+(i*32)-1:i*32]), .tx_sof(tx_raw_sof[i]), 
      .tx_we(tx_raw_we[i]), .tx_stop(tx_raw_stop[i]),
      .tx_count(tx_raw_count[32+(i*32)-1:i*32]),
      .int_tx_clk(int_tx_clk),
      .int_tx_dout(int_tx_dout), .int_tx_en(int_tx_en), .int_tx_er(int_tx_er),
      .debug(tx_debug));
end
endgenerate

//generate for(i=0; i<STREAM_TXIF_COUNT; i=i+1) begin:tx_stream
//  tx_stream U_tx
//     (.usr_clk(usr_clk), .reset_n(reset_n), 
//      .src_mac_addr(tx_stream_src_mac_addr), .dst_mac_addr(tx_stream_dst_mac_addr),
//      .ethertype(tx_stream_ethertype),
//      .ifg(ifg), .jumboframes(jumboframes),
//      .tx_data(tx_stream_data[32+(i*32)-1:i*32]), 
//      .tx_eof(tx_stream_eof[4+(i*4)-1:i*4]), 
//      .tx_we(tx_stream_we[i]), .tx_stop(tx_stream_stop[i]),
//      .tx_count(tx_stream_count[32+(i*32)-1:i*32]),
//      .int_tx_clk(int_tx_clk),
//      .int_tx_dout(int_tx_dout), .int_tx_en(int_tx_en), .int_tx_er(int_tx_er),
//      .debug(tx_debug));
//end
//endgenerate

wire [17:0] rx_debug;
generate for(i=0; i<RAW_RXIF_COUNT; i=i+1) begin:rx_raw
  rx_raw #(RX_CKSM_CHECK) U_rx_raw
     (.usr_clk(usr_clk), .reset_n(reset_n), 
      .promiscuous(promiscuous), .jumboframes(jumboframes),
      .mac_addr_filter(rx_raw_mac_addr), .rx_count(rx_raw_count),
      .int_rx_clk(int_rx_clk),
      .int_rx_din(int_rx_din), .int_rx_dv(int_rx_dv), .int_rx_er(int_rx_er),
      .rx_data(rx_raw_data), .rx_sof(rx_raw_sof), 
      .rx_dv(rx_raw_dv), .rx_ack(rx_raw_ack),
      .debug(rx_debug));
end
endgenerate

mii_mgmt #(USR_CLK_PERIOD) U_miim
  (.clk(usr_clk), .reset_n(reset_n),
   .phyad(miim_phyad), .addr(miim_addr), 
   .wdata(miim_wdata), .rdata(miim_rdata),
   .req(miim_req), .we(miim_we), .ack(miim_ack),
   .mdc(phy_MDC), .mdio(phy_MDIO),
   .debug(), .debug_sel(1'b0));

assign debug = {6'b0, rx_debug, tx_debug};

endmodule


      
   
