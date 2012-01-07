( Flash: driver for Atmel AT45DB161D         JCB 13:14 08/24/10)
module[ flash"

: atmel-sel     d# 0 spi_ss ! ;
: atmel-unsel   d# 1 spi_ss ! ;
: atmel-cold    atmel-unsel d# 0 spi_sck ! ;
: atmel-cmd     atmel-sel spi-wr ;
: atmel-status  h# d7 atmel-cmd spi-rd atmel-unsel ;
: atmel-ready   begin atmel-status h# 80 and until ;
: atmel-wraddr  ( addr. -- ) spi-wr spi-wr16 ;
d# 132 constant atmel-pagesz \ page size in words

: atmel-cfg-rd  \ read config from OTP, store in serial, mac
    h# 77 atmel-cmd
    spi-dummy spi-dummy spi-dummy
    \ config revision: only zero is valid
    spi-rd 0= if
        spi-rd16 spi-rd16 swap serial 2!
        d# 3 0do spi-rd16 mac i 2* + ! loop
    else
        \ If there is no configuration set
        \ a default MAC/Serial combo so
        \ the board can be remotely programmed:
        serial dz
        mac
        h# 0024 over ! 2+
        h# cd00 over ! 2+
        h# 0000 swap !
    then
    atmel-unsel
;

: atmel-rd ( addr. -- ) \ prepare to read page from addr.
    h# D2 atmel-cmd
    atmel-wraddr
    spi-dummy spi-dummy spi-dummy spi-dummy 
;

: atmel-identity-valid \ verify identity page checksum
    h# 1FFE00. atmel-rd
    d# 0 atmel-pagesz 0do spi-rd16 + loop
    atmel-unsel
    h# ffff =
;

: atmel-ip ( -- ip. ) \ read name (to PAD) IP from identity page
    pad d# 40
    atmel-identity-valid if
        h# 1FFE00. atmel-rd
        bounds begin
            spi-rd over c!
            1+ 2dup=
        until 2drop
        spi-rd16 spi-rd16
        atmel-unsel
    else
        d# 0 fill
        ip# 169.254.0.1
    then
;

2variable flash-ip-addr

: atmel-id-rd \ load name and IP from identity page, if valid
    atmel-ip 2dup ip-addr 2! flash-ip-addr 2!
    pad camera_name d# 40 cmove ;

: atmel-cfg-wr ( addr1 ) \ write 64 bytes to OTP - XXX untested
    h# 9B atmel-cmd
    d# 0 spi-wr d# 0 spi-wr d# 0 spi-wr
    d# 32 spi-wrbuf
    atmel-unsel
    atmel-ready
;

: atmel-page-rd ( addr. -- )  \ read page from addr.
    atmel-rd
    atmel-pagesz 0do
        spi-rd16 enc-pkt-,
    loop
    atmel-unsel
;

: atmel-page-wr ( addr1 addr. -- ) \ page write addr1 -> addr.
    h# 82 atmel-cmd
    atmel-wraddr
    atmel-pagesz spi-wrbuf
    atmel-unsel
    atmel-ready
;
]module
