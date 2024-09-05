#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

int main()
{	
    printf("PID: %d\n", getpid());
    FILE *fp = fopen("lock.txt", "w");
    int fd = fileno(fp);
    int ret = flock(fd, LOCK_EX);
    if (ret == 0)
    {
        printf("Lock created successfully\n");
        while(1);
        flock(fd, LOCK_UN);
    }
    else
    {
        printf("Lock creation failed\n");
    }
    return 0;
    
}