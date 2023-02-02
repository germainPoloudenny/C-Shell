typedef struct _job{
    char* command;
    pid_t pid;
    int id;
    int exitCode;
    struct _job* next;
    bool foreground;
    bool stopped;
} job;
job* jobHead;
void initJobHead();
void addJob(char** command, int nbArg, pid_t pid, bool foreground, bool stopped);
job* getJobByPid(pid_t pid);
void delJobByPid(pid_t pid);
pid_t switchJob(pid_t pid, bool foreground);
void clearJobs();
int getNbJob();
void myjobs();
void killAll();