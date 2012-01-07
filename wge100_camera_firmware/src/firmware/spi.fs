( SPI: Serial Peripheral Interface           JCB 13:14 08/24/10)
module[ spi"

: spix  ( x -- y )
    d# 8 lshift
    d# 8 0do
        dup 0< spi_mosi !
        d# 1 spi_sck !
        2* spi_miso @ or
        d# 0 spi_sck !
    loop
;

: spi-wr        spix drop ;
: spi-rd        d# 0 spix ;
: spi-dummy     spi-rd drop ;
: spi-rd16      spi-rd d# 8 lshift spi-rd or ;
: spi-wr16      dup d# 8 rshift spi-wr spi-wr ;
: spi-wrbuf     ( addr u -- ) 0do dup @ spi-wr16 2+ loop drop ;
]module
