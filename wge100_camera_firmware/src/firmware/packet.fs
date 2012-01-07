( Packet construction, tx, rx                JCB 13:25 08/24/10)
module[ packet"

\ create incoming d# 1500 allot
\ create outgoing d# 1500 allot
[ 16384 512 - 1500 - ] constant incoming
[ 16384 512 - 3000 - ] constant outgoing

: enc-offset incoming + ;

: enc-c@    dup @ swap d# 1 and if d# 255 and else d# 8 rshift then ;

: enc@n ( n addr -- d0 .. dn )
    swap 0do dup @ swap 2+ loop drop ; 

: packet@ incoming + @ ;

: packetd@ incoming + 2@ swap ;

: packetout-off           \  compute offset in output packet
    outgoing +
;

( words for constructing packet data         JCB 07:01 08/20/10)
variable writer

: enc-pkt-begin outgoing writer !  ;
: bump  ( n -- ) writer +! ;
: enc-pkt-c,    ( n -- )
    h# ff and
    writer @ d# 1 and if 
        writer @ @ or 
    else
        d# 8 lshift
    then
    writer @ !
    d# 1 bump
;
: enc-pkt-,     ( n -- ) writer @ ! d# 2 bump ;
: enc-pkt-d,    ( d -- ) enc-pkt-, enc-pkt-, ;
: enc-pkt-2,    ( n0 n1 -- ) swap enc-pkt-, enc-pkt-, ;
: enc-pkt-3,    rot enc-pkt-, enc-pkt-2, ;
: enc-pkt-,0    ( n -- ) 0do d# 0 enc-pkt-, loop ;
: enc-pkt-s,    ( caddr u -- )
    0do
        dup c@
        enc-pkt-c,
        1+
    loop
    drop
;
: enc-pkt-src ( n offset ) \ copy n words from incoming+offset
    incoming +
    swap 0do
        dup @ enc-pkt-,
        2+
    loop
    drop
;

: enc! ( n addr -- ) ! ;

: enc-pkt-complete ( -- length = set up TXST and TXND )
    writer @ outgoing -
;

: mac-ready \ wait until MAC is ready for next 32 bits
    begin morse-a morseflash MAC_w_stop @ invert until ;

: enc-send
    \ enc-pkt-begin
    \ d# 104 0do i enc-pkt-c, loop
    \ h# 800 outgoing d# 12 + !

    mac-ready

    d# 1 MAC_w_sof !
    writer @ 1+ h# fffe and outgoing -
    dup MAC_w_0 !
    d# 3 + h# fffc and
    outgoing d# 2 + + outgoing
    dup d# 2 + swap @ MAC_w_we !

    d# 0 MAC_w_sof !

    begin
        MAC_w_stop @ if
            mac-ready
        then
        dup@ MAC_w_0 !
        d# 2 +
        dup@ MAC_w_we !
        d# 2 + 2dup=
    until
    2drop

    \ MAC_w_count hex4 cr halt
;

: enc-checksum ( addr nwords -- sum )
    d# 0 swap
    0do
        over @       ( addr sum v )
        +1c
        swap 2+ swap
    loop
    nip
    invert
;

: enc-fullness ( -- f )
    time 2+ @ d# 3 rshift led !
    \ no data => return false
    \ data+sof => handle it, return true
    \ data+no sof => ack, return false
    MAC_rd_dv @
    dup if
        MAC_rd_sof @ 0= if
            d# 1 MAC_rd_ack !
            [char] ! emit
            drop d# 0 exit
        then
        MAC_rd_1 @ incoming !
        \ N byte packet, N arrives.  Length takes 2 bytes,
        \ so (N+2) total bytes. Number of transactions is
        \ ((N+2)+3) / 4.  Subtract 1 for 1st transaction.
        \ So number of bytes is ((N+5)&~3)-4.

        MAC_rd_0 @ d# 1500 > if
            [char] * emit cr
            MAC_rd_sof @ hex4 cr
            begin again
        then
        MAC_rd_0 @ d# 5 +
        h# fffc and d# 4 -
        incoming 2+ swap bounds
        d# 1 MAC_rd_ack !
        begin
            MAC_rd_dv @ d# 0 = if
                \ starved.  if no data after 4 clocks, bail 
                noop noop noop noop
                MAC_rd_dv @ d# 0 = if
                    [char] % emit
                    2drop drop d# 0
                    exit
                then
            then
            MAC_rd_0 @ over ! d# 2 +
            MAC_rd_1 @ over ! d# 2 +
            d# 1 MAC_rd_ack !
            2dup=
        until
        2drop
    then
;

]module
