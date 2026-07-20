# GDB helpers for RADPx-OS on the Pi Zero 2 W over JTAG (OpenOCD on :3333).
#   aarch64-none-elf-gdb tools/embedded/rad_pi_zero2w/RADKRN.ELF -x jtag_debug.gdb
#
# Load RADKRN.ELF (symbols) BEFORE sourcing so $pc/globals resolve.
set pagination off
set confirm off
target extended-remote :3333

define regs
  info registers pc sp x0 x1 x30
  printf "CurrentEL check via ELR/SPSR as needed\n"
end
document regs
  Dump the key core registers of the halted core.
end

# --- Page allocator sanity (the fixed latent bug: usable_base must clear the
#     ~78 MB image; 0x08000000 after RAD_A53_USABLE_FLOOR). ------------------
define allocstate
  printf "usable_base = 0x%llx\n", g_a53.summary.usable_base
  printf "usable_limit= 0x%llx\n", g_a53.summary.usable_limit
  printf "free_pages  = %lu  reserved = %lu\n", g_a53.summary.free_pages, g_a53.summary.reserved_pages
end
document allocstate
  Print the a53 page-allocator window -- usable_base should be >= 0x08000000.
end

# --- LAYOUT-BUG HUNT (why linking bcm283x_wifi.o breaks login) --------------
# Procedure (run against the WIFI-LINKED image = the failing build):
#  1. Build with WiFi linked (add bcm283x_wifi.o to OBJS, re-enable the probe).
#  2. openocd ... ; gdb RADKRN.ELF -x jtag_debug.gdb ; `monitor reset halt`.
#  3. The corruption is a shifted section (newlib .bss) colliding with an OOB
#     write. Candidates to watch, in order of suspicion:
#       - the newlib malloc arena / heap (login allocates during auth)
#       - the a53 user-process / task structures (login runs as a task)
#       - g_init_image / VFS read buffers (the auth file is read through these)
#  4. Set a HARDWARE watchpoint on a suspect symbol and continue; the write that
#     trips it (with `bt`) is the overrunning code. Example:
#       watch -location *(char*)&<symbol>       # rwatch/awatch as needed
#       continue
#  5. Compare the same symbol's neighbours between the wifi and no-wifi ELF maps
#     (RADKRN.map) to see which object's section moved into the write's path.
define watchsym
  if $argc != 1
    printf "usage: watchsym <symbol>\n"
  else
    watch *$arg0
    printf "watching %s; `continue` and inspect the trapping bt\n", "$arg0"
  end
end
document watchsym
  Set a hardware watchpoint on a symbol to catch the layout-bug corruptor.
end

# --- Console-over-JTAG (build with RAD_PI_EXTRA_DEFINES=-DRAD_PI_JTAG_CONSOLE) --
# Drains the kernel's g_rad_jtag_console ring over the JTAG memory port so you get
# the serial console with NO UART wire (only the DirtyJTAG). Non-halting reads
# work while the core runs on aarch64.
define jtagcon
  set $__c = &g_rad_jtag_console
  while $__c->tail != $__c->head
    printf "%c", $__c->buf[$__c->tail % 8192]
    set $__c->tail = $__c->tail + 1
  end
end
document jtagcon
  Print + drain any pending console output from the g_rad_jtag_console ring.
end

define jtagsend
  # jtagsend "root\r"  -- inject a line into the kernel's input ring.
  set $__i = &g_rad_jtag_input
  set $__s = $arg0
  set $__k = 0
  while $__s[$__k] != '\000'
    set $__i->buf[$__i->head % 256] = $__s[$__k]
    set $__i->head = $__i->head + 1
    set $__k = $__k + 1
  end
end
document jtagsend
  Inject a NUL-terminated string into g_rad_jtag_input (debugger -> kernel stdin).
end

printf "RADPx-OS JTAG session. Commands: regs, allocstate, watchsym <sym>,\n"
printf "  jtagcon (drain console-over-JTAG), jtagsend \"root\\r\" (inject input).\n"
printf "Halt with `monitor reset halt` or `interrupt`.\n"
