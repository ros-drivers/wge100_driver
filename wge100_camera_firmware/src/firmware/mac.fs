(                                            JCB 13:23 08/24/10)

: mac-pretty-w
    dup endian hex2
    [char] : emit
    hex2
;

: mac-pretty
    swap rot
    mac-pretty-w [char] : emit
    mac-pretty-w [char] : emit
    mac-pretty-w
;

: ip-pretty-byte
    h# ff and
    \ d# 0 u.r
    hex2
;

: ip-pretty-2
    dup swab ip-pretty-byte [char] . emit ip-pretty-byte
;

: ip-pretty
    swap
    ip-pretty-2 [char] . emit
    ip-pretty-2
;

: showmii
    s" mii: " type 
    MAC_MII_ack @ hex4 space MAC_MII_rdata @ hex4 cr
;



: mac-mii ( we -- )
    MAC_MII_we ! h# f MAC_MII_phyaddr ! d# 1 MAC_MII_req !
    begin MAC_MII_ack @ until 
    d# 0 MAC_MII_req !
;
: mac-mii@ ( reg -- v )
    MAC_MII_addr !
    d# 0 mac-mii
    MAC_MII_rdata @
;
: mac-mii! ( v reg -- )
    MAC_MII_addr ! MAC_MII_wdata !
    d# 1 mac-mii
;

: mac-cold   ( ethaddr -- )
    rx_mac_filter d# 4 + !
    rx_mac_filter d# 2 + !
    rx_mac_filter !

    \ MAC seems to need a long reset
    d# 0 MAC_reset ! sleep.1 d# 1 MAC_reset ! sleep.1
    \ Wait for link up
    begin
        sleep.1
        d# 1 mac-mii@
        h# 24 and h# 24 = \ Autoneg complete + Link established
    until
;
