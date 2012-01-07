module crc_gen
  ( input clk, input reset_n,
    input init,
    input [7:0] data,
    input rden,
    output [7:0] crc);

wire [31:0] crc_int;
wire [31:0] new_crc;
wire [31:0] next_crc = rden ? {crc_int[23:0], 8'hff} : new_crc;
greg #(32, 32'hffffffff) crc_reg(clk, next_crc, init, ~reset_n, 1'b1, crc_int);

genvar i;
generate
   for (i=0;i<8;i=i+1) begin:out
      assign crc[i] = ~crc_int[31-i];
   end
endgenerate                                          

wire [7:0] D = data;
wire [31:0] C = crc_int;   
assign new_crc[0]  = C[24]^C[30]^D[1]^D[7];								   
assign new_crc[1]  = C[25]^C[31]^D[0]^D[6]^C[24]^C[30]^D[1]^D[7];					   
assign new_crc[2]  = C[26]^D[5]^C[25]^C[31]^D[0]^D[6]^C[24]^C[30]^D[1]^D[7];				   
assign new_crc[3]  = C[27]^D[4]^C[26]^D[5]^C[25]^C[31]^D[0]^D[6];					   
assign new_crc[4]  = C[28]^D[3]^C[27]^D[4]^C[26]^D[5]^C[24]^C[30]^D[1]^D[7];				   
assign new_crc[5]  = C[29]^D[2]^C[28]^D[3]^C[27]^D[4]^C[25]^C[31]^D[0]^D[6]^C[24]^C[30]^D[1]^D[7];	   
assign new_crc[6]  = C[30]^D[1]^C[29]^D[2]^C[28]^D[3]^C[26]^D[5]^C[25]^C[31]^D[0]^D[6];		   
assign new_crc[7]  = C[31]^D[0]^C[29]^D[2]^C[27]^D[4]^C[26]^D[5]^C[24]^D[7];				   
assign new_crc[8]  = C[0]^C[28]^D[3]^C[27]^D[4]^C[25]^D[6]^C[24]^D[7];				   
assign new_crc[9]  = C[1]^C[29]^D[2]^C[28]^D[3]^C[26]^D[5]^C[25]^D[6];				   
assign new_crc[10] = C[2]^C[29]^D[2]^C[27]^D[4]^C[26]^D[5]^C[24]^D[7];				   
assign new_crc[11] = C[3]^C[28]^D[3]^C[27]^D[4]^C[25]^D[6]^C[24]^D[7];				   
assign new_crc[12] = C[4]^C[29]^D[2]^C[28]^D[3]^C[26]^D[5]^C[25]^D[6]^C[24]^C[30]^D[1]^D[7];		   
assign new_crc[13] = C[5]^C[30]^D[1]^C[29]^D[2]^C[27]^D[4]^C[26]^D[5]^C[25]^C[31]^D[0]^D[6];		   
assign new_crc[14] = C[6]^C[31]^D[0]^C[30]^D[1]^C[28]^D[3]^C[27]^D[4]^C[26]^D[5];			   
assign new_crc[15] = C[7]^C[31]^D[0]^C[29]^D[2]^C[28]^D[3]^C[27]^D[4];				   
assign new_crc[16] = C[8]^C[29]^D[2]^C[28]^D[3]^C[24]^D[7];						   
assign new_crc[17] = C[9]^C[30]^D[1]^C[29]^D[2]^C[25]^D[6];						   
assign new_crc[18] = C[10]^C[31]^D[0]^C[30]^D[1]^C[26]^D[5];						   
assign new_crc[19] = C[11]^C[31]^D[0]^C[27]^D[4];							   
assign new_crc[20] = C[12]^C[28]^D[3];								   
assign new_crc[21] = C[13]^C[29]^D[2];								   
assign new_crc[22] = C[14]^C[24]^D[7];								   
assign new_crc[23] = C[15]^C[25]^D[6]^C[24]^C[30]^D[1]^D[7];						   
assign new_crc[24] = C[16]^C[26]^D[5]^C[25]^C[31]^D[0]^D[6];						   
assign new_crc[25] = C[17]^C[27]^D[4]^C[26]^D[5];							   
assign new_crc[26] = C[18]^C[28]^D[3]^C[27]^D[4]^C[24]^C[30]^D[1]^D[7];				   
assign new_crc[27] = C[19]^C[29]^D[2]^C[28]^D[3]^C[25]^C[31]^D[0]^D[6];				   
assign new_crc[28] = C[20]^C[30]^D[1]^C[29]^D[2]^C[26]^D[5];						   
assign new_crc[29] = C[21]^C[31]^D[0]^C[30]^D[1]^C[27]^D[4];						   
assign new_crc[30] = C[22]^C[31]^D[0]^C[28]^D[3];							   
assign new_crc[31] = C[23]^C[29]^D[2];                                                                   

endmodule
   
