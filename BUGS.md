# Bug list
Older bugs from 1.18 and older are not available.

## Current bugs

### 'ls' command hangs x86_64 EFI (1.19+)
 * Reported by calmsacibis995 (Stefanos Stefanidis) at 2025/07/20
 * Status: Fix in progress.

'Dir' is not closed on x86_64 platforms and the system enters into a
deadlock with no crash reported on the serial console.

AArch64 platforms are not affected and the function returns successfully
back to the prompt.

## Fixed bugs

