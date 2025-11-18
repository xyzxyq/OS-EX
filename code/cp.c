#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

// 定义一个缓冲区，用于数据的中转
// xv6系统中内存有限，512字节是一个安全且常用的大小
char buf[512];

// 定义选项模式
#define MODE_INTERACTIVE 0  // 交互式：覆盖前询问
#define MODE_FORCE       1  // 强制：直接覆盖，不询问
#define MODE_NOCLOBBER   2  // 不覆盖：如果目标存在则退出

int main(int argc, char *argv[])
{
    int fd_source, fd_target, n;
    struct stat st;
    char response[2];
    int mode = MODE_INTERACTIVE;  // 默认模式：交互式
    int arg_offset = 1;  // 参数偏移量，用于处理选项
    
    // --- 步骤 1: 解析命令行选项 ---
    if(argc >= 2 && argv[1][0] == '-') {
        // 检查选项
        if(strcmp(argv[1], "-f") == 0) {
            mode = MODE_FORCE;
            arg_offset = 2;
        } else if(strcmp(argv[1], "-i") == 0) {
            mode = MODE_INTERACTIVE;
            arg_offset = 2;
        } else if(strcmp(argv[1], "-n") == 0) {
            mode = MODE_NOCLOBBER;
            arg_offset = 2;
        } else {
            printf(1, "cp: 无效的选项 '%s'\n", argv[1]);
            printf(1, "用法: cp [-f|-i|-n] 源文件 目标文件\n");
            printf(1, "  -f  强制覆盖，不提示\n");
            printf(1, "  -i  覆盖前询问（默认）\n");
            printf(1, "  -n  不覆盖已存在的文件\n");
            exit();
        }
    }
    
    // --- 步骤 2: 检查命令行参数 ---
    // 需要有源文件和目标文件两个参数
    if(argc != arg_offset + 2) {
        printf(1, "用法: cp [-f|-i|-n] 源文件 目标文件\n");
        printf(1, "  -f  强制覆盖，不提示\n");
        printf(1, "  -i  覆盖前询问（默认）\n");
        printf(1, "  -n  不覆盖已存在的文件\n");
        exit();
    }
    
    char *source_file = argv[arg_offset];
    char *target_file = argv[arg_offset + 1];
    
    // --- 步骤 3: 打开源文件 ---
    // O_RDONLY 表示只读
    if((fd_source = open(source_file, O_RDONLY)) < 0) {
        printf(1, "cp: 无法打开源文件 %s\n", source_file);
        exit();
    }
    
    // --- 步骤 4: 检查目标文件是否存在，根据模式处理 ---
    if(stat(target_file, &st) == 0) {
        // 目标文件存在
        if(mode == MODE_NOCLOBBER) {
            // -n 模式：不覆盖，直接退出
            printf(1, "cp: 目标文件 '%s' 已存在，不覆盖（使用 -n 选项）\n", target_file);
            close(fd_source);
            exit();
        } else if(mode == MODE_INTERACTIVE) {
            // -i 模式（或默认）：询问用户
            printf(1, "cp: 目标文件 '%s' 已存在，是否覆盖? (y/n): ", target_file);
            
            // 从标准输入读取用户输入
            if(read(0, response, sizeof(response)) <= 0) {
                printf(1, "cp: 读取用户输入失败\n");
                close(fd_source);
                exit();
            }
            
            // 检查用户输入
            if(response[0] != 'y' && response[0] != 'Y') {
                printf(1, "cp: 操作已取消\n");
                close(fd_source);
                exit();
            }
            
            printf(1, "cp: 正在覆盖文件...\n");
        }
        // MODE_FORCE 模式：不做任何提示，直接继续
    }
    
    // --- 步骤 5: 删除目标文件（如果存在）---
    // 为了实现"完全覆盖"而不是"逐字覆盖"，
    // 我们在创建/打开目标文件之前，先尝试删除它。
    unlink(target_file);
    
    // --- 步骤 6: 创建/打开目标文件 ---
    // O_CREATE: 若不存在则创建 | O_WRONLY: 只写
    if((fd_target = open(target_file, O_CREATE | O_WRONLY)) < 0) {
        printf(1, "cp: 无法创建目标文件 %s\n", target_file);
        close(fd_source);
        exit();
    }
    
    // --- 步骤 7: 循环读写 ---
    // read(文件描述符, 缓冲区, 读取字节数)
    // 当read返回的 n > 0 时，表示成功读取了n个字节
    while((n = read(fd_source, buf, sizeof(buf))) > 0) {
        // 将从源文件读到的n个字节，从缓冲区写入目标文件
        // 如果write返回的字节数不等于我们要求写入的字节数n，说明发生了错误
        if(write(fd_target, buf, n) != n) {
            printf(1, "cp: 写入目标文件 %s 时发生错误\n", target_file);
            close(fd_source);
            close(fd_target);
            exit();
        }
    }
    
    // 检查循环退出是否因为读取错误 (read 返回 -1)
    if(n < 0) {
        printf(1, "cp: 读取源文件 %s 时发生错误\n", source_file);
    }
    
    // --- 步骤 8: 关闭文件 ---
    // 完成所有操作后，关闭两个文件描述符以释放资源
    close(fd_source);
    close(fd_target);
    
    // --- 步骤 9: 成功退出 ---
    exit();
}
