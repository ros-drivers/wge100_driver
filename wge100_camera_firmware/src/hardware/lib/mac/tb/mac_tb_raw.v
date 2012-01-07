`timescale 1ns / 1ns

// Very dumb mirror test. Mirrors exactly what is received by the phy
// to the transmission port (header and all). If the register stall is
// 0, then mirroring is stalled.

module mac_tb;
parameter GMII = 1'b1;
parameter PROMISCUOUS = 1'b0;
parameter JUMBOFRAMES = 1'b1;

initial begin
   $dumpfile("mac_tb_raw.vcd");
   $dumpvars;
end


wire usr_clk;
wire reset_n;
fake_clk_rn #(100) usr_clk_gen(usr_clk, reset_n);

wire clk_125;
fake_clk #(125) clk_125_gen(clk_125);


reg stall;
initial begin
   stall = 1;
   #20000;
   stall = 0;
   #100000 $finish;
end

// PHY interface
wire [7:0] txd;
wire tx_en;
wire gtx_clk;
wire tx_er;
wire tx_clk;
wire rx_clk;
wire [7:0] rxd;
wire rx_er;
wire rx_dv;
fake_phy U_fake_phy 
  (.gmii(GMII), 
   .TXD(txd), .TXEN(tx_en), .GTXCLK(gtx_clk), .TXER(tx_er), .TXCLK(tx_clk),
   .RXCLK(rx_clk), .RXD(rxd), .RXER(rx_er), .RXDV(rx_dv), 
   .RESET_N(reset_n));

wire [31:0] mac_rx_data;
wire mac_rx_sof;
wire mac_tx_stop;
wire mac_rx_dv;
wire mac_rx_ack = (mac_tx_stop | stall) ? 1'b0 : mac_rx_dv;
wire [31:0] mac_tx_count;
mac U_mac
  (.usr_clk(usr_clk), .reset_n(reset_n), .clk_125(clk_125),
   .gmii(GMII), .promiscuous(PROMISCUOUS), .jumboframes(JUMBOFRAMES), .ifg(10'h0d),
   .tx_raw_data(mac_rx_data), .tx_raw_sof(mac_rx_sof), 
   .tx_raw_we(mac_rx_ack), .tx_raw_stop(mac_tx_stop), .tx_raw_count(mac_tx_count),
   .rx_raw_data(mac_rx_data), .rx_raw_sof(mac_rx_sof), 
   .rx_raw_dv(mac_rx_dv), .rx_raw_ack(mac_rx_ack),
   .rx_raw_mac_addr(48'h001122334455),
   .miim_addr(5'h0), .miim_wdata(16'h0), .miim_req(1'b0), .miim_we(1'b0),
   .miim_ack(), .miim_rdata(),
   .phy_TXD(txd), .phy_TXEN(tx_en), .phy_GTXCLK(gtx_clk), .phy_TXER(tx_er), 
   .phy_TXCLK(tx_clk), .phy_RXCLK(rx_clk),
   .phy_RXD(rxd), .phy_RXER(rx_er), .phy_RXDV(rx_dv),
   .phy_RESET_N(), .phy_MDC(), .phy_MDIO());
   
endmodule
