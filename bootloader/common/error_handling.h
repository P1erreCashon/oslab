#ifndef ERROR_HANDLING_H
#define ERROR_HANDLING_H

// Stage 3.3.3: 统一错误处理和恢复系统

// === 统一错误代码系统 ===
typedef enum {
    // 成功
    ERROR_SUCCESS = 0,
    
    // 通用错误 (1-10)
    ERROR_INVALID_PARAM = 1,
    ERROR_OUT_OF_MEMORY = 2,
    ERROR_TIMEOUT = 3,
    ERROR_NOT_FOUND = 4,
    ERROR_PERMISSION_DENIED = 5,
    
    // 硬件错误 (11-20)
    ERROR_HARDWARE_FAULT = 11,
    ERROR_DEVICE_NOT_READY = 12,
    ERROR_DEVICE_TIMEOUT = 13,
    ERROR_DEVICE_ERROR = 14,
    ERROR_INVALID_DEVICE = 15,
    
    // VirtIO错误 (21-30)
    ERROR_VIRTIO_INIT_FAILED = 21,
    ERROR_VIRTIO_QUEUE_FULL = 22,
    ERROR_VIRTIO_BAD_RESPONSE = 23,
    ERROR_VIRTIO_DEVICE_ERROR = 24,
    
    // ELF加载错误 (31-40)
    ERROR_ELF_INVALID_MAGIC = 31,
    ERROR_ELF_INVALID_CLASS = 32,
    ERROR_ELF_INVALID_ENDIAN = 33,
    ERROR_ELF_NO_SEGMENTS = 34,
    ERROR_ELF_LOAD_FAILED = 35,
    
    // 内存管理错误 (41-50)
    ERROR_MEMORY_OVERLAP = 41,
    ERROR_MEMORY_OUT_OF_BOUNDS = 42,
    ERROR_MEMORY_PROTECTION = 43,
    ERROR_MEMORY_ALIGNMENT = 44,
    
    // 磁盘I/O错误 (51-60)
    ERROR_DISK_READ_FAILED = 51,
    ERROR_DISK_WRITE_FAILED = 52,
    ERROR_DISK_BAD_SECTOR = 53,
    ERROR_DISK_NOT_READY = 54,
    
    // 引导流程错误 (61-70)
    ERROR_BOOT_INVALID_STAGE = 61,
    ERROR_BOOT_KERNEL_INVALID = 62,
    ERROR_BOOT_MEMORY_CONFLICT = 63,
    ERROR_BOOT_DEVICE_TREE_FAILED = 64,
    
    // 系统错误 (71-80)
    ERROR_SYSTEM_HALT = 71,
    ERROR_CRITICAL_FAILURE = 72
} error_code_t;

// === 错误信息结构 ===
struct error_info {
    error_code_t code;
    const char *message;
    const char *function;
    int line;
    uint64 context_data[4];  // 额外的上下文信息
};

// === 错误处理回调类型 ===
typedef enum {
    ERROR_ACTION_CONTINUE,    // 继续执行
    ERROR_ACTION_RETRY,       // 重试操作
    ERROR_ACTION_FALLBACK,    // 使用备选方案
    ERROR_ACTION_ABORT        // 中止操作
} error_action_t;

typedef error_action_t (*error_handler_t)(const struct error_info *error);

// === 错误处理API ===

// 初始化错误处理系统
void error_system_init(void);

// 注册错误处理器
int error_register_handler(error_code_t code, error_handler_t handler);

// 报告错误
error_action_t error_report(error_code_t code, const char *function, int line, 
                           uint64 ctx1, uint64 ctx2, uint64 ctx3, uint64 ctx4);

// 获取错误描述
const char *error_get_description(error_code_t code);

// 错误统计
int error_get_count(error_code_t code);
void error_print_statistics(void);

// === 便利宏 ===
#define ERROR_REPORT(code) \
    error_report(code, __FUNCTION__, __LINE__, 0, 0, 0, 0)

#define ERROR_REPORT_CTX1(code, ctx1) \
    error_report(code, __FUNCTION__, __LINE__, ctx1, 0, 0, 0)

#define ERROR_REPORT_CTX2(code, ctx1, ctx2) \
    error_report(code, __FUNCTION__, __LINE__, ctx1, ctx2, 0, 0)

#define ERROR_REPORT_CTX4(code, ctx1, ctx2, ctx3, ctx4) \
    error_report(code, __FUNCTION__, __LINE__, ctx1, ctx2, ctx3, ctx4)

// === 恢复机制 ===

// 重试配置
struct retry_config {
    int max_attempts;
    int delay_ms;
    bool exponential_backoff;
};

// 执行带重试的操作
typedef int (*retryable_operation_t)(void *context);
int error_retry_operation(retryable_operation_t operation, void *context, 
                         const struct retry_config *config);

// === 预定义的错误处理器 ===
error_action_t default_error_handler(const struct error_info *error);
error_action_t critical_error_handler(const struct error_info *error);
error_action_t disk_error_handler(const struct error_info *error);

#endif // ERROR_HANDLING_H
