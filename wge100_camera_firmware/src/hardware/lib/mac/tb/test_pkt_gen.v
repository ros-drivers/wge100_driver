module pkt_gen
#(parameter TARGET_MAC=48'h00_C0_A8_7F_8B_A4,
  parameter ETHERTYPE=16'h0800)
(input clk, input reset_n,

 input send_pkt,
 
 input  mac_tx_stop,
 output mac_tx_we,
 output [31:0] mac_tx_data,
 output [3:0] mac_tx_eof);

wire [0:31] ip_hdr[0:4];
assign ip_hdr[0][0:3]   = 4'h4;      // version
assign ip_hdr[0][4:7]   = 4'h5;      // header length
assign ip_hdr[0][8:15]  = 8'h00;     // TOS
assign ip_hdr[0][16:31] = 16'h0020;  // total length = 20 + 8 + 4 = 32
assign ip_hdr[1][0:15]  = 16'h0000;  // ID
assign ip_hdr[1][16:18] = 3'b000;    // flags
assign ip_hdr[1][19:31] = 13'h0000;  // fragment offset
assign ip_hdr[2][0:7]   = 8'h05;     // TTL
assign ip_hdr[2][8:15]  = 8'h11;     // protocol (= 17, udp)
assign ip_hdr[2][16:31] = 16'h2a7b;  // header checksum (http://mna.nlanr.net/Software/HEC/hec.html)
assign ip_hdr[3][0:31]  = 32'hc0a80501; // source ip
assign ip_hdr[4][0:31]  = 32'hc0a80502; // dest ip

wire [0:31] udp_hdr[0:1];
assign udp_hdr[0][0:15]  = 16'h0000; // source port
assign udp_hdr[0][16:31] = 16'hcafe; // dest port
assign udp_hdr[1][0:15]  = 16'h000C; // length 8 hdr + 4 data
assign udp_hdr[1][16:31] = 16'h0000; // checksum (0 = unused)

wire [3:0] count;
wire [3:0] count_p1 = count + 1;
wire pkt_done = (count == 4'd9);
greg #(4) r1(clk, count_p1, pkt_done, ~reset_n, mac_tx_we, count);

wire sending_pkt;
wire next_sending_pkt = send_pkt | (sending_pkt & ~pkt_done);
greg #(1) sending_reg(clk, next_sending_pkt, 1'b0, ~reset_n, 1'b1, sending_pkt);

wire [31:0] data;
wire [31:0] data_p1 = data + 1;
greg #(32) r2(clk, data_p1, 1'b0, ~reset_n, pkt_done, data);

wire [31:0] data_to_send;
gmux #(32, 4, 1) data_mux
  (.d({TARGET_MAC, ETHERTYPE,
       ip_hdr[0], ip_hdr[1], ip_hdr[2], ip_hdr[3], ip_hdr[4],
       udp_hdr[0], udp_hdr[1], 
       data, {6{32'b0}}}),
   .sel(count),
   .z(data_to_send));

assign mac_tx_data = data_to_send;
assign mac_tx_eof = {3'b0, pkt_done}; 
assign mac_tx_we = ~mac_tx_stop & sending_pkt;

endmodule
