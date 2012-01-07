( Willow Garage's Ethernet camera protocol   JCB 07:01 08/20/10)

module[ wge"
: arp-seed ( -- )
    arp-oldest
    arp-age         \  because this new entry will have age d# 0
    d# 0 over !        \  age d# 0
    >r

    d# 3 ETH.IP.UDP.WGE.REPLYTO.MAC enc-offset enc@n 
    r@ d# 6 + !-- !-- !-- drop
    d# 2 ETH.IP.UDP.WGE.REPLYTO.IP enc-offset enc@n 
    r> d# 8 + 2!
;

: WG-MAGIC h# deaf42. ;
: CONFIG_PRODUCT_ID d# 6805018. ;

: enc-pkt-s,pad           ( caddr u pad -- )
    over - -rot
    enc-pkt-s,
    0do d# 0 enc-pkt-c, loop
;

: wge-packet-header ( srcip -- )
    2>r
    ETH.IP.UDP.SOURCEPORT packet@ d# 1627
    \ zeroes in REPLYTO.IP mean reply to sender
    d# 2 ETH.IP.UDP.WGE.REPLYTO.IP enc-offset enc@n 2dup d0<> if
        ETH.IP.UDP.WGE.REPLYTO.MAC enc-offset
    else
        2drop
        d# 2 ETH.IP.SRCIP enc-offset enc@n
        d# 2 ETH.IP.SRCIP enc-offset enc@n arp-lookup
    then
    2r> rot
    ( dst-port src-port dst-ip src-ip *ethaddr )
    udp-header
;

: wge-packetgeneric ( hrt type )
    WG-MAGIC            enc-pkt-d,
    s>d                 enc-pkt-d,
    d# 16 enc-pkt-s,pad
    d# 8 enc-pkt-,0
;

: send-status
    net-my-ip wge-packet-header
    s" STATUS" h# 82 wge-packetgeneric
    d# 0.  enc-pkt-d,
    d# 10. enc-pkt-d,
    udp-wrapup enc-send
;

: wge-announce ( srcip )
    wge-packet-header
    s" ANNOUNCE" h# 80 wge-packetgeneric

    net-my-mac          enc-pkt-3,
    CONFIG_PRODUCT_ID   enc-pkt-d,
    serial 2@           enc-pkt-d,
    s" productname!" d# 40 enc-pkt-s,pad
    camera_name      d# 40 enc-pkt-s,
    flash-ip-addr 2@
                        enc-pkt-2,
    hdl_version @ d# 4 lshift pcb_rev @ + d# 0 enc-pkt-d,
    version version-n enc-pkt-d,

    \ in_addr is unused
    h# deadbeef. enc-pkt-d,

    udp-wrapup enc-send
;

: handle-discover
    d# 2 ETH.IP.UDP.WGE.DISCOVER.IP enc-offset enc@n
    2dup d0= if
        2drop net-my-ip
    then
    wge-announce
;

: handle-configure
    ETH.IP.UDP.WGE.CONFIGURE.SERIAL packetd@
    serial 2@ d= if
        d# 2 ETH.IP.UDP.WGE.CONFIGURE.IP enc-offset enc@n
        ip-addr!
        net-my-ip wge-announce
    then
;

: handle-vidstart
    mt9v-vidstart
    send-status
    mt9v-waitvblank
;

: handle-vidstop
    mt9v-vidstop
    send-status
;

: handle-reset true trig_reset ! begin again ;

: handle-timer
    net-my-ip wge-packet-header
    s" Time Reply" h# 81 wge-packetgeneric
    time,
    udp-wrapup enc-send
;

: handle-flashread
    net-my-ip wge-packet-header
    s" FLASHDATA" h# 83 wge-packetgeneric

    ETH.IP.UDP.WGE.FLASHREAD.ADDRESS packetd@
    \ s" read: " type 2dup hex8 cr
    2dup enc-pkt-d,

    atmel-page-rd \ append 264 bytes of page data

    udp-wrapup enc-send
;

: handle-flashwrite
    ETH.IP.UDP.WGE.FLASHWRITE.DATA enc-offset
    ETH.IP.UDP.WGE.FLASHWRITE.ADDRESS packetd@
    atmel-page-wr
    send-status
;

: handle-trigcontrol
    ETH.IP.UDP.WGE.TRIGCONTROL.TRIGSTATE packetd@ 
    drop
    triggermode !

    0 [IF]
        s" trigger:" type triggermode @ hex4 space
        trigger-ext? if s" ext " type then
        trigger-neg? if s" neg " type then
        trigger-alt? if s" alt " type then
        cr
    [THEN]

    send-status
;

: handle-sensorread
    net-my-ip wge-packet-header
    s" SENSORDATA" h# 84 wge-packetgeneric

    ETH.IP.UDP.WGE.SENSORREAD.ADDRESS enc-offset enc-c@

    dup     enc-pkt-c,
    \ misaligned, so write bytes
    mt9v@   dup d# 8 rshift enc-pkt-c, enc-pkt-c,
    udp-wrapup enc-send
;

: enc-@mis  ( addr -- u )   \ misaligned 16-bit fetch
    dup enc-c@ d# 8 lshift swap 1+ enc-c@ or
;

: handle-sensorwrite
    ETH.IP.UDP.WGE.SENSORWRITE.DATA enc-offset enc-@mis
    ETH.IP.UDP.WGE.SENSORWRITE.ADDRESS enc-offset enc-c@
    false if
        s" write_i2c(0x" type dup hex4 s" , 0x" type over hex4 s" )" type cr
    then
    mt9v!
    send-status
;

: handle-sensorselect
    ETH.IP.UDP.WGE.SENSORSELECT.ADDRESS enc-offset 2+ enc-@mis
    ETH.IP.UDP.WGE.SENSORSELECT.INDEX enc-offset enc-c@ d# 3 and
    cells sensors + !
    send-status
;

: handle-imagermode
    mt9v-reset
    ETH.IP.UDP.WGE.IMAGERMODE.MODE packetd@ drop
    mt9v-init
    send-status
;

: handle-imagersetres
    mt9v-reset
    ETH.IP.UDP.WGE.IMAGERSETRES.HORIZONTAL packet@ cols !
    ETH.IP.UDP.WGE.IMAGERSETRES.VERTICAL packet@ rows !
    send-status
;

: handle-sysconfig
    d# 0 pad !
    ETH.IP.UDP.WGE.SYSCONFIG.SERIAL packetd@ pad 2+ 2!
    ETH.IP.UDP.WGE.SYSCONFIG.MAC enc-offset pad d# 6 + d# 6 cmove
    \ d# 32 0do pad i 2* + @ hex4 cr loop
    pad atmel-cfg-wr
    send-status
;

: handle-configureFPGA
    hardreset ;

: handle-flashlaunch send-status h# 3e02 execute ;

jumptable handle-packet
( 0 )    | handle-discover
( 1 )    | handle-configure
( 2 )    | handle-vidstart
( 3 )    | handle-vidstop
( 4 )    | handle-configureFPGA \ handle-reset
( 5 )    | handle-timer
( 6 )    | handle-flashread
( 7 )    | handle-flashwrite
( 8 )    | handle-trigcontrol
( 9 )    | handle-sensorread
( 10)    | handle-sensorwrite
( 11)    | handle-sensorselect
( 12)    | handle-imagermode
( 13)    | handle-imagersetres
( 14)    | handle-sysconfig
( 15)    | handle-configureFPGA
( 16)    | handle-flashlaunch

: camera-handler ( dstport -- )
    IP_PROTO_UDP ip-isproto 
    OFFSET_UDP_DESTPORT packet@ d# 1627 = and
    ETH.IP.UDP.WGE.MAGIC packetd@ WG-MAGIC d= and
    if
        udp-checksum? if
            \ The packet reply-to field specifies a MAC for the 
            \ sender IP.  If the entry isn't found in the ARP,
            \ add it.
            d# 2 ETH.IP.UDP.WGE.REPLYTO.IP enc-offset enc@n
            arp-lookup 0= if
                arp-seed
            then

            false if
                ETH.IP.UDP.WGE.TYPE 2+ packet@ hex4 space

                ETH.IP.UDP.WGE.HRT enc-offset
                begin
                    dup d# 1 xor c@ ?dup
                while
                    emit 1+
                repeat
                drop
                cr
            then

            ETH.IP.UDP.WGE.TYPE packetd@ drop
            dup d# 17 u< if handle-packet else drop then
            execute
        then
    then
;
]module
