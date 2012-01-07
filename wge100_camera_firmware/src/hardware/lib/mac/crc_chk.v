module crc_chk
(input clk, input reset_n,
 input init,
 input [7:0] data,
 output good);

wire [31:0] crc;
wire [31:0] next_crc;
greg #(32, 32'hffffffff) crc_reg(clk, next_crc, init, ~reset_n, 1'b1, crc);
assign good = (crc == 32'hc704dd7b);

wire [7:0] D = data;
wire [31:0] C = crc;   
assign next_crc[0]  = C[24]^C[30]^D[1]^D[7];								   
assign next_crc[1]  = C[25]^C[31]^D[0]^D[6]^C[24]^C[30]^D[1]^D[7];					   
assign next_crc[2]  = C[26]^D[5]^C[25]^C[31]^D[0]^D[6]^C[24]^C[30]^D[1]^D[7];				   
assign next_crc[3]  = C[27]^D[4]^C[26]^D[5]^C[25]^C[31]^D[0]^D[6];					   
assign next_crc[4]  = C[28]^D[3]^C[27]^D[4]^C[26]^D[5]^C[24]^C[30]^D[1]^D[7];				   
assign next_crc[5]  = C[29]^D[2]^C[28]^D[3]^C[27]^D[4]^C[25]^C[31]^D[0]^D[6]^C[24]^C[30]^D[1]^D[7];	   
assign next_crc[6]  = C[30]^D[1]^C[29]^D[2]^C[28]^D[3]^C[26]^D[5]^C[25]^C[31]^D[0]^D[6];		   
assign next_crc[7]  = C[31]^D[0]^C[29]^D[2]^C[27]^D[4]^C[26]^D[5]^C[24]^D[7];				   
assign next_crc[8]  = C[0]^C[28]^D[3]^C[27]^D[4]^C[25]^D[6]^C[24]^D[7];				   
assign next_crc[9]  = C[1]^C[29]^D[2]^C[28]^D[3]^C[26]^D[5]^C[25]^D[6];				   
assign next_crc[10] = C[2]^C[29]^D[2]^C[27]^D[4]^C[26]^D[5]^C[24]^D[7];				   
assign next_crc[11] = C[3]^C[28]^D[3]^C[27]^D[4]^C[25]^D[6]^C[24]^D[7];				   
assign next_crc[12] = C[4]^C[29]^D[2]^C[28]^D[3]^C[26]^D[5]^C[25]^D[6]^C[24]^C[30]^D[1]^D[7];		   
assign next_crc[13] = C[5]^C[30]^D[1]^C[29]^D[2]^C[27]^D[4]^C[26]^D[5]^C[25]^C[31]^D[0]^D[6];		   
assign next_crc[14] = C[6]^C[31]^D[0]^C[30]^D[1]^C[28]^D[3]^C[27]^D[4]^C[26]^D[5];			   
assign next_crc[15] = C[7]^C[31]^D[0]^C[29]^D[2]^C[28]^D[3]^C[27]^D[4];				   
assign next_crc[16] = C[8]^C[29]^D[2]^C[28]^D[3]^C[24]^D[7];						   
assign next_crc[17] = C[9]^C[30]^D[1]^C[29]^D[2]^C[25]^D[6];						   
assign next_crc[18] = C[10]^C[31]^D[0]^C[30]^D[1]^C[26]^D[5];						   
assign next_crc[19] = C[11]^C[31]^D[0]^C[27]^D[4];							   
assign next_crc[20] = C[12]^C[28]^D[3];								   
assign next_crc[21] = C[13]^C[29]^D[2];								   
assign next_crc[22] = C[14]^C[24]^D[7];								   
assign next_crc[23] = C[15]^C[25]^D[6]^C[24]^C[30]^D[1]^D[7];						   
assign next_crc[24] = C[16]^C[26]^D[5]^C[25]^C[31]^D[0]^D[6];						   
assign next_crc[25] = C[17]^C[27]^D[4]^C[26]^D[5];							   
assign next_crc[26] = C[18]^C[28]^D[3]^C[27]^D[4]^C[24]^C[30]^D[1]^D[7];				   
assign next_crc[27] = C[19]^C[29]^D[2]^C[28]^D[3]^C[25]^C[31]^D[0]^D[6];				   
assign next_crc[28] = C[20]^C[30]^D[1]^C[29]^D[2]^C[26]^D[5];						   
assign next_crc[29] = C[21]^C[31]^D[0]^C[30]^D[1]^C[27]^D[4];						   
assign next_crc[30] = C[22]^C[31]^D[0]^C[28]^D[3];							   
assign next_crc[31] = C[23]^C[29]^D[2];                                                                   

endmodule

