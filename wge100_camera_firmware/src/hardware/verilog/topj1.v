`define hdl_version 12'h600

module topj1(/*AUTOARG*/
   // Outputs
   reload_b, uart_tx, ledout, phy_TXD, phy_TXEN, phy_RESET_N, phy_MDC,
   spi_mosi, spi_sck, spi_ss, IMG_RESET_B, IMG_CLK,
   // Inouts
   phy_MDIO, i2c_scl, i2c_sda,
   // Inputs
   MCLK, phy_TXCLK, phy_RXCLK, phy_RXD, phy_RXER, phy_RXDV, spi_miso,
   PIXCLK, EXT_TRIG_B, exposing_i, frame_valid_i, line_valid_i,
   IMG_DAT, PCB_REV
   );
   
   localparam RESET_CYCLES   = 10000; // About 200 microseconds. Phi needs 100 microseconds.
   
   parameter RX_CKSM_CHECK = 1; 

   // Clock, reset and configuration signals
   input MCLK;
   output reload_b;

   // Debug signals

   output uart_tx;
   output ledout;

   // PHY signals

   output [3:0] phy_TXD;
   output       phy_TXEN;
   input        phy_TXCLK;
   input        phy_RXCLK;
   input [3:0]  phy_RXD;
   input        phy_RXER;
   input        phy_RXDV;
   output       phy_RESET_N;

   output       phy_MDC;
   inout        phy_MDIO;

   // SPI (Flash)
   output spi_mosi;
   output spi_sck;
   output spi_ss;
   input spi_miso;
   reg spi_mosi;
   reg spi_sck;
   reg spi_ss;

   // I2C (Imager)
   inout i2c_scl;
   inout i2c_sda;

   // Imager
   output IMG_RESET_B;
   reg IMG_RESET_B;
   input PIXCLK;
   output IMG_CLK;

   input EXT_TRIG_B;
   input exposing_i;
   input frame_valid_i;
   input line_valid_i;
   input [9:2] IMG_DAT;

   // Versions
   
   input [2:0] PCB_REV;

   /*AUTOREGINPUT*/

   /*AUTOWIRE*/
   wire                 sys_rst_i;              // From reset_gen of reset_gen.v
   wire                 timer_ack_o;            // From timer of timer.v
   wire [31:0]          timer_dat_o;            // From timer of timer.v
   reg                  trig_reconf;            // From dio of dio.v
   reg                  trig_reset;             // From dio of dio.v
   wire                 trig_watchdog_rst;      // From trig_watchdog_blk of trig_watchdog.v
   wire                 j1_uart_we;           // From j1 of j1.v

   wire                 j1_io_rd;
   wire                 j1_io_wr;
   wire                 [15:0] j1_io_addr;
   reg                  [15:0] j1_io_din;
   wire                 [15:0] j1_io_dout;

   wire                 uart_busy;             // From uart of uart.v
   wire                 watchdog;               // From dio of dio.v
   wire                 watchdog_rst;           // From watchdog_blk of watchdog.v

   assign reload_b = trig_watchdog_rst | watchdog_rst | trig_reconf ? 1'h0 : 1'hz; 

   // Clock generation

   wire sys_clk_i;
   wire CLK_IMG; // IMG_CLK before the BGFGCE that allows it to be disabled.
   ck_div #(.DIV_BY(12), .MULT_BY(17)) sys_ck_gen(.ck_in(MCLK), .ck_out(sys_clk_i));

   ck_div #(.DIV_BY(6), .MULT_BY(2)) img_ck_gen(.ck_in(MCLK), .ck_out(CLK_IMG));
   reg img_clk_ena_o;
   BUFGCE_1 img_clk_bufg(.CE(img_clk_ena_o), .I(CLK_IMG), .O(IMG_CLK));

   // Reset generation
   
   reset_gen #(/*AUTOINSTPARAM*/
               // Parameters
               .RESET_CYCLES            (RESET_CYCLES)) 
   reset_gen(/*AUTOINST*/
             // Outputs
             .sys_rst_i                 (sys_rst_i),
             // Inputs
             .trig_reset                (trig_reset),
             .sys_clk_i                 (sys_clk_i));
`ifdef 0
  assign sys_rst_i = trig_reset;
`endif
   
   watchdog watchdog_blk(/*AUTOINST*/
                         // Outputs
                         .watchdog_rst          (watchdog_rst),
                         // Inputs
                         .sys_clk_i             (sys_clk_i),
                         .watchdog              (watchdog));

   trig_watchdog trig_watchdog_blk(/*AUTOINST*/
                                   // Outputs
                                   .trig_watchdog_rst   (trig_watchdog_rst),
                                   // Inputs
                                   .sys_clk_i           (sys_clk_i),
                                   .EXT_TRIG_B          (EXT_TRIG_B));

   // J1

   j1 j1(/*AUTOINST*/
             // Inputs
             .sys_clk_i                 (sys_clk_i),
             .sys_rst_i                 (sys_rst_i),

        // Inputs

        .io_rd(j1_io_rd),
        .io_wr(j1_io_wr),
        .io_addr(j1_io_addr),
        .io_din(j1_io_din),
        .io_dout(j1_io_dout)
        );

  // ================================================

  reg  [31:0]     clock;
  reg  [8:0]      clockus;
  wire [8:0]      _clockus;
  assign _clockus = (clockus != 67) ? (clockus + 1) : 0;

  always @(posedge sys_clk_i)
  begin
    if (sys_rst_i) begin
      clock <= 0;
      clockus <= 0;
    end else begin
      clockus <= _clockus;
      if (_clockus == 0)
        clock <= clock + 1;
    end
  end

  // ================================================

  reg mac_reset_n;               // (I)

  wire tx_raw_stop;             // (O) mac's input buffers are full
  wire [31:0] tx_raw_count;     // (O) 
  reg [31:0] tx_raw_data;      // (O) 
  reg tx_raw_sof;
  wire tx_raw_we;
  wire _rx_raw_we;

  wire [31:0] rx_raw_data;      // (O) data recv by mac from phy
  wire rx_raw_sof;              // (O) start of frame?
  wire rx_raw_dv;               // (O) rx_data is valid data
  wire [31:0] rx_raw_count;
  reg [47:0] rx_raw_mac_addr;
  reg rx_raw_ack;
  wire _rx_raw_ack;

  reg [0:4]   miim_phyad;       // (I)
  reg [0:4]   miim_addr;        // (I)
  reg [0:15]  miim_wdata;       // (I)
  reg         miim_we;          // (I)
  wire [15:0] miim_rdata;       // (O)
  reg         miim_req;         // (I)
  wire        miim_ack;         // (O)

  // wire [31:0] mac_debug;

  reg        shadow_ack;
  reg [15:0] shadow_rdata;
  always @(posedge sys_clk_i)
  begin
    if (miim_ack & miim_req & !shadow_ack) begin
      shadow_ack <= 1;
      shadow_rdata <= miim_rdata;
    end
    if (miim_req == 0)
      shadow_ack <= 0;
  end
  assign _rx_raw_ack = j1_io_wr & (j1_io_addr == 16'h610c);
  assign tx_raw_we = j1_io_wr & (j1_io_addr == 16'h6208);
  always @(posedge sys_clk_i)
  begin
    rx_raw_ack <= _rx_raw_ack;
  end

   mac #(/*AUTOINSTPARAM*/
         // Parameters
         .USR_CLK_PERIOD                (20),
         .RX_CKSM_CHECK                 (1),
         .RAW_TXIF_COUNT                (1),
         .RAW_RXIF_COUNT                (1),
         .STREAM_TXIF_COUNT             (0),
         .STREAM_RXIF_COUNT             (0)) 
   mac(
           .jumboframes                 (1'b0),
           .clk_125                     (1'b0),
           .reset_n                     (mac_reset_n),
           .usr_clk                     (sys_clk_i),
           .gmii                        (0),
           // .debug                       (mac_debug),
       //.tx_stream_count                 (),
       //.rx_stream_data                  (),
       //.rx_stream_eof                   (),
       //.rx_stream_dv                    (),
       //.rx_stream_count                 (),
       //.tx_stream_stop                  (),
       //.tx_stream_src_mac_addr          (),
       //.tx_stream_dst_mac_addr          (),
       //.tx_stream_ethertype             (),
       //.tx_stream_data                  (),
       //.tx_stream_eof                   (),
       //.tx_stream_we                    (),
       //.rx_stream_mac_addr              (),
       //.rx_stream_ack                   (),
       .phy_TXD                         (phy_TXD),  // phy_TXD_H for hi part
       .phy_RXD                         ({4'h0, phy_RXD[3:0]}),
           /*AUTOINST*/
       // Outputs
       .tx_raw_stop                     (tx_raw_stop),
       .tx_raw_count                    (tx_raw_count),
       .rx_raw_data                     (rx_raw_data),
       .rx_raw_sof                      (rx_raw_sof),
       .rx_raw_dv                       (rx_raw_dv),
       .rx_raw_count                    (rx_raw_count),
       .miim_ack                        (miim_ack),
       .miim_rdata                      (miim_rdata[15:0]),
       .phy_TXEN                        (phy_TXEN),
       .phy_GTXCLK                      (phy_GTXCLK),
       .phy_TXER                        (phy_TXER),
       .phy_RESET_N                     (phy_RESET_N),
       .phy_MDC                         (phy_MDC),
       // Inouts
       .phy_MDIO                        (phy_MDIO),
       // Inputs
       .promiscuous                     (1'b0),
       .ifg                             (10'h0d),   // XXX wtf
       .tx_raw_data                     ({tx_raw_data[31:16], j1_io_dout}),
       .tx_raw_sof                      (tx_raw_sof),
       .tx_raw_we                       (tx_raw_we),
       .rx_raw_mac_addr                 (rx_raw_mac_addr),
       .rx_raw_ack                      (rx_raw_ack),
       .miim_phyad                      (miim_phyad[0:4]),
       .miim_addr                       (miim_addr[0:4]),
       .miim_wdata                      (miim_wdata[0:15]),
       .miim_req                        (miim_req),
       .miim_we                         (miim_we),
       .phy_TXCLK                       (phy_TXCLK),
       .phy_RXCLK                       (phy_RXCLK),
       .phy_RXER                        (phy_RXER),
       .phy_RXDV                        (phy_RXDV));

  // ================================================
  // Hardware multiplier

  reg [15:0] mult_a;
  reg [15:0] mult_b;
  wire [31:0] mult_p;
  MULT18X18SIO #(
    .AREG(0),
    .BREG(0),
    .PREG(0))
  MULT18X18SIO(
    .A(mult_a),
    .B(mult_b),
    .P(mult_p));

  // ================================================
  // Pixel FIFO.  Resets at frame start.

  reg pixfifo_reset;
  wire [9:0] pixfifo_fullness;
  wire pixfifo_full;
  wire pixfifo_empty;
  wire [18:0] pixfifo_data;
  wire pixfifo_rd;
  assign pixfifo_rd = (j1_io_rd & ((j1_io_addr == 16'h6606) | (j1_io_addr == 16'h6614)));

  reg _frame_valid_i;
  reg frame_start;
  always @(posedge PIXCLK)
  begin
    frame_start = (frame_valid_i & ~_frame_valid_i );
    _frame_valid_i <= frame_valid_i;
  end

  pixfifo U_pixfifo(
    .rst(pixfifo_reset | frame_start),
    .wr_clk(PIXCLK),
    .din(IMG_DAT),
    .wr_en(line_valid_i),
    .rd_clk(sys_clk_i),

    .rd_en(pixfifo_rd),
    .dout(pixfifo_data),
    .full(pixfifo_full),
    .empty(pixfifo_empty),
    .rd_data_count(pixfifo_fullness));

  reg [31:0] pixel_counter;
  always @(posedge PIXCLK)
  begin
    if (line_valid_i) begin
      pixel_counter <= pixel_counter + 1;
    end
  end

  // ================================================
  // XOR line

  wire [15:0] xorline_rd;
  wire [15:0] xorline_prev;
  reg [15:0] xorline_addr;

  RAMB16_S18_S18 xorline_ram(
    .DIA(0),
    .DIPA(0),
    .DOA(xorline_rd),
    .WEA(0),
    .ENA(1),
    .CLKA(sys_clk_i),
    .ADDRA(j1_io_addr[15:1]),

    .DIB(xorline_prev ^ j1_io_dout),
    .DIPB(2'b00),
    .WEB((j1_io_addr == 16'h5f00) & j1_io_wr),
    .ENB(1),
    .CLKB(sys_clk_i),
    .ADDRB(xorline_addr),
    .DOB(xorline_prev));

  always @(posedge sys_clk_i)
  begin
    if ((j1_io_addr == 16'h5f00) & j1_io_wr)
      xorline_addr = xorline_addr + 1;
    else if ((j1_io_addr == 16'h5f02) & j1_io_wr)
      xorline_addr = 0;
  end

  // ================================================
  // LED, I2C, UART

  reg ledout;
  
  // Open-drain for i2c
  reg i2c_sda_reg;
  reg i2c_scl_reg;
  wire i2c_sda = i2c_sda_reg ? 1'hZ : 1'h0;
  wire i2c_scl = i2c_scl_reg ? 1'hZ : 1'h0;

  assign j1_uart_we = (j1_io_addr == 16'hf000) & j1_io_wr;

  uart uart(/*AUTOINST*/
            // Outputs
            .uart_busy                 (uart_busy),
            .uart_tx                   (uart_tx),
            // Inputs
            .uart_wr_i                 (j1_uart_we),
            .uart_dat_i                (j1_io_dout),
            .sys_clk_i                 (sys_clk_i),
            .sys_rst_i                 (sys_rst_i));

  // ================================================
  // J1's Memory Mapped I/O

  // ============== J1 READS ========================
  always @*
  begin
    if (j1_io_addr[15:12] == 5)
      j1_io_din = xorline_rd;
    else 
      case (j1_io_addr)

        16'h6302: j1_io_din = PCB_REV;
        16'h6304: j1_io_din = i2c_sda;
        16'h6306: j1_io_din = i2c_scl;

        16'h6406: j1_io_din = spi_miso;
        16'h6500: j1_io_din = frame_valid_i;

        16'h6600: j1_io_din = pixel_counter[15:0];
        16'h6602: j1_io_din = pixel_counter[31:16];
        16'h6604: j1_io_din = pixfifo_fullness;
        16'h6606: j1_io_din = 16'h660c;
        16'h6608: j1_io_din = frame_valid_i;
        16'h660a: j1_io_din = pixfifo_empty;
        16'h660c: j1_io_din = {pixfifo_data[16:9], pixfifo_data[7:0]};
        // 660e is output
        16'h6610: j1_io_din = exposing_i;
        16'h6612: j1_io_din = EXT_TRIG_B;
        16'h6614: j1_io_din = {pixfifo_data[16:9], pixfifo_data[7:0]};

        16'h6704: j1_io_din = mult_p[15:0];
        16'h6706: j1_io_din = mult_p[31:16];

        16'he00c: j1_io_din = shadow_ack;
        16'he00e: j1_io_din = shadow_rdata;

        16'h6100: j1_io_din = rx_raw_count;
        16'h6102: j1_io_din = rx_raw_dv;
        16'h6104: j1_io_din = rx_raw_data[15:0];
        16'h6106: j1_io_din = rx_raw_data[31:16];
        16'h6108: j1_io_din = rx_raw_sof;

        16'h6200: j1_io_din = {16{tx_raw_stop}};
        16'h620a: j1_io_din = tx_raw_count;

        // 16'h6ffc: j1_io_din = mac_debug[15:0];
        // 16'h6ffe: j1_io_din = mac_debug[31:16];

        16'h7000: j1_io_din = `hdl_version;

        16'hf000: j1_io_din = uart_busy;

        16'hfff0: j1_io_din = clock[15:0];
        16'hfff2: j1_io_din = clock[31:16];

        default: j1_io_din = 16'h0666;
      endcase
  end

  // ============== J1 WRITES =======================
  reg [15:0] slow_io_dout;
  reg [15:0] slow_io_addr;

  // latch addr+data to reduce fanout load on the J1
  always @(posedge sys_clk_i)
  begin
    if (j1_io_wr) begin
      slow_io_addr <= j1_io_addr;
      slow_io_dout <= j1_io_dout;
    end
  end

  always @(posedge sys_clk_i)
  begin
    if (sys_rst_i) begin
      trig_reconf <= 0;
      trig_reset <= 0;
    end else begin
      case (slow_io_addr)
        16'h6300: ledout <= slow_io_dout[0];
        16'h6304: i2c_sda_reg <= slow_io_dout[0];
        16'h6306: i2c_scl_reg <= slow_io_dout[0];
        16'h6308: IMG_RESET_B <= slow_io_dout[0];
        16'h630a: trig_reconf <= slow_io_dout[0];
        16'h630c: trig_reset <= slow_io_dout[0];
        16'h630e: img_clk_ena_o <= slow_io_dout[0];
        16'h6400: spi_sck <= slow_io_dout[0];
        16'h6402: spi_ss  <= slow_io_dout[0];
        16'h6404: spi_mosi <= slow_io_dout[0];

        16'h660e: pixfifo_reset <= slow_io_dout[0];

        16'h6700: mult_a <= slow_io_dout;
        16'h6702: mult_b <= slow_io_dout;

        16'he000: mac_reset_n <= slow_io_dout[0];
        16'he002: miim_phyad <= slow_io_dout[4:0];
        16'he004: miim_addr <= slow_io_dout[4:0];
        16'he006: miim_wdata <= slow_io_dout;
        16'he008: miim_we <= slow_io_dout[0];
        16'he00a: miim_req <= slow_io_dout[0];

        16'h6010: rx_raw_mac_addr[47:32] = slow_io_dout;
        16'h6012: rx_raw_mac_addr[31:16] = slow_io_dout;
        16'h6014: rx_raw_mac_addr[15:0] = slow_io_dout;

        16'h6204: tx_raw_data[31:16] <= slow_io_dout;
        16'h6206: tx_raw_sof <= slow_io_dout[0];

      endcase
    end
  end

endmodule // top
