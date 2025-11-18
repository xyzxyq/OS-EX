/*******************************************************************************

  @file         lsh.c

  @author       Stephen Brennan, Nicola Bicocchi
                (Completed by your OS expert assistant)

*******************************************************************************/

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define LSH_RL_BUFSIZE 1024
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"

#define EXEC   100
#define PIPE   101
#define REDIN  102
#define REDOUT 103

/**
  @brief struct describing commands to be executed
*/
struct cmd {
  int type;
  char *left[LSH_TOK_BUFSIZE];
  char *right[LSH_TOK_BUFSIZE];
};

/**
  @brief Builtin functions
*/
int lsh_cd(struct cmd *cmd);
int lsh_pwd(struct cmd *cmd);
int lsh_help(struct cmd *cmd);
int lsh_exit(struct cmd *cmd);

char *builtin_str[] = {
  "cd",
  "pwd",
  "help",
  "exit"
};

int (*builtin_func[]) (struct cmd *) = {
  &lsh_cd,
  &lsh_pwd,
  &lsh_help,
  &lsh_exit
};

int lsh_num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}

// 维护 lsh 的“当前路径”字符串（仅用于 pwd 展示）
#define LSH_PATH_MAX 128
static char lsh_cwd[LSH_PATH_MAX] = "/";

static void lsh_join_paths(const char *base, const char *add, char *out) {
  int i = 0;
  if (add[0] == '/') {
    while (add[i] && i < LSH_PATH_MAX - 1) {
      out[i] = add[i];
      i++;
    }
    out[i] = 0;
    return;
  }
  int j = 0;
  if (base[0] != 0) {
    while (base[j] && i < LSH_PATH_MAX - 1) {
      out[i++] = base[j++];
    }
  }
  if (i == 0) {
    out[i++] = '/';
  } else {
    if (out[i-1] != '/' && i < LSH_PATH_MAX - 1)
      out[i++] = '/';
  }
  j = 0;
  while (add[j] && i < LSH_PATH_MAX - 1) {
    out[i++] = add[j++];
  }
  out[i] = 0;
}

static void lsh_normalize_path(const char *base, const char *input, char *out) {
  char temp[LSH_PATH_MAX];
  char pathbuf[LSH_PATH_MAX];
  char *segs[64];
  int segc = 0;

  lsh_join_paths(base, input, temp);

  int k = 0;
  while (temp[k] && k < LSH_PATH_MAX - 1) {
    pathbuf[k] = temp[k];
    k++;
  }
  pathbuf[k] = 0;

  int i = 0;
  while (pathbuf[i]) {
    while (pathbuf[i] == '/') i++;
    if (pathbuf[i] == 0) break;
    int start = i;
    while (pathbuf[i] && pathbuf[i] != '/') i++;
    int end = i;
    pathbuf[end] = 0;
    if (pathbuf[start] == 0) continue;
    if (strcmp(&pathbuf[start], ".") == 0) {
      // skip
    } else if (strcmp(&pathbuf[start], "..") == 0) {
      if (segc > 0) segc--;
    } else {
      if (segc < 64) segs[segc++] = &pathbuf[start];
    }
  }

  int pos = 0;
  if (segc == 0) {
    out[0] = '/';
    out[1] = 0;
    return;
  }
  for (i = 0; i < segc; i++) {
    if (pos < LSH_PATH_MAX - 1) out[pos++] = '/';
    char *s = segs[i];
    while (*s && pos < LSH_PATH_MAX - 1) out[pos++] = *s++;
  }
  out[pos] = 0;
}

// 在 xv6 中没有 PATH 搜索；为了支持从任意目录运行根目录下的程序，
// 当命令名中不含 '/' 时，先尝试当前目录，再回退尝试以 '/' 前缀。
static void lsh_try_exec(char *file, char **argv) {
  if (strchr(file, '/') == 0) {
    exec(file, argv);
    // fallback: 尝试从根目录执行
    char buf[128];
    buf[0] = '/';
    buf[1] = 0;
    strcpy(buf + 1, file);
    exec(buf, argv);
  } else {
    exec(file, argv);
  }
}

/**
  @brief Bultin command: change directory.
  @param cmd Parsed command struct.
  @return 1 on success.
 */
int lsh_cd(struct cmd *cmd) {
  if (cmd->left[1] == 0) {
    printf(2, "lsh: expected argument to \"cd\"\n");
  } else {
    // Change directory using chdir()
    if (chdir(cmd->left[1]) < 0) {
      printf(2, "lsh: cannot change directory to %s\n", cmd->left[1]);
    } else {
      // 成功后更新路径字符串（仅用于 pwd 展示）
      lsh_normalize_path(lsh_cwd, cmd->left[1], lsh_cwd);
    }
  }
  return 1;
}

int lsh_pwd(struct cmd *cmd) {
  printf(1, "%s\n", lsh_cwd);
  return 1;
}

int lsh_help(struct cmd *cmd) {
  printf(1, "xv6 LSH\n");
  printf(1, "Type program names and arguments, and hit enter.\n");
  printf(1, "The following are built in:\n");

  for (int i = 0; i < lsh_num_builtins(); i++) {
    printf(1, "  %s\n", builtin_str[i]);
  }
  return 1;
}

/**
  @brief Bultin command: exit.
  @param cmd Parsed command struct.
  @return 0 on exit.
 */
int lsh_exit(struct cmd *cmd) {
  exit(); // 退出程序
  return 0; // 这行其实不会被执行
}

/**
  @brief Launch a program and wait for it to terminate.
  @param args Null terminated list of arguments (including program).
  @return Always returns 1
 */

int lsh_execute(struct cmd *cmd) {
  int fd, p[2];

  // 检查内建命令
  for (int i = 0; i < lsh_num_builtins(); i++) {
    if (strcmp(cmd->left[0], builtin_str[i]) == 0) {
      return (*builtin_func[i])(cmd);
    }
  }

  // 统一由子进程执行外部命令（不在父进程直接 exec 替换 shell）

  switch (cmd->type) {
    case EXEC:
      if (fork() == 0) {
        lsh_try_exec(cmd->left[0], cmd->left);
        printf(2, "lsh: exec %s 失败\n", cmd->left[0]);
        exit();
      }
      wait();
      break;

    case REDIN: // 例如: cat < file.txt
      if (fork() == 0) {
        // --- 子进程 ---
        close(0); // 关闭标准输入
        
        // 打开文件 (只读)
        if ((fd = open(cmd->right[0], O_RDONLY)) < 0) {
          printf(2, "lsh: 无法打开文件 %s\n", cmd->right[0]);
          exit();
        }
        // 现在 fd 是 0
        
        // 执行命令
        lsh_try_exec(cmd->left[0], cmd->left);
        printf(2, "lsh: exec %s 失败\n", cmd->left[0]);
        exit();
      }
      wait(); // 父进程等待子进程结束
      break;

    case REDOUT: // 例如: ls > file.txt
      if (fork() == 0) {
        // --- 子进程 ---
        close(1); // 关闭标准输出
        
        // 打开/创建文件 (只写)
        if ((fd = open(cmd->right[0], O_WRONLY | O_CREATE)) < 0) {
          printf(2, "lsh: 无法创建文件 %s\n", cmd->right[0]);
          exit();
        }
        // fd 此时是 1
        
        // 执行命令
        lsh_try_exec(cmd->left[0], cmd->left);
        printf(2, "lsh: exec %s 失败\n", cmd->left[0]);
        exit();
      }
      wait(); // 父进程等待子进程结束
      break;

    case PIPE: // 例如: ls | cat
      // 创建管道
      if (pipe(p) < 0) {
        printf(2, "lsh: 管道创建失败\n");
        break;
      }

      // fork 子进程1 (左侧命令，写入管道)
      if (fork() == 0) {
        close(1); // 关闭标准输出
        dup(p[1]); // 将标准输出重定向到管道写端
        
        // 关闭两个管道描述符
        close(p[0]); 
        close(p[1]);
        
        lsh_try_exec(cmd->left[0], cmd->left);
        printf(2, "lsh: exec %s 失败\n", cmd->left[0]);
        exit();
      }

      // fork 子进程2 (右侧命令，读取管道)
      if (fork() == 0) {
        close(0); // 关闭标准输入
        dup(p[0]);  // 将标准输入重定向到管道读端
        
        // 关闭两个管道描述符
        close(p[0]);
        close(p[1]);
        
        lsh_try_exec(cmd->right[0], cmd->right);
        printf(2, "lsh: exec %s 失败\n", cmd->right[0]);
        exit();
      }

      // 父进程
      close(p[0]); // 必须关闭两个管道口
      close(p[1]);
      wait(); // 等待子进程1
      wait(); // 等待子进程2
      break;
  }
  return 1;
}


/* @brief Count how many of the leading characters there are in "string"
  	 before there's one that's also in "chars".
  @return The return value is the index of the first character in "string"
          that is also in "chars".  If there is no such character, then
          the return value is the length of "string".
*/
int strcspn(char *string, char *chars) {
    register char c, *p, *s;

    for (s = string, c = *s; c != 0; s++, c = *s) {
        for (p = chars; *p != 0; p++) {
            if (c == *p) {
                return s - string;  // Found a character from "chars" in "string"
            }
        }
    }
    return s - string;  // No character from "chars" found
}

/**
   @brief Split a string up into tokens
   @return If the first argument is non-NULL then a pointer to the
           first token in the string is returned.  Otherwise the
           next token of the previous string is returned.  If there
           are no more tokens, NULL is returned.
*/
char *strtok(char *s, char *delim) {
    static char *lasts;
    register int ch;

    if (s == 0)
	s = lasts;
    do {
	if ((ch = *s++) == '\0')
	    return 0;
    } while (strchr(delim, ch));
    --s;
    lasts = s + strcspn(s, delim);
    if (*lasts != 0)
	*lasts++ = 0;
    return s;
}

/**
   @brief Main entry point.
   @param argc Argument count.
   @param argv Argument vector.
   @return status code
 */
int main(int argc, char **argv) {
  int left_i, right_i;
  char *line;  
  struct cmd *cmd;
  
  line = (char *)malloc(LSH_RL_BUFSIZE * sizeof(char));
  cmd = (struct cmd *)malloc(sizeof(struct cmd));

  do {
    printf(1, "lsh> ");

    // clear buffers and get a new line
    memset(line, 0, LSH_RL_BUFSIZE * sizeof(char));
    memset(cmd, 0, sizeof(struct cmd));
    gets(line, LSH_RL_BUFSIZE);

    // fill struct cmd and splits args between left and right (if symbols >,<,| are detected)
    left_i = right_i = 0;
    cmd->type = EXEC;
  {
    int parsingRight = 0;        // 0: 填充左侧命令参数，1: 管道右侧命令参数
    int expectRedirFile = 0;     // 期待一个重定向文件名

    for (char *token = strtok(line, LSH_TOK_DELIM); token != 0; token = strtok(0, LSH_TOK_DELIM)) {
      if (strcmp(token, "<") == 0) {
        cmd->type = REDIN;
        expectRedirFile = 1;     // 下一项应为输入重定向文件
        continue;
      }
      if (strcmp(token, ">") == 0) {
        cmd->type = REDOUT;
        expectRedirFile = 1;     // 下一项应为输出重定向文件
        continue;
      }
      if (strcmp(token, "|") == 0) {
        cmd->type = PIPE;
        parsingRight = 1;        // 之后的 token 属于右侧命令
        continue;
      }

      if (expectRedirFile) {
        // 重定向目标文件只应进入 right[0]
        if (right_i == 0)
          cmd->right[right_i++] = token;
        expectRedirFile = 0;
        continue;
      }

      if (!parsingRight) {
        cmd->left[left_i++] = token;
      } else {
        cmd->right[right_i++] = token;
      }
    }
  }

    // check for faulty commands
    // (Adjusted logic: cmd->left[0] is always the first command)
    if (cmd->left[0] == 0) {
      continue; // Empty line
    }
    if ((cmd->type != EXEC) && (cmd->right[0] == 0)) {
      printf(2, "lsh: 语法错误：重定向或管道缺少目标\n");
      continue;
    }

    // execute
    lsh_execute(cmd);    
  } while (1);
}
