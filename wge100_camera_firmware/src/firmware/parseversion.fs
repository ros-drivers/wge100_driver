( parse the string returned by svnversion    JCB 15:59 10/26/10)

: version-n ( caddr u -- u )
    d# 0. 2swap
    begin
        d# 0. 2swap
        >number ( prev this str )
        2>r dmax 2r>
        dup 0<> if d# 1 /string then
        dup 0=
    until
    2drop ;

0 [IF]
s" 404:1337M" version-n d.
s" 404:1337MS" version-n d.
s" 1337" version-n d.
s" 1337M" version-n d.
.s
bye
[THEN]
