module trig_watchdog(/*AUTOARG*/
   // Outputs
   trig_watchdog_rst,
   // Inputs
   sys_clk_i, EXT_TRIG_B
   );
  localparam WATCHDOG_COUNT = 50000000;
  input sys_clk_i;
  input EXT_TRIG_B;
  output trig_watchdog_rst;

  reg [25:0] count = WATCHDOG_COUNT;
  reg trig_watchdog_rst = 0;
  reg armed = 0;

  always @(posedge sys_clk_i) begin
    if (~EXT_TRIG_B) begin
      armed <= 1;
      count <= WATCHDOG_COUNT;
    end else
      count <= count - 1;

    if (armed & ~|count)
      trig_watchdog_rst <= 1;
  end
endmodule
