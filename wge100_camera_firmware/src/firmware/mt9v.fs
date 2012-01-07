( Hardware interface for MT9V032/4           JCB 13:09 08/24/10)

include rate.fs

module[ mt9v"
create receiver_ip d# 4 allot
create receiver_mac d# 6 allot
create receiver_port d# 2 allot

create linepacketlen d# 2 allot
create linepacket d# 42 allot  \ prebuilt UDP network packet {
create frame_number d# 4 allot \ Note: in network order
create line_number d# 2 allot  \ -1 means not in frame
variable cols                  \ 
variable rows                  \ } end of prebuilt packet

variable running
variable coroutining

4 constant nsensors
create sensors [ nsensors 2 * ] allot
create sensor_data [ nsensors 2 * ] allot
variable sensor_valid

variable imagerversion
variable triggermode
variable shadowclock
2variable frame_number_pulse

: trigger? triggermode @ and 0<> ;
: trigger-ext? d# 1 trigger? ;
: trigger-neg? d# 2 trigger? ;
: trigger-alt? d# 4 trigger? ;
: imager-v034? imagerversion @ h# 1324 = ;
: inframe      line_number @ d# -1 <> ;

: mt9v-clock! ( f -- )
    dup shadowclock !
    img_clk_ena_o ! ;

: mt9v-vidstop ( -- )
    false running !
    true mt9v-clock!
    frame_number dz 
    false coroutining ! ;

( xorline                                    JCB 09:30 10/05/10)

================================================================

Because the camera sends images as a series of lines over UDP,
sometimes a packet can disappear, resulting in an unusable
frame.

xorline is a fix for this.  an extra line sent at the end of
each frame which is the xor of all the lines in the frame.
This lets the receiver recover from the single lost line by
xoring all the lines it does have together: this gives the
missing line.

Computing the xorline in the transmit loop below would make
transmission too slow.  Instead there is a little bit of
hardware that computes the xorline.

The hardware keeps an xorline RAM, 1000 words or 2 Kbytes.
There is an xorline pointer register that points into the
RAM, which is reset to the start of the buffer on a write to
'xorline_reset'.  This RAM is mapped starting at address
'xorline' Writing a value to 'xorline_w' causes the value
to be xored with the old value in the RAM, then the pointer
is bumped.

================================================================

: xorline-reset ( -- ) \ reset the xorline pointer
    true xorline_reset ! ;

: xorline-erase ( cols -- ) \ cleverly erase by read then write
    xorline-reset
    xorline swap bounds
    begin
        dup @ xorline_w !
        cell+ 2dup=
    until
    2drop ;

: xorline-, ( cols -- ) \ append cols of xorline data to packet
    xorline-reset
    xorline swap bounds
    begin
        dup @ enc-pkt-,
        cell+ 2dup=
    until
    2drop ;

( i2c                                        JCB 07:41 09/11/10)

================================================================

Some camera modes - alternating - require i2c traffic per frame.
Because the camera clock is only running during readout, and i2c
on the imager is 400KHz, there is not enough time at frame end
for the i2c operations.

So must run the i2c operations concurrently with frame readout.
Since the i2c half-cycle time is 1.25 us, use the us alarm words
to set an alarm every 3 us, guaranteeing 2 us between actions.

There is time for about 1300 actions during readout.

half-clock is 1.25 us, need to wait 3 us to be sure

================================================================

2variable i2c-alarm

: SDA-rd i2c_sda @ ;
: SCL-0 false i2c_scl ! ;
: SCL-1 true i2c_scl ! ;
: SDA i2c_sda ! ;
: i2c-half     \ 6 * 33 = 200 clocks
    \ d# -35 begin d# 1 + dup d# 0 = until drop ;
    d# 3. i2c-alarm setalarm
    begin
        coroutining @ if pause then
        i2c-alarm isalarm
    until ;

include i2c.fs
h# 90 constant MT9VWRITE
h# 91 constant MT9VREAD

( Register access                            JCB 17:18 09/12/10)

: mt9v@ ( regaddr -- value )
    \ true mt9v-clock!
    i2c-start
    MT9VWRITE i2c-tx drop
          i2c-tx drop
    i2c-start
    MT9VREAD i2c-tx drop
    d# 0 i2c-rx
    d# 1 i2c-rx
    i2c-stop
    swap d# 8 lshift or
;

: mt9v! ( value regaddr -- )
    \ true mt9v-clock!
    i2c-start
    MT9VWRITE i2c-tx drop
    i2c-tx drop
    dup d# 8 rshift
    i2c-tx drop
    i2c-tx drop
;

( Low level controls                         JCB 17:18 09/12/10)

: mt9v-reset
    false IMG_RESET_B ! sleep.1 true IMG_RESET_B ! ;

: mt9v-cold
    false coroutining !
    true mt9v-clock!
    mt9v-vidstop
    mt9v-reset
    sensors [ nsensors 2 * ] literal h# ff fill
    d# 0 mt9v@ imagerversion !
    d# 0 triggermode !
    d# -1 line_number !
;

( i2c coroutine: run i2c during readout      JCB 11:48 09/14/10)

create taskstore 130 allot \ 2 + 64 + 64
variable ming

: i2c-task
    true coroutining !
        \ begin pause d# 11 line_number @ < until
    begin
        shadowclock @ 0=
    while
        pause
    repeat

    h# 0000             \ valid flags
    nsensors 0do
        2/
        sensors i 2* + @ dup 0< if
            drop d# 0
        else
            swap d# 8 or swap
            mt9v@
        then
        sensor_data i 2* + !
    loop
    sensor_valid !

    trigger-alt? imager-v034? and if
        frame_number 2@
        frame_number_pulse 2@ dxor
        nip even? invert h# 8000 and
        h# 0388 or d# 7 mt9v!
    then

    line_number @ ming !

    begin pause again
;

: i2c-launch
    h# 0947 ming !
    taskstore taskptr !
    ['] i2c-task launch ;
: i2c-regular   false coroutining ! ;
    
( Important Registers                        JCB 16:42 08/24/10)

h# 03 constant MT9V_NUM_ROWS
h# 04 constant MT9V_NUM_COLS
h# 05 constant MT9V_HORZ_BLANK
h# 06 constant MT9V_VERT_BLANK
h# 0C constant MT9V_RESET
h# 0D constant MT9V_READ_MODE
h# 1B constant MT9V_LED_OUT
h# 7F constant MT9V_DIGITAL_TEST_PATTERN
h# BD constant MT9V_MAX_SHUTTER
h# FE constant MT9V_LOCK

( unused mode init word                      JCB 07:41 09/11/10)

================================================================

mt9v-init is a leftover from an idea to create standalone
camera modes.  Instead we decided to use the driver on the
host to configure the camera.  It could be used by a future
selftest.

The imager effectively divides the Hres, Vres, and Hblank
values by the binning coefficient to generate internal values.
The Vblank value is used as-is.  The values in the table
represent what the internal values need to be, so the
programmed values need to be pre-multiplied.

================================================================

create modelist
\ Mode  Hres    Vres    Frame Rate  Binning HBLANK  VBLANK
(  0 )    752 ,    480 ,    150 ,        1 ,      974 ,    138 ,
(  1 )    752 ,    480 ,    150 ,        1 ,      848 ,    320 ,
(  2 )    640 ,    480 ,    300 ,        1 ,      372 ,     47 ,
(  3 )    640 ,    480 ,    250 ,        1 ,      543 ,     61 ,
(  4 )    640 ,    480 ,    150 ,        1 ,      873 ,    225 ,
(  5 )    640 ,    480 ,    125 ,        1 ,      960 ,    320 ,
(  6 )    320 ,    240 ,    600 ,        2 ,      106 ,     73 ,
(  7 )    320 ,    240 ,    500 ,        2 ,      180 ,     80 ,
(  8 )    320 ,    240 ,    300 ,        2 ,      332 ,    169 ,
(  9 )    320 ,    240 ,    250 ,        2 ,      180 ,    400 ,

: col ( ptr n -- ptr *ptr+2n ) cells over + @ ;

: mt9v-init ( mode -- )
    mt9v-vidstop

    cells d# 6 * modelist +

    d# 0 col MT9V_NUM_COLS     mt9v!
    d# 1 col MT9V_NUM_ROWS     mt9v!
    d# 4 col
    MT9V_HORZ_BLANK   mt9v!
    d# 5 col MT9V_VERT_BLANK   mt9v!

    \ We have to make sure the Automatic Exposure Controller
    \ (AEC) doesn't expose for longer than our desired frame
    \ period.  Subtracting a value of d# 30 from the
    \ max. shutter is arbitrary; the max shutter should be
    \ less than the total frame period, but by how much is
    \ undetermined

    d# 1 col >r d# 5 col r> + d# 30 - MT9V_MAX_SHUTTER mt9v!

    \ Read Mode Context A setting
    \ See MT9V033 Datasheet Rev A 2/08 Page d# 30 for details

    MT9V_READ_MODE 
    d# 3 col d# 1 = if
        \ Flip column readout order so that pixels read
        \ left-to-right, top-to-bottom, row & col bin d# 1 (normal)
        h# 0320
    else
        \ Flip column readout order so that pixels read
        \ left-to-right, top-to-bottom, row & col bin d# 2
        h# 0325
    then
    mt9v!

    drop
;


( video streaming                            JCB 07:41 09/11/10)

: wge-line-header ( -- )
    receiver_port @ d# 1627
    receiver_ip 2@ 
    net-my-ip
    receiver_mac
    ( dst-port src-port dst-ip src-ip *ethaddr )
    udp-header
;

create prevpix d# 4 allot
create prevtime d# 4 allot

: mt9v-drain
    true pixfifo_reset ! false pixfifo_reset !  ;

2variable blankwait

: mt9v-waitvblank
    true mt9v-clock!
    d# 500000. blankwait setalarm
    begin frame_valid_i @ morse-v morseflash blankwait isalarm or until
    begin frame_valid_i @ morse-v morseflash 0= blankwait isalarm or until
    blankwait isalarm if 
        s" No activity on frame_valid" type cr
    then
    mt9v-drain
    pixel_counter 2@ prevpix 2!
;

: mt9v-vidstart
    ETH.IP.UDP.WGE.VIDSTART.PORT packet@ ?dup if
        receiver_port !
        d# 2 ETH.IP.UDP.WGE.VIDSTART.IP enc-offset enc@n receiver_ip 2!
        ETH.IP.UDP.WGE.VIDSTART.MAC enc-offset
        receiver_mac d# 3 move
    else
        ETH.IP.UDP.SOURCEPORT packet@ receiver_port !
        d# 2 ETH.IP.SRCIP enc-offset enc@n 2dup receiver_ip 2!
        arp-lookup receiver_mac d# 3 move
    then

    s" port: " type receiver_port @ hex4 cr
    s" ip " type receiver_ip 2@ ip-pretty cr

    true running !
    d# 0. frame_number 2!
    d# -1 line_number !
    \ h# 3800 MT9V_DIGITAL_TEST_PATTERN mt9v!

    \ make the line packet once
    wge-line-header
    d# 5 enc-pkt-,0    \ field values do not matter
    cols @ 2/ enc-pkt-,0
    udp-wrapup
    d# 0 ETH.IP.UDP.CHECKSUM packetout-off !    \ zero checksum
    writer @ outgoing - linepacketlen !
    outgoing linepacket d# 21 move
;

\ Sending a line.  Packet is 54 byte prefix - so 13.5 dwords
\ then the actual data.

: send32
    begin MAC_w_stop @ invert until
    dup@ MAC_w_0 !
    d# 2 +
    dup@ MAC_w_we !
    d# 2 +
;

: sendline  \ send a line of image data from imager to MAC
    xorline-reset
    cols @ 2- d# 2 rshift          ( loopcount )

    linepacketlen
    d# 1 MAC_w_sof ! send32 d# 0 MAC_w_sof !
    send32 send32 send32 send32
    send32 send32 send32 send32
    send32 send32 send32 send32
    @ MAC_w_0 !

    \ image data, first send single word to w_1
    pixfifo_clock @ drop
    pixfifo_data_ck @ MAC_w_we 2dup! drop xorline_w !
                                ( loopcount )
    \ 20 clocks, 21 seems OK
    d# 0 swap
    begin
        MAC_w_stop @ if     ( 3 )
            mac-ready
        then
        pixfifo_data_ck @ MAC_w_0 2dup! drop xorline_w ! ( 5 )
        pixfifo_data_ck @ MAC_w_we 2dup! drop xorline_w ! ( 5 )
        1- 2dup=                ( 3 )
    until
    2drop
    \ now the trailing single word to w_0
    mac-ready
    pixfifo_data @ MAC_w_0 2dup! drop xorline_w !
    h# 0000 MAC_w_we !
;

: time, \ append time fields: hi, lo, freq
    timeq ( lsw . . msw )
    enc-pkt-, enc-pkt-, enc-pkt-, enc-pkt-,
    d# 1000000. swap enc-pkt-2,
;

: sensors,  \ append the i2c sensors readings
    sensor_data nsensors 2* bounds
    begin
        dup @ enc-pkt-,
        2+ 2dup =
    until 2drop
    sensor_valid @ s>d swap enc-pkt-2,
;

: nextframe
    d# -1 line_number !
    frame_number 2@ swap d1+ swap frame_number 2!
;

DO-XORLINE [IF]
: send-xorline
    wge-line-header
    frame_number 2@     enc-pkt-2,
    h# fffa             enc-pkt-,
    cols @              enc-pkt-,
    rows @              enc-pkt-,
    cols @ xorline-,
    udp-wrapup enc-send
;
[ELSE]
: send-xorline ;
[THEN]

: capture
    progress
    begin
        pixfifo_fullness @ 2* cols @ >=
    while
        sendline
        d# 1 line_number +!
        line_number @ rows @ =
        if
            send-xorline
            \ trigger-alt? if slug then

            i2c-regular
            wge-line-header
            frame_number 2@     enc-pkt-2,
            h# ffff             enc-pkt-,
            cols @              enc-pkt-,
            rows @              enc-pkt-,

            time,

            sensors,

            udp-wrapup
            enc-send

            0 [IF]
                false if
                    pixel_counter 2@ 2dup
                    prevpix 2@ d-
                    2dup rows @ cols @ m* d<> if
                        [char] > emit space hex8 cr \ overflow
                    else
                        2drop
                    then
                    prevpix 2!
                then
                frame_number 2@ nip h# ff and 0= if
                    s" Fn: " type frame_number 2@ swap hex8 space
                    true if
                        space ming @ hex4 space
                        sensor_data nsensors 2* bounds
                        begin dup @ hex4 space 2+ 2dup = until 2drop
                    then
                    cr
                then
                false if
                    time@
                    2dup prevtime 2@ d- decimal d. cr
                    prevtime 2!
                then
            [THEN]

            nextframe
        then
    repeat
;

( External triggering                        JCB 14:59 09/03/10)

================================================================

In external trigger mode, the imager still thinks it is running
free, and its timings are set up so that it completes frames
slightly faster than the trigger pulses arrive.  Then our job
is to stop the imager's clock each frame, and start it again
when the external trigger arrives.

The logic for this comes down to:

    when exposing_i falls, stop clock
    on trigger, start clock

When the WGE is part of a stereo pair, it can also run in
alternating mode, where we need to alternate imager register 
sets, depending on odd/even frames.  The frame parity is
communicated to us by a double pulse on the trigger line.
This word handles the double pulse by copying frame_number into
frame_number_pulse.

================================================================

variable prevtrigger
variable prevexposing

: triggering
    exposing_i @ prevexposing fall? if
        inframe if s" stopping clock at line " type
        line_number @ hex4 cr
        pixfifo_fullness @ hex4 cr 
        pixfifo_fullness @ hex4 cr then
        false mt9v-clock!
    then
    EXT_TRIG_B @ prevtrigger trigger-neg? if rise? else fall? then if
        true mt9v-clock!
        inframe if frame_number 2@ frame_number_pulse 2! then
    then
;

variable prevframe_valid_i

: mt9v-live
    pixfifo_fullness @ h# 3ff = inframe and if
        s" pixfifo overflow at line " type line_number @ hex4 cr
        nextframe
    then
    frame_valid_i @ prevframe_valid_i rise? if
        inframe if
            line_number @ hex4 space
            pixfifo_fullness @ hex4 space
            s" frame in frame" type cr
            nextframe
        then
        d# 0 line_number !
        cols @ xorline-erase
        i2c-launch
    then
    inframe if
        capture
    then
    trigger-ext? if triggering then
;

: mt9v-cycle
    running @ if
        mt9v-live
    else
        true mt9v-clock!
    then ;

: mt9v-random \ 100 samples of video data
    mt9v-drain
    d# 1.
    d# 100 0do
        begin pixfifo_fullness @ until
        pixfifo_data @ @ s>d
        d+ swap
    loop
;
]module
