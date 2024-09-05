#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <fcntl.h>

int main(){
    FILE *fp = fopen("lock.txt", "w");
    // error handling
    if(fp == NULL){
        perror("fopen");
        printf("Error opening file\n");   
    }
    fprintf(fp, "Hellofege World\n");
    while(1);
}