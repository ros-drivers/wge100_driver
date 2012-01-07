( TCP general service scheme                 JCB 13:25 08/24/10)

module[ tcpservice"
: setup \ Search for service, and if found set up the connection
    OFFSET_TCP_DESTPORT packet@ 
    [ service-list@ ] literal
    begin
        ?dup
    while
        2dup addrcell+ @ ( p srv-addr p port )
        = if
            dup _srv !
            setup-conn
            d# 2 addrcells + @ execute
            drop exit
        then
        @
    repeat
    drop
;


: servetcp \ process incoming TCP packet
    \ process the packet only if the sender is in the ARP cache
    ip-packet-srcip arp-lookup 0<> if
        NUM_TCPS 0do i tcp-ptr !
            TCP_SYN flag? if
                tcp-state@ 0= if setup unloop exit then
            else
                ismine? if
                    payload# dup TCP_FIN flag? d# 1 and + ack+
                    TCP_FIN flag? if gotFIN else gotACK then
                    >payload
                    onbuffer
                    tcp-state@ ESTAB = if
                        respond
                    then
                    unloop exit
                then
            then
        loop
    then
;

]module
