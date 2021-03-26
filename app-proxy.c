// forceful injector of libintercept.so

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

int main(int argc, char *argv[])
{
    char conf_path_default[] = "./app_proxy.conf";

    char *conf_path = getenv("APP_PROXY_CONFIG");
    if (!conf_path) {
        conf_path = conf_path_default;
    }

    if (argc < 2) {
        printf("%s - proxy for exec-family syscalls\n", argv[0]);
        printf("Run it like this: %s some-program\n", argv[0]);
        printf("Config file in use is %s which is defined by env var APP_PROXY_CONFIG\n", conf_path);
        FILE *f;
        if ((f = fopen(conf_path, "r"))) {
            printf("Config file exists and can be opened\n");
            fclose(f);
        }
        else {
            fprintf(stderr, "Warning: config file doesn't exist or can't be read, so %s will do nothing\n", argv[0]);
        }
        return 0;
    }

    printf("Trying to run via exec:");
    for(int i=1; i<argc; i++) {
        printf(" %s", argv[i]);
    }
    printf("\n");

    
    //char *envp[] = { "HELLO=hello, env", NULL};

    // use this
    if (-1 == execvp(argv[1], &argv[1])) {
        perror("Wasn't able to run the program specified");
        return 1;
    }
    
    return 0;
}
