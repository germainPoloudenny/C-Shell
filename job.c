#include "utils.h"
#include "job.h"
#include <sys/wait.h>
#include <unistd.h>

int nbJob=0;

void initJobHead(){
    jobHead=malloc(sizeof(job));
    jobHead->next=NULL;
}

void addJob(char** command, int nbArg, pid_t pid, bool foreground, bool stopped){
    if(!foreground)
        setpgid(pid,pid);
    job* newJob= malloc(sizeof(job));
    newJob->command = malloc(1);
    newJob->command[0]='\0';
    int commandLength=0;
    for(int i=0;i<nbArg;i++){
        commandLength+=strlen(command[i]);
        newJob->command = realloc(newJob->command,commandLength+3);
        strcat(newJob->command,command[i]);
        strcat(newJob->command," \0");
    }
    newJob->pid=pid;
    newJob->exitCode=-1;
    newJob->next=NULL;
    job* currentJob = jobHead->next;
    job* prec=jobHead;
    newJob->id=1;
    newJob->stopped=stopped;
    newJob->foreground=foreground;
    while(currentJob !=  NULL){
        newJob->id=currentJob->id+1;
        prec=currentJob;
        currentJob=currentJob->next;
    }
    prec->next=newJob;
    nbJob++;
    if(stopped)
        printf("[%d] %s %s\n",newJob->id,"Stopped     ",newJob->command);
    else
        printf("[%d] %d\n",newJob->id,pid);

}

job* getJobByPid(pid_t pid){
    job* currentJob = jobHead->next;
    if(!currentJob)
        return NULL;
    while(currentJob != NULL ){
        if(currentJob->pid==pid){
            return currentJob;
        }
        currentJob=currentJob->next;
    }
    return NULL;
}

// on fait passer un job du foreground au background ou inversement
pid_t switchJob(pid_t pid, bool foreground){
    job* currentJob = jobHead->next;
    job* prec=jobHead;
    job* lastJobPrec=NULL;
    if(! currentJob)
        return ERR;
    while(currentJob != NULL ){
        if((currentJob->stopped || (currentJob->foreground && !foreground) || (!currentJob->foreground && foreground))  && pid==-1){
            lastJobPrec=prec;
        }
        else if (currentJob->pid==pid){
            if(currentJob->stopped || (currentJob->foreground && !foreground) || (!currentJob->foreground && foreground)){
                lastJobPrec=prec;
                break;
            }
            return  ERR;
        }
        prec=currentJob;
        currentJob=currentJob->next;
    }
    if(!lastJobPrec)
        return ERR;
    job* stoppedJob = lastJobPrec->next;
    if(!stoppedJob->foreground  && !foreground){
        fprintf(stderr,"[%d] Already in background\n",stoppedJob->id);
        return ERR;
    }
    stoppedJob->foreground = foreground;
    stoppedJob->next =  stoppedJob->next;
    pid =  stoppedJob->pid;
    return pid;
}

void delJobByPid(pid_t pid){
    job* currentJob = jobHead->next;
    job* prec=jobHead;
    if(!currentJob)
        return;
    while(currentJob){
        if(currentJob->pid==pid){
            break;
        }
        prec = currentJob;
        currentJob=currentJob->next;
    }
    job* jobToDel = prec->next;
    prec->next = jobToDel->next;
    if(!jobToDel->foreground)
        printf("%s (jobs=[%d], pid=%d) terminated with status=%d\n",jobToDel->command,jobToDel->id, jobToDel->pid, jobToDel->exitCode);
    free(jobToDel->command);
    free(jobToDel);
    nbJob--;
}

int getNbJob(){
    return nbJob;
}

void clearJobs(){
    job* currentJob = jobHead;
    job* prec=NULL;
    while(currentJob){
        prec=currentJob;
        currentJob=currentJob->next;
        free(prec);
    }
}

// commande interne myjobs
void myjobs(){
    int status;
    job* currentJob = jobHead->next;
    job* prec=jobHead;
    while(currentJob){
        pid_t pid =  waitpid(currentJob->pid, &status, WNOHANG | WUNTRACED);
        if(pid>0 && WIFEXITED(status)){
            delJobByPid(currentJob->pid);
            currentJob=prec;
        }
        else{
            if (pid>0)
                currentJob->stopped=true;
            printf("[%d] %d %s %s\n",currentJob->id,currentJob->pid, currentJob->stopped ? "Stopped" : "Runing", currentJob->command);
        }
        prec=currentJob;
        currentJob=currentJob->next;
    }
}

void killAll(){
    job* currentJob = jobHead->next;
    while(currentJob){
        kill(currentJob->pid,SIGKILL);
        currentJob=currentJob->next;
    }
}