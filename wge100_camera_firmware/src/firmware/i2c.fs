( i2c start,stop,tx,rx                       JCB 13:08 08/24/10)
module[ i2c"
: i2c-start     \ with SCL high, change SDA from 1 to 0 
    d# 1 SDA i2c-half SCL-1 i2c-half d# 0 SDA i2c-half SCL-0 ;
: i2c-stop      \ with SCL high, change SDA from 0 to 1
    d# 0 SDA i2c-half SCL-1 i2c-half d# 1 SDA i2c-half ;

: i2c-rx-bit ( -- b )
    d# 1 SDA i2c-half SCL-1 i2c-half SDA-rd SCL-0 ;
: i2c-tx-bit ( f -- )
    0<> SDA i2c-half SCL-1 i2c-half SCL-0 ;

: i2c-tx    ( b -- nak )
    d# 8 d# 0 do dup d# 128 and i2c-tx-bit 2* loop drop i2c-rx-bit ;

: i2c-rx    ( nak -- b )
    d# 0 d# 8 d# 0 do 2* i2c-rx-bit + loop swap i2c-tx-bit ;

]module
