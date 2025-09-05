typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;

typedef uint64 pde_t;

// 时间结构体，用于 gettimeofday 系统调用
struct timeval {
  uint64 tv_sec;   // 秒数
  uint64 tv_usec;  // 微秒数
};
