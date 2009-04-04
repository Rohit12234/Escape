[BITS 32]

%include "syscalls.s"

[extern errno]

SYSC_RET_2ARGS_ERR open,SYSCALL_OPEN
SYSC_RET_3ARGS_ERR read,SYSCALL_READ
SYSC_RET_2ARGS_ERR getFileInfo,SYSCALL_STAT
SYSC_RET_1ARGS_ERR eof,SYSCALL_EOF
SYSC_RET_2ARGS_ERR seek,SYSCALL_SEEK
SYSC_RET_3ARGS_ERR write,SYSCALL_WRITE
SYSC_RET_1ARGS_ERR dupFd,SYSCALL_DUPFD
SYSC_RET_2ARGS_ERR redirFd,SYSCALL_REDIRFD
SYSC_VOID_1ARGS close,SYSCALL_CLOSE
