module watchdog(/*AUTOARG*/
   // Outputs
   watchdog_rst,
   // Inputs
   sys_clk_i, watchdog
   );
  localparam WATCHDOG_COUNT = 50000000;
  input sys_clk_i;
  input watchdog;
  output watchdog_rst;

  reg [25:0] count = WATCHDOG_COUNT;
  reg watchdog_last = 0;
  reg watchdog_rst = 0;

  always @(posedge sys_clk_i) begin
    if (watchdog_last != watchdog)
      count <= WATCHDOG_COUNT;
    else
      count <= count - 1;

//    if (~|count)
//      watchdog_rst <= 1;
    watchdog_last <= watchdog;
  end
endmodule
