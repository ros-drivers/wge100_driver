module rx_usr_if
(input clk, input reset_n,

 input [35:0] rxff_dout,
 input rxff_empty,
 output rxff_ack,

 input [13:0] rfq_dout,
 input rfq_dv,
 output rfq_ack,

 output [31:0] rx_data,
 output rx_dv,
 output rx_sof,
 input rx_ack,
 output [17:0] debug);


localparam SW = 2;
localparam ST_IDLE = 2'd0;
localparam ST_DATA = 2'd1;
localparam ST_DROP = 2'd2;
localparam ST_CRC  = 2'd3;

localparam CW = 4;
// ctrl[0] = rxff_ack
// ctrl[1] = rfq_ack
// ctrl[2] = rx_dv
// ctrl[3] = rx_sof
// ctrl[SW+CW-1:CW] = next state
reg [SW+CW-1:0] ctrl;
wire [SW-1:0] state;
wire [SW-1:0] next_state = ctrl[SW+CW-1:CW];
greg #(SW) state_reg(clk, next_state, 1'b0, ~reset_n, 1'b1, state);

wire rfq_fr_good = |rfq_dout;

wire [3:0] rx_eof = {rxff_dout[35], rxff_dout[26], rxff_dout[17], rxff_dout[8]};

always @* begin
   case (state)
   ST_IDLE:
     if (rfq_dv & ~rxff_empty)        //nxt_st   rx_sof rx_dv rfq_ack rxff_ack
       if (rfq_fr_good)         ctrl = {ST_DATA, 1'b1,  1'b1, 1'b0,   1'b1};
       else
         if (|rx_eof)           ctrl = {ST_IDLE, 1'b0,  1'b0, 1'b1,   1'b1};
         else                   ctrl = {ST_DROP, 1'b0,  1'b0, 1'b0,   1'b1};
     else                       ctrl = {ST_IDLE, 1'b0,  1'b0, 1'b1,   1'b0};
   ST_DATA:                                                           
     if (rxff_empty)            ctrl = {ST_DATA, 1'b0,  1'b0, 1'b0,   rx_ack};
     else                                                             
       if (|rx_eof & rx_ack)    ctrl = {ST_CRC,  1'b0,  1'b0, 1'b1,   1'b1};
       else                     ctrl = {ST_DATA, 1'b0,  1'b1, 1'b0,   rx_ack};
   ST_DROP:                                                           
     if (|rx_eof & ~rxff_empty) ctrl = {ST_CRC,  1'b0,  1'b0, 1'b1,   1'b1};
     else                       ctrl = {ST_DROP, 1'b0,  1'b0, 1'b0,   1'b1};
   ST_CRC:                                                            
     if (rfq_dv & ~rxff_empty)                                        
       if (rfq_fr_good)         ctrl = {ST_DATA, 1'b1,  1'b1, 1'b0,   1'b1};
       else                     ctrl = {ST_DROP, 1'b0,  1'b0, 1'b0,   1'b1};
     else                       ctrl = {ST_IDLE, 1'b0,  1'b0, 1'b0,   1'b0};
   default:                     ctrl = {ST_IDLE, 1'b0,  1'b0, 1'b0,   1'b0};
   endcase
end

assign rxff_ack = ctrl[0];
assign rfq_ack = ctrl[1];

// delay output one cycle to drop crc
// want to latch sof until they ack it
wire next_rx_sof = ctrl[3] | (rx_sof & ~rx_ack);
greg #(1) rx_sof_reg(clk, next_rx_sof, 1'b0, ~reset_n, 1'b1, rx_sof);

wire [31:0] next_rx_data = 
            next_rx_sof ? {2'b0, rfq_dout[13:0], 
                           rxff_dout[16:9], rxff_dout[7:0]} :
               {rxff_dout[34:27], rxff_dout[25:18], rxff_dout[16:9], rxff_dout[7:0]};
greg #(32) rx_data_reg(clk, next_rx_data, 1'b0, ~reset_n, rxff_ack, rx_data);

greg #(1) rx_dv_reg(clk, ctrl[2], 1'b0, ~reset_n, 1'b1, rx_dv);

assign debug = {7'b0, state, rxff_empty, rfq_dout, rfq_dv, rfq_ack, rx_dv, rx_ack};

endmodule
