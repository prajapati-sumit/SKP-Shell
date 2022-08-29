/*
 *	Author	: Sumit Kumar Prajapati
 *	E-mail 	: prajapati.3@iitj.ac.in
 *	Roll no.: B20CS074
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
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
FILE* readStream;
int exitCode;
int batchMode;
int backgroundMode;
char* historyPath;

void init() {
    exitCode = 0;
    if ((historyPath = getenv("HOME")) == NULL) {
        historyPath = getpwuid(getuid())->pw_dir;
    }
    strcat(historyPath, "/.skp_history");
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
                MAGENTA "%s@%s " YELLOW CYAN "|" YELLOW " %s" GREEN
                        " :)\n" WHITE,
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
    if (tmp[strlen(tmp) - 1] == '&') {
        backgroundMode = 1;
        tmp[strlen(tmp) - 1] = '\0';
    } else
        backgroundMode = 0;
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

    hist = fopen(historyPath, "a+");
    if (hist == NULL) {
        fprintf(stderr, "error opening history file\n");
        return;
    }
    fprintf(hist, "%s\n", cmdline);
    fclose(hist);
}
int execInteralCommands(struct Process* P) {
    if (strcmp(P->cmd, "clear") == 0) {
        if (P->nargs > 1) {
            fprintf(stderr, "unknown option '%s'\n", P->args[1]);
            return 1;
        }
        system("clear");
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
    } else if (strcmp(P->cmd, "exit") == 0) {
        if (P->nargs > 1) {
            fprintf(stderr, "%s: too many arguments\n", P->cmd);
            return 1;
        }
        exit(0);
    } else
        return -1;
    return 0;
}

int execProcess(struct Process* P) {
    if (strcmp(P->cmd, "env") == 0) {
        for (int i = 0; *(envVars + i) != NULL; i++)
            fprintf(stdout, "\n%s", *(envVars + i));
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
        char dirPath[MAX_BUF_LEN];
        int lOption = 0;
        if (P->nargs == 1) {
            strcpy(dirPath, ".");
        } else if (strcmp(P->args[1], "-l") == 0) {
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
        struct stat mystat;
        while ((entity = readdir(dir)) != NULL) {
            if (entity->d_name[0] == '.') continue;
            if (lOption) {
                char buff[100000];
                sprintf(buff, "%s/%s", dirPath, entity->d_name);
                stat(buff, &mystat);
                printf("%c%c%c%c%c%c%c%c%c%c",
                       S_ISDIR(mystat.st_mode) ? 'd' : '-',
                       (mystat.st_mode & S_IRUSR) ? 'r' : '-',
                       (mystat.st_mode & S_IWUSR) ? 'w' : '-',
                       (mystat.st_mode & S_IXUSR) ? 'x' : '-',
                       (mystat.st_mode & S_IRGRP) ? 'r' : '-',
                       (mystat.st_mode & S_IWGRP) ? 'w' : '-',
                       (mystat.st_mode & S_IXGRP) ? 'x' : '-',
                       (mystat.st_mode & S_IROTH) ? 'r' : '-',
                       (mystat.st_mode & S_IWOTH) ? 'w' : '-',
                       (mystat.st_mode & S_IXOTH) ? 'x' : '-');
                struct passwd* pw = getpwuid(mystat.st_uid);
                struct group* gr = getgrgid(mystat.st_gid);
                char date[100];
                strftime(date, 50, "%b  %d  %I: %M",
                         gmtime(&(mystat.st_ctime)));
                printf(" %5d %10s %10s %10lld  %10s  %s\n",
                       (int)mystat.st_nlink, pw->pw_name, gr->gr_name,
                       (long long int)mystat.st_size, date, entity->d_name);
            } else {
                if (entity->d_type == DT_DIR)
                    fprintf(stdout, BLUE "%s\n" WHITE, entity->d_name);
                else
                    fprintf(stdout, GREEN "%s\n" WHITE, entity->d_name);
            }
        }
        closedir(dir);
    } else if (strcmp(P->cmd, "history") == 0) {
        if (P->nargs > 2) {
            fprintf(stderr, "%s: too many arguments\n", P->cmd);
            return 1;
        }
        hist = fopen(historyPath, "a+");
        if (hist == NULL) {
            fprintf(stderr, "error opening history file\n");
            return 1;
        }
        char line[MAX_BUF_LEN];
        int lineNum = 0;
        char histories[MAX_BUF_LEN][MAX_BUF_LEN];
        while (fgets(line, MAX_BUF_LEN, hist)) {
            strcpy(histories[lineNum++], line);
        }
        int numLines = P->nargs == 2 ? toInt(P->args[1]) : lineNum;
        for (int i = (lineNum - numLines >= 0 ? lineNum - numLines : 0);
             i < lineNum; i++)
            fprintf(stdout, "%d %s", i + 1, histories[i]);

        fclose(hist);
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
void closeAllFileDescriptors(int nPipes, int fd[][2]) {
    for (int i = 0; i < nPipes; i++) {
        close(fd[i][0]);
        close(fd[i][1]);
    }
}
int execCommands() {
    if (strcmp(cmdline, "") == 0) return 0;
    int internalCommandStatus = execInteralCommands(&procs[0]);
    if (internalCommandStatus != -1) {
        return internalCommandStatus;
    }
    int status = 0;
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
        } else {
            if (i != 0) {
                close(fd[i - 1][0]);
            }
            if (i != nProcess - 1) {
                close(fd[i][1]);
            }
            if (!backgroundMode) waitpid(pid, &status, 0);
            if (status != 0) {
                break;
            }
        }
    }

    return status;
}

int main(int argc, char* argv[], char* envp[]) {
    init();
    if (argc == 1) {
        batchMode = 0;
        readStream = stdin;
    }
    if (argc == 2) {
        batchMode = 1;
        readStream = fopen(argv[1], "r+");
        if (readStream == NULL) {
            fprintf(stderr, "error opening '%s'\n", argv[1]);
            return 1;
        }
    } else if (argc > 2) {
        fprintf(stderr, "too many arguments for batch mode.\n");
        return 1;
    }
    char* tmp;
    envVars = envp;
    char buff[MAX_BUF_LEN];
    printPrompt();
    while (fgets(buff, MAX_BUF_LEN, readStream)) {
        buff[strcspn(buff, "\n")] = 0;
        strcpy(cmdline, buff);
        processInput(cmdline);
        exitCode = execCommands();
        fprintf(stdout, "\n");
        updateHistory(cmdline);
        printPrompt();
    }
    if (batchMode) fclose(readStream);
}