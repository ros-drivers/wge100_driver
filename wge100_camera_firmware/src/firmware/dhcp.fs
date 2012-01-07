( DHCP: Dynamic Host Configuration Protocol  JCB 13:13 08/24/10)
module[ dhcp"

\  Since DHCP alarm is only used when there is no lease, it is
\  safe to use the ip-subnetmask for the same purpose.

ip-subnetmask constant dhcp-alarm

: dhcp-xid
    ip-router 2@
;

: dhcp-xid!
    ip-router 2!
;

: dhcp-option  \  ( ... n code -- )
    enc-pkt-c,
    dup enc-pkt-c,
    0do
        enc-pkt-c,
    loop
;

: dhcp-common   \  ( messagetype -- )
    d# 67 d# 68
    d# 0 invert dup
    d# 0 dup
    d# 0               \  broadcast ethaddr
                            ( dst-port src-port dst-ip src-ip *ethaddr -- )
    udp-header
    h# 0101 h# 0600 enc-pkt-2,
    dhcp-xid enc-pkt-2,
    d# 10 enc-pkt-,0
    net-my-mac enc-pkt-3,
    d# 101 enc-pkt-,0          \  d# 5 + d# 96 zeroes

    h# 6382 h# 5363
    enc-pkt-2,

    \  DHCP option 53: DHCP Discover
                            \  messagetype
    d# 1 d# 53                    \  messagetype 1 53
    dhcp-option

    \  DHCP option 50: 192.168.1.100 requested

    \  DHCP option 55: Parameter Request List: 
    \  Request Subnet Mask (1), Router (3),
    \  Domain Name Server (6)
    d# 1 d# 3 d# 6   d# 3 d# 55 dhcp-option
;

: dhcp-wrapup
    \  Finish options
    h# ff enc-pkt-c,
    \ enc-wrptr @ d# 1 and 
    d# 1 if    \ XXX
        h# ff enc-pkt-c,
    then

    udp-wrapup
    enc-send
;

\  memory layout is little-endian

: encc@++ ( c-addr -- c-addr+1 c ) dup 1+ swap enc-c@ ;

: dhcp-field    \  ( match -- ptr/0 )
    OFFSET_DHCP_OPTIONS d# 4 + enc-offset 
                            \  match ptr
    begin
        encc@++             \  match ptr code
        dup h# ff <>
    while                   \  match ptr code
        d# 2 pick =                  
        if
            nip             \  ptr
            exit
        then                \  match ptr
        encc@++ +           \  match ptr'
    repeat
    \  fail - return false
    2drop false
;

: dhcp-yiaddr
    d# 2 OFFSET_DHCP_YIADDR enc-offset enc@n
;

: dhcp-field4
    dhcp-field d# 1 +
    encc@++ swap encc@++ swap encc@++ swap enc-c@
                            ( a b c d )
    swap d# 8 lshift or -rot
    swap d# 8 lshift or
    swap
;

build-debug? [IF]
: .pad ( ip. c-addr u -- )  d# 14 typepad ip-pretty cr ;

: dhcp-status
    ip-addr 2@ s" IP" .pad
    ip-router 2@ s" router" .pad
    ip-subnetmask 2@ s" subnetmask" .pad
;
[ELSE]
: dhcp-status ;
[THEN]

: lease-setalarm
    d# 0 >r
    begin
        2dup d# 63. d>
    while
        d2/ r> 1+ >r
    repeat
    r>
    hex4 space hex8 cr
;

: dhcp-wait-offer
    h# 11 ip-isproto
    OFFSET_UDP_SOURCEPORT packet@ d# 67 = and
    OFFSET_UDP_DESTPORT packet@ d# 68 = and
    d# 2 OFFSET_DHCP_XID enc-offset enc@n dhcp-xid d= and
    if
        d# 53 dhcp-field ?dup
        if 
            d# 1 + enc-c@ 
            dup d# 2 =
            if
                \ [char] % emit
                d# 3 dhcp-common

                \  option 50: request IP
                h# 3204
                dhcp-yiaddr
                enc-pkt-3,

                \  Option 54: server
                h# 3604
                d# 54 dhcp-field4
                enc-pkt-3,

                dhcp-wrapup
            then
            d# 5 =
            if
                \ clrwdt
                \ [char] & emit

                dhcp-yiaddr ip-addr 2!
                d# 1 dhcp-field4 ip-subnetmask 2!
                \  For the router and DNS server, send out ARP requests right now.  This
                \  reduces start-up time.
                d# 3 dhcp-field4 2dup ip-router 2! arp-lookup drop 
                d# 6 dhcp-field4 2dup ip-dns    2! arp-lookup drop 
                \  Option 51: lease time
                s" expires in " type
                d# 51 dhcp-field4 swap d. cr
            then
        then
    then
;

: dhcp-discover d# 1 dhcp-common dhcp-wrapup ;

]module
