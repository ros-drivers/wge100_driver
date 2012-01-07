( NTP                                        JCB 09:54 11/17/10)

: ntp-server 
   \ h# 02830a00.
   \ ip# 91.189.94.4 \ time.ubuntu
   ip# 17.151.16.20  \ time.apple.com
;

: ntp-request
    d# 123 d# 9999
    ntp-server
    net-my-ip
    2over arp-lookup
    ( dst-port src-port dst-ip src-ip *ethaddr )
    udp-header
    h# 2304 enc-pkt-, h# 04ec enc-pkt-, 
    d# 6 enc-pkt-,0

    d# 4 enc-pkt-,0 \ originate
    d# 4 enc-pkt-,0 \ reference
    d# 4 enc-pkt-,0 \ receive
    \ d# 4 enc-pkt-,0 \ transmit
    time@ enc-pkt-d, d# 2 enc-pkt-,0
    udp-wrapup enc-send
;

variable seconds
variable minutes
variable hours
variable days
variable months
variable years
variable weekday

: setdate ( ud -- )
    [ -8 3600 * ] literal s>d d+
    d# 1 d# 60 m*/mod seconds !
    d# 1 d# 60 m*/mod minutes !
    d# 1 d# 24 m*/mod hours !
    d# 59. d- \ Days since Mar 1 1900
    2dup d# 1 d# 7 m*/mod weekday ! 2drop
    d# 365 um/mod ( days years )
    dup d# 1900 + years !
    d# 4 / 1- - \ subtract leaps ( daynum 0-365 )
    dup d# 5 * d# 308 + d# 153 / d# 2 - months !
    months @ d# 4 + d# 153 d# 5 */ - d# 122 + days !

    years @ .
    s" MARAPRMAYJUNJULAUGSEPOCTNOVDECJANFEB" drop
    months @ d# 3 * + d# 3 type space
    days @ .
    hours @ .
    minutes @ .
    seconds @ .
    s" THUFRISATSUNMONTUEWED" drop
    weekday @ d# 3 * + d# 3 type space
    cr
;

: ntp-handler
    IP_PROTO_UDP ip-isproto
    ETH.IP.UDP.SOURCEPORT packet@ d# 123 = and
    ETH.IP.UDP.DESTPORT packet@ d# 9999 = and
    if
        ETH.IP.UDP.NTP.TRANSMIT packetd@ setdate

        time@ ETH.IP.UDP.NTP.ORIGINATE packetd@ d-
        s" ntp delay " type
        d. cr
    then
;

