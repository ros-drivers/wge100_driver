module rx_raw
#(parameter CKSM_CHK=1)
(input usr_clk,
 input reset_n,

 // config
 input        promiscuous,
 input        jumboframes,
 input [47:0] mac_addr_filter,
 output [31:0] rx_count,

 // phy interface
 input       int_rx_clk,
 input [7:0] int_rx_din,
 input       int_rx_dv,
 input       int_rx_er,

 // user interface
 output [31:0] rx_data,
 output        rx_sof,
 output        rx_dv,
 input         rx_ack,

 output [17:0] debug);

         
wire crc_init;
wire [7:0] crc_data;
wire crc_good;
generate if (CKSM_CHK == 1) begin
   crc_chk U_crc_chk
     (.clk(int_rx_clk), .reset_n(reset_n), 
      .init(crc_init), .data(crc_data), .good(crc_good));
end
else begin
   assign crc_good = 1'b1;
end
endgenerate

wire rfq_we;
wire [13:0] rfq_din;
wire rfq_ready;
wire rfq_dv;
wire [13:0] rfq_dout;
wire rfq_ack;
wire rfq_empty;
wire rfq_full;

rx_pkt_fifo U_rx_pkt_fifo
  (.reset_n(reset_n), .int_rx_clk(int_rx_clk), .usr_clk(usr_clk),
   .we(rfq_we), .din(rfq_din), .ready(rfq_ready),
   .dv(rfq_dv), .dout(rfq_dout), .ack(rfq_ack),
   .fifo_empty(rfq_empty), .fifo_full(rfq_full));

wire [35:0] rxff_din;
wire rxff_we;
wire rxff_almost_full;
wire [35:0] rxff_dout;
wire rxff_empty;
wire rxff_ack;
wire rxff_full;
rxfifo U_rxfifo
  (.rst(~reset_n),
   .wr_clk(int_rx_clk), .din(rxff_din), .wr_en(rxff_we), 
   .full(rxff_full), .almost_full(rxff_almost_full),
   .rd_clk(usr_clk), .dout(rxff_dout), .rd_en(rxff_ack), .empty(rxff_empty));
 
rx_engine_raw U_rx_engine_raw
  (.clk(int_rx_clk), .reset_n(reset_n),
   .promiscuous(promiscuous), .jumboframes(jumboframes),
   .mac_addr_filter(mac_addr_filter), .rx_count(rx_count),
   .int_rx_din(int_rx_din), .int_rx_dv(int_rx_dv), .int_rx_er(int_rx_er),
   .crc_init(crc_init), .crc_data(crc_data), .crc_good(crc_good),
   .rxff_din(rxff_din), .rxff_we(rxff_we), .rxff_almost_full(rxff_almost_full),
   .rfq_din(rfq_din), .rfq_we(rfq_we), .rfq_ready(rfq_ready));

rx_usr_if U_rx_usr_if
  (.clk(usr_clk), .reset_n(reset_n),
   .rxff_dout(rxff_dout), .rxff_empty(rxff_empty), .rxff_ack(rxff_ack),
   .rfq_dout(rfq_dout), .rfq_dv(rfq_dv), .rfq_ack(rfq_ack),
   .rx_data(rx_data), .rx_dv(rx_dv), .rx_sof(rx_sof), .rx_ack(rx_ack),
   .debug());
assign debug = { rfq_full, rfq_empty };
endmodule
