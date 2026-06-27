p = 2013265921
g = pow(31, 15, p)
print("babybear  TWO_ADIC_GEN =", g,
      "order_ok =", pow(g, 2**27, p) == 1 and pow(g, 2**26, p) == p - 1)

pk = 2130706433
gk = pow(3, 127, pk)
print("koalabear TWO_ADIC_GEN =", gk,
      "order_ok =", pow(gk, 2**24, pk) == 1 and pow(gk, 2**23, pk) == pk - 1)

pg = (1 << 64) - (1 << 32) + 1
gg = 1753635133440165772
print("goldilocks TWO_ADIC_GEN =", gg,
      "order_ok =", pow(gg, 2**32, pg) == 1 and pow(gg, 2**31, pg) == pg - 1)
