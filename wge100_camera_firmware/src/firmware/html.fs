( HTML: construct an HTML page               JCB 13:18 08/24/10)
module[ html"

: html-emit enc-pkt-c, ;

: html-cr d# 13 html-emit d# 10 html-emit ;
: html-space d# 32 html-emit ;

: html-begin
    2dup
    [char] < html-emit
    enc-pkt-s,
    [char] > html-emit
;

: html-html  s" html"  html-begin ;
: html-body  s" body"  html-begin ;
: html-head  s" head"  html-begin ;
: html-title s" title" html-begin ;
: html-h1    s" h1"    html-begin ;
: html-p     s" p"     html-begin ;

: html-hr    s" <hr>"  enc-pkt-s, ;

\ : html-form
\    s" form"
\ ;

: html-end
    [char] < html-emit
    [char] / html-emit
    enc-pkt-s,
    [char] > html-emit
    html-cr
;

: html-response
    \ debug-response
    s" HTTP/1.1 " enc-pkt-s,
    dup s>d <# #s #> enc-pkt-s, html-space
    dup d# 200 = if
        s" OK"
    else
        dup d# 401 = if 
            s" Authorization Required"
        else
            dup d# 303 = if
                s" See Other"
            then
        then
    then
    enc-pkt-s,
    html-cr
    d# 303 = if
        s" Location: /"
    else
        s" Connection: close\r\nContent-Type: text/html; charset=iso-8859-1"
    then
    enc-pkt-s,
    html-cr
    html-cr
;

: mac#w d# 0 # # [char] : hold # # ;
: mac#
    <#
    mac#w [char] : hold 2drop
    mac#w [char] : hold 2drop
    mac#w
    #>
;

: html-stat ( ex str -- ) \ Append a formatted statistic
    html-p 2swap
    enc-pkt-s,
    s" : " enc-pkt-s,
    rot execute enc-pkt-s,
    html-end
;

: dec#    decimal <# #s #> ;
: serial# serial 2@ dec# ;
: pcbrev# pcb_rev @ s>d dec# ;
: hdlrev# hdl_version @ s>d hex <# # # # #> ;
: fwrev#  version ;
: frame#  frame_number 2@ swap dec# ;
: my-mac# hex     net-my-mac mac# ;
: uptime# decimal time@ <# # # # # # # [char] . hold #s #> ;

: name,   s" WGE Camera " enc-pkt-s, serial# enc-pkt-s, ;

: test#
     serial# ;

: passfail#  if s" <ok>pass</ok>" else s" <b>FAIL</b>" then ;

: htmltestrunner ( testlist -- )
    begin
        dup @ ?dup
    while
        execute ( str. f )
        >r
        html-p 2swap
        enc-pkt-s,
        s" : " enc-pkt-s,
        r> passfail# enc-pkt-s,
        html-end
        cell+
    repeat
    drop
;

: html-page-root
    d# 200 html-response
    html-html

        html-head
            html-title name, html-end
            s' <style type="text/css">body { font-family:Verdana,sans; } b {color:red;} ok {color:green;}</style>' enc-pkt-s,
        html-end

        html-body
            html-h1 name, html-end

            ['] pcbrev# s" PCB rev"         html-stat
            ['] hdlrev# s" HDL rev"         html-stat
            ['] fwrev#  s" Firmware rev"    html-stat
            ['] serial# s" Serial"          html-stat
            ['] my-mac# s" MAC"             html-stat
            ['] uptime# s" Uptime"          html-stat
            ['] frame#  s" Frame number"    html-stat
            html-hr
            mt9v-tests htmltestrunner
            html-hr
        html-end

    html-end            \  html
;

: html-page
    decimal
    html-page-root
;

]module
