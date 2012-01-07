create scope 128 allot
create started 4 allot
create finish 4 allot

\ 64 samples 128 clocks apart
: window [ 64 2048 * dup 16 rshift ] 2literal ;
: scope-start
    scope d# 128 h# ff fill
    time@ 2dup started 2!
    window d+ finish 2! ;
: scope-frob
    >r
    started 2@ or if
        time@ started 2@ d-
        2dup window d< if
            \ time @ 
            d2/ drop d# 10 rshift 2* scope + r@ swap !
        else
            2drop
            d# 64 0do scope i 2* + @ dup 0< if drop [char] . emit else hex1 then loop cr
            started dz
        then
    then
    r> drop
;
