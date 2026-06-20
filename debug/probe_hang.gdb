# Decide WHY the first post-sl_Start command (sl_NetCfgGet probe) hangs.
# Synchronous: stop+bt on each blocking wait; silent-count the completion paths.
# The LAST backtrace printed before it wedges (script ends / times out) = culprit.
set pagination off
set confirm off

target extended-remote localhost:3333
monitor reset halt

# Run past sl_Start to the probe command.
break sl_NetCfgGet
echo \n==== running to sl_NetCfgGet (past sl_Start) ====\n
continue
bt
delete

# Silent counters: each printed line == one occurrence.
break SPITivaDMA_hwiFxn
commands
  silent
  printf "[hwiFxn] SPI-DMA completion ISR fired\n"
  continue
end
break vSimpleLinkSpawnTask
commands
  silent
  printf "[spawn] vSimpleLinkSpawnTask body ran\n"
  continue
end

# Stop+bt on the two blocking primitives.
break osi_SyncObjWait
commands
  printf "\n---- osi_SyncObjWait (response/pool wait) ----\n"
  bt
end
break osi_LockObjLock
commands
  printf "\n---- osi_LockObjLock (command lock) ----\n"
  bt
end

# Walk through the waits. The one that never returns is the wedge:
# its bt is the last one printed before this sequence stalls.
continue
continue
continue
continue
continue
continue
echo \n==== reached end of continues without wedging ====\n
