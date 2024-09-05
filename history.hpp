#ifndef __HISTORY_HPP
#define __HISTORY_HPP

#include <readline/readline.h>
#include <readline/history.h>
#include <deque>
#include <string>

#define HISTORY_FILE ".history"
#define MAX_SIZE 1000

using namespace std;

class history
{
private:
    deque<string> dequeue;
    int max_size;
    FILE *fp;

public:
    history();
    int curr_ind;
    int get_size();
    bool isempty();
    void add_history(string);
    void decrement_history();
    void increment_history();
    string get_curr();
    ~history();
};

#endif