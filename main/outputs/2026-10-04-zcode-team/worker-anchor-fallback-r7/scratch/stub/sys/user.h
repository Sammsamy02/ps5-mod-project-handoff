#pragma once
/* Syntax-gate stub (host has no FreeBSD kinfo_vmentry): the TU only needs the
   type + sizeof; field access is via raw byte offsets in the TU itself. */
struct kinfo_vmentry { unsigned char opaque[1160]; };
