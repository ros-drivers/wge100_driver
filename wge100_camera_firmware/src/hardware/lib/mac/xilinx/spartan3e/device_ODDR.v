module device_ODDR
(inout S, inout R, inout CE,
 input C0, input D0,
 input C1, input D1,
 output Q);

ODDR2 inst
  (.S(S), .R(R), .CE(CE),
   .C0(C0), .D0(D0),
   .C1(C1), .D1(D1),
   .Q(Q));
endmodule
