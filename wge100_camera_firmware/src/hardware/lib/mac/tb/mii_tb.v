`timescale 1ns / 1ns

module mii_tb;
initial begin
   $dumpfile("mii_tb.vcd");
   $dumpvars;
end

// 100 MHz clock
wire sys_clk;
wire reset_n;
fake_clk_rn #(200) U_sys_clk(sys_clk, reset_n);

reg [4:0] phyad;
reg [4:0] addr;
reg [15:0] wdata;
reg req;
reg we;
wire ack;
wire [15:0] rdata;
wire mdc;
wire mdio;

mii_mgmt #(5) U_mii
  (.clk(sys_clk), .reset_n(reset_n),
   .phyad(phyad), .addr(addr), 
   .wdata(wdata), .req(req), .we(we), .ack(ack), .rdata(rdata),
   .mdc(mdc), .mdio(mdio));

fake_mii U_fake_mii(mdc, mdio);

task check_mdc;
real t1, t2, freq;
   begin
      wait(mdc) t1 = $time;
      wait(!mdc);
      wait(mdc) t2 = $time;
      freq = 1000/(t2-t1);
      if (freq == 0 | freq >= 2.5) begin
         $display("Period of mdc is %d (%f MHz)... ERROR", (t2-t1), freq);
         $finish;
      end else
         $display("Period of mdc is %d (%f MHz)... OK", (t2-t1), freq);      
   end
endtask

task read_mii(input [4:0] _addr); begin
   addr = _addr;
   req = 1;
   wait (ack);
   $display("Read %x from %x.", rdata, addr);
   @(posedge sys_clk) #2;
   req = 0;
end
endtask

task write_mii(input [4:0] _addr, input [15:0] _data); begin
   addr = _addr;
   wdata = _data;
   we = 1;
   req = 1;
   wait (ack);
   $display("Wrote %x to %x.", wdata, addr);
   @(posedge sys_clk) #2;
   we = 0;
   req = 0;
end
endtask   

initial begin
   phyad = 5'h12;
   addr = 0;
   wdata = 0;
   req = 0;
   we = 0;      
   #100;

   check_mdc();
   read_mii(5'h1c);
   #60;
   write_mii(5'h0a, 16'hbeef);
   read_mii(5'h0a);
   $finish;
end

initial #100000 $finish;
endmodule
        
