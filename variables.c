#include "utils.h"
#include "variables.h"
#include <string.h>
#include <stdio.h>
#include <memory.h>
#include <sys/shm.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>


int nbShellInstancesId;
int* nbShellInstances;

envVar* envVarHead;
localVar* localVarHead;

sem_t sem_writeVar, sem_readVar, sem_nbShellInstances;

void initVarLists(){
    sem_init(&sem_writeVar,1,1);
    sem_init(&sem_readVar,1,1);
    sem_init(&sem_nbShellInstances,1,1);
    key_t nbShellInstancesKey = 1000;
    key_t envVarHeadKey = 2000;
    int sharedMemoryId; 

    if((nbShellInstancesId=shmget(nbShellInstancesKey , sizeof(int), 0777 | IPC_CREAT | IPC_EXCL ))==ERR && errno != EEXIST){
        perror(0);
        return;
    }
    bool firstInstance=false;
    if(errno != EEXIST){
        firstInstance=true;
    }
    else{
        if((nbShellInstancesId=shmget(nbShellInstancesKey , sizeof(int), 0777))==ERR){
            perror(0);
            return;
        }
    }
    sem_wait(&sem_nbShellInstances);
    if((nbShellInstances=(int *) shmat(nbShellInstancesId, NULL, 0 ))==(void*) ERR){
        perror(0);
        return;
    };
    if(firstInstance)
        *nbShellInstances=1;
    else
        (*nbShellInstances)++;
    sem_post(&sem_nbShellInstances);
    if((sharedMemoryId=shmget(envVarHeadKey , sizeof(envVar), 0777 | IPC_CREAT))==ERR ){
        perror(0);
        return;
    }
    if((envVarHead=(envVar*) shmat(sharedMemoryId, NULL, 0 ))==(void*) ERR){
        perror(0);
        return;
    }
    envVarHead->id=sharedMemoryId;
    envVarHead->next=-1;
    localVarHead = malloc(sizeof(localVar));
    localVarHead->next=NULL;

}


void storeEnv(char *envp[]){
    for (char **env = envp; *env != 0; env++){
        addEnvVar(*env);
    }
    return;
}

char** getUpdatedEnv(){
    int shId = envVarHead->next;
    int nbEnv=0;
    char** envp = malloc(0);
    while(shId!=-1){
        envp = realloc(envp, (1 + ++nbEnv) * sizeof(char*));
        char* name = getEnvVarName(shId);
        char* val = getEnvVarVal(shId);
        envp[nbEnv-1] = malloc( strlen(name) + strlen(val) + 2);
        strcpy(envp[nbEnv-1],name);
        strcat(envp[nbEnv-1],"=");
        strcat(envp[nbEnv-1],val);
        shId=getEnvVarByShId(shId)->next;
    }
    envp[nbEnv] = 0;
    return envp;
}


void addLocalVar(char* var){
    char* name = strtok(var,"=");
    char* val = strtok(NULL,"=");
    if(!val){
        val="\0";
    }
    localVar* lVar = localVarHead->next;
    localVar* prec= localVarHead;

    while(lVar){
        if(strcmp(lVar->name,name)==0){
            lVar->val = realloc(lVar->val,  strlen(val)+1);
            strcpy(lVar->val,val);
            return;
        }
        prec=lVar;
        lVar=lVar->next;
    }
    localVar* varNode = malloc(sizeof(localVar));
    varNode->next=NULL;
    prec->next=varNode;
    varNode->name = malloc(strlen(name)+1);
    strcpy(varNode->name,name);
    varNode->val = malloc(  strlen(val)+1);
    strcpy(varNode->val,val);
    return;
}

void addEnvVar(char* var){
    sem_wait(&sem_readVar);
    sem_wait(&sem_writeVar);
    char newvar[strlen(var)];
    strcpy(newvar,var);
    char* name = strtok(newvar,"=");
    char* val = strtok(NULL,"=");
    if(!val){
        val="\0";
    }
    int shId = envVarHead->next;
    int prec= envVarHead->id;
    while(shId!=-1){
        if(strcmp(getEnvVarName(shId),name)==0){
            setEnvVarVal(shId, val);
            sem_post(&sem_writeVar);
            sem_post(&sem_readVar);
            return;
        }
        prec=shId;
        shId=getEnvVarByShId(shId)->next;
    }
    envVar* varNode = initEnvVar(name,val);
    getEnvVarByShId(prec)->next=varNode->id;
    sem_post(&sem_writeVar);
    sem_post(&sem_readVar);
    return;
}

localVar* getLocalVar(char* name){
    localVar* localVar = localVarHead->next;
    while(localVar){
        if(strcmp(localVar->name,name)==0)
            return localVar;
        localVar=localVar->next;
    }
    return NULL;
}


envVar* getEnvVar(char* name){
    int shId = envVarHead->next;
    while(shId!=-1){
        if(strcmp(getEnvVarName(shId),name)==0)
            return getEnvVarByShId(shId);
        shId=getEnvVarByShId(shId)->next;
    }
    return NULL;
}

char* getVarVal(char* name){
    localVar* localVar = getLocalVar(name);
    if(!localVar){
        envVar* envVar = getEnvVar(name);
        if(!envVar)
            return "\0";
        return getEnvVarVal(envVar->id);
    }
    return localVar->val;
}

void delLocalVar(char* name){
    localVar* var = localVarHead->next;
    localVar* prec= localVarHead;
    while(var){
        if(strcmp(var->name,name)==0){
            break;
        }
        prec=var;
        var=var->next;
    }
    if(!var)
        return;
    prec->next=var->next;
    free(var->name);
    free(var->val);
    free(var);
}

void delEnvVar(char* name){
    sem_wait(&sem_readVar);
    sem_wait(&sem_writeVar);
    int shId = envVarHead->next;
    int prec= envVarHead->id;
    while(shId!=-1){
        if(strcmp(getEnvVarName(shId),name)==0){
            break;
        }
        prec=shId;
        shId=getEnvVarByShId(shId)->next;
    }
    if(shId==-1){
        sem_post(&sem_writeVar);
        sem_post(&sem_readVar);
        return;
    }
    getEnvVarByShId(prec)->next=getEnvVarByShId(shId)->next;
    shmctl(getEnvVarByShId(shId)->name, IPC_RMID, NULL);
    shmctl(getEnvVarByShId(shId)->val, IPC_RMID, NULL);
    shmctl(getEnvVarByShId(shId)->id, IPC_RMID, NULL);

    sem_post(&sem_writeVar);
    sem_post(&sem_readVar);
}

void printLocalVars(){
    localVar* localVar = localVarHead->next;
    while(localVar){
        printf("%s=%s\n", localVar->name, localVar->val);
        localVar=localVar->next;
    }
}

void printEnvVars(){
    int shId = envVarHead->next;
    while(shId!=-1){
        printf("%s=%s\n", getEnvVarName(shId), getEnvVarVal(shId));
        shId=getEnvVarByShId(shId)->next;
    }
}

void clearVars(){
    sem_wait(&sem_nbShellInstances);
    while(localVarHead->next)
        delLocalVar(localVarHead->next->name);
    free(localVarHead);
    (*nbShellInstances)--;
    if(*nbShellInstances){
        sem_post(&sem_nbShellInstances);
        return;
    }
    int shId = envVarHead->next;
    int prec;
    while(shId!=-1){
        prec = shId;
        shId=getEnvVarByShId(shId)->next;
        delEnvVar(getEnvVarName(prec));
    }
    if(shmctl(envVarHead->id, IPC_RMID, NULL)==ERR){
        perror(0);
    }
    if(shmctl(nbShellInstancesId, IPC_RMID, NULL)==ERR){
        perror(0);
    }
    sem_destroy(&sem_writeVar);
    sem_destroy(&sem_readVar);
    sem_destroy(&sem_nbShellInstances);
}

envVar* initEnvVar(char* name, char* val){
    int shVarId =  shmget(IPC_PRIVATE  , sizeof(envVar), 0777 | IPC_CREAT );
    int shNameId = shmget(IPC_PRIVATE  , strlen(name)+1, 0777 | IPC_CREAT );
    int shValId = shmget(IPC_PRIVATE  , strlen(val)+1, 0777 | IPC_CREAT );
    envVar* envVar = getEnvVarByShId(shVarId);
    envVar->id=shVarId;
    envVar->name=shNameId;
    envVar->val=shValId;
    setEnvVarName(shVarId, name);
    setEnvVarVal(shVarId, val);
    envVar->next=-1;

    return envVar;
}

envVar* getEnvVarByShId(int shId){
    return (envVar*) shmat(shId, NULL, 0 );
}

char* getEnvVarName(int shId){
    return (char*) shmat(getEnvVarByShId(shId)->name, NULL, 0 );
}

char* getEnvVarVal(int shId){
    return (char*) shmat(getEnvVarByShId(shId)->val, NULL, 0 );
}

void setEnvVarName(int shId, char* name){
    char* varName = getEnvVarName(shId);
    strcpy(varName,name);
}

void setEnvVarVal(int shId, char* val){
    char* varVal = getEnvVarVal(shId);
    strcpy(varVal,val);
}