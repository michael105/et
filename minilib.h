// Include this file in order to use minilib functions.
// define the names of the functions you'd like to use here,
// uncomment unwanted functions.

/// Len of buf used by read, mprintf, ..
//#define mini_buf 4096
#define mini_buf 65536

#define mini_start start
//#define mini_vsyscalls // vsyscalls.(+ ~92 bytes). 
#define mini_errno
// ~twice faster than standard syscalls here.


#define mini_write
#define mini_open
#define mini_close
#define mini_read
#define mini_lseek

#define mini_mprints
#define mini_printf
//#define mini_perror
#define mini_msprintf
#define mini_mfprintf
#define mini_itohex
#define mini_itodec  // also conversion %d in printf
//#define mini_dtodec  // 
#define mini_atoi  // also needed for printf
//#define mini_itobin
//#define mini_exit
//#define mini_syscall
#define mini_ioctl
#define mini_tcgetattr
#define mini_tcsetattr
#define mini_select 
//
#define mini_mstrcmp 
#define mini_mstrlen

#define  mini_memset
#define  mini_memcpy

//#define mini_isprint
#define mini_isspace

//#define mini_epoll // TODO:: doesn't ork
//#define mini_malloc
#define mini_print //print,printl


// Whether to overwrite the standard calls to printf, ...
#define mini_overwrite





#include "minilib/include/minilib_header.h"
//#include "minilib/minilib_implementation.c"

