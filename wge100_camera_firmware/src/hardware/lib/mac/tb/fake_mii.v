`timescale 1ns / 1ns


module fake_mii
  (input mdc,
    inout mdio);

   reg [15:0] regs[0:31];
   reg [15:0] save;
   
   reg mdo;
   reg mdo_en;
   reg mdio_del;
   reg di;
   
   reg [1:0] op;
   reg [4:0] phyad;
   reg [4:0] regad;   
   
   integer count;
   
   always @(mdio) #10 mdio_del <= mdio;

   assign mdio = mdo_en ? mdo : 1'bz;

   integer i, j;
   initial begin
      mdo = 0;
      mdo_en = 0;
      for (i=0; i<32; i=i+1) begin
         j = i+1;
         regs[i] = {j[7:0], i[7:0]};
      end
      #20
      while (1) handle_op;
   end
   
   task handle_op;
      begin
         wait_for_clk; sample_mdio; 
         while (di !== 1) begin wait_for_clk; sample_mdio; end // wait for preamble
         while (di !== 0) begin wait_for_clk; sample_mdio; end // gobble up preamble
         wait_for_clk; sample_mdio; // eat 0
         if (di !== 1) $display ($time, " ERROR! Bad MII start frame");
         wait_for_clk; sample_mdio; // eat 1
         op[1] = di; wait_for_clk; sample_mdio;
         op[0] = di; wait_for_clk; sample_mdio;
         phyad[4] = di; wait_for_clk; sample_mdio;
         phyad[3] = di; wait_for_clk; sample_mdio;
         phyad[2] = di; wait_for_clk; sample_mdio;
         phyad[1] = di; wait_for_clk; sample_mdio;
         phyad[0] = di; wait_for_clk; sample_mdio;
         regad[4] = di; wait_for_clk; sample_mdio;
         regad[3] = di; wait_for_clk; sample_mdio;
         regad[2] = di; wait_for_clk; sample_mdio;
         regad[1] = di; wait_for_clk; sample_mdio;
         regad[0] = di; wait_for_clk; sample_mdio;
         if (op == 2'b10) begin
            $display ($time, " MII: reading register %x from phy %x (%x).", regad, phyad, regs[regad]);
            if (di !== 1'bz) $display ($time, " ERROR! Bad MII turn around");
            wait_for_nclk;
            mdo_en = 1'b1;
            mdo = 1'b0;
            count = 15;
            while (count >= 0) begin
               wait_for_nclk;
               mdo = regs[regad][count];
               count = count - 1;
            end              
            wait_for_nclk;
            mdo = 0;
            mdo_en = 0;
            wait_for_clk;
            $display ($time, " MII: done reading.");
         end
         else if (op == 2'b01) begin
            $display ($time, " MII: writing register %x to phy %x.", regad, phyad);
            if (di !== 1'b1) $display ($time, " ERROR! Bad MII turn around");
            wait_for_clk; sample_mdio;
            if (di !== 1'b0) $display ($time, " ERROR! Bad MII turn around");
            count = 15;
            while (count >= 0) begin
               wait_for_clk;
               save[count] = mdio;
               count = count - 1;
            end
            regs[regad] = save;
            wait_for_clk;
            $display ($time, " MII: done writing %x to register %x.", regs[regad], regad);
         end                  
         else
           $display ($time, " ERROR! Invalid MII op %b.", op);
      end
   endtask

   task wait_for_clk; 
      @(posedge mdc) begin end
   endtask

   task wait_for_nclk;
      @(negedge mdc) begin end
   endtask
   
   task sample_mdio;
      begin
         if (mdio != mdio_del)
           $display($time, " ERROR! mdio not setup for 10 ns prior to rising edge of mdc.");
         #10 if (mdio != mdio_del) 
           $display ($time, " ERROR! mdio not held for 10 ns after rising edge of mdc.");
         di = mdio_del;
      end
   endtask
endmodule

// module test_fake_mii;
//    initial begin
//       $dumpfile("test_fake_mii.vcd");
//       $dumpvars;
//    end

//    reg reset_n;
//    initial begin
//       reset_n = 0;
//       #7 reset_n = 1;
//    end

//    wire mdc;
//    fake_clk #(2) U_mdc(reset_n, mdc);

//    reg mdo;
//    reg mdo_en;
//    wire mdio = mdo_en ? mdo : 1'bz;   
//    fake_mii U_fake_mii(mdc, mdio);

//    reg [65:0] tosend;
//    reg [65:0] tosend_en;
//    initial begin
//       tosend = 0;
//       tosend_en = 0;
//       #200;
//       tosend    = {{32{1'b1}}, 4'b0110, 5'h00, 5'h1c, 2'b00, 16'h0000, 1'b0};
//       tosend_en = {{32{1'b1}}, 4'b1111, 5'h1f, 5'h1f, 2'b00, 16'h0000, 1'b0};
//       #100000;
//       $finish;
//    end      

//    always @(negedge mdc) begin
//       mdo = tosend[65];
//       mdo_en = tosend_en[65];
//       tosend = tosend << 1;
//       tosend_en = tosend_en << 1;
//    end

// endmodule
