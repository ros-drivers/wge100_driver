( Low level TCP words                        JCB 13:24 08/24/10)
module[ newtcp"
\ service is:
\   d# 0   link
\   d# 1   port
\   d# 2   setup-word
\   d# 3   char-handler-word
\   d# 4   output-word

variable tcp-ptr

create tcps d# 64 allot
create tcp1 d# 32 allot
: tcp-base          tcp-ptr @ d# 5 lshift tcps + + ;

: _ip            d# 0 tcp-base ;
: _port          d# 4 tcp-base ;
: _statecount    d# 6 tcp-base ;
: _seq           d# 8 tcp-base ;
: _ack           d# 12 tcp-base ;
: _alarm-ptr     d# 16 tcp-base ;
: _fsm           d# 18 tcp-base ;
: _srv           d# 20 tcp-base ;   \ This is an XT, so needs d# 1 cell

: tcp-alarm _alarm-ptr d# 5 ;
: tcp-state@        _statecount c@ d# 15 and ;

    meta
    variable service-list
    : tcp-service
        there
        service-list @ t,
        service-list !

        t,
        ', ', ', ;
    : service-list@ service-list @ ; immediate
    target

: service@ ( n -- v ) cells _srv @ + @ ;

d# 0 constant LISTEN
d# 1 constant SYN_RCVD
d# 2 constant ESTAB
d# 3 constant LAST_ACK
d# 4 constant FIN_WAIT1
d# 5 constant FIN_WAIT2

: state!
    \ [ d# 1 seconds ] literal tcp-alarm j-setalarm
    _statecount c! ;

: >LISTEN LISTEN state! ;
: >SYN_RCVD SYN_RCVD state! ;
: >ESTAB ESTAB state! ;
: >LAST_ACK LAST_ACK state! ;
: >FIN_WAIT1 FIN_WAIT1 state! ;
: >FIN_WAIT2 FIN_WAIT2 state! ;

: tcp-header               \  ( flags -- )
    _ip 2@
    net-my-ip
    2over arp-lookup        ( dst-ip src-ip *ethaddr )
    h# 6 ip-header
    d# 1 service@ enc-pkt-,
    _port @ enc-pkt-,
    _seq 2@ swap enc-pkt-2,
    _ack 2@ swap enc-pkt-2,
    h# 5000 or enc-pkt-,   \  flags
    d# 1024 enc-pkt-,         \  window
    d# 2 enc-pkt-,0 ;         \  checksum, urgent

: seq+      s>d _seq d+! ;
: ack+      s>d _ack d+! ;
: tcp-wrapup  \ close out the TCP packet: compute checksum
    enc-pkt-complete
    dup OFFSET_TCP -   ( bytelen tcplen )
    dup d# 20 - seq+
    OFFSET_TCP packetout-off        \ Start of TCP packet
    over 1+ 2/                      ( tcplen TCP_off tcplen/2 )
    enc-checksum invert             \ Checksum for TCP header and data
                                    \ the pseudo-header
    d# 4 OFFSET_IP_SRCIP packetout-off enc@n
    d# 6
    +1c +1c +1c +1c +1c +1c invert
    OFFSET_TCP_CHECKSUM packetout-off enc!
                                    ( bytelen )
    ip-wrapup ;
: flag?     OFFSET_TCP_FLAGS packet@ and 0<> ;
: response ( flags -- ) \ No data, just send a response packet
    TCP_ACK or tcp-header tcp-wrapup enc-send ;
: SYN       TCP_SYN response d# 1 seq+ ;
: resendSYN d# -1 seq+ SYN ;
: ACK       d# 0 response ;
: ACK>TIMED ACK >LISTEN ;
: FIN>LAST  ACK TCP_FIN response d# 1 seq+ >LAST_ACK ;
: header#   OFFSET_TCP_FLAGS packet@ d# 10 rshift h# 3c and ;
: payload#  OFFSET_IP_LENGTH packet@ header# d# 20 + - ;

: senddata 
    [ TCP_ACK TCP_PSH or TCP_FIN or ] literal
    tcp-header
    d# 4 service@ execute
    tcp-wrapup
    d# 1 seq+   \ for the FIN
    enc-send
    ;
: null ;

: resenddata d# 1 d# 1000 _seq 2! senddata ;

: execstate tcp-state@ addrcells + @ execute ;
\                                     | SYN_RCVD   | ESTAB      | LAST_ACK   | FIN_WAIT1  | FIN_WAIT2
createdoes gotACK execstate | null    | >ESTAB     | ACK        | >LISTEN    | >FIN_WAIT2 | >FIN_WAIT2
createdoes gotFIN execstate | null    | null       | FIN>LAST   | null       | ACK>TIMED  | ACK>TIMED    
createdoes gotTIM execstate | >LISTEN | resendSYN  | ACK        | resenddata | resenddata | null

: setup-conn \ set up a connection
    ip-packet-srcip _ip 2!
    OFFSET_TCP_SOURCEPORT packet@ _port !
    OFFSET_TCP_SEQNUM packetd@ d1+ _ack 2!
    d# 0 d# 1000 _seq 2!
    SYN >SYN_RCVD ;

variable ENC_ERDPT

: >payload \ move read pointer to TCP payload
    header# OFFSET_TCP + enc-offset ENC_ERDPT ! ;

: onbuffer ( # -- ) \ pass consecutive chars to the char-handler
    ?dup if
        d# 3 service@ swap ( function # )
        0do ENC_ERDPT @ d# 1 xor c@ d# 1 ENC_ERDPT +!  over execute loop
        drop
    then ;

: ismine?
    ip-packet-srcip _ip 2@ d=
    OFFSET_TCP_SOURCEPORT packet@ _port @ = and
    \  has other side seen my latest?
    OFFSET_TCP_ACK packetd@ _seq 2@ d= and
    \  is their sequence what I expect?
    OFFSET_TCP_SEQNUM packetd@ _ack 2@ d= and ;

: tcp-cold
    NUM_TCPS
    0do 
        i tcp-ptr !
        d# 0 _statecount c!
    loop ;

: tcp-timeout
    h# 10 _statecount c+!
    _statecount c@ h# 50 <
    if
        gotTIM
    else
        \  d# 4 timeouts: kill the connection
        >LISTEN
    then
;

build-debug? [IF]
: tcp-alarms
    NUM_TCPS 0do
        i tcp-ptr !
        tcp-timeout
    loop
;
[THEN]
]module
