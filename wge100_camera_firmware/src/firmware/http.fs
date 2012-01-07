( Implement the HTTP protocol                JCB 13:19 08/24/10)
module[ http"
\  'GET / HTTP/1.1'
\  'POST /0 HTTP/1.1'
\  'POST /1 HTTP/1.1'
\  'Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ=='
\  ''

\  during parsing, column # kept
\  d# 5 bits, candidates

\  completed = false
\  sofar = true

\  sofar &= compare(c)
\  if eol:
\    completed |= sofar
\    sofar = true
\ 

\  outcomes of parsing
\    h# 8XXX  bit set means header complete


\  return true if char i of str is c
: str-match         ( c i str. -- flag )
    rot                 \  c str. i
                        \  c ptr len i
    d# 63 and
    tuck >              \  c ptr i len>i
    if
                        \  c ptr i
        +               
        c@ =
    else
        drop 2drop d# -1
    then
;

: get-matcher       ( c i -- flag )
    2dup
    s" GET / "
    str-match
;

: post-0-matcher       ( c i -- flag )
    2drop d# 0
;

: post-1-matcher       ( c i -- flag )
    2drop d# 0
;

: authorization-matcher       ( c i -- flag )
    2drop d# 0
;

: empty-matcher     ( c i -- flag )
    over h# a =
    over d# 63 and 0=
    and
;
    
: any-matcher           ( c i matcher mask -- c i )
    >r
                                        \  c i matcher
    execute
    r> invert or                           \  pass=-1, fail=~mask
                                        \  c i passfail
    and                                 \  c i'
;

: _fsm@     ( -- overall current column )
    _fsm @
    dup
    d# 11 rshift h# 1f and
    swap
    dup
    d# 6 rshift h# 1f and
    swap
    h# 3f and
;

: _fsm!     ( overall current column -- )
    swap
    d# 6 lshift or
    swap d# 11 lshift or
    _fsm !
;


build-debug? [IF]
: debug-fsm
    _fsm@ . . . 
;
: debug-handler
    dup emit
;
[THEN]

: respond
    _fsm@ 2drop h# 10 and
    if
        senddata >FIN_WAIT1
    then
;

: display-char
    \ debug-handler
    dup h# D = if
        drop
    else
        _fsm                        \  c ptr
        tuck @                          \  ptr c i
        dup d# 64      and if ['] get-matcher            d# 64      any-matcher then
        dup d# 1024    and if ['] empty-matcher          d# 1024    any-matcher then
                                         \  ptr c i
        swap
        h# A = if
            \    overall |= current
            \    current = h# 1f
            \    column = d# 0
            dup d# 5 lshift or
            h# 7c0 or
            h# ffc0 and
        else
            dup h# ffc0 or 1+ if 1+ then
        then
        swap !
    then
;

: http-create
    d# 0 h# 1f d# 0 _fsm!
;

d# 80 tcp-service http-create display-char html-page

d# 0 [IF]
: http-handler
    \  When in ESTABLISHED_L and _fsm is 1000, send the data and go to ESTABLISHED_S
    \  When in ESTABLISHED_L, and _fsm is 1001, must have been ACKed, so send FIN and go to FIN_WAIT_1

    NUM_TCPS
    0do 
        i tcp-ptr !
        tcp-state@ d# 2 =
        if
            _fsm@ 2drop h# 10 and
            if
                tcp-send-data
                d# 5 tcp-state!
            then
        then
    loop
;
[THEN]

]module
