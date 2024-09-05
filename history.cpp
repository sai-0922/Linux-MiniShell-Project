#include "history.hpp"

history::history() : max_size(MAX_SIZE)
{
    fp = fopen(HISTORY_FILE, "r");
    if (fp != NULL)
    {
        char *line = NULL;
        size_t len = 0;
        ssize_t read;
        while ((read = getline(&line, &len, fp)) != -1)
        {
            if (get_size() == max_size)
                dequeue.pop_front();
            int linen = strlen(line);
            if(line[linen-1] == '\n')
                line[linen-1] = '\0';
            dequeue.push_back(line);
            free(line);
            line = (char *) NULL;
        }
        fclose(fp);
    }
    
    curr_ind = dequeue.size();
}

history::~history()
{   
    fp = fopen(HISTORY_FILE, "w");
    if (fp != NULL)
    {
        for (int i = 0; i < get_size(); i++)
        {
            fprintf(fp, "%s\n", dequeue[i].c_str());
        }
        fclose(fp);
    }
}

int history::get_size()
{
    return dequeue.size();
}

bool history::isempty()
{
    return dequeue.empty();
}

void history::add_history(string line)
{
    if (get_size() == max_size)
        dequeue.pop_front();
    dequeue.push_back(line);
    curr_ind = dequeue.size();
}

void history::decrement_history()
{
    if (curr_ind > 0)
        curr_ind--;
}

void history::increment_history()
{
    if (curr_ind < get_size())
        curr_ind++;
}

string history::get_curr()
{
    if (curr_ind == get_size())
        return "";
    return dequeue[curr_ind];
}
