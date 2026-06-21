# Flash the rebuilt image and check the post-sl_Start command now completes.
set pagination off
set confirm off

target extended-remote localhost:3333
monitor reset halt
load
monitor reset halt

break sl_NetCfgGet
break sl_WlanPolicySet

echo \n==== run to sl_NetCfgGet probe ====\n
continue
echo \n==== reached probe; now finish it (returns => NWP responded) ====\n
finish

echo \n==== continue to sl_WlanPolicySet (proves we got past the probe) ====\n
continue
echo \n==== reached sl_WlanPolicySet -- bring-up is progressing ====\n
bt
echo \n==== done ====\n
