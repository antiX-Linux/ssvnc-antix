#include <stdio.h>
#include <stdlib.h>

extern int rfbEncryptAndStorePasswd(char *, char*);

static void usage(void)
{
    printf("\nusage:  vncstorepw <password> <filename>\n\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    if (argc != 3) 
        usage();

    if (rfbEncryptAndStorePasswd(argv[1], argv[2]) != 0) {
        printf("storing password failed.\n");
        return 1;
    } else {
        printf("storing password succeeded.\n");
        return 0;
    }
}
