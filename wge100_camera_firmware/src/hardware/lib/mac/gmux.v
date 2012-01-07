// input is a concatenation of all inputs, with
// input "0" beginning at bit 0
module gmux
#(parameter DWIDTH = 1,
  parameter SELWIDTH = 2,
  parameter BIGENDIAN = 0,
  parameter TOT_DWIDTH = (DWIDTH << SELWIDTH),
  parameter NUM_WORDS = (1 << SELWIDTH))
(input [TOT_DWIDTH-1:0] d,
 input [SELWIDTH-1:0] sel,
 output [DWIDTH-1:0] z);

genvar b,w;
generate
   for (b=0;b<DWIDTH;b=b+1)
     begin:out
     wire [NUM_WORDS-1:0] tmp;
        for (w=0;w<NUM_WORDS;w=w+1)
          begin : mx
	     if (BIGENDIAN == 1)
	       assign tmp[NUM_WORDS-w-1] = d[w*DWIDTH+b];
	     else
               assign tmp[w] = d[w*DWIDTH+b];
          end
        assign z[b] = tmp[sel];
     end
endgenerate
endmodule

