.global _start
_start:
  // NOTE: Reversed Engineered mmap syscall
  mov x8, #0xde // mmap syscall code
  mov x0, #0x0 // Current mapped addr (NULL)
  mov x1, #0x7530 // Size (30000 bytes)
  mov x2, #0x3 // Protection Flags (PROT_READ|PROT_WRITE)
  mov x3, #0x22 // Map Flags (MAP_ANONYMOUS|MAP_PRIVATE)
  mov x4, #-1 // File Descriptor (-1)
  mov x5, #0x0 // Offset 
  svc #0

  bl 24

  // NOTE: munmap
  // x0: addr
  // x1: len
  mov x8, #0xd7
  svc #0
  
  // Return with 0
  mov x8, #0x5d
  mov x0, #0x0
  svc #0
