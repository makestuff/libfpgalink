#!/bin/bash
rm -rf {in,out}{0,1}
for bank in 0 1; do
  mkdir {in,out}${bank}
  for block in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do
    dd if=/dev/urandom of=in${bank}/${block}.dat bs=4096 count=1 > /dev/null 2>&1
  done
done
for i in in0 in1; do
  printf "$i: "
  hxd $i/0.dat | head -1
done
for bank in 0 1; do
  echo "Bank ${bank}:"
  for block in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do
    printf "  Writing ${block}..."
    cat in${bank}/${block}.dat | ../lin.x64/rel/ucm -v 1d50:602b:0002 -o 0xA2 0x${block}000 0x000${bank} 0x1000
    echo "done"
  done
done
for bank in 0 1; do
  echo "Bank ${bank}:"
  for block in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do
    printf "  Reading ${block}..."
    ../lin.x64/rel/ucm -v 1d50:602b:0002 -i 0xA2 0x${block}000 0x000${bank} 0x1000 > out${bank}/${block}.dat
    echo "done"
  done
done
for i in 0 1; do
  diff -r in$i out$i
done
