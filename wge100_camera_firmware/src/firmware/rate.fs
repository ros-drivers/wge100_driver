( Rate reporting tool                        JCB 17:25 09/09/10)
create first 4 allot
variable ecount

: event
    ecount @ d# 0 = if
        time@ first 2!
    then
    d# 1 ecount +!
    ecount @ d# 101 = if
        d# 0 ecount !
        time@ first 2@ d-
        d# 1 d# 1000 m*/
        drop
        >r
        d# 10000000. d# 1 r> m*/
        <# # # [char] . hold #S #> type s"  Hz" type cr
    then
;

