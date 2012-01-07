module reset_gen(/*AUTOARG*/
   // Outputs
   sys_rst_i,
   // Inputs
   trig_reset, sys_clk_i
   );
  parameter RESET_CYCLES = 0;
  output sys_rst_i;
  input trig_reset;
  input sys_clk_i;

  reg [13:0] reset_count = RESET_CYCLES;
  wire sys_rst_i = |reset_count;

  always @(posedge sys_clk_i) begin
    if (trig_reset)
      reset_count <= RESET_CYCLES;
    else if (sys_rst_i)
      reset_count <= reset_count - 1;
  end
endmodule
