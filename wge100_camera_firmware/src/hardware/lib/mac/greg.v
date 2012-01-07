module greg
#(parameter WIDTH=1,
  parameter INIT={WIDTH{1'b0}})
(input clk,
 input [WIDTH-1:0] d,
 input sclr,
 input aclr,
 input en,
 output reg [WIDTH-1:0] q);

always @(posedge clk or posedge aclr) begin
   if (aclr) q <= INIT;
   else begin
      if (en) begin
         if (sclr) q <= INIT;
         else q <= d;
      end
   end
end
endmodule

