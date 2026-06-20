# Attach to the already-hung firmware (no reset) and inspect BIOS task state.
set pagination off
set confirm off
set mi-async off

target extended-remote localhost:3333
monitor halt

echo \n==== current (idle/all-blocked) context ====\n
bt

echo \n==== conn mgr Task_Struct ====\n
print task_connection_manager_struct

echo \n==== uart log Task_Struct (for comparison) ====\n
print task_uart_log_struct

echo \n==== done ====\n
