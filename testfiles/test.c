#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include<unistd.h>
const int micro = 1e6;
int main(int argc, char* argv[]) {
    printf("Not sleeping now\n");
    printf("sleeping...\n");
    usleep(2*micro);
    printf("awake: sleep done\n");
}
