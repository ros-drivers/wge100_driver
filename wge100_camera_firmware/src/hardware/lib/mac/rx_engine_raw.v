module rx_engine_raw
(input clk, input reset_n,

 input        promiscuous,
 input        jumboframes,
 input [47:0] mac_addr_filter,
 output [31:0] rx_count,

 input [7:0] int_rx_din,
 input       int_rx_dv,
 input       int_rx_er,

 output       crc_init,
 output [7:0] crc_data,
 input        crc_good,

 output [35:0] rxff_din,
 output       rxff_we,
 input        rxff_almost_full,

 output [13:0] rfq_din,
 output        rfq_we,
 input         rfq_ready);

localparam SW = 3;
localparam ST_IDLE     = 3'd0;
localparam ST_PREAMBLE = 3'd1;
localparam ST_DMAC     = 3'd2;
localparam ST_DMAC_CHK = 3'd3;
localparam ST_DATA     = 3'd4;
localparam ST_DROP     = 3'd5;

localparam CW = 5;
// ctrl[0] = eof
// ctrl[1] = rfq_we
// ctrl[2] = rfq_din
// ctrl[3] = rxff_reg_we,
// ctrl[4] = clear count
// ctrl[SW+CW-1:CW] = next state

reg [SW+CW-1:0] ctrl;
wire [SW-1:0] state;
wire [SW-1:0] next_state = ctrl[SW+CW-1:CW];
greg #(SW) state_reg(clk, next_state, 1'b0, ~reset_n, 1'b1, state);

wire [13:0] count; // needs to go up to over 9000
wire [13:0] count_p1 = count + 1; // only incr count if we are writing to rxfifo
greg #(14) count_reg(clk, count_p1, ctrl[4], 1'b0, |ctrl[4:3], count);

wire start_frame = int_rx_dv & (int_rx_din == 8'h55);
wire sfd = (int_rx_din == 8'hd5);
wire got_mac = (count[2:0] == 7); // add two bytes for size

wire [47:0] mac_rd;
wire [47:0] next_mac_rd = {mac_rd[40:0], int_rx_din};
wire mac_reg_en = (state == ST_DMAC); 
greg #(48) mac_reg(clk, next_mac_rd, 1'b0, ~reset_n, mac_reg_en, mac_rd);
wire mac_equal = (mac_rd == mac_addr_filter);
wire mac_bcast = &mac_rd;
wire mac_okay = promiscuous | mac_equal | mac_bcast;
wire dmac_error = int_rx_er | ~mac_okay;

wire too_short = ~int_rx_dv & (count < 66); // 2 size, 6 dest, 6 source, 2 ethertype, 46 payload, 4 crc
wire too_long  = jumboframes ? count > 9020 : count > 1520; // 2 + 6 + 6 + 2 + (1500 or 9000) + 4
wire crc_error = ~int_rx_dv & ~crc_good;
wire rx_problem = int_rx_er | too_short | crc_error | rxff_almost_full | ~rfq_ready;

// note: two bytes are written, one on start_frame and one one sfd
// to offset the data by two bytes to make room for the size field at
// the beginning

always @* begin
   case(state)
   //                                                                c r r r
   //                                                                l x f f e
   //                                                                c w d w o
   ST_IDLE: //                                                       t e i e f
     if (start_frame)
       if (rx_problem)                       ctrl = {ST_DROP,     5'b1_0_0_0_0};
       else                                  ctrl = {ST_PREAMBLE, 5'b0_1_0_0_0};
     else                                    ctrl = {ST_IDLE,     5'b1_0_0_0_0};
   ST_PREAMBLE:
     if (rx_problem)                         ctrl = {ST_DROP,     5'b0_0_0_1_1};
     else if (sfd)                           ctrl = {ST_DMAC,     5'b0_1_0_0_0};
     else                                    ctrl = {ST_PREAMBLE, 5'b0_0_0_0_0};
   ST_DMAC:
     if (rx_problem)                         ctrl = {ST_DROP,     5'b0_0_0_1_1};
     else if (got_mac)                       ctrl = {ST_DMAC_CHK, 5'b0_1_0_0_0};   
     else                                    ctrl = {ST_DMAC,     5'b0_1_0_0_0};
   ST_DMAC_CHK:
     if (rx_problem | ~mac_okay)             ctrl = {ST_DROP,     5'b0_0_0_1_1};
     else                                    ctrl = {ST_DATA,     5'b0_1_0_0_0}; 
   ST_DATA:
     if (rx_problem)                         ctrl = {ST_DROP,     5'b0_0_0_1_1};
     else if (~int_rx_dv)                    ctrl = {ST_IDLE,     5'b1_0_1_1_1};
     else if (too_long)                      ctrl = {ST_DROP,     5'b0_0_0_1_1};
     else                                    ctrl = {ST_DATA,     5'b0_1_0_0_0};
   ST_DROP:
     if (~int_rx_dv)                         ctrl = {ST_IDLE,     5'b1_0_0_0_0};
     else                                    ctrl = {ST_DROP,     5'b1_0_0_0_0};   
   default:                                  ctrl = {ST_IDLE,     5'b1_0_0_0_0};
   endcase
end

assign crc_init = (state == ST_PREAMBLE);
assign crc_data = int_rx_din;

wire [7:0] next_rxff_reg_din = int_rx_din;
wire next_rxff_reg_we = ctrl[3];
wire [3:0] rxff_reg_byte;
wire [3:0] next_rxff_reg_byte = ctrl[0] ? 4'h1 : {rxff_reg_byte[2:0], rxff_reg_byte[3]};
greg #(4, 4'h1) rxff_reg_byte_reg
  (clk, next_rxff_reg_byte, 1'b0, ~reset_n, ctrl[0] | ctrl[3], rxff_reg_byte);

wire [3:0] rxff_reg_bwe = rxff_reg_byte & {4{ctrl[3]}};
wire rxff_reg_newword;

greg #(8) rxff_reg_0
  (clk, next_rxff_reg_din, 1'b0, ~reset_n, rxff_reg_bwe[0], rxff_din[34:27]);
greg #(8) rxff_reg_1
  (clk, next_rxff_reg_din, 1'b0, ~reset_n, rxff_reg_bwe[1], rxff_din[25:18]);
greg #(8) rxff_reg_2
  (clk, next_rxff_reg_din, 1'b0, ~reset_n, rxff_reg_bwe[2], rxff_din[16:9]);
greg #(8) rxff_reg_3
  (clk, next_rxff_reg_din, 1'b0, ~reset_n, rxff_reg_bwe[3], rxff_din[7:0]);
greg #(1) rxff_reg_nw_reg
  (clk, rxff_reg_bwe[3], 1'b0, ~reset_n, 1'b1, rxff_reg_newword);

// when eof is raised, it actually means the last byte written was the end
wire [3:0] rxff_eof = {rxff_reg_byte[0], rxff_reg_byte[3:1]} & {4{ctrl[0]}};
assign rxff_din[35] = rxff_eof[0];
assign rxff_din[26] = rxff_eof[1];
assign rxff_din[17] = rxff_eof[2];
assign rxff_din[8]  = rxff_eof[3];

assign rxff_we = ctrl[0] | rxff_reg_newword;

// rfq_ready should always be ready when we want to write... but just in case,
// give one more level of buffering
// subtract 4 from count for the CRC and 2 for the size
wire [13:0] next_rfq_din = (count - 14'd6) & {14{ctrl[2]}};
wire next_rfq_we  = ctrl[1];
wire rfq_wr_req;
greg #(14) rfq_din_reg(clk, next_rfq_din, 1'b0, ~reset_n, next_rfq_we, rfq_din);
greg #(1) rfq_we_reg(clk, 1'b1, rfq_we, ~reset_n, next_rfq_we | rfq_we, rfq_wr_req);
assign rfq_we = rfq_wr_req & rfq_ready;

wire [31:0] rx_count_p1 = rx_count + 1;
wire incr_rx_count = rfq_we & |rfq_din;
greg #(32) rx_count_reg(clk, rx_count_p1, 1'b0, ~reset_n, incr_rx_count, rx_count);

endmodule


     
