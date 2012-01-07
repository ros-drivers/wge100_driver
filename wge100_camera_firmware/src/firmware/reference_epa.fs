( Arbitrary Precision Arithmetic             JCB 20:07 09/03/10)

4 constant qs   qs cells constant qcell   qs 1- constant qs-1
variable busy
create store 16 qs * cells allot

: new ( -- a )
    -1 begin 1+ 1 over lshift busy @ and 0= until
    1 over lshift busy @ or busy !
    qcell * store + ;
: qdrop ( a -- )
    store - qcell / 1 swap lshift invert busy @ and busy ! ; 
: qnip swap qdrop ;
: qclone ( a1 -- a2 ) new tuck qcell move ;
: qdup dup qclone ;
: qover over qclone ;

( The standard set of operators              JCB 15:28 09/04/10)

variable cc
: (tocell)  cc @ + @ ;
: (tocell!) cc @ + ! ;
: (end)     -1 cells cc +! cc @ 0< ;

: pre1      s" dup >r" evaluate ; immediate
: pre2      s" 2dup 2>r qnip" evaluate ; immediate
: for[      s" qs-1 cells cc ! begin" evaluate ; immediate
: ]end1     s" (end) until r> drop" evaluate ; immediate
: ]end2     s" (end) until 2r> 2drop" evaluate ; immediate
: t@        s" r@ (tocell)" evaluate ; immediate
: n@        s" 2r@ drop (tocell)" evaluate ; immediate
: t!        s" r@ (tocell!)" evaluate ; immediate

: q0<       dup @ 0< ;
: q0=       pre1 qdrop true for[ t@ 0= and ]end1 ;
: qinvert   pre1 for[ t@ invert t! ]end1 ;
: qxor      pre2 for[ t@ n@ xor t! ]end2 ;
: qand      pre2 for[ t@ n@ and t! ]end2 ;
: qor       pre2 for[ t@ n@ or t! ]end2 ;
: q+c       pre2 0 for[ 0 t@ 0 d+ n@ 0 d+ swap t! ]end2 ;
: q+        q+c drop ;
: q1+       pre1 1 for[ 0 t@ 0 d+ swap t! ]end1 drop ;
: u>q       new pre1 swap for[ t! 0 ]end1 drop ;
: s>q       new pre1 swap for[ dup t! 0< ]end1 drop ;
: >q        new qs 0 do tuck i cells + ! loop ;
: q>        pre1 qdrop for[ t@ ]end1 ;
: q@        new tuck qcell move ;
: q!        over qdrop qcell move ;
: q>s       dup qs-1 cells + @ swap qdrop ;
: q2*       qdup q+ ;
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
: q2/h      pre1 for[ 0 t@ d2/ drop t! ]end1 ;
: q2/l      pre1 for[ t@ 2/ t! ]end1 ;
: q2/       qdup q2/l swap
            q> qs-1 roll drop 0 >q q2/h
            qor ;

( qm*/ is the equivalent of m*/              JCB 15:24 09/04/10)

: *+ ( a b c -- b*c+a ) um* rot 0 d+ ;
variable divisor
: qm*/mod
    divisor !
    over >r 0 swap ( c f )
    for[
        tuck    ( f c f )
        t@      ( f c f x )
        *+      ( f f*x+a )
        swap t! \ low cell to T
        swap    ( c f )
    ]end1 drop
                             ( qLower upper )
    qcell 8 * 0 do
        swap qdup q+c rot 2* +
        dup divisor @ >= if
            divisor @ -
            swap q1+ swap
        then
    loop ;
: qm*/ qm*/mod drop ;

( Formatted output                           JCB 15:22 09/04/10)

: q<# <# ; : q#> 0 #> ;
: q# 1 base @ qm*/mod 0 # 2drop ;
: q#s begin q# qdup q0= until ;
: q. <# q#s q#> type space ;

( Unit tests                                 JCB 12:15 09/04/10)

page hex
create Q0 qcell allot
create Q1 qcell allot

: qdump qcell dump ;
\ new qdup qxor q1+ qdump bye

: assert ( f -- )
    invert throw
    depth 0<> throw busy @ 0<> throw ;
Q0 qcell 1 fill
Q0 q@ Q1 q!
Q0 q@ Q1 q@ q=                  assert
Q0 q@ Q1 q@ q<> invert          assert
Q1 q@ q1+ Q1 q@ q<>              assert
Q1 q@ q1+ Q1 q@ q=  invert       assert
7777 s>q -7777 s>q q+ q0=       assert
1234 s>q qdup q- q0=            assert
808 s>q qdup q=                 assert
100 s>q 1000 s>q qu<            assert
100 s>q 100  s>q qu<=           assert
100 s>q 100  s>q qu>=           assert
100 s>q 1001 s>q q<>            assert
7 s>q q> >q 7 s>q q=            assert
1 2 3 4 >q q2/ q2* 0 2 3 4 >q q= assert
107 s>q q>s 107 =               assert    
deadbeef dup dup dup >q Q0 q!
Q0 q@ 947 947 qm*/ Q0 q@ q=     assert
