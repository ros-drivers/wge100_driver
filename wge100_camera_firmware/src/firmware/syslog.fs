( syslog, RFC 5424                           JCB 10:57 08/30/10)

module[ syslog"

variable syslogc
create syslogline 80 allot

: syslog-server 
   ip# 10.0.2.131 ;

: syslog-cold
    d# 0 syslogc !
    syslog-server arp-lookup drop
;

: syslog-send ( addr u -- )
    syslog-server arp-lookup 0= if
        2drop 
    else
        d# 514 d# 9999
        syslog-server
        net-my-ip
        2over arp-lookup
        ( dst-port src-port dst-ip src-ip *ethaddr )
        udp-header
        s" <4>" enc-pkt-s,
        camera_name dup strlen enc-pkt-s,
        bl enc-pkt-c,
        enc-pkt-s,
        udp-wrapup enc-send
    then
;

: emit-syslog ( x -- ) \ append to syslog line, cr transmits
    dup d# 10 = if drop exit then
    dup d# 13 <> if
        syslogline syslogc @ + c!
        syslogc @ 1+ d# 79 min
    else
        drop syslogline syslogc @ syslog-send
        d# 0
    then
    syslogc !
;
]module
