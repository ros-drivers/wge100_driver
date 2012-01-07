( Continuation                               JCB 14:57 09/11/10)

\ simple system for saving and restoring program state: the
\ data and return stacks.  Example usage for coroutines is
\
\ : task1 begin pause again ;
\ 16 allot taskptr !
\ ' task1 launch
\ ... progress ... progress ... progress ...

variable taskptr variable _dsp

: pause \ save D and R in taskptr
    dsp _dsp @ - taskptr @ !
    taskptr @ dup cell+ swap @ dup>r h# ff and
    begin
        dup
    while
        >r
        tuck !
        cell+ 
        r> 1-
    repeat
    drop
    r> d# 7 rshift bounds
    begin
        2dupxor
    while
        r> over !
        cell+
    repeat
    2drop
;

: progress
    dsp _dsp !
    taskptr @ dup @ cells
    dup>r h# ff and over + r> d# 8 rshift over +
    begin
        2dupxor
    while
        dup@ >r
        2-
    repeat

    drop

    begin
        2dupxor
    while
        dup@ -rot
        2-
    repeat
    2drop
;

: launch ( xt -- )
    taskptr @ h# 0100 over ! cell+ ! ;

\ create scratch 16 allot
h# 3800 constant scratch
: zot d# 3 0do pause loop ;

: subtask
    true
    begin
        s" subtask pausing " type dsp hex4 space .s cr
        pause
        s" subtask progressd " type dsp hex4 space .s cr
        sleep.1
    again ;

: do3 d# 56 progress progress progress drop ;
: do9 do3 do3 do3 ;

: cdemo
    scratch taskptr !
    ['] subtask launch
    d# 6 0do progress loop
;

: nulltask begin pause again ;

: cbench
    scratch taskptr !
    ['] nulltask launch
;
