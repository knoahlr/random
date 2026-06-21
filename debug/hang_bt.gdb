# Catch the SimpleLink post-sl_Start hang and dump where it's blocked.
# Usage: gdb-multiarch -q -x debug/hang_bt.gdb build/random.out
set pagination off
set confirm off
set print pretty on

target extended-remote localhost:3333

# Start clean, let the firmware run to the hang on its own.
monitor reset halt

# Async so we can let it run and then interrupt it in place.
set target-async on
continue &

# Give bring-up time to reach sl_Start and the first blocking command.
shell sleep 4

# Freeze the core wherever it is (the hang) and dump the stack.
interrupt

echo \n==== BACKTRACE (where it is stuck) ====\n
bt

echo \n==== REGISTERS ====\n
info registers

echo \n==== CURRENT FRAME LOCALS ====\n
info locals

echo \n==== done ====\n
