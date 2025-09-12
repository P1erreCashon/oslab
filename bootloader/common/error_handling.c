#include "boot_types.h"
#include "error_handling.h"

// === 错误描述表 ===
static const struct {
    error_code_t code;
    const char *description;
} error_descriptions[] = {
    {ERROR_SUCCESS, "Success"},
    {ERROR_INVALID_PARAM, "Invalid parameter"},
    {ERROR_OUT_OF_MEMORY, "Out of memory"},
    {ERROR_TIMEOUT, "Operation timeout"},
    {ERROR_NOT_FOUND, "Resource not found"},
    {ERROR_PERMISSION_DENIED, "Permission denied"},
    
    {ERROR_HARDWARE_FAULT, "Hardware fault"},
    {ERROR_DEVICE_NOT_READY, "Device not ready"},
    {ERROR_DEVICE_TIMEOUT, "Device timeout"},
    {ERROR_DEVICE_ERROR, "Device error"},
    {ERROR_INVALID_DEVICE, "Invalid device"},
    
    {ERROR_VIRTIO_INIT_FAILED, "VirtIO initialization failed"},
    {ERROR_VIRTIO_QUEUE_FULL, "VirtIO queue full"},
    {ERROR_VIRTIO_BAD_RESPONSE, "VirtIO bad response"},
    {ERROR_VIRTIO_DEVICE_ERROR, "VirtIO device error"},
    
    {ERROR_ELF_INVALID_MAGIC, "Invalid ELF magic"},
    {ERROR_ELF_INVALID_CLASS, "Invalid ELF class"},
    {ERROR_ELF_INVALID_ENDIAN, "Invalid ELF endianness"},
    {ERROR_ELF_NO_SEGMENTS, "No loadable ELF segments"},
    {ERROR_ELF_LOAD_FAILED, "ELF load failed"},
    
    {ERROR_MEMORY_OVERLAP, "Memory region overlap"},
    {ERROR_MEMORY_OUT_OF_BOUNDS, "Memory out of bounds"},
    {ERROR_MEMORY_PROTECTION, "Memory protection violation"},
    {ERROR_MEMORY_ALIGNMENT, "Memory alignment error"},
    
    {ERROR_DISK_READ_FAILED, "Disk read failed"},
    {ERROR_DISK_WRITE_FAILED, "Disk write failed"},
    {ERROR_DISK_BAD_SECTOR, "Bad disk sector"},
    {ERROR_DISK_NOT_READY, "Disk not ready"},
    
    {ERROR_BOOT_INVALID_STAGE, "Invalid boot stage"},
    {ERROR_BOOT_KERNEL_INVALID, "Invalid kernel"},
    {ERROR_BOOT_MEMORY_CONFLICT, "Boot memory conflict"},
    {ERROR_BOOT_DEVICE_TREE_FAILED, "Device tree generation failed"},
    
    {ERROR_SYSTEM_HALT, "System halt requested"},
    {ERROR_CRITICAL_FAILURE, "Critical system failure"}
};

static const int error_description_count = sizeof(error_descriptions) / sizeof(error_descriptions[0]);

// === 错误处理状态 ===
static struct {
    bool initialized;
    error_handler_t handlers[ERROR_CRITICAL_FAILURE + 1];
    int error_counts[ERROR_CRITICAL_FAILURE + 1];
    int total_errors;
} error_system = {false, {0}, {0}, 0};

// === 错误系统初始化 ===
void error_system_init(void) {
    if (error_system.initialized) {
        return; // 已初始化
    }
    
    // 清零所有处理器
    for (int i = 0; i <= ERROR_CRITICAL_FAILURE; i++) {
        error_system.handlers[i] = default_error_handler;
        error_system.error_counts[i] = 0;
    }
    
    // 注册特殊错误的处理器
    error_system.handlers[ERROR_CRITICAL_FAILURE] = critical_error_handler;
    error_system.handlers[ERROR_SYSTEM_HALT] = critical_error_handler;
    error_system.handlers[ERROR_DISK_READ_FAILED] = disk_error_handler;
    error_system.handlers[ERROR_DISK_WRITE_FAILED] = disk_error_handler;
    error_system.handlers[ERROR_VIRTIO_DEVICE_ERROR] = disk_error_handler;
    
    error_system.total_errors = 0;
    error_system.initialized = true;
    
    uart_puts("Error handling system initialized\n");
}

// === 错误处理器注册 ===
int error_register_handler(error_code_t code, error_handler_t handler) {
    if (!error_system.initialized) {
        return -1;
    }
    
    if (code > ERROR_CRITICAL_FAILURE || !handler) {
        return -1;
    }
    
    error_system.handlers[code] = handler;
    return 0;
}

// === 错误报告 ===
error_action_t error_report(error_code_t code, const char *function, int line,
                           uint64 ctx1, uint64 ctx2, uint64 ctx3, uint64 ctx4) {
    if (!error_system.initialized) {
        error_system_init(); // 自动初始化
    }
    
    // 更新统计
    if (code <= ERROR_CRITICAL_FAILURE) {
        error_system.error_counts[code]++;
    }
    error_system.total_errors++;
    
    // 构建错误信息
    struct error_info error_info = {
        .code = code,
        .message = error_get_description(code),
        .function = function,
        .line = line,
        .context_data = {ctx1, ctx2, ctx3, ctx4}
    };
    
    // 显示错误信息
    uart_puts("ERROR ");
    uart_put_dec(code);
    uart_puts(": ");
    uart_puts(error_info.message);
    uart_puts("\n");
    uart_puts("  At: ");
    uart_puts(function);
    uart_puts(":");
    uart_put_dec(line);
    uart_puts("\n");
    
    if (ctx1 != 0 || ctx2 != 0 || ctx3 != 0 || ctx4 != 0) {
        uart_puts("  Context: ");
        uart_put_hex(ctx1);
        uart_puts(" ");
        uart_put_hex(ctx2);
        uart_puts(" ");
        uart_put_hex(ctx3);
        uart_puts(" ");
        uart_put_hex(ctx4);
        uart_puts("\n");
    }
    
    // 调用错误处理器
    error_handler_t handler = error_system.handlers[code];
    if (!handler) {
        handler = default_error_handler;
    }
    
    return handler(&error_info);
}

// === 错误描述查找 ===
const char *error_get_description(error_code_t code) {
    for (int i = 0; i < error_description_count; i++) {
        if (error_descriptions[i].code == code) {
            return error_descriptions[i].description;
        }
    }
    return "Unknown error";
}

// === 错误统计 ===
int error_get_count(error_code_t code) {
    if (code > ERROR_CRITICAL_FAILURE) {
        return -1;
    }
    return error_system.error_counts[code];
}

void error_print_statistics(void) {
    uart_puts("=== Error Statistics ===\n");
    uart_puts("Total errors: ");
    uart_put_dec(error_system.total_errors);
    uart_puts("\n");
    
    for (int i = 1; i <= ERROR_CRITICAL_FAILURE; i++) {
        if (error_system.error_counts[i] > 0) {
            uart_puts("  ");
            uart_put_dec(i);
            uart_puts(": ");
            uart_puts(error_get_description(i));
            uart_puts(" (");
            uart_put_dec(error_system.error_counts[i]);
            uart_puts(")\n");
        }
    }
    uart_puts("========================\n");
}

// === 重试机制 ===
int error_retry_operation(retryable_operation_t operation, void *context,
                         const struct retry_config *config) {
    int attempts = 0;
    int delay = config->delay_ms;
    
    while (attempts < config->max_attempts) {
        int result = operation(context);
        
        if (result == 0) {
            if (attempts > 0) {
                uart_puts("Retry succeeded after ");
                uart_put_dec(attempts);
                uart_puts(" attempts\n");
            }
            return 0; // 成功
        }
        
        attempts++;
        
        if (attempts < config->max_attempts) {
            uart_puts("Attempt ");
            uart_put_dec(attempts);
            uart_puts(" failed, retrying");
            if (delay > 0) {
                uart_puts(" in ");
                uart_put_dec(delay);
                uart_puts("ms");
            }
            uart_puts("...\n");
            
            // 简单延时 (在bootloader中我们没有精确的计时器)
            for (volatile int i = 0; i < delay * 1000; i++) {
                // 空循环延时
            }
            
            if (config->exponential_backoff) {
                delay *= 2;
            }
        }
    }
    
    uart_puts("Operation failed after ");
    uart_put_dec(config->max_attempts);
    uart_puts(" attempts\n");
    
    return -1;
}

// === 预定义错误处理器 ===

error_action_t default_error_handler(const struct error_info *error) {
    // 默认处理器：记录错误并继续
    uart_puts("[Default Handler] Error ");
    uart_put_dec(error->code);
    uart_puts(" handled\n");
    
    // 对于严重错误，中止操作
    if (error->code >= ERROR_SYSTEM_HALT) {
        return ERROR_ACTION_ABORT;
    }
    
    // 对于硬件错误，尝试重试
    if (error->code >= ERROR_HARDWARE_FAULT && error->code <= ERROR_INVALID_DEVICE) {
        return ERROR_ACTION_RETRY;
    }
    
    return ERROR_ACTION_CONTINUE;
}

error_action_t critical_error_handler(const struct error_info *error) {
    uart_puts("CRITICAL ERROR ");
    uart_put_dec(error->code);
    uart_puts(": ");
    uart_puts(error->message);
    uart_puts("\n");
    uart_puts("System will halt.\n");
    
    // 显示错误统计
    error_print_statistics();
    
    return ERROR_ACTION_ABORT;
}

error_action_t disk_error_handler(const struct error_info *error) {
    uart_puts("[Disk Handler] ");
    uart_puts(error->message);
    uart_puts("\n");
    
    // 磁盘错误通常可以重试
    int error_count = error_get_count(error->code);
    if (error_count < 3) {
        uart_puts("Attempting disk recovery...\n");
        return ERROR_ACTION_RETRY;
    } else {
        uart_puts("Disk error retry limit exceeded\n");
        return ERROR_ACTION_ABORT;
    }
}

// === 诊断和调试工具 ===
void error_dump_context(const struct error_info *error) {
    uart_puts("=== Error Context Dump ===\n");
    uart_puts("Error: ");
    uart_put_dec(error->code);
    uart_puts(" (");
    uart_puts(error->message);
    uart_puts(")\n");
    uart_puts("Location: ");
    uart_puts(error->function);
    uart_puts(":");
    uart_put_dec(error->line);
    uart_puts("\n");
    uart_puts("Context Data:\n");
    for (int i = 0; i < 4; i++) {
        if (error->context_data[i] != 0) {
            uart_puts("  ctx");
            uart_put_dec(i);
            uart_puts(": ");
            uart_put_hex(error->context_data[i]);
            uart_puts("\n");
        }
    }
    uart_puts("==========================\n");
}
