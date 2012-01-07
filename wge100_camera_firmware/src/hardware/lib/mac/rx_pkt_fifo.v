module rx_pkt_fifo
(input usr_clk,
 input int_rx_clk,
 input reset_n,

 input we,
 input [13:0] din,
 output ready,
 output dv,
 output [13:0] dout,
 input ack,
 output fifo_empty,
 output fifo_full);

wire sync_ack;

// latch we/din in int_rx_clk domain
wire [3:0] w;
wire [13:0] d[0:2];

assign ready = ~w[0];
wire wd_r0_en = sync_ack | ready; // enable if we havn't latched or are clearing
greg #(15) wd_r0(int_rx_clk, {we, din}, sync_ack, ~reset_n, wd_r0_en, {w[0], d[0]});

// sync we/din into usr_clk domain
greg #(15) wd_r1(usr_clk, {w[0], d[0]}, 1'b0, ~reset_n, 1'b1, {w[1], d[1]});
greg #(15) wd_r2(usr_clk, {w[1], d[1]}, 1'b0, ~reset_n, 1'b1, {w[2], d[2]});

// delay valid one more so we can detect rising edge
greg #(1) w_r2(usr_clk, w[2], 1'b0, ~reset_n, 1'b1, w[3]);

wire [13:0] fifo_dout;
// wire fifo_empty;
// wire fifo_full;
wire [13:0] fifo_din = d[2];
wire fifo_wr_en = w[2] & ~w[3] & ~fifo_full;
wire fifo_rd_en = ack;
rx_pkt_fifo_sync U_rx_pkt_fifo_sync
  (.clk(usr_clk), .rst(~reset_n),
   .din(fifo_din), .rd_en(fifo_rd_en), .wr_en(fifo_wr_en),
   .dout(fifo_dout), .empty(fifo_empty), .full(fifo_full));

// ack when they want to write and we can
wire [3:0] a;
assign a[0] = w[3] & ~fifo_full;
// sync ack into int_rx_clk and detect rising edge to clear initial reg
greg #(1) a_r1(int_rx_clk, a[0], 1'b0, ~reset_n, 1'b1, a[1]);
greg #(1) a_r2(int_rx_clk, a[1], 1'b0, ~reset_n, 1'b1, a[2]);
greg #(1) a_r3(int_rx_clk, a[2], 1'b0, ~reset_n, 1'b1, a[3]);
assign sync_ack = a[2] & ~a[3];

assign dv = ~fifo_empty;
assign dout = fifo_dout;
endmodule



