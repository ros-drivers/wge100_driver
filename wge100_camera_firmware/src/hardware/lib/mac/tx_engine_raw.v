module tx_engine_raw
(input clk, input reset_n,

 input [9:0]  ifg,
 input        jumboframes,

 input [7:0] txff_dout,
 input       txff_sof,
 input       txff_empty,
 output      txff_rden,

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
localparam ST_SIZE     = 4'd1;
localparam ST_PREAMBLE = 4'd2;
localparam ST_DATA     = 4'd3;
localparam ST_PAD      = 4'd4;
localparam ST_FCS      = 4'd5;
localparam ST_DRAIN    = 4'd6;
localparam ST_IFG      = 4'd7;

localparam CW = 7;
// ctrl[0] = txff_rden
// ctrl[1] = tx_er
// ctrl[2] = tx_en
// ctrl[4:3] = tx_dout_sel, 0=preamble, 1=data, 2=header, 3=crc
// ctrl[5] = clear count
// ctrl[6] = size we
// ctrl[SW+CW-1:CW] = next state
reg [SW+CW-1:0] ctrl;
wire [SW-1:0] state;
wire [SW-1:0] next_state = ctrl[SW+CW-1:CW];
greg #(SW) state_reg(clk, next_state, 1'b0, ~reset_n, 1'b1, state);

wire [13:0] count; // needs to go up to 9000
wire [13:0] count_p1 = count + 1;
greg #(14) count_reg(clk, count_p1, ctrl[5], 1'b0, 1'b1, count);

// first two bytes that come in are the size, store it
wire [15:0] size;
greg #(16) size_reg(clk, {size[7:0], txff_dout}, 1'b0, ~reset_n, ctrl[6], size);

// some control signals that don't depend on state
// to avoid big paths from count -> control -> count_en, we register all of these
wire next_send_sfd = (count[2:0] == 6);
wire send_sfd;

wire next_fr_full = count > 57;
wire fr_full;

wire next_fr_ovf  = jumboframes ? count > 9013 : count > 1513;
wire fr_ovf;

wire next_sent_fcs = (count[1:0] == 2);
wire sent_fcs;

wire next_fr_done = (count == (size[13:0] - 2));
wire fr_done;

wire next_ifg_done = (count[9:0] > ifg);
wire ifg_done;

greg #(6) ctrl_reg(clk,
                   {next_send_sfd, next_fr_full, next_fr_ovf,
                    next_fr_done, next_sent_fcs, next_ifg_done},
                   1'b0, ~reset_n, 1'b1,
                   {send_sfd, fr_full, fr_ovf, fr_done, sent_fcs, ifg_done});

always @* begin
   case(state)
   ST_IDLE:                    //nxt_ste      sz_we clr_ct txd_sel tx_en tx_er txff_rden
     if (txff_empty)     ctrl = {ST_IDLE,     1'b0, 1'b1,  2'd1,   1'b0, 1'b0, 1'b0};
     else if (txff_sof)  ctrl = {ST_SIZE,     1'b1, 1'b1,  2'd1,   1'b0, 1'b0, 1'b1};
     else                ctrl = {ST_IDLE,     1'b0, 1'b1,  2'd1,   1'b0, 1'b0, 1'b1}; // oops, out of sync
   ST_SIZE:              ctrl = {ST_PREAMBLE, 1'b1, 1'b0,  2'd1,   1'b0, 1'b0, 1'b1};
   ST_PREAMBLE:
     if (send_sfd)       ctrl = {ST_DATA,     1'b0, 1'b1,  2'd0,   1'b1, 1'b0, 1'b0};
     else                ctrl = {ST_PREAMBLE, 1'b0, 1'b0,  2'd0,   1'b1, 1'b0, 1'b0};
   ST_DATA:
     if (txff_empty)     ctrl = {ST_DRAIN,    1'b0, 1'b1,  2'd1,   1'b1, 1'b1, 1'b0}; // gap in input
     else if (fr_done)
       if (fr_full)      ctrl = {ST_FCS,      1'b0, 1'b1,  2'd1,   1'b1, 1'b0, 1'b0}; // just right
       else              ctrl = {ST_PAD,      1'b0, 1'b0,  2'd1,   1'b1, 1'b0, 1'b1}; // too short
     else if (txff_sof)  ctrl = {ST_IFG,      1'b0, 1'b1,  2'd1,   1'b1, 1'b1, 1'b0}; // sof too early
     else if (fr_ovf)    ctrl = {ST_DRAIN,    1'b0, 1'b1,  2'd1,   1'b1, 1'b1, 1'b1}; // too long
     else                ctrl = {ST_DATA,     1'b0, 1'b0,  2'd1,   1'b1, 1'b0, 1'b1}; // still going
   ST_PAD:
     if (fr_full)        ctrl = {ST_FCS,      1'b0, 1'b1,  2'd1,   1'b1, 1'b0, 1'b0};
     else                ctrl = {ST_PAD,      1'b0, 1'b0,  2'd1,   1'b1, 1'b0, 1'b0};
   ST_FCS:
     if (sent_fcs)       ctrl = {ST_DRAIN,    1'b0, 1'b1,  2'd3,   1'b1, 1'b0, 1'b0};
     else                ctrl = {ST_FCS,      1'b0, 1'b0,  2'd3,   1'b1, 1'b0, 1'b0};
   ST_DRAIN:
     if (txff_sof |
         txff_empty)     ctrl = {ST_IFG,      1'b0, 1'b0,  2'd1,   1'b0, 1'b0, 1'b0};
     else                ctrl = {ST_DRAIN,    1'b0, 1'b0,  2'd1,   1'b0, 1'b0, 1'b1};
   ST_IFG:
     if (ifg_done)
       if (txff_empty)   ctrl = {ST_IDLE,     1'b0, 1'b1,  2'd1,   1'b0, 1'b0, 1'b0};
       else              ctrl = {ST_SIZE,     1'b1, 1'b1,  2'd1,   1'b0, 1'b0, 1'b1};
     else                ctrl = {ST_IFG,      1'b0, 1'b0,  2'd1,   1'b0, 1'b0, 1'b0};
   // to prevent latch:
   default:              ctrl = {ST_IDLE,     1'b0, 1'b1,  2'd1,   1'b0, 1'b0, 1'b0}; 
   endcase   
end

wire [7:0] preamble = {send_sfd, 7'h55};

wire [7:0] next_tx_dout;
gmux #(.DWIDTH(8), .SELWIDTH(2)) tx_dout_mux
  (.d({crc_crc, 8'h0, txff_dout[7:0], preamble}),
   .sel(ctrl[4:3]),
   .z(next_tx_dout));

assign txff_rden = ctrl[0];

assign crc_init = (state == ST_PREAMBLE);
assign crc_rden = (state == ST_FCS);
assign crc_data = next_tx_dout;

greg #(8) tx_dout_reg(clk, next_tx_dout, 1'b0, 1'b0, 1'b1, int_tx_dout);
greg #(1) tx_en_reg(clk, ctrl[2], 1'b0, 1'b0, 1'b1, int_tx_en);
greg #(1) tx_er_reg(clk, ctrl[1], 1'b0, 1'b0, 1'b1, int_tx_er);

wire [31:0] tx_count_p1 = tx_count + 1;
wire        tx_count_en = (state == ST_FCS) & sent_fcs;
greg #(32) tx_count_reg(clk, tx_count_p1, 1'b0, ~reset_n, tx_count_en, tx_count);

assign debug = { txff_empty, state[2:0]};
endmodule

 
