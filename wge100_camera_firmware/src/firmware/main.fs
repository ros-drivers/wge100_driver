( Main for WGE firmware                      JCB 13:24 08/24/10)

\ warnings off require tags.fs

include crossj1.fs
meta
    : TARGET? 1 ;
    : ONBENCH 0 ;
    : SYSLOG 0 ;
    : DO-XORLINE 1 ;
    : build-debug? 1 ;
    : build-tcp? 0 ;

include basewords.fs
target
include hwwge.fs
include boot.fs
include doc.fs

4 org
module[ eveything"
include nuc.fs

create mac         6 allot
create serial      4 allot
create camera_name 40 allot

: net-my-mac mac dup @ swap 2+ dup @ swap 2+ @ ;

: halt  [char] # emit begin again ;

: alarm
    s" ** failed selftest: code " type hex2 cr
    halt
;

include version.fs
include parseversion.fs

: rapidflash
    time 2+ @ led ! ;
: morseflash ( code -- )
    time 2+ @ 2/ h# f and rshift invert led ! ;

include morse.fs
include time.fs
include mac.fs
include continuation.fs
include packet.fs
include ip0.fs
include defines_tcpip.fs
include defines_tcpip2.fs
include arp.fs
include ip.fs
include dhcp.fs

include spi.fs
include flash.fs

: hardreset 
    true trig_reconf ! begin again ;

: softreset
    ['] emit-uart is emit
    sleep1 [char] a emit cr sleep1
    begin dsp h# ff and while drop repeat
    begin dsp d# 8 rshift while r> drop repeat
    dsp hex4 cr
    d# 0 >r
    sleep1 [char] b emit cr sleep1
;

include mt9v.fs

: setrouter ( router subnet -- )
    ip-subnetmask 2! ip-router 2!
    arp-reset
    net-my-ip arp-lookup drop arp-announce ;

: guess-mask ( ip -- )
    ip# 0.0.0.0 \ the world is my LAN
    ip# 0.0.0.0
    setrouter

    ip# 255.255.0.0 dand
    2dup ip# 10.0.0.0 d= if
        ip# 10.0.0.1 
        ip# 255.255.248.0 
        setrouter
    then
    2dup ip# 10.68.0.0 d= if
        ip# 10.68.0.1 
        ip# 255.255.255.0 
        setrouter
    then
    ip# 10.69.0.0 d= if
        ip# 10.69.0.11
        ip# 255.255.255.0 
        setrouter
    then
;

: ip-addr! ( ip -- )
    2dup ip-addr 2@ d<> if
        2dup ip-addr 2!
        guess-mask
        arp-reset
    else
        2drop
    then
;

include wge.fs
build-tcp? [IF]
    include newtcp.fs
    include html.fs
    include http.fs
    include tcpservice.fs
[THEN]


( IP address formatting                      JCB 14:50 10/26/10)

: #ip1  h# ff and s>d #s 2drop ;
: #.    [char] . hold ;
: #ip2  dup #ip1 #. d# 8 rshift #ip1 ;
: #ip   ( ip -- c-addr u) dup #ip2 #. over #ip2 ;

( net-watchdog                               JCB 09:26 10/13/10)

2variable net-watchdog
: net-watch-reset
    d# 10000000. net-watchdog setalarm ;
: net-watch-expired?
    net-watchdog isalarm ;

: preip-handler
    begin
        enc-fullness
    while
        OFFSET_ETH_TYPE packet@ h# 800 =
        if
           dhcp-wait-offer
        then
        camera-handler
    repeat
;

: strlen ( addr -- u ) dup begin count 0= until swap - 1- ;

include ntp.fs

: haveip-handler
    begin
        enc-fullness
    while
        net-watch-reset
        arp-handler
        OFFSET_ETH_TYPE packet@ h# 800 =
        if
            d# 2 OFFSET_IP_DSTIP enc-offset enc@n net-my-ip d=
            if
                icmp-handler
                \ IP_PROTO_TCP ip-isproto if servetcp then
                \ ntp-handler
            then
            camera-handler
        then
        depth if .s cr then
        depth d# 6 u> if hardreset then
    repeat
;

: bench   
    cbench
    d# 1000 >r \ iterations
    time@
    r@ negate begin
        \ d# 33. d# 101. 2d+ drop drop
        \ d# 33 d# 101 +1c drop
        \ d# 23 s>q d# 11 d# 17 qm*/ qdrop
        progress

        d# 1 + dup d# 0 =
    until drop
    time@
    decimal s" bench: " type
    2swap d- d# 6800 r> m*/ d# 600. d- <# # # [char] . hold #s #> type
    s"  cycles" type cr
;

ONBENCH [IF]
    : banner
        cr cr
        d# 64 0do [char] * emit loop cr
        s" J1 running" type cr
        cr

        s" Imager:  " type imagerversion @ hex4 cr
        s" PCB rev: " type pcb_rev @ . cr
        s" HDL rev: " type hdl_version @ hex4 cr
        s" FW rev:  " type version type
            s"  reports as " type version version-n hex d. decimal cr
        s" serial:  " type serial 2@ d. cr
        s" MAC:     " type net-my-mac mac-pretty cr
        cr
    ;
    : phy-report s" PHY status: " type d# 1 mac-mii@ hex4 cr ;

    create prev d# 4 allot
    : clocker
        time@ prev 2@ d- d# 1000000. d> if
            time@ prev 2!
            time@ hex8 space
            cr

            \ ntp-server arp-lookup if ntp-request then
        then
    ;
[ELSE]
    : phy-report ;
[THEN]

: .mii ( reg -- ) \ print MII reg value
    s" PHY" type dup . mac-mii@ hex4 ;

0 constant MIICONTROL
1 constant MIISTATUS
27 constant SPECIALS

: hackit
    \ MAC seems to need a long reset
    d# 0 MAC_reset ! sleep.1 d# 1 MAC_reset ! sleep.1
    \ Turn off auto-neg
    
    begin
         \ Register 0, Bit 12  = 0
         \ Register 0, Bit 13 = 1
         \  Register 0, Bit 8 = 1
        MIICONTROL mac-mii@
            h# efff and
            h# 2100 or
        snap MIICONTROL mac-mii!
        snap
        h# 8000 SPECIALS mac-mii!
        snap
        MIICONTROL .mii space MIISTATUS .mii space SPECIALS .mii cr
        sleep1
    again
;

SYSLOG [IF]
include syslog.fs
[THEN]

: get-dhcp
    net-my-mac xor mt9v-random d+ dhcp-xid!
    d# 0. dhcp-alarm setalarm

    d# 1000
    begin
        net-my-ip d0=
    while
        dhcp-alarm isalarm if
            dhcp-discover
            2* d# 8000 min
            dup d# 1000 m* dhcp-alarm setalarm
        then
        preip-handler
    repeat
    snap
    drop
    depth if begin again then
;

2variable ntp-alarm

: silence drop ;

: main
    decimal
    mt9v-cold
    atmel-cold
    atmel-cfg-rd
    atmel-id-rd

    ONBENCH [IF]
        banner
    [THEN]

    \ hackit

    net-my-mac mac-cold
    phy-report

    net-my-ip d0= if
        get-dhcp
    else
        net-my-ip guess-mask
    then

    arp-reset

    build-tcp? [IF]
        tcp-cold
    [THEN]

    ONBENCH [IF]
        dhcp-status
    [THEN]
    SYSLOG [IF]
        begin
            haveip-handler syslog-server arp-lookup 0=
        while
            sleep.1
        repeat
        s" syslog -> " type syslog-server ip-pretty cr 
        syslog-cold ['] emit-syslog is emit
    [ELSE]
        ['] silence is emit
    [THEN]

    build-debug? [IF]
        s" booted serial://" type serial 2@ d.
        s" from " type
        h# 3ffe @ if s" mcs" else s" flash" then type
        s"  ip " type
        net-my-ip <# #ip #> type space
        s" xorline=" type
        [ DO-XORLINE ] literal hex1 space
        version type
        cr
        s" ready" type cr
    [THEN]

    \ net-my-ip arp-lookup drop arp-announce

    d# 1000000. ntp-alarm setalarm
    net-watch-reset
    begin
        \ clocker
        inframe invert if
            haveip-handler
        then
        mt9v-cycle
        net-watch-expired? if
            net-watch-reset
            mt9v-cold
            net-my-mac mac-cold
        then
        \ ntp-alarm isalarm if
        \     ntp-request
        \     d# 1000000. ntp-alarm setalarm
        \ then
    again

    halt
;
]module

0 org

code 0jump
    \ h# 3e00 ubranch
    main ubranch
    main ubranch
end-code

meta

hex


: create-output-file w/o create-file throw to outfile ;
s" j1.mem" create-output-file
:noname
    s" @ 20000" type cr
    4000 0 do i t@ s>d <# # # # # #> type cr 2 +loop
; execute

s" j1.bin" create-output-file
:noname 4000 0 do i t@ dup 8 rshift emit emit 2 +loop ; execute

s" j1.lst" create-output-file
d# 0
h# 2000 disassemble-block
