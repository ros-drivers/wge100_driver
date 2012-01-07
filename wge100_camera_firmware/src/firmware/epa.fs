( Arbitrary Precision Arithmetic             JCB 20:07 09/03/10)

module[ epa"
4 constant qs   [ qs 2* ] constant qcell   [ qs 1- ] constant qs-1

variable busy
create store [ 16 qs * 2* ] allot

: new ( -- a )
    d# -1 begin 1+ d# 1 over lshift busy @ and 0= until
    d# 1 over lshift busy @ or busy !
    qcell * store + ;
: qdrop ( a -- )
    store - d# 3 rshift d# 1 swap lshift invert busy @ and busy ! ; 
: qnip swap qdrop ;
: qclone ( a1 -- a2 ) new tuck qcell cmove ;
: qdup dup qclone ;
: qover over qclone ;

( The standard set of operators              JCB 15:28 09/04/10)

variable cc
: (for)     [ qs-1 2* ] literal cc ! ;
: (tocell)  cc @ + @ ;
: (tocell!) cc @ + ! ;
: (end)     cc @ d# -2 + dup cc ! 0< ;

meta
: pre1      s" dup >r" evaluate ; immediate
: pre2      s" 2dup 2>r qnip" evaluate ; immediate
: for[      s" (for) begin" evaluate ; immediate
: ]end1     s" (end) until r> drop" evaluate ; immediate
: ]end2     s" (end) until 2r> 2drop" evaluate ; immediate
: tt@       s" r@ (tocell)" evaluate ; immediate
: n@        s" 2r@ drop (tocell)" evaluate ; immediate
: tt!       s" r@ (tocell!)" evaluate ; immediate
target

: q0<       dup @ 0< ;
: q0=       pre1 qdrop true for[ tt@ 0= and ]end1 ;
: qinvert   pre1 for[ tt@ invert tt! ]end1 ;
: qxor      pre2 for[ tt@ n@ xor tt! ]end2 ;
: qand      pre2 for[ tt@ n@ and tt! ]end2 ;
: qor       pre2 for[ tt@ n@ or tt! ]end2 ;
: q+c       pre2 d# 0 for[ d# 0 tt@ d# 0 d+ n@ d# 0 d+ swap tt! ]end2 ;
: q+        q+c drop ;
: q1+       pre1 d# 1 for[ d# 0 tt@ d# 0 d+ swap tt! ]end1 drop ;
: u>q       new pre1 swap for[ tt! d# 0 ]end1 drop ;
: s>q       new pre1 swap for[ dup tt! 0< ]end1 drop ;
: >q        new qs d# 0 do tuck i cells + ! loop ;
: q>        pre1 qdrop for[ tt@ ]end1 ;
: q@        new tuck qcell cmove ;
: q!        over qdrop qcell cmove ;
: q>s       dup qs-1 cells + @ swap qdrop ;
: qnegate   qinvert q1+ ;
: qabs      q0< if qnegate then ;
: q0<>      q0= invert ;
: q-        qnegate q+ ;
: qu>=      qnegate q+c qnip 0<> ;
: qu<=      swap qu>= ;
: qu<       qu>= invert ;
: qu>       swap qu< ;
: q=        qxor q0= ;
: q<>       q= invert ;
: q2/h      pre1 for[ d# 0 tt@ d2/ drop tt! ]end1 ;
: q2/l      pre1 for[ tt@ 2/ tt! ]end1 ;
: q2/       qdup q2/l swap
            q> qs-1 roll drop d# 0 >q q2/h
            qor ;
: q2*c      pre1 d# 0 for[
                tt@ tuck 2* or tt! 0< d# 1 and
            ]end1 ;
: q2*       q2*c drop ;

( qm*/ is the equivalent of m*/              JCB 15:24 09/04/10)

: *+ ( a b c -- b*c+a ) um* rot d# 0 d+ ;
: qm*/mod
    divisor !
    over >r d# 0 swap ( c f )
    for[
        tuck    ( f c f )
        tt@      ( f c f x )
        *+      ( f f*x+a )
        swap tt! \ low cell to T
        swap    ( c f )
    ]end1 drop
                             ( qLower upper )
    qcell d# 8 * d# 0 do
        swap q2*c rot 2* +
        dup divisor @ >= if
            divisor @ -
            swap q1+ swap
        then
    loop ;
: qm*/ qm*/mod drop ;

( Formatted output                           JCB 15:22 09/04/10)

: q<#   <# ;
: q#>   qdrop d# 0. #> ;
: q#    d# 1 base @ qm*/mod d# 0 # 2drop ;
: q#s   begin q# qdup q0= until ;
: q.    <# q#s q#> type space ;

]module
