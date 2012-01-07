( Self-test                                  JCB 15:10 09/03/10)

build-debug? [IF]
    : regline ( reg -)
        dup hex2
        [char] : emit
        d# 2 spaces
        d# 16 0do
            dup mt9v@ hex4 space i d# 7 = if [char] - emit space then
            1+
        loop
        drop cr
    ;

    : regdump   d# 0 begin dup regline d# 16 + dup d# 256 = until drop ;

: mt9v-test-i2c ( -- c-addr u f )
    s" imager i2c"
    mt9v-cold
    d# 1 begin
        dup h# 46 mt9v! \ use reg 46 - it is 16-bit
        dup h# 46 mt9v@ <> if
            drop false exit
        then
        2* dup 0=
    until drop true
;

create histo 4 allot

: sleepus   time @ begin dup time @ <> until drop ;

: mt9v-test-framevalid ( -- c-addr u f ) \ 
    s" imager frame_valid_i"
    mt9v-cold d# 2 mt9v-init
    histo dz
    \ watch line for 50ms
    d# 50000 begin
        d# 1 frame_valid_i @ cells histo + +!
        sleepus
        1- dup 0=
    until drop
    histo 2@ 0<> swap 0<> and
    histo 2@ + d# 50000 = and
;

: mt9v-test-registers
    s" imager reg r/w"
    mt9v-cold d# 2 mt9v-init
    mt9v-waitvblank
    mt9v-waitvblank
    MT9V_NUM_ROWS mt9v@ d# 480 =
    MT9V_NUM_COLS mt9v@ d# 640 = and
;

\ tests from hereon assume running 640x480

: mt9v-test-pixclk
    s" imager PIXCLK"
    mt9v-waitvblank
    pixel_counter 2@ d# 307200. d+
    mt9v-waitvblank
    pixel_counter 2@ d=
;

: mt9v-test-IMG_DAT
    s" imager IMG_DAT[9:2]"
    h# 0014 h# 70 mt9v! \ must disable row noise correction
    d# 1 begin
        mt9v-waitvblank
        dup d# 2 lshift h# 2400 or 
        MT9V_DIGITAL_TEST_PATTERN mt9v!
        begin pixfifo_fullness @ until
        pixfifo_data @ @
        over dup d# 8 lshift or <> if
            drop false exit
        then
        
        2* dup d# 256 =
    until drop
    d# 0 MT9V_DIGITAL_TEST_PATTERN mt9v!
    true
;

create mt9v-tests
    | mt9v-test-i2c
    | mt9v-test-framevalid
    | mt9v-test-registers
    | mt9v-test-pixclk
    | mt9v-test-IMG_DAT
    0 ,

: passfail  if s" pass" else s" FAIL" then ;

: testrunner ( testlist -- )
    begin
        dup @ ?dup
    while
        execute
        >r d# 30 typepad r> passfail type cr
        cell+
    repeat
    drop
;

: mt9v-selftest mt9v-tests testrunner mt9v-cold ;

