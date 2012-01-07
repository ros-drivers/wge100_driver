( Hardware debug code                        JCB 13:19 08/24/10)
create rstk d# 64 allot

: hwdebug
    s" hw debug" type cr cr

   d# -100 begin h# 1111 >r d# 1 + dup d# 0 = until drop

\  d# 947303. d# 1000 um/mod          \ usage d# 4
\  d# 79218. <# # # # # # #>       \ usage d# 7
\  d# 79218. <# #s #>              \ usage d# 8
\  d# 79218. ip-pretty             \ usage d# 12
   d# 79218. #ip                   \ usage d# 10

   r> [ rstk d# 0 + ] literal !
   r> [ rstk d# 1 2* + ] literal !
   r> [ rstk d# 2 2* + ] literal !
   r> [ rstk d# 3 2* + ] literal !
   r> [ rstk d# 4 2* + ] literal !
   r> [ rstk d# 5 2* + ] literal !
   r> [ rstk d# 6 2* + ] literal !
   r> [ rstk d# 7 2* + ] literal !
   r> [ rstk d# 8 2* + ] literal !
   r> [ rstk d# 9 2* + ] literal !
   r> [ rstk d# 10 2* + ] literal !
   r> [ rstk d# 11 2* + ] literal !
   r> [ rstk d# 12 2* + ] literal !
   r> [ rstk d# 13 2* + ] literal !
   r> [ rstk d# 14 2* + ] literal !
   r> [ rstk d# 15 2* + ] literal !
   r> [ rstk d# 16 2* + ] literal !
   r> [ rstk d# 17 2* + ] literal !
   r> [ rstk d# 18 2* + ] literal !
   r> [ rstk d# 19 2* + ] literal !
   r> [ rstk d# 20 2* + ] literal !
   r> [ rstk d# 21 2* + ] literal !
   r> [ rstk d# 22 2* + ] literal !
   r> [ rstk d# 23 2* + ] literal !
   r> [ rstk d# 24 2* + ] literal !
   r> [ rstk d# 25 2* + ] literal !
   r> [ rstk d# 26 2* + ] literal !
   r> [ rstk d# 27 2* + ] literal !
   r> [ rstk d# 28 2* + ] literal !
   r> [ rstk d# 29 2* + ] literal !
   r> [ rstk d# 30 2* + ] literal !
   r> [ rstk d# 31 2* + ] literal !
   d# 32 0do i hex2 space rstk i 2* + @ hex4 cr loop

    .s cr
    halt
;
