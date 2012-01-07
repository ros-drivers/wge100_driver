// Reconciliation sublayer. This block deals with all the differences
// between MII and GMII (and eventually RGMII).

module reconciliation
(input clk_125, input reset_n,
 
 // decides whether we are in gmii or mii mode
 input gmii,

 input [7:0] int_tx_dout,
 input int_tx_en,
 input int_tx_er,
 output int_tx_clk,

 output [7:0] int_rx_din,
 output int_rx_dv,
 output int_rx_er,
 output int_rx_clk,
 
 output [7:0] phy_TXD,
 output       phy_TXEN,
 output       phy_TXER,
 output       phy_GTXCLK,
 input        phy_TXCLK,
 input  [7:0] phy_RXD,
 input        phy_RXDV,
 input        phy_RXER,
 input        phy_RXCLK
 );

// Transmission
// ------------

// Generate int_tx_clk, which is what the mac is clocking data in on,
// and tx_clk which is what we are clocking data out on. If we are in
// MII mode, we ship 4 bits every phy_TXCLK, so we want the mac to
// clock the data in every other phy_TXCLK since the mac interface is
// 8 bits.  We divide phy_TXCLK by two and send that as the
// int_tx_clk.  Otherwise, we are in GMII mode and sending 8 bits
// every phy_GTXCLK, so send the full phy_GTXCLK as int_tx_clk.

wire mii_tx_clk;
greg mtc_reg(phy_TXCLK, ~mii_tx_clk, 1'b0, ~reset_n, 1'b1, mii_tx_clk);
BUFGMUX int_tx_clk_mux(.O(int_tx_clk), .I0(mii_tx_clk), .I1(clk_125), .S(gmii));

// Now generate tx_clk, which is used to clock the data out, this is
// simply clk_125 if gmii or phy_TXCLK if not.

wire tx_clk;
// BUFGMUX tx_clk_mux(.O(tx_clk), .I0(phy_TXCLK), .I1(clk_125), .S(gmii));
assign tx_clk = phy_TXCLK;


// Generate the output GTX clock to the phy, this will only be used in
// gmii mode.  We use an ODDR to delay and flip the clock so that
// the rising edge is sent in the middle of the data (sent below)
wire tmp0 = 1'b0;
wire tmp1 = 1'b1;
device_ODDR gtx_clk_out(.S(tmp0), .R(tmp0), .CE(tmp1), 
                        .D0(1'b0), .C0(tx_clk), .D1(1'b1), .C1(~tx_clk), 
                        .Q(phy_GTXCLK));

// now deal with the data.  For mii we alternate between nibbles.  For
// gmii we just blast the whole int_tx_dout each clock.
wire mii_tx_sel;
greg mts_reg(tx_clk, ~mii_tx_sel, ~int_tx_en, ~reset_n, 1'b1, mii_tx_sel);

wire [3:0] mii_txd;
gmux #(4, 1) mii_txd_mux(.d(int_tx_dout), .sel(mii_tx_sel), .z(mii_txd));

wire [7:0] next_txd;
gmux #(8, 1) txd_sel(.d({int_tx_dout, 4'h0, mii_txd}), .sel(gmii), .z(next_txd));

// TODO crossing clock domains... metastability??  Should be in phase...

greg #(8) TXD_reg(tx_clk, next_txd, 1'b0, ~reset_n, 1'b1, phy_TXD);
greg #(1) TXEN_reg(tx_clk, int_tx_en, 1'b0, ~reset_n, 1'b1, phy_TXEN);
greg #(1) TXER_reg(tx_clk, int_tx_er, 1'b0, ~reset_n, 1'b1, phy_TXER);



// Reception
// ---------

// Again deal with the clocks first.  If we are in MII mode, the phy
// sends 4 bits every phy_RXCLK, so the mac should be running at half
// phy_RXCLK to get 8 bits each clock tick.  If we are in GMII mode,
// the phy sends 8 bits every phy_RXCLK, so we can just send the
// clock directly through.
wire mii_rx_clk;
greg rcc_reg(phy_RXCLK, ~mii_rx_clk, 1'b0, ~reset_n, 1'b1, mii_rx_clk);
BUFGMUX int_rx_clk_mux(.O(int_rx_clk), .I0(mii_rx_clk), .I1(phy_RXCLK), .S(gmii));

// the data is a bit harder.  We start by registering the signals
// coming in the from the phy
wire [7:0] rx_din_0;
wire [3:0] rx_din_1;
wire rx_dv_0, rx_dv_1;
wire rx_er_0, rx_er_1;
greg #(8) rx_din_r0(phy_RXCLK, phy_RXD, 1'b0, ~reset_n, 1'b1, rx_din_0);
greg #(1) rx_dv_r0(phy_RXCLK, phy_RXDV, 1'b0, ~reset_n, 1'b1, rx_dv_0);
greg #(1) rx_er_r0(phy_RXCLK, phy_RXER, 1'b0, ~reset_n, 1'b1, rx_er_0);

// the above signal are fine for gmii, but for mii, we need to hold
// them for two phy_RXCLK cycles, and not just any two, a cycle with
// int_rx_clk low and a cycle with int_rx_clk high. So we want to
// bring a new value in when int_rx_clk is high.  To know what delay
// registers to use, we sample it when phy_RXDV goes high indicating
// a new frame.  If int_rx_clk is high on new_frame is high, rx_din
// will be get it's first nibble when int_rx_clk is low.
wire new_frame = ~rx_dv_0 & phy_RXDV;
wire started_on_low;
greg #(1) sol_reg(phy_RXCLK, mii_rx_clk, 1'b0, ~reset_n, new_frame, started_on_low);

greg #(4) rx_din_r1(phy_RXCLK, rx_din_0[3:0], 1'b0, ~reset_n, 1'b1, rx_din_1);
greg #(1) rx_dv_r1(phy_RXCLK, rx_dv_0, 1'b0, ~reset_n, 1'b1, rx_dv_1);
greg #(1) rx_er_r1(phy_RXCLK, rx_er_0, 1'b0, ~reset_n, 1'b1, rx_er_1);

wire [7:0] next_mii_rx_din;
wire next_mii_rx_dv;
wire next_mii_rx_er;
gmux #(10, 1) mii_rx_din_mux
  (.d({phy_RXER, phy_RXDV & ~new_frame, phy_RXD[3:0], rx_din_0[3:0],
       rx_er_1, rx_dv_1, rx_din_0[3:0], rx_din_1[3:0]}),
   .sel(started_on_low),
   .z({next_mii_rx_er, next_mii_rx_dv, next_mii_rx_din}));

wire [7:0] mii_rx_din; 
wire mii_rx_dv;
wire mii_rx_er;
greg #(8) mii_rx_din_r1(phy_RXCLK, next_mii_rx_din, 1'b0, ~reset_n, ~mii_rx_clk, mii_rx_din);
greg #(1) mii_rx_dv_r1(phy_RXCLK, next_mii_rx_dv, 1'b0, ~reset_n, ~mii_rx_clk, mii_rx_dv);
greg #(1) mii_rx_er_r1(phy_RXCLK, next_mii_rx_er, 1'b0, ~reset_n, ~mii_rx_clk, mii_rx_er);

wire [7:0] next_int_rx_din;
wire next_int_rx_dv;
wire next_int_rx_er;
gmux #(10, 1) rx_mux
  (.d({rx_er_0, rx_dv_0, rx_din_0, mii_rx_er, mii_rx_dv, mii_rx_din}),
   .sel(gmii),
   .z({next_int_rx_er, next_int_rx_dv, next_int_rx_din}));

// register the output of all this craziness to maintain 125 MHz these
// registers act as the cross from the phy_RXCLK domain to the
// int_rx_clk domain.  TODO - metastability issues.
greg #(8) int_rx_din_reg(phy_RXCLK, next_int_rx_din, 1'b0, ~reset_n, 1'b1, int_rx_din);
greg #(1) int_rx_dv_reg(phy_RXCLK, next_int_rx_dv, 1'b0, ~reset_n, 1'b1, int_rx_dv); 
greg #(1) int_rx_er_reg(phy_RXCLK, next_int_rx_er, 1'b0, ~reset_n, 1'b1, int_rx_er);

endmodule
