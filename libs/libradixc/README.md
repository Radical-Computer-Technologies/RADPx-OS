# libradixc

`libradixc` is the current freestanding RADPx C/POSIX compatibility library.
The x86_64 GRUB target builds this source into both `libradixc.a` and
`libradixc.rso`, then stages those artifacts into its generated root filesystem.

Public compatibility headers live in `include/`. Target builds copy those
headers into generated sysroots rather than owning separate source copies.
