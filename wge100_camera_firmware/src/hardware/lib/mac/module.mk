vfiles += $(libdir)/mac/mac.v \
          $(libdir)/mac/reconciliation.v \
          $(libdir)/mac/mii_mgmt.v \
	  $(libdir)/mac/crc_gen.v \
          $(libdir)/mac/greg.v \
	  $(libdir)/mac/gmux.v \
	  $(libdir)/mac/rx_pkt_fifo.v \
	  $(libdir)/mac/crc_chk.v \
	  $(libdir)/mac/rx_raw.v \
	  $(libdir)/mac/rx_pkt_fifo.v \
	  $(libdir)/mac/rx_usr_if.v \
	  $(libdir)/mac/rx_engine_raw.v \
	  $(libdir)/mac/tx_raw.v \
	  $(libdir)/mac/tx_engine_raw.v \
	  $(libdir)/mac/tx_stream.v \
	  $(libdir)/mac/tx_engine_stream.v \
	  $(libdir)/mac/$(vendor)/$(family)/device_ODDR.v \
#

xilinx_cores += $(libdir)/mac/xilinx/$(family)/coregen/txfifo.xco \
		$(libdir)/mac/xilinx/$(family)/coregen/rxfifo.xco \
		$(libdir)/mac/xilinx/$(family)/coregen/rx_pkt_fifo_sync.xco \
#

#sim_vfiles += $(libdir)/mac/tb/fake_phy.v \
#              $(libdir)/mac/tb/fake_mii.v \
#
