#include "squashbug.hpp"

int main()
{   
    while(1)
    {
        for(int i=0; i<NUM_CHILD; i++)
        {
            pid_t pid = fork();
            if(pid == 0)
            {
                for(int j=0; j<NUM_CHILD_CHILD; j++)
                {
                    pid_t pid2 = fork();
                    if(pid2==0)
                    {
                        while(1);
                        return 0;
                    }
                }
                while(1);
                return 0;
            }
        }
        sleep(SLEEP_DUR_MIN*60);
    }
}