( IP networking: headers and wrapup         JCB 13:21 08/24/10)
module[ ip"


: ip-datalength
    OFFSET_IP_LENGTH packet@
    d# 20 - 2/
;

: ip-isproto
    OFFSET_IP_TTLPROTO packet@ h# ff and =
;

\  : checksum          # ( addr n  -- sum )
\      d# 0 swap                  # addr sum n
\      begin
\          -rot                # n addr sum
\          over d# 1 swap enc@n   # n addr sum w
\          +1c                 # n addr sum
\          -rot                # sum n addr
\          2+
\          -rot                # addr sum n
\          1- dup 0=
\      until
\      drop nip
\      invert
\  ;

: ip-identification
    ip-id-counter d# 1 over +! @
;

: @ethaddr ( eth-addr -- mac01 mac23 mac45 )
    ?dup
    if
        dup @ swap 2+ 2@
    else
        mac-broadcast
    then
;

: ip-header         ( dst-ip src-ip eth-addr protocol -- )
    >r
    enc-pkt-begin

    @ethaddr enc-pkt-3,
    net-my-mac enc-pkt-3,
    h# 800 enc-pkt-,

    h# 4500
    h# 0000                  \  length
    ip-identification
    enc-pkt-3,
    h# 4000                  \  do not fragment
    h# 4000 r> or            \  TTL, protocol
    d# 0                        \  checksum
    enc-pkt-3,
    enc-pkt-2,              \  src ip
    enc-pkt-2,              \  dst ip
;

: ip-wrapup ( bytelen -- )
    \  write IP length
    OFFSET_IP -
    OFFSET_IP_LENGTH packetout-off enc!

    \  write IP checksum
    OFFSET_IP packetout-off d# 10 enc-checksum
    OFFSET_IP_CHKSUM packetout-off enc!
;

: ip-packet-srcip
    d# 2 OFFSET_IP_SRCIP enc-offset enc@n
;

( ICMP return and originate                  JCB 13:22 08/24/10)

\  Someone pings us, generate a return packet

: icmp-handler
    IP_PROTO_ICMP ip-isproto
    OFFSET_ICMP_TYPECODE packet@ h# 800 =
    and if
        ip-packet-srcip
        2dup arp-lookup
        ?dup if
            \  transmit ICMP reply
                                    \  dstip *ethaddr
            net-my-ip rot           \  dstip srcip *ethaddr
            d# 1 ip-header

            \  Now the ICMP header
            d# 0 enc-pkt-,

            OFFSET_ICMP_IDENTIFIER enc-offset
            ip-datalength 2-        ( offset n )
            tuck
            enc-checksum enc-pkt-,
            OFFSET_ICMP_IDENTIFIER enc-pkt-src

            enc-pkt-complete
            ip-wrapup
            enc-send
        else
            2drop
        then
    then
;
    
: ping ( ip. -- ) \ originate
    2dup arp-lookup
    ?dup if
        \  transmit ICMP request
                                \  dstip *ethaddr
        net-my-ip rot           \  dstip srcip *ethaddr
        d# 1 ip-header

        \  Now the ICMP header
        h# 800 enc-pkt-,

        \  id is h# 550b, seq is lo word of time
        h# 550b time@ drop
        2dup +1c h# 800 +1c
        d# 28 begin swap d# 0 +1c swap 1- dup 0= until drop
        invert enc-pkt-,     \  checksum
        enc-pkt-2,
        d# 28 enc-pkt-,0

        enc-pkt-complete
        ip-wrapup
        enc-send
    else
        2drop
    then
;

( UDP header and wrapup                      JCB 13:22 08/24/10)

: udp-header ( dst-port src-port dst-ip src-ip *ethaddr -- )
    h# 11 ip-header
    enc-pkt-,              \  src port
    enc-pkt-,              \  dst port
    d# 2 enc-pkt-,0        \  length and checksum
;

variable packetbase
: packet packetbase @ + ;

: udp-checksum ( addr -- u ) \ compute UDP checksum on packet
    packetbase !
    ETH.IP.UDP.LENGTH packet @ d# 1 and if
        ETH.IP.UDP ETH.IP.UDP.LENGTH packet @ + packet
        dup @ h# ff00 and swap !
    then
    ETH.IP.UDP packet
    ETH.IP.UDP.LENGTH packet @ 1+ 2/
    enc-checksum invert
    d# 4 ETH.IP.SRCIP packet enc@n
    +1c +1c +1c +1c
    IP_PROTO_UDP +1c
    ETH.IP.UDP.LENGTH packet @ +1c
    invert
;

: udp-checksum? incoming udp-checksum 0= ;

: udp-wrapup
    enc-pkt-complete dup
    ip-wrapup

    OFFSET_UDP -
    OFFSET_UDP_LENGTH packetout-off enc!

    outgoing udp-checksum ETH.IP.UDP.CHECKSUM packetout-off !
;

]module


( IP address literals                        JCB 14:30 10/26/10)

================================================================

It is neat to write IP address literals e.g.
ip# 192.168.0.1

================================================================

meta

: octet# ( c -- u ) 0. rot parse >number throw 2drop ;

: ip# 
    [char] . octet# 8 lshift
    [char] . octet# or do-number
    [char] . octet# 8 lshift
    bl octet#       or do-number
;
target
