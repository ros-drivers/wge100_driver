module tx_engine_stream
(input clk, input reset_n,

 input [47:0] src_mac_addr,
 input [47:0] dst_mac_addr,
 input [15:0] ethertype,
 input [9:0]  ifg,
 input        jumboframes,

 input [8:0] txff_dout,
 input txff_empty,
 output txff_rden,

 output crc_init,
 output crc_rden,
 output [7:0] crc_data,
 input [7:0] crc_crc,

 output [7:0] int_tx_dout,
 output       int_tx_en,
 output       int_tx_er,

 output [31:0] tx_count,
 output [3:0] debug
);

   
localparam SW = 4;
localparam ST_IDLE     = 4'd0;
localparam ST_PREAMBLE = 4'd1;
localparam ST_HEADER   = 4'd2;
localparam ST_DATA     = 4'd3;
localparam ST_PAD      = 4'd4;
localparam ST_FCS      = 4'd5;
localparam ST_DROP     = 4'd6;
localparam ST_DRAIN    = 4'd7;
localparam ST_IFG      = 4'd8;

localparam CW = 7;
// ctrl[0] = txff_rden
// ctrl[1] = tx_er
// ctrl[2] = tx_en
// ctrl[4:3] = tx_dout_sel, 0=preamble, 1=data, 2=header, 3=crc
// ctrl[5] = clear count
// ctrl[6] = incr. tx_count
// ctrl[SW+CW-1:CW] = next state
reg [SW+CW-1:0] ctrl;
wire [SW-1:0] state;
wire [SW-1:0] next_state = ctrl[SW+CW-1:CW];
greg #(SW) state_reg(clk, next_state, 1'b0, ~reset_n, 1'b1, state);

wire [13:0] count; // needs to go up to 9000
wire [13:0] count_p1 = count + 1;
greg #(14) count_reg(clk, count_p1, ctrl[5], 1'b0, 1'b1, count);

wire [31:0] tx_count_p1 = tx_count + 1;
greg #(32) tx_count_reg(clk, tx_count_p1, 1'b0, ~reset_n, ctrl[6], tx_count);

// some control signals that don't depend on state
// to avoid big paths from count -> control -> count_en, we register all of these
wire next_send_sfd = (count[2:0] == 6);
wire send_sfd;
wire next_sent_header = (count[3:0] == 12);
wire sent_header;
wire next_full_pkt = count > 45;
wire full_pkt;
wire next_pkt_ovf  = jumboframes ? count > 8999 : count > 1499;
wire pkt_ovf;
wire next_sent_fcs = (count[1:0] == 2);
wire sent_fcs;
wire next_ifg_done = (count[9:0] > ifg);
wire ifg_done;
greg #(6) ctrl_reg(clk,
                   {next_send_sfd, next_sent_header, next_full_pkt, next_pkt_ovf,
                    next_sent_fcs, next_ifg_done},
                   1'b0, ~reset_n, 1'b1,
                   {send_sfd, sent_header, full_pkt, pkt_ovf, sent_fcs, ifg_done});

wire txff_eof = txff_dout[8];

// TODO Carrier Extension necessary?

always @* begin
   case(state)
   ST_IDLE:                    //nxt_ste     tx_ct clr_ct txd_sel tx_en tx_er txff_rden
     if (txff_empty)    ctrl = {ST_IDLE,     1'b0, 1'b1,  2'd1,   1'b0, 1'b0, 1'b0};
     else               ctrl = {ST_PREAMBLE, 1'b0, 1'b1,  2'd1,   1'b0, 1'b0, 1'b0};
   ST_PREAMBLE:
     if (send_sfd)      ctrl = {ST_HEADER,   1'b0, 1'b1,  2'd0,   1'b1, 1'b0, 1'b0};
     else               ctrl = {ST_PREAMBLE, 1'b0, 1'b0,  2'd0,   1'b1, 1'b0, 1'b0};
   ST_HEADER:
     if (sent_header)   ctrl = {ST_DATA,     1'b0, 1'b1,  2'd2,   1'b1, 1'b0, 1'b0};
     else               ctrl = {ST_HEADER,   1'b0, 1'b0,  2'd2,   1'b1, 1'b0, 1'b0};   
   ST_DATA:
     if (txff_empty)    ctrl = {ST_DROP,     1'b0, 1'b0,  2'd1,   1'b1, 1'b1, 1'b0};
     else if (txff_eof)
       if (full_pkt)    ctrl = {ST_FCS,      1'b0, 1'b1,  2'd1,   1'b1, 1'b0, 1'b1};
       else             ctrl = {ST_PAD,      1'b0, 1'b0,  2'd1,   1'b1, 1'b0, 1'b1};
     else if (pkt_ovf)  ctrl = {ST_DRAIN,    1'b0, 1'b1,  2'd1,   1'b1, 1'b1, 1'b1}; 
     else               ctrl = {ST_DATA,     1'b0, 1'b0,  2'd1,   1'b1, 1'b0, 1'b1};   
   ST_PAD:
     if (full_pkt)      ctrl = {ST_FCS,      1'b0, 1'b1,  2'd1,   1'b1, 1'b0, 1'b0};
     else               ctrl = {ST_PAD,      1'b0, 1'b0,  2'd1,   1'b1, 1'b0, 1'b0};
   ST_FCS:
     if (sent_fcs)      ctrl = {ST_IFG,      1'b1, 1'b1,  2'd3,   1'b1, 1'b0, 1'b0};
     else               ctrl = {ST_FCS,      1'b0, 1'b0,  2'd3,   1'b1, 1'b0, 1'b0};
   ST_DROP:
     if (txff_eof)      ctrl = {ST_IFG,      1'b0, 1'b1,  2'd1,   1'b1, 1'b1, 1'b1};
     else if (pkt_ovf)  ctrl = {ST_DRAIN,    1'b0, 1'b1,  2'd1,   1'b1, 1'b1, 1'b1};
     else               ctrl = {ST_DROP,     1'b0, 1'b1,  2'd1,   1'b1, 1'b1, 1'b1};
   ST_DRAIN:
     if (txff_eof)      ctrl = {ST_IFG,      1'b0, 1'b0,  2'd1,   1'b0, 1'b0, 1'b1};
     else               ctrl = {ST_DRAIN,    1'b0, 1'b0,  2'd1,   1'b0, 1'b0, 1'b1};
   ST_IFG:
     if (ifg_done)
       if (txff_empty)  ctrl = {ST_IDLE,     1'b0, 1'b1,  2'd1,   1'b0, 1'b0, 1'b0};
       else             ctrl = {ST_PREAMBLE, 1'b0, 1'b1,  2'd1,   1'b0, 1'b0, 1'b0};
     else               ctrl = {ST_IFG,      1'b0, 1'b0,  2'd1,   1'b0, 1'b0, 1'b0};
   // to prevent latch:
   default:             ctrl = {ST_IDLE,     1'b0, 1'b1,  2'd1,   1'b0, 1'b0, 1'b0}; 
   endcase   
end

wire [7:0] preamble = {send_sfd, 7'h55};

wire [7:0] header_out;
gmux #(.DWIDTH(8), .SELWIDTH(4), .BIGENDIAN(1)) header_out_mux
  (.d({dst_mac_addr, src_mac_addr, ethertype, 16'b0}),
   .sel(count[3:0]),
   .z(header_out));

wire [7:0] next_tx_dout;
gmux #(.DWIDTH(8), .SELWIDTH(2)) tx_dout_mux
  (.d({crc_crc, header_out, txff_dout[7:0], preamble}),
   .sel(ctrl[4:3]),
   .z(next_tx_dout));

// we may have a couple of bytes of garbage left in the fifo after the end of frame
wire [1:0] align_count;
wire [1:0] align_count_p1 = align_count + 1;
wire align_count_en = txff_rden;
greg #(2) ac_reg(clk, align_count_p1, 1'b0, ~reset_n, align_count_en, align_count);

wire aligned = ~|align_count;
assign txff_rden = ctrl[0] | (~aligned & state == ST_IFG);

assign crc_init = (state == ST_PREAMBLE);
assign crc_rden = (state == ST_FCS);
assign crc_data = next_tx_dout;

greg #(8) tx_dout_reg(clk, next_tx_dout, 1'b0, 1'b0, 1'b1, int_tx_dout);
greg #(1) tx_en_reg(clk, ctrl[2], 1'b0, 1'b0, 1'b1, int_tx_en);
greg #(1) tx_er_reg(clk, ctrl[1], 1'b0, 1'b0, 1'b1, int_tx_er);

assign debug = state[3:0];
endmodule

 
