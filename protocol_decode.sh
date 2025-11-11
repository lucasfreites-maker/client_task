for o in 1 2 3; do
  for p in $(seq 0 300); do
    ./client2 read $o $p
  done
done