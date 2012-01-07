( docforth                                   JCB 07:31 11/23/10)

: parse-word
    bl word count ;

: method>
    postpone create postpone ,
    postpone does> postpone @
; immediate

\ 0 link
\ 1 key
\ 2 value

: link ( root new -- )
    over @ over !
    swap !
;

: mapadd ( value key map )
    method>
    3 cells allocate throw
    tuck link
    cell+ tuck !
    cell+
    !
;

: mapfind ( key map -- key false | value true )
    method>
    begin
        dup
    while
        2dup cell+ @ = if
            nip cell+ cell+ @ true exit
        then
        @
    repeat
;

: map
    here 0 ,
    dup mapadd mapfind
;

variable old>in
2variable jam

: preserve  ( c-addr1 u -- c-addr ) \ save string in an allocated counted string
    dup 1+ allocate throw dup >r
    2dup c! 1+
    swap cmove r> ;
: cpreserve ( c-addr1 u -- c-addr ) \ like preserve, but uses a cell for length
    dup cell+ allocate throw dup >r
    2dup ! cell+
    swap cmove r> ;
: ccount ( c-addr -- c-addr1 u ) \ like count, but uses a cell for length
    dup cell+ swap @ ;

: moretib ( -- f ) >in @ #tib @ < ;
: skipsp ( -- ) \ skip spaces in TIB
    begin
        moretib
        tib >in @ + c@ bl = and
    while
        1 >in +!
    repeat
;
: wordstr ( "name" -- c-addr u )
    skipsp
    >in @ old>in !
    >in @ >r bl word count r> >in !
;

: 2array
    create 2* cells allot
    does>  swap 2* cells +
;

variable htmlfile
s" " preserve value htmlfilename

: >html ( c-addr u ) \ write string to html
    htmlfile @ write-file throw ;
: >>html ( c-addr u ) \ write line to html
    htmlfile @ write-line throw ;
: u>html ( u -- ) \ write decimal u to the HTML
    base @ >r decimal
    0 <# #s #> >html
    r> base !
;

512 constant maxline
maxline 2array deco

map +xt->uniq xt->uniq
map +uniq->fn uniq->fn
map +uniq->comment uniq->comment

: counter
    create 100 ,
    does>  1 over +! @
;
counter uniq

: anchor ( u -- )
    s" <a class=name name=" htmlfile @ write-file throw
    xt->uniq drop u>html
    s" >" htmlfile @ write-file throw
;

: anchor1 ( -- uniq )
    uniq
    htmlfilename over +uniq->fn
    dup ['] anchor
    old>in @ deco 2!
;
: anchor2 ( uniq c-addr u -- )
    get-current search-wordlist
    0= throw
    +xt->uniq
;
: anchorit ( c-addr -- ) \ counted string
    anchor1 swap count anchor2 ;

: create-html ( caddr u -- )
    2dup preserve to htmlfilename
    w/o create-file throw htmlfile !
    s" <html>"                          >>html
    s" <head>"                          >>html
    s" <style type=text/css>"           >>html
    s" div.code {"                      >>html
    s"   padding:0.5em;"                >>html
    s"   font-size:large;"              >>html
    s"   line-height:110%;"             >>html
    s" }"                               >>html
    s" div.code a.name {"               >>html
    s"   font-weight:bold;"             >>html
    s" }"                               >>html
    s" div.code a.name:target {"        >>html
    s"   background-color:#ff7"         >>html
    s" }"                               >>html
    s" div.code a.ref {"                >>html
    s"   text-decoration:none;"         >>html
    s"   color:#007"                    >>html
    s" }"                               >>html
    s" div.code a.ref:hover {"          >>html
    s"   text-decoration:underline;"    >>html
    s" }"                               >>html
    s" .footer {"                       >>html
    s"   font-size:x-small;"              >>html
    s"   line-height:110%;"             >>html
    s" }"                               >>html
    s" </style>"                        >>html
    s" </head>"                         >>html
    s" <body>"                          >>html
    s" <div class=code>"                >>html
;
: close-html
    s" </div>" >>html
    s" <p class=footer>Created with <a href=http://excamera.com/sphinx/docforth.html>docforth</a>" >>html
    s" </body></html>" >>html
    htmlfile @ close-file throw
;

\    --STANDARD--                             \  Wil Baden  2003-02-22

\  *******************************************************************
\  *                                                                 *
\  *                    ONLY STANDARD DEFINITIONS                    *
\  *                                                                 *
\  *******************************************************************
WORDLIST CONSTANT STANDARD
STANDARD SET-CURRENT
FORTH-WORDLIST STANDARD FORTH-WORDLIST 3 SET-ORDER

\ Standard-Clone

: TIB TIB ;
: ! ! ; 
: # # ; 
: #> #> ; 
: #S #S ; 
: #TIB #TIB ; 
: ' ' ; 
: ( POSTPONE ( ; IMMEDIATE 
: (LOCAL) POSTPONE (LOCAL) ; IMMEDIATE
: * * ; 
: */ */ ; 
: */MOD */MOD ; 
: + + ; 
: +! +! ; 
: +LOOP POSTPONE +LOOP ; IMMEDIATE 
: , , ; 
: - - ; 
: -TRAILING -TRAILING ; 
: . . ; 
: ." POSTPONE ." ; IMMEDIATE 
: .( POSTPONE .( ; IMMEDIATE 
: .R .R ; 
: .S .S ; 
: / / ; 
: /MOD /MOD ; 
: /STRING /STRING ; 
: 0< 0< ; 
: 0<> 0<> ; 
: 0= 0= ; 
: 0> 0> ; 
: 1+ 1+ ; 
: 1- 1- ; 
: 2! 2! ; 
: 2* 2* ; 
: 2/ 2/ ; 
: 2>R POSTPONE 2>R ; IMMEDIATE
: 2@ 2@ ;
: 2CONSTANT wordstr preserve >r 2CONSTANT r> anchorit ; 
: 2DROP 2DROP ; 
: 2DUP 2DUP ; 
: 2LITERAL POSTPONE 2LITERAL ; IMMEDIATE 
: 2OVER 2OVER ; 
: 2R> POSTPONE 2R> ; IMMEDIATE
: 2R@ POSTPONE 2R@ ; IMMEDIATE
: 2ROT 2ROT ; 
: 2SWAP 2SWAP ; 
: 2VARIABLE wordstr preserve >r 2VARIABLE r> anchorit ; 
: : wordstr preserve anchor1 swap jam 2! : ; 
: :NONAME :NONAME ; 
: ; POSTPONE ; jam 2@ count anchor2 ; IMMEDIATE 
: ;CODE POSTPONE ;CODE ; IMMEDIATE 
: < < ; 
: <# <# ; 
: <> <> ; 
: = = ; 
: > > ; 
: >BODY >BODY ; 
: >FLOAT >FLOAT ; 
: >IN >IN ; 
: >NUMBER >NUMBER ; 
: >R POSTPONE >R ; IMMEDIATE
: ? POSTPONE ? ; IMMEDIATE 
: ?DO POSTPONE ?DO ; IMMEDIATE 
: ?DUP ?DUP ; 
: @ @ ; 
: ABORT ABORT ; 
: ABORT" POSTPONE ABORT" ; IMMEDIATE 
: ABS ABS ; 
: ACCEPT ACCEPT ; 
: AGAIN POSTPONE AGAIN ; IMMEDIATE 
: AHEAD POSTPONE AHEAD ; IMMEDIATE
: ALIGN ALIGN ; 
: ALIGNED ALIGNED ; 
: ALLOCATE ALLOCATE ; 
: ALLOT ALLOT ; 
: ALSO ALSO ; 
: AND AND ; 
: ASSEMBLER ASSEMBLER ; 
: AT-XY AT-XY ; 
: BASE BASE ; 
: BEGIN POSTPONE BEGIN ; IMMEDIATE 
: BIN BIN ; 
: BL BL ; 
: BLANK BLANK ; 
: BLK BLK ; 
: BLOCK BLOCK ; 
\ BUFFER
: BYE BYE ; 
: C! C! ; 
: C" POSTPONE C" ; IMMEDIATE 
: C, C, ; 
: C@ C@ ; 
: CASE POSTPONE CASE ; IMMEDIATE 
: CATCH CATCH ; 
: CELL+ CELL+ ; 
: CELLS CELLS ;
: CHAR CHAR ; 
: CHAR+ CHAR+ ; 
: CHARS CHARS ; 
: CLOSE-FILE CLOSE-FILE ; 
: CMOVE CMOVE ; 
: CMOVE> CMOVE> ; 
: CODE CODE ; 
: COMPARE COMPARE ; 
: COMPILE, COMPILE, ; 
: CONSTANT wordstr preserve >r CONSTANT r> anchorit ; 
: COUNT COUNT ; 
: CR CR ; 
: CREATE wordstr preserve >r CREATE r> anchorit ; 
: CREATE-FILE CREATE-FILE ; 
: CS-PICK CS-PICK ; 
: CS-ROLL CS-ROLL ; 
: D+ D+ ; 
: D- D- ; 
: D. D. ; 
: D.R D.R ; 
: D0< D0< ; 
: D0= D0= ; 
: D2* D2* ; 
: D2/ D2/ ; 
: D< D< ; 
: D= D= ; 
: D>F D>F ; 
: D>S D>S ; 
: DABS DABS ; 
: DECIMAL DECIMAL ; 
: DEFINITIONS DEFINITIONS ; 
: DELETE-FILE DELETE-FILE ; 
: DEPTH DEPTH ; 
: DF! DF! ; 
: DF@ DF@ ; 
: DFALIGN DFALIGN ; 
: DFALIGNED DFALIGNED ; 
: DFLOAT+ DFLOAT+ ; 
: DFLOATS DFLOATS ; 
: DMAX DMAX ; 
: DMIN DMIN ; 
: DNEGATE DNEGATE ; 
: DO POSTPONE DO ; IMMEDIATE 
: DOES> POSTPONE DOES> ; IMMEDIATE 
: DROP DROP ; 
: DU< DU< ; 
: DUMP DUMP ; 
: DUP DUP ; 
\ : EDITOR EDITOR ; 
: EKEY EKEY ; 
: EKEY>CHAR EKEY>CHAR ; 
: EKEY? EKEY? ; 
: ELSE POSTPONE ELSE ; IMMEDIATE 
: EMIT EMIT ; 
\ : EMIT? EMIT? ; 
\ EMPTY-BUFFERS
: ENDCASE POSTPONE ENDCASE ; IMMEDIATE 
: ENDOF POSTPONE ENDOF ; IMMEDIATE 
: ENVIRONMENT? ENVIRONMENT? ; 
: ERASE ERASE ; 
: EVALUATE EVALUATE ; 
: EXECUTE EXECUTE ; 
: EXIT POSTPONE EXIT ; IMMEDIATE 
: F! F! ; 
: F* F* ; 
: F** F** ; 
: F+ F+ ; 
: F- F- ; 
: F. F. ; 
: F/ F/ ; 
: F0< F0< ; 
: F0= F0= ; 
: F< F< ; 
: F>D F>D ; 
: F@ F@ ; 
: FABS FABS ; 
: FACOS FACOS ; 
: FACOSH FACOSH ; 
: FALIGN FALIGN ; 
: FALIGNED FALIGNED ; 
: FALOG FALOG ; 
: FALSE FALSE ; 
: FASIN FASIN ; 
: FASINH FASINH ; 
: FATAN FATAN ; 
: FATAN2 FATAN2 ; 
: FATANH FATANH ; 
: FCONSTANT wordstr preserve >r FCONSTANT r> anchorit ; 
: FCOS FCOS ; 
: FCOSH FCOSH ; 
: FDEPTH FDEPTH ; 
: FDROP FDROP ; 
: FDUP FDUP ; 
: FE. FE. ; 
: FEXP FEXP ; 
: FEXPM1 FEXPM1 ; 
: FILE-POSITION FILE-POSITION ; 
: FILE-SIZE FILE-SIZE ; 
: FILE-STATUS FILE-STATUS ; 
: FILL FILL ; 
: FIND FIND ; 
: FLITERAL POSTPONE FLITERAL ; IMMEDIATE 
: FLN FLN ; 
: FLNP1 FLNP1 ; 
: FLOAT+ FLOAT+ ; 
: FLOATS FLOATS ; 
: FLOG FLOG ; 
: FLOOR FLOOR ; 
\ FLUSH
: FLUSH-FILE FLUSH-FILE ; 
: FM/MOD FM/MOD ; 
: FMAX FMAX ; 
: FMIN FMIN ; 
: FNEGATE FNEGATE ; 
: FORTH GET-ORDER NIP STANDARD SWAP SET-ORDER ; 
: FORTH-WORDLIST STANDARD ; 
: FOVER FOVER ; 
: FREE FREE ; 
: FROT FROT ; 
: FROUND FROUND ; 
: FS. FS. ; 
: FSIN FSIN ; 
: FSINCOS FSINCOS ; 
: FSINH FSINH ; 
: FSQRT FSQRT ; 
: FSWAP FSWAP ; 
: FTAN FTAN ; 
: FTANH FTANH ; 
: FVARIABLE FVARIABLE ; 
: F~ F~ ; 
: GET-CURRENT GET-CURRENT ; 
: GET-ORDER GET-ORDER ; 
: HERE HERE ; 
: HEX HEX ; 
: HOLD HOLD ; 
: I POSTPONE I ;  IMMEDIATE
: IF POSTPONE IF ; IMMEDIATE 
: IMMEDIATE IMMEDIATE ; 
: INCLUDE-FILE INCLUDE-FILE ; 
: INCLUDED INCLUDED ; 
: INVERT INVERT ; 
: J POSTPONE J ; IMMEDIATE
: KEY KEY ; 
: KEY? KEY? ; 
: LEAVE POSTPONE LEAVE ; IMMEDIATE
\ LIST
: LITERAL POSTPONE LITERAL ; IMMEDIATE 
\ LOAD
: LOCALS| POSTPONE LOCALS| ; IMMEDIATE 
: LOOP POSTPONE LOOP ; IMMEDIATE 
: LSHIFT LSHIFT ; 
: M* M* ; 
: M*/ M*/ ; 
: M+ M+ ; 
: MARKER MARKER ; 
: MAX MAX ; 
: MIN MIN ; 
: MOD MOD ; 
: MOVE MOVE ; 
: MS MS ; 
: NEGATE NEGATE ; 
: NIP NIP ; 
: OF POSTPONE OF ; IMMEDIATE 
: ONLY STANDARD 1 SET-ORDER ; 
: OPEN-FILE OPEN-FILE ; 
: OR OR ; 
: ORDER ORDER ; 
: OVER OVER ; 
: PAD PAD ; 
: PAGE PAGE ; 
: PARSE PARSE ; 
: PICK PICK ; 
: POSTPONE POSTPONE POSTPONE ; IMMEDIATE 
: PRECISION PRECISION ; 
: PREVIOUS PREVIOUS ; 
: QUIT QUIT ; 
: R/O R/O ; 
: R/W R/W ; 
: R> POSTPONE R> ; IMMEDIATE
: R@ POSTPONE R@ ; IMMEDIATE
: READ-FILE READ-FILE ; 
: READ-LINE READ-LINE ; 
: RECURSE POSTPONE RECURSE ; IMMEDIATE 
: REFILL REFILL ; 
: RENAME-FILE RENAME-FILE ; 
: REPEAT POSTPONE REPEAT ; IMMEDIATE 
: REPOSITION-FILE REPOSITION-FILE ; 
: REPRESENT REPRESENT ; 
: RESIZE RESIZE ; 
: RESIZE-FILE RESIZE-FILE ; 
: RESTORE-INPUT RESTORE-INPUT ; 
: ROLL ROLL ; 
: ROT ROT ; 
: RSHIFT RSHIFT ; 
: S" STATE @ IF POSTPONE S" ELSE ['] S" EXECUTE THEN ; IMMEDIATE 
: S>D S>D ; 
\ SAVE-BUFFERS
: SAVE-INPUT SAVE-INPUT ; 
\ SCR
: SEARCH SEARCH ; 
: SEARCH-WORDLIST SEARCH-WORDLIST ; 
: SEE SEE ; 
: SET-CURRENT SET-CURRENT ; 
: SET-ORDER SET-ORDER ; 
: SET-PRECISION SET-PRECISION ; 
: SF! SF! ; 
: SF@ SF@ ; 
: SFALIGN SFALIGN ; 
: SFALIGNED SFALIGNED ; 
: SFLOAT+ SFLOAT+ ; 
: SFLOATS SFLOATS ; 
: SIGN SIGN ; 
: SLITERAL POSTPONE SLITERAL ; IMMEDIATE 
: SM/REM SM/REM ; 
: SOURCE SOURCE ; 
: SOURCE-ID SOURCE-ID ; 
: SPACE SPACE ; 
: SPACES SPACES ; 
: STATE STATE ; 
: SWAP SWAP ; 
: THEN POSTPONE THEN ; IMMEDIATE 
: THROW THROW ; 
\ THRU
: TIME&DATE TIME&DATE ; 
: TO STATE @ IF POSTPONE TO ELSE ['] TO EXECUTE THEN ; IMMEDIATE 
: TRUE TRUE ; 
: TUCK TUCK ; 
: TYPE TYPE ; 
: U. U. ; 
: U.R U.R ; 
: U< U< ; 
: U> U> ; 
: UM* UM* ; 
: UM/MOD UM/MOD ; 
: UNLOOP POSTPONE UNLOOP ; IMMEDIATE
: UNTIL POSTPONE UNTIL ; IMMEDIATE 
: UNUSED UNUSED ; 
\ UPDATE
: VALUE wordstr preserve >r VALUE r> anchorit ; 
: VARIABLE wordstr preserve >r VARIABLE r> anchorit ; 
: W/O W/O ; 
: WHILE POSTPONE WHILE ; IMMEDIATE 
: WITHIN WITHIN ; 
: WORD WORD ; 
: WORDLIST WORDLIST ; 
: WORDS WORDS ; 
: WRITE-FILE WRITE-FILE ; 
: WRITE-LINE WRITE-LINE ; 
: XOR XOR ; 
: [ POSTPONE [ ; IMMEDIATE 
: ['] POSTPONE ['] ; IMMEDIATE 
: [CHAR] POSTPONE [CHAR] ; IMMEDIATE 
: [COMPILE] POSTPONE [COMPILE] ; IMMEDIATE 
: [ELSE] POSTPONE [ELSE] ; IMMEDIATE 
: [IF] POSTPONE [IF] ; IMMEDIATE 
: [THEN] POSTPONE [THEN] ; IMMEDIATE 
: \ POSTPONE \ ; IMMEDIATE 
: ] ] ; 

forth-wordlist 1 set-order
forth-wordlist set-current

create >countedbuf 512 allot

: >counted ( c-addr1 u -- c-addr )
    dup >countedbuf c!
    >countedbuf 1+ swap cmove
    >countedbuf
;

variable dpl

: NUMBER? ( a u -- d -1 | a u 0 )
  OVER C@ [CHAR] - = DUP >R IF 1 /STRING THEN
  >R >R  0 DUP  R> R>  -1 DPL !
  BEGIN >NUMBER DUP
  WHILE OVER C@ [CHAR] . XOR
    IF ROT DROP ROT R> 2DROP  0 EXIT
    THEN 1 - DPL !  CHAR+  DPL @
  REPEAT 2DROP R> IF DNEGATE THEN -1 ;
: single? dpl @ -1 = ;

\ tags is a linked list of (xt, toc) pairs
\ 0   next
\ 1   xt
\ 2   toc

: discard-word parse-word 2drop ;
map +uniq->ref uniq->ref
: know
    parse-word standard search-wordlist if
        uniq >r
        r@ swap +xt->uniq
        s" STD" preserve r@ +uniq->comment
        parse-word preserve r> +uniq->ref
    else
        discard-word
    then
    discard-word
;


: searchtag ( uniq -- uniq 0 | c-addr u )
    uniq->ref dup if drop count then
;

\ s" +" standard search-wordlist drop searchtag .s bye

: find. ( addr -- addr u ) \ u is number of characters before .
    0 begin
        2dup + c@ [char] . <>
    while
        1+
    repeat
;

\ given a short reference, write the complete url
\ http://forth.sourceforge.net/std/dpans/dpans6.htm#6.2.2182
: url ( c-addr u -- ) 
    s" http://forth.sourceforge.net/std/dpans/dpans" >html
    over find. >html
    s" .htm#" >html
    >html
;

: reference ( uniq -- )
    >r
    s" <a class=ref href=" >html
    r@ searchtag ?dup if
        url
    else
        \ dup xt->fn if drop ." found" cr else drop ." not found" cr then
        r@ uniq->fn if
            count >html
        else
            drop ." not found" cr
        then
        \ dup xt->fn if ." found" cr count >html else drop s" (missing)" >html then 
        s" #" >html u>html
    then
    \ experimental hover support
    \ s"  title='" >html
    \ r@ uniq->comment if count >html else drop then
    \ s" ' " >html
    s" >" >html
    r> drop
;

variable ref>in

: freshline ( -- ) \ erase deco, read for fresh line
    0 deco maxline 2* cells erase
;

: showline ( -- ) \ write the current line as HTML
    s" <code>" >>html
    0
    #tib @ 0 ?do
        i deco @ if
            i deco 2@ execute
            1+
        then
        dup 0<> tib i + c@ bl = and if
            1- s" </a>" >html
        then
        tib i + c@
        case
        bl       of s" &nbsp;" >html endof
        [char] < of s" &lt;" >html endof
        [char] & of s" &amp;" >html endof
        tib i + 1 >html
        endcase
    loop
    if
        s" </a>" >html
    then
    s" </code>" >>html
    s" <br>" >>html
    s" " htmlfile @ write-line throw
;

: fevaluate \ evaluate the TIB
    freshline
    begin
        skipsp
        moretib if
            >in @ ref>in !
            parse-word dup
        else
            0 0 0
        then
    while
        preserve find
        ?dup if
            over xt->uniq if 
                ['] reference ref>in @ deco 2!
            else
                drop
            then
            0> state @ 0= or if
                execute
            else
                compile,
            then
        else
            dup count number? if 
                rot drop
                single? if drop then
                state @ if single? if postpone literal else postpone 2literal then then
            else
                2drop count
                >float invert abort" disaster"
                state @ if postpone fliteral then
            then
        then
    repeat
    2drop
    showline
;

variable suffixlen
create suffixbuf 512 allot

: suffix suffixbuf suffixlen @ ;
: +suffix ( caddr u )
    tuck suffix + swap cmove suffixlen +! ;

: fs->html
    0 suffixlen !
    +suffix
    s" .html" +suffix
    suffix
;

variable sourcefile

: includelink ( c-addr -- )
    s" <a class=ref href=" >html
    count >html
    s" >" >html
;

: doc-included ( c-addr u -- )
    \ if filename is in TIB, add helpful link to file
    over tib - >r
    r@ #tib u< if
        2dup fs->html preserve ['] includelink
        r@ deco 2!
    then
    r> drop

    htmlfilename >r
    htmlfile @ >r
    >in @ >r
    tib #tib @ preserve >r
    0 deco #tib @ 2* cells cpreserve >r
    sourcefile @ >r

    2dup fs->html create-html

    r/o open-file throw >r
    r@ sourcefile !
    begin
        tib maxline r@ read-line throw
    while
        #tib !  0 >IN !
        fevaluate
    repeat
    drop
    r> close-file throw

    close-html

    r> sourcefile !
    r> ccount 0 deco swap cmove
    r> count dup #tib ! tib swap cmove
    r> >in !
    r> htmlfile !
    r> to htmlfilename

;

: doc-refill
    showline
    tib maxline sourcefile @ read-line throw dup if
        swap #tib !  0 >in ! freshline
    else
        nip
    then
;


: getword ( -- a u )
    begin
        bl word count dup 0=
    while
        2drop doc-refill true <> abort" Failed to find word"
    repeat
;

: get) ( -- a u )
;

: doc-( 
    begin
        [char] ) parse
        + c@ [char] ) <>
    while
        doc-refill true <> abort" Failed to find )"
    repeat
; immediate

: document ( c-addr u -- ) \ document named file
    forth-wordlist 1 2>r
    standard 1 set-order
    standard set-current
    included
    2r> set-order
    forth-wordlist set-current
;

standard set-current
: included doc-included ;
: refill   doc-refill ; 
: (        postpone doc-( ; immediate

( KNOW FORTH                                 JCB 07:31 11/23/10)

\ All words from the DPANS94 Forth Standard
\ format is
\   KNOW <word> <reference-number> <word-set>

KNOW               !        6.1.0010 CORE
KNOW               #        6.1.0030 CORE
KNOW              #>        6.1.0040 CORE
KNOW              #S        6.1.0050 CORE
KNOW            #TIB        6.2.0060 CORE-EXT
KNOW               '        6.1.0070 CORE
KNOW               (        6.1.0080 CORE
KNOW               (     11.6.1.0080 FILE
KNOW         (LOCAL)     13.6.1.0086 LOCAL
KNOW               *        6.1.0090 CORE
KNOW              */        6.1.0100 CORE
KNOW           */MOD        6.1.0110 CORE
KNOW               +        6.1.0120 CORE
KNOW              +!        6.1.0130 CORE
KNOW           +LOOP        6.1.0140 CORE
KNOW               ,        6.1.0150 CORE
KNOW               -        6.1.0160 CORE
KNOW       -TRAILING     17.6.1.0170 STRING
KNOW               .        6.1.0180 CORE
KNOW              ."        6.1.0190 CORE
KNOW              .(        6.2.0200 CORE-EXT
KNOW              .R        6.2.0210 CORE-EXT
KNOW              .S     15.6.1.0220 TOOLS
KNOW               /        6.1.0230 CORE
KNOW            /MOD        6.1.0240 CORE
KNOW         /STRING     17.6.1.0245 STRING
KNOW              0<        6.1.0250 CORE
KNOW             0<>        6.2.0260 CORE-EXT
KNOW              0=        6.1.0270 CORE
KNOW              0>        6.2.0280 CORE-EXT
KNOW              1+        6.1.0290 CORE
KNOW              1-        6.1.0300 CORE
KNOW              2!        6.1.0310 CORE
KNOW              2*        6.1.0320 CORE
KNOW              2/        6.1.0330 CORE
KNOW             2>R        6.2.0340 CORE-EXT
KNOW              2@        6.1.0350 CORE
KNOW       2CONSTANT      8.6.1.0360 DOUBLE
KNOW           2DROP        6.1.0370 CORE
KNOW            2DUP        6.1.0380 CORE
KNOW        2LITERAL      8.6.1.0390 DOUBLE
KNOW           2OVER        6.1.0400 CORE
KNOW             2R>        6.2.0410 CORE-EXT
KNOW             2R@        6.2.0415 CORE-EXT
KNOW            2ROT      8.6.2.0420 DOUBLE-EXT
KNOW           2SWAP        6.1.0430 CORE
KNOW       2VARIABLE      8.6.1.0440 DOUBLE
KNOW               :        6.1.0450 CORE
KNOW         :NONAME        6.2.0455 CORE-EXT
KNOW               ;        6.1.0460 CORE
KNOW           ;CODE     15.6.2.0470 TOOLS-EXT
KNOW               <        6.1.0480 CORE
KNOW              <#        6.1.0490 CORE
KNOW              <>        6.2.0500 CORE-EXT
KNOW               =        6.1.0530 CORE
KNOW               >        6.1.0540 CORE
KNOW           >BODY        6.1.0550 CORE
KNOW          >FLOAT     12.6.1.0558 FLOATING
KNOW             >IN        6.1.0560 CORE
KNOW         >NUMBER        6.1.0570 CORE
KNOW              >R        6.1.0580 CORE
KNOW               ?     15.6.1.0600 TOOLS
KNOW             ?DO        6.2.0620 CORE-EXT
KNOW            ?DUP        6.1.0630 CORE
KNOW               @        6.1.0650 CORE
KNOW           ABORT        6.1.0670 CORE
KNOW           ABORT      9.6.2.0670 EXCEPTION-EXT
KNOW          ABORT"        6.1.0680 CORE
KNOW          ABORT"      9.6.2.0680 EXCEPTION-EXT
KNOW             ABS        6.1.0690 CORE
KNOW          ACCEPT        6.1.0695 CORE
KNOW           AGAIN        6.2.0700 CORE-EXT
KNOW           AHEAD     15.6.2.0702 TOOLS-EXT
KNOW           ALIGN        6.1.0705 CORE
KNOW         ALIGNED        6.1.0706 CORE
KNOW        ALLOCATE     14.6.1.0707 MEMORY
KNOW           ALLOT        6.1.0710 CORE
KNOW            ALSO     16.6.2.0715 SEARCH-EXT
KNOW             AND        6.1.0720 CORE
KNOW       ASSEMBLER     15.6.2.0740 TOOLS-EXT
KNOW           AT-XY     10.6.1.0742 FACILITY
KNOW            BASE        6.1.0750 CORE
KNOW           BEGIN        6.1.0760 CORE
KNOW             BIN     11.6.1.0765 FILE
KNOW              BL        6.1.0770 CORE
KNOW           BLANK     17.6.1.0780 STRING
KNOW             BLK      7.6.1.0790 BLOCK
KNOW           BLOCK      7.6.1.0800 BLOCK
KNOW          BUFFER      7.6.1.0820 BLOCK
KNOW             BYE     15.6.2.0830 TOOLS-EXT
KNOW              C!        6.1.0850 CORE
KNOW              C"        6.2.0855 CORE-EXT
KNOW              C,        6.1.0860 CORE
KNOW              C@        6.1.0870 CORE
KNOW            CASE        6.2.0873 CORE-EXT
KNOW           CATCH      9.6.1.0875 EXCEPTION
KNOW           CELL+        6.1.0880 CORE
KNOW           CELLS        6.1.0890 CORE
KNOW            CHAR        6.1.0895 CORE
KNOW           CHAR+        6.1.0897 CORE
KNOW           CHARS        6.1.0898 CORE
KNOW      CLOSE-FILE     11.6.1.0900 FILE
KNOW           CMOVE     17.6.1.0910 STRING
KNOW          CMOVE>     17.6.1.0920 STRING
KNOW            CODE     15.6.2.0930 TOOLS-EXT
KNOW         COMPARE     17.6.1.0935 STRING
KNOW        COMPILE,        6.2.0945 CORE-EXT
KNOW        CONSTANT        6.1.0950 CORE
KNOW         CONVERT        6.2.0970 CORE-EXT
KNOW           COUNT        6.1.0980 CORE
KNOW              CR        6.1.0990 CORE
KNOW          CREATE        6.1.1000 CORE
KNOW     CREATE-FILE     11.6.1.1010 FILE
KNOW         CS-PICK     15.6.2.1015 TOOLS-EXT
KNOW         CS-ROLL     15.6.2.1020 TOOLS-EXT
KNOW              D+      8.6.1.1040 DOUBLE
KNOW              D-      8.6.1.1050 DOUBLE
KNOW              D.      8.6.1.1060 DOUBLE
KNOW             D.R      8.6.1.1070 DOUBLE
KNOW             D0<      8.6.1.1075 DOUBLE
KNOW             D0=      8.6.1.1080 DOUBLE
KNOW             D2*      8.6.1.1090 DOUBLE
KNOW             D2/      8.6.1.1100 DOUBLE
KNOW              D<      8.6.1.1110 DOUBLE
KNOW              D=      8.6.1.1120 DOUBLE
KNOW             D>F     12.6.1.1130 FLOATING
KNOW             D>S      8.6.1.1140 DOUBLE
KNOW            DABS      8.6.1.1160 DOUBLE
KNOW         DECIMAL        6.1.1170 CORE
KNOW     DEFINITIONS     16.6.1.1180 SEARCH
KNOW     DELETE-FILE     11.6.1.1190 FILE
KNOW           DEPTH        6.1.1200 CORE
KNOW             DF!     12.6.2.1203 FLOATING-EXT
KNOW             DF@     12.6.2.1204 FLOATING-EXT
KNOW         DFALIGN     12.6.2.1205 FLOATING-EXT
KNOW       DFALIGNED     12.6.2.1207 FLOATING-EXT
KNOW         DFLOAT+     12.6.2.1208 FLOATING-EXT
KNOW         DFLOATS     12.6.2.1209 FLOATING-EXT
KNOW            DMAX      8.6.1.1210 DOUBLE
KNOW            DMIN      8.6.1.1220 DOUBLE
KNOW         DNEGATE      8.6.1.1230 DOUBLE
KNOW              DO        6.1.1240 CORE
KNOW           DOES>        6.1.1250 CORE
KNOW            DROP        6.1.1260 CORE
KNOW             DU<      8.6.2.1270 DOUBLE-EXT
KNOW            DUMP     15.6.1.1280 TOOLS
KNOW             DUP        6.1.1290 CORE
KNOW          EDITOR     15.6.2.1300 TOOLS-EXT
KNOW            EKEY     10.6.2.1305 FACILITY-EXT
KNOW       EKEY>CHAR     10.6.2.1306 FACILITY-EXT
KNOW           EKEY?     10.6.2.1307 FACILITY-EXT
KNOW            ELSE        6.1.1310 CORE
KNOW            EMIT        6.1.1320 CORE
KNOW           EMIT?     10.6.2.1325 FACILITY-EXT
KNOW   EMPTY-BUFFERS      7.6.2.1330 BLOCK-EXT
KNOW         ENDCASE        6.2.1342 CORE-EXT
KNOW           ENDOF        6.2.1343 CORE-EXT
KNOW    ENVIRONMENT?        6.1.1345 CORE
KNOW           ERASE        6.2.1350 CORE-EXT
KNOW        EVALUATE        6.1.1360 CORE
KNOW        EVALUATE      7.6.1.1360 BLOCK
KNOW         EXECUTE        6.1.1370 CORE
KNOW            EXIT        6.1.1380 CORE
KNOW          EXPECT        6.2.1390 CORE-EXT
KNOW              F!     12.6.1.1400 FLOATING
KNOW              F*     12.6.1.1410 FLOATING
KNOW             F**     12.6.2.1415 FLOATING-EXT
KNOW              F+     12.6.1.1420 FLOATING
KNOW              F-     12.6.1.1425 FLOATING
KNOW              F.     12.6.2.1427 FLOATING-EXT
KNOW              F/     12.6.1.1430 FLOATING
KNOW             F0<     12.6.1.1440 FLOATING
KNOW             F0=     12.6.1.1450 FLOATING
KNOW              F<     12.6.1.1460 FLOATING
KNOW             F>D     12.6.1.1470 FLOATING
KNOW              F@     12.6.1.1472 FLOATING
KNOW            FABS     12.6.2.1474 FLOATING-EXT
KNOW           FACOS     12.6.2.1476 FLOATING-EXT
KNOW          FACOSH     12.6.2.1477 FLOATING-EXT
KNOW          FALIGN     12.6.1.1479 FLOATING
KNOW        FALIGNED     12.6.1.1483 FLOATING
KNOW           FALOG     12.6.2.1484 FLOATING-EXT
KNOW           FALSE        6.2.1485 CORE-EXT
KNOW           FASIN     12.6.2.1486 FLOATING-EXT
KNOW          FASINH     12.6.2.1487 FLOATING-EXT
KNOW           FATAN     12.6.2.1488 FLOATING-EXT
KNOW          FATAN2     12.6.2.1489 FLOATING-EXT
KNOW          FATANH     12.6.2.1491 FLOATING-EXT
KNOW       FCONSTANT     12.6.1.1492 FLOATING
KNOW            FCOS     12.6.2.1493 FLOATING-EXT
KNOW           FCOSH     12.6.2.1494 FLOATING-EXT
KNOW          FDEPTH     12.6.1.1497 FLOATING
KNOW           FDROP     12.6.1.1500 FLOATING
KNOW            FDUP     12.6.1.1510 FLOATING
KNOW             FE.     12.6.2.1513 FLOATING-EXT
KNOW            FEXP     12.6.2.1515 FLOATING-EXT
KNOW          FEXPM1     12.6.2.1516 FLOATING-EXT
KNOW   FILE-POSITION     11.6.1.1520 FILE
KNOW       FILE-SIZE     11.6.1.1522 FILE
KNOW     FILE-STATUS     11.6.2.1524 FILE-EXT
KNOW            FILL        6.1.1540 CORE
KNOW            FIND        6.1.1550 CORE
KNOW            FIND     16.6.1.1550 SEARCH
KNOW        FLITERAL     12.6.1.1552 FLOATING
KNOW             FLN     12.6.2.1553 FLOATING-EXT
KNOW           FLNP1     12.6.2.1554 FLOATING-EXT
KNOW          FLOAT+     12.6.1.1555 FLOATING
KNOW          FLOATS     12.6.1.1556 FLOATING
KNOW            FLOG     12.6.2.1557 FLOATING-EXT
KNOW           FLOOR     12.6.1.1558 FLOATING
KNOW           FLUSH      7.6.1.1559 BLOCK
KNOW      FLUSH-FILE     11.6.2.1560 FILE-EXT
KNOW          FM/MOD        6.1.1561 CORE
KNOW            FMAX     12.6.1.1562 FLOATING
KNOW            FMIN     12.6.1.1565 FLOATING
KNOW         FNEGATE     12.6.1.1567 FLOATING
KNOW          FORGET     15.6.2.1580 TOOLS-EXT
KNOW           FORTH     16.6.2.1590 SEARCH-EXT
KNOW  FORTH-WORDLIST     16.6.1.1595 SEARCH
KNOW           FOVER     12.6.1.1600 FLOATING
KNOW            FREE     14.6.1.1605 MEMORY
KNOW            FROT     12.6.1.1610 FLOATING
KNOW          FROUND     12.6.1.1612 FLOATING
KNOW             FS.     12.6.2.1613 FLOATING-EXT
KNOW            FSIN     12.6.2.1614 FLOATING-EXT
KNOW         FSINCOS     12.6.2.1616 FLOATING-EXT
KNOW           FSINH     12.6.2.1617 FLOATING-EXT
KNOW           FSQRT     12.6.2.1618 FLOATING-EXT
KNOW           FSWAP     12.6.1.1620 FLOATING
KNOW            FTAN     12.6.2.1625 FLOATING-EXT
KNOW           FTANH     12.6.2.1626 FLOATING-EXT
KNOW       FVARIABLE     12.6.1.1630 FLOATING
KNOW              F~     12.6.2.1640 FLOATING-EXT
KNOW     GET-CURRENT     16.6.1.1643 SEARCH
KNOW       GET-ORDER     16.6.1.1647 SEARCH
KNOW            HERE        6.1.1650 CORE
KNOW             HEX        6.2.1660 CORE-EXT
KNOW            HOLD        6.1.1670 CORE
KNOW               I        6.1.1680 CORE
KNOW              IF        6.1.1700 CORE
KNOW       IMMEDIATE        6.1.1710 CORE
KNOW    INCLUDE-FILE     11.6.1.1717 FILE
KNOW        INCLUDED     11.6.1.1718 FILE
KNOW          INVERT        6.1.1720 CORE
KNOW               J        6.1.1730 CORE
KNOW             KEY        6.1.1750 CORE
KNOW            KEY?     10.6.1.1755 FACILITY
KNOW           LEAVE        6.1.1760 CORE
KNOW            LIST      7.6.2.1770 BLOCK-EXT
KNOW         LITERAL        6.1.1780 CORE
KNOW            LOAD      7.6.1.1790 BLOCK
KNOW         LOCALS|     13.6.2.1795 LOCAL-EXT
KNOW            LOOP        6.1.1800 CORE
KNOW          LSHIFT        6.1.1805 CORE
KNOW              M*        6.1.1810 CORE
KNOW             M*/      8.6.1.1820 DOUBLE
KNOW              M+      8.6.1.1830 DOUBLE
KNOW          MARKER        6.2.1850 CORE-EXT
KNOW             MAX        6.1.1870 CORE
KNOW             MIN        6.1.1880 CORE
KNOW             MOD        6.1.1890 CORE
KNOW            MOVE        6.1.1900 CORE
KNOW              MS     10.6.2.1905 FACILITY-EXT
KNOW          NEGATE        6.1.1910 CORE
KNOW             NIP        6.2.1930 CORE-EXT
KNOW              OF        6.2.1950 CORE-EXT
KNOW            ONLY     16.6.2.1965 SEARCH-EXT
KNOW       OPEN-FILE     11.6.1.1970 FILE
KNOW              OR        6.1.1980 CORE
KNOW           ORDER     16.6.2.1985 SEARCH-EXT
KNOW            OVER        6.1.1990 CORE
KNOW             PAD        6.2.2000 CORE-EXT
KNOW            PAGE     10.6.1.2005 FACILITY
KNOW           PARSE        6.2.2008 CORE-EXT
KNOW            PICK        6.2.2030 CORE-EXT
KNOW        POSTPONE        6.1.2033 CORE
KNOW       PRECISION     12.6.2.2035 FLOATING-EXT
KNOW        PREVIOUS     16.6.2.2037 SEARCH-EXT
KNOW           QUERY        6.2.2040 CORE-EXT
KNOW            QUIT        6.1.2050 CORE
KNOW             R/O     11.6.1.2054 FILE
KNOW             R/W     11.6.1.2056 FILE
KNOW              R>        6.1.2060 CORE
KNOW              R@        6.1.2070 CORE
KNOW       READ-FILE     11.6.1.2080 FILE
KNOW       READ-LINE     11.6.1.2090 FILE
KNOW         RECURSE        6.1.2120 CORE
KNOW          REFILL        6.2.2125 CORE-EXT
KNOW          REFILL      7.6.2.2125 BLOCK-EXT
KNOW          REFILL     11.6.2.2125 FILE-EXT
KNOW     RENAME-FILE     11.6.2.2130 FILE-EXT
KNOW          REPEAT        6.1.2140 CORE
KNOW REPOSITION-FILE     11.6.1.2142 FILE
KNOW       REPRESENT     12.6.1.2143 FLOATING
KNOW          RESIZE     14.6.1.2145 MEMORY
KNOW     RESIZE-FILE     11.6.1.2147 FILE
KNOW   RESTORE-INPUT        6.2.2148 CORE-EXT
KNOW            ROLL        6.2.2150 CORE-EXT
KNOW             ROT        6.1.2160 CORE
KNOW          RSHIFT        6.1.2162 CORE
KNOW              S"        6.1.2165 CORE
KNOW              S"     11.6.1.2165 FILE
KNOW             S>D        6.1.2170 CORE
KNOW    SAVE-BUFFERS      7.6.1.2180 BLOCK
KNOW      SAVE-INPUT        6.2.2182 CORE-EXT
KNOW             SCR      7.6.2.2190 BLOCK-EXT
KNOW          SEARCH     17.6.1.2191 STRING
KNOW SEARCH-WORDLIST     16.6.1.2192 SEARCH
KNOW             SEE     15.6.1.2194 TOOLS
KNOW     SET-CURRENT     16.6.1.2195 SEARCH
KNOW       SET-ORDER     16.6.1.2197 SEARCH
KNOW   SET-PRECISION     12.6.2.2200 FLOATING-EXT
KNOW             SF!     12.6.2.2202 FLOATING-EXT
KNOW             SF@     12.6.2.2203 FLOATING-EXT
KNOW         SFALIGN     12.6.2.2204 FLOATING-EXT
KNOW       SFALIGNED     12.6.2.2206 FLOATING-EXT
KNOW         SFLOAT+     12.6.2.2207 FLOATING-EXT
KNOW         SFLOATS     12.6.2.2208 FLOATING-EXT
KNOW            SIGN        6.1.2210 CORE
KNOW        SLITERAL     17.6.1.2212 STRING
KNOW          SM/REM        6.1.2214 CORE
KNOW          SOURCE        6.1.2216 CORE
KNOW       SOURCE-ID        6.2.2218 CORE-EXT
KNOW       SOURCE-ID     11.6.1.2218 FILE
KNOW           SPACE        6.1.2220 CORE
KNOW          SPACES        6.1.2230 CORE
KNOW            SPAN        6.2.2240 CORE-EXT
KNOW           STATE        6.1.2250 CORE
KNOW           STATE     15.6.2.2250 TOOLS-EXT
KNOW            SWAP        6.1.2260 CORE
KNOW            THEN        6.1.2270 CORE
KNOW           THROW      9.6.1.2275 EXCEPTION
KNOW            THRU      7.6.2.2280 BLOCK-EXT
KNOW             TIB        6.2.2290 CORE-EXT
KNOW       TIME&DATE     10.6.2.2292 FACILITY-EXT
KNOW              TO        6.2.2295 CORE-EXT
KNOW              TO     13.6.1.2295 LOCAL
KNOW            TRUE        6.2.2298 CORE-EXT
KNOW            TUCK        6.2.2300 CORE-EXT
KNOW            TYPE        6.1.2310 CORE
KNOW              U.        6.1.2320 CORE
KNOW             U.R        6.2.2330 CORE-EXT
KNOW              U<        6.1.2340 CORE
KNOW              U>        6.2.2350 CORE-EXT
KNOW             UM*        6.1.2360 CORE
KNOW          UM/MOD        6.1.2370 CORE
KNOW          UNLOOP        6.1.2380 CORE
KNOW           UNTIL        6.1.2390 CORE
KNOW          UNUSED        6.2.2395 CORE-EXT
KNOW          UPDATE      7.6.1.2400 BLOCK
KNOW           VALUE        6.2.2405 CORE-EXT
KNOW        VARIABLE        6.1.2410 CORE
KNOW             W/O     11.6.1.2425 FILE
KNOW           WHILE        6.1.2430 CORE
KNOW          WITHIN        6.2.2440 CORE-EXT
KNOW            WORD        6.1.2450 CORE
KNOW        WORDLIST     16.6.1.2460 SEARCH
KNOW           WORDS     15.6.1.2465 TOOLS
KNOW      WRITE-FILE     11.6.1.2480 FILE
KNOW      WRITE-LINE     11.6.1.2485 FILE
KNOW             XOR        6.1.2490 CORE
KNOW               [        6.1.2500 CORE
KNOW             [']        6.1.2510 CORE
KNOW          [CHAR]        6.1.2520 CORE
KNOW       [COMPILE]        6.2.2530 CORE-EXT
KNOW          [ELSE]     15.6.2.2531 TOOLS-EXT
KNOW            [IF]     15.6.2.2532 TOOLS-EXT
KNOW          [THEN]     15.6.2.2533 TOOLS-EXT
KNOW               \        6.2.2535 CORE-EXT
KNOW               \      7.6.2.2535 BLOCK-EXT
KNOW               ]        6.1.2540 CORE

forth-wordlist set-current
