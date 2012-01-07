( Bootstrap system from flash                JCB 06:29 08/25/10)

\ This module is the system bootstrap.  It checks flash for a
\ current copy of system code, then loads that code into
\ memory and runs it.
\ This module cannot share any executable code with the main
\ system, because it will overwrite that code as it loads it.
\ Hence it appears first in main.fs, and must define local
\ copies of any non-primitive words it uses.

h# 3e04 org

\ meta \ prevent word pollution
\ added this test to avoid crash on 0.6.2
\ : is-gforth-7 version-string s" 0.7.0" compare 0> ;
\ is-gforth-7 [IF] marker bootprivate [THEN]
\ target

( Local copies of low level words, as reqd.  JCB 06:52 08/25/10)
: 1+ d# 1 + ;
: 2+ d# 2 + ;
: 2* d# 1 lshift ;
: 0< d# 0 < ;

: looptest  ( -- FIN )
    r>          ( xt )
    r>          ( xt i )
    1+
    r@ over =   ( xt i FIN )
    dup if
        nip r> drop
    else
        swap >r
    then        ( xt FIN )
    swap
    >r
;

include spi.fs

: atmel-sel     d# 0 spi_ss ! ;
: atmel-unsel   d# 1 spi_ss ! ;
: atmel-cold    atmel-unsel d# 0 spi_sck ! ;
: atmel-cmd     atmel-sel spi-wr ;
: atmel-status  h# d7 atmel-cmd spi-rd atmel-unsel ;
: atmel-ready   begin atmel-status h# 80 and until ;
: atmel-wraddr  ( addr. -- ) spi-wr spi-wr16 ;
d# 132 constant atmel-pagesz \ page size in words

: atmel-rd ( addr. -- ) \ prepare to read page from addr.
    h# D2 atmel-cmd
    atmel-wraddr
    spi-dummy spi-dummy spi-dummy spi-dummy 
;

: page  ( u -- du ) dup d# 9 lshift swap d# 7 rshift ;
: bootloader
    h# 3e 0do
    \ h# 3e h# 38 do
        i d# 2001 + page atmel-rd
        d# 0 begin
            spi-rd16 over i d# 8 lshift + !
            2+ dup d# 256 =
        until drop
        atmel-unsel
    loop
;

: dangerboot
    atmel-cold bootloader
    d# 0 h# 3ffe !
    begin dsp h# ff and while drop repeat
    begin dsp d# 8 rshift while r> drop repeat
    d# 2 >r \ jump to the post-load vector
;

: safeboot
    atmel-cold
    d# 2000 page atmel-rd spi-rd16 d# 947 = atmel-unsel
    if
        bootloader
    then
    d# 2 >r     \ jump via location 2, the post-load vector
;


h# 3e00 org
code bootjump
    safeboot ubranch
    dangerboot ubranch
end-code

meta
\ is-gforth-7 [IF] bootprivate [THEN]
target
