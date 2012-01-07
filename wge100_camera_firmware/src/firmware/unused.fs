( unused scraps of code                      JCB 15:38 10/26/10)

0 [IF]
: j2r@
    ]asm
        T                   r-2 alu
        rT    T->N          r+1 d+1 alu 
        rT    T->N          r+1 d+1 alu 
    asm[
;
: j2>r
    ]asm
        rT    t->R          alu
    asm[
    swap
;

: demo
    begin
        timeq >q
        d# 1 d# 1000 qm*/
        d# 1 d# 1000 qm*/
        d# 1 d# 60 qm*/mod swap
        d# 1 d# 60 qm*/mod swap ( s m qhours )
        d# 1 d# 24 qm*/mod swap ( s m h qdays )
        q>s .
        d# 100 m*
        rot s>d d+
        d# 100 d# 1 m*/
        rot s>d d+
        <# # # [char] : hold # # [char] : hold # # #> type cr
    again
    halt
;
[THEN]

