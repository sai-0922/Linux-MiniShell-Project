#include "delep.hpp"

void delep(char *argpath, int fd)
{   
    DIR *dirp = opendir("/proc");
    if (!dirp) {
        perror("opendir");
        exit(1);
    }

    string msgPIDS = "";

    struct dirent *entry;
    char path[1024];
    while ((entry = readdir(dirp)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;
        if (entry->d_type != DT_DIR)
            continue;

        sprintf(path, "/proc/%s/fd", entry->d_name);
        DIR *fd_dirp = opendir(path);
        if (!fd_dirp)
            continue;

        struct dirent *fd_entry;
        char link[1024];
        while ((fd_entry = readdir(fd_dirp)) != NULL) {
            if (!strcmp(fd_entry->d_name, ".") || !strcmp(fd_entry->d_name, ".."))
                continue;

            sprintf(link, "/proc/%s/fd/%s", entry->d_name, fd_entry->d_name);
            char buf[1024];
            
            ssize_t len = readlink(link, buf, sizeof buf);
            if (len == -1)
                continue;
            buf[len] = '\0';
            
            if (!strcmp(buf, argpath)){
                char path[1024];
                sprintf(path, "/proc/%s/fdinfo/%s", entry->d_name, fd_entry->d_name);
                FILE *fptemp = fopen(path, "r");
                char line[1024];
                bool lock = false;
                while(fgets(line, sizeof(line), fptemp)){
                    if(strcmp(strtok(line, ":"), "lock")==0){
                        lock = true;
                    }
                }
                if(lock) 
                    msgPIDS += "Lock:" + string(entry->d_name) + ",";
                else
                    msgPIDS += "NoLock:" + string(entry->d_name) + ",";
                fclose(fptemp);
            }
        }
        closedir(fd_dirp);
    }
    closedir(dirp);
    write(fd, msgPIDS.c_str(), msgPIDS.length()+1);
}