#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_BUF_LEN 1024
#define MAX_PROCESS 10
#define SPACE_DELIM " \t"
#define PIPE_DELIM "|"
#define RED "\033[0;31m"
#define GREEN "\x1b[32m"
#define BLUE "\x1b[34m"
#define WHITE "\x1b[37m"
#define CYAN "\x1B[36m"
#define MAGENTA "\x1B[35m"
#define YELLOW "\x1B[33m"

/* --------------------------*/
struct Process {
    char* cmdline;
    char cmd[MAX_BUF_LEN];
    char args[MAX_BUF_LEN][MAX_BUF_LEN];
    int nargs;
} procs[MAX_PROCESS];

int nProcess;
char userName[MAX_BUF_LEN];
char hostName[MAX_BUF_LEN];
char cmdline[MAX_BUF_LEN];
char cwd[MAX_BUF_LEN];
char** envVars;
FILE* hist;
int exitCode;

void printIntro() {
    exitCode = 0;
    fprintf(stdout, "\t\t\tWelcome to " MAGENTA "SKP-Shell\t\t\t\n\n" WHITE);
}
void printPrompt() {
    int errUser = getlogin_r(userName, sizeof(userName));
    int errHost = gethostname(hostName, sizeof(hostName));
    getcwd(cwd, sizeof(cwd));
    if (errUser) {
        char* tmp = getenv("USER");
        if (!tmp)
            strcpy(userName, "nouser");
        else {
            strncpy(userName, tmp, strlen(tmp) + 1);
        }
    }
    if (!cwd) strcpy(cwd, "~");
    char promptSymbol = '$';
    if (errHost) strcpy(hostName, "");
    if (exitCode == 0)
        fprintf(stdout,
                MAGENTA "%s@%s " YELLOW CYAN "|" YELLOW " %s" GREEN" :)\n" WHITE,
                userName, hostName, cwd);
    else
        fprintf(stdout,
                MAGENTA "%s@%s " YELLOW CYAN "|" YELLOW " %s" RED " :(\n" WHITE,
                userName, hostName, cwd);
    fprintf(stdout, CYAN "%c " WHITE, promptSymbol);
}
void trim(char s[]) {
    char* p = s;
    int l = strlen(p);
    while (isspace(p[l - 1])) p[--l] = 0;
    while (*p && isspace(*p)) ++p, --l;
    memmove(s, p, l + 1);
}
void removeQuotes(char s[]) {
    char* p = s;
    int l = strlen(p);
    while (l > 0 && p[l - 1] == '"' || p[l - 1] == '\'') p[--l] = '\0';
    while (l > 0 && *p == '"' || *p == '\'') ++p, --l;
    memmove(s, p, l + 1);
}
void makeProcess(struct Process* P, char s[]) {
    char* tmp = malloc(strlen(s) + 1);
    strcpy(tmp, s);
    trim(tmp);
    if (strcmp(tmp, "") == 0) {
        strcpy(P->cmd, "");
        return;
    }
    P->nargs = 0;
    while ((tmp = strtok(tmp, SPACE_DELIM)) != NULL) {
        strcpy(P->args[P->nargs++], tmp);
        tmp = NULL;
    }
    strcpy(P->cmd, P->args[0]);
}

void processInput(char s[]) {
    char* tmp = malloc(strlen(s) + 1);
    strcpy(tmp, s);
    trim(tmp);
    if (strcmp(tmp, "") == 0) {
        return;
    }
    nProcess = 0;
    while ((tmp = strtok(tmp, PIPE_DELIM)) != NULL) {
        procs[nProcess++].cmdline = tmp;
        tmp = NULL;
    }
    for (int i = 0; i < nProcess; i++) {
        makeProcess(&procs[i], procs[i].cmdline);
        for (int j = 0; j < procs[i].nargs; j++) {
            removeQuotes(procs[i].args[j]);
        }
    }
}
int toInt(char s[]) {
    int res = 0;
    for (int i = 0; s[i] != '\0'; i++) res = 10 * res + s[i] - '0';
    return res;
}
void updateHistory(char s[]) {
    if (strcmp(s, "") == 0) {
        return;
    }
    hist = fopen("/home/sumit/Documents/OS/Lab1/Part2/SKP-Shell/history.txt",
                 "a+");
    if (hist == NULL) {
        fprintf(stderr, "error opening history file\n");
        return;
    }
    fprintf(hist, "%s\n", cmdline);
    fclose(hist);
}

int execProcess(struct Process* P) {
    if (strcmp(P->cmd, "clear") == 0) {
        if (P->nargs > 1) {
            fprintf(stderr, "unknown option '%s'\n", P->args[1]);
            return 1;
        }
        system("clear");
    } else if (strcmp(P->cmd, "env") == 0) {
        for (int i = 0; *(envVars + i) != NULL; i++)
            fprintf(stdout, "\n%s", *(envVars + i));

    } else if (strcmp(P->cmd, "cd") == 0) {
        if (P->nargs < 2) {
            fprintf(stderr, "%s: too few arguments\n", P->cmd);
            return 1;
        } else if (P->nargs > 2) {
            fprintf(stderr, "%s: too many arguments\n", P->cmd);
            return 1;
        }
        int error = chdir(P->args[1]);
        if (error) {
            fprintf(stderr, "%s: no such file or directory: %s\n", P->cmd,
                    P->args[1]);
            return 1;
        }
    } else if (strcmp(P->cmd, "pwd") == 0) {
        if (P->nargs > 1) {
            fprintf(stderr, "%s: too many arguments\n", P->cmd);
            return 1;
        }
        char curDirectory[MAX_BUF_LEN];
        getcwd(curDirectory, sizeof(curDirectory));
        if (!cwd) {
            fprintf(stderr, "%s: error reterieving path\n", P->cmd);
            return 1;
        }
        fprintf(stdout, "%s\n", curDirectory);
    } else if (strcmp(P->cmd, "mkdir") == 0) {
        if (P->nargs < 2) {
            fprintf(stderr, "%s: missing operands\n", P->cmd);
            return 1;
        }
        for (int i = 1; i < P->nargs; i++) {
            int result = mkdir(P->args[i], 0777);
            if (result == -1) {
                fprintf(stderr,
                        "%s: cannot create directory '%s': File exists or "
                        "permission denied\n",
                        P->cmd, P->args[i]);
            }
        }
    } else if (strcmp(P->cmd, "rmdir") == 0) {
        if (P->nargs < 2) {
            fprintf(stderr, "%s: missing operands\n", P->cmd);
            return 1;
        }
        for (int i = 1; i < P->nargs; i++) {
            int result = rmdir(P->args[i]);
            if (result == -1) {
                fprintf(stderr,
                        "%s: failed to remove '%s': No such file or directory "
                        "or permission denied\n",
                        P->cmd, P->args[i]);
            }
        }
    } else if (strcmp(P->cmd, "ls") == 0) {
        if (P->nargs == 1) strcpy(P->args[P->nargs++], ".");
        char dirPath[MAX_BUF_LEN];
        int lOption = 0;
        if (strcmp(P->args[1], "-l") == 0) {
            lOption = 1;
            if (P->nargs == 2)
                strcpy(dirPath, ".");
            else
                strcpy(dirPath, P->args[2]);
        } else {
            strcpy(dirPath, P->args[1]);
        }

        DIR* dir = opendir(dirPath);
        if (dir == NULL) {
            fprintf(stderr,
                    "%s: cannot access '%s': No such file or directory\n",
                    P->cmd, dirPath);
            return 1;
        }
        struct dirent* entity;

        while ((entity = readdir(dir)) != NULL) {
            if (entity->d_name[0] == '.') continue;
            if (entity->d_type == DT_DIR)
                fprintf(stdout, BLUE "%s\n" WHITE, entity->d_name);
            else
                fprintf(stdout, GREEN "%s\n" WHITE, entity->d_name);
        }
        closedir(dir);
    } else if (strcmp(P->cmd, "history") == 0) {
        if (P->nargs > 2) {
            fprintf(stderr, "%s: too many arguments\n", P->cmd);
            return 1;
        }
        hist = fopen(
            "/home/sumit/Documents/OS/Lab1/Part2/SKP-Shell/history.txt", "a+");
        if (hist == NULL) {
            fprintf(stderr, "error opening history file\n");
            return 1;
        }
        char line[MAX_BUF_LEN];
        int numLines = P->nargs == 2 ? toInt(P->args[1]) : -1;
        int lineNum = 1;
        while (fgets(line, MAX_BUF_LEN, hist) && numLines-- != 0) {
            fprintf(stdout, "%d %s", lineNum++, line);
        }
        fclose(hist);
    } else if (strcmp(P->cmd, "exit") == 0) {
        if (P->nargs > 1) {
            fprintf(stderr, "%s: too many arguments\n", P->cmd);
            return 1;
        }
        exit(0);
    } else {
        char* tmpargs[P->nargs + 1];
        for (int i = 0; i < P->nargs; i++) {
            tmpargs[i] = P->args[i];
        }
        tmpargs[P->nargs] = NULL;
        int status = execvp(P->cmd, tmpargs);
        if (status == -1) {
            fprintf(stderr, "Error executing process\n\n");
            exit(1);
        }
    }

    return 0;
}
int initProcess(struct Process* P, int inFd, int outFd) {}
void closeAllFileDescriptors(int nPipes, int fd[][2]) {
    for (int i = 0; i < nPipes; i++) {
        close(fd[i][0]);
        close(fd[i][1]);
    }
}
int execCommands() {
    if(strcmp(procs[0].cmd,"exit")==0){
        exit(0);
    }
    int status;
    // creating the necessary pipes
    // creating minimum of 1 pipe to make the code clean.
    int nPipes = nProcess == 1 ? 1 : nProcess - 1;

    int fd[nPipes][2];
    for (int p = 0; p < nPipes; p++) {
        if (pipe(fd[p]) == -1) {
            fprintf(stderr, "error creating pipe\n");
            return 1;
        }
    }
    for (int i = 0; i < nProcess; i++) {
        int pid = fork();
        if (pid == -1) {
            fprintf(stderr, "error forking child for '%s'\n", procs[i].cmd);
            break;
        } else if (pid == 0) {
            // overwriting the reading and writing end of the current pipe
            // and closing redundant file descriptors
            if (i != 0) dup2(fd[i - 1][0], STDIN_FILENO);
            if (i != nProcess - 1) dup2(fd[i][1], STDOUT_FILENO);
            closeAllFileDescriptors(nPipes, fd);
            int status = execProcess(&procs[i]);
            exit(status);
        }
        if (i != 0) {
            close(fd[i - 1][0]);
        }
        if (i != nProcess - 1) {
            close(fd[i][1]);
        }
        waitpid(pid, &status, 0);
        if (status != 0) {
            break;
        }
    }

    return status;
}

int main(int argc, char* argv[], char* envp[]) {
    printIntro();
    char* tmp;
    envVars = envp;
    do {
        printPrompt();
        tmp = fgets(cmdline, sizeof(cmdline), stdin);
        processInput(cmdline);
        updateHistory(cmdline);
        exitCode = execCommands();
        fprintf(stdout, "\n");
    } while (tmp);
}