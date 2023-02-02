#include "utils.h"
#include "job.h"
#include "variables.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <glob.h>
#include <termios.h> 


bool resetInput=false; // utile pour gérer les conflits lorsqu'un CTRL-C intervient pendant un getchar()
static struct termios oldt, newt; // utile pour permettre à chaque caractère d'être pris en input directement
int status; // buffer du code de retour du processus fils
int exitCode; // code de retour du processus fils
char** command; // commande entrée par l'utilisateur
char* arg; // argument courrant de la commande
int nbArg=0; 
int nbChar=0;
int nbPipe=0;
int* pipesFd;

// CTRL-C (si aucun processus en foreground)
static void askExitShell(int sig){
    printf("\nDo you really want to exit ? [y/n]\n");
    tcsetattr( STDIN_FILENO, TCSANOW, &newt);
    resetInput=true;
}

// CTRL-C (si processus en foreground)
static void exitForeground(int sig){
    printf("\n");
}

// CTRL-Z (si processus en foreground)
static void stopForeGround(int sig){
    printf("\n");
}

// on arrête le programme en nettoyant la mémoire
static void cleanExit(int id){
    for(int i=0;i<nbArg;i++)
        free(command[i]);
    free(command);
    free(arg);
    free(pipesFd);
    clearJobs();
    clearVars();
    exit(id);
}

// on nettoie la ligne
static void clearCommandLine(){
    for(int i=0;i<nbArg;i++)
        free(command[i]);
    command = realloc(command,0);
    nbArg=0;
    nbChar=0;
}

// rediriger un flux vers un fichier
static int redirectStd(int *stdStreams, char* redirection){
    int fd;
    if(redirection[0]=='2')
        redirection++;
    if (strcmp(redirection,">>")==0){
        if((fd=open(command[nbArg-1],O_WRONLY | O_CREAT | O_APPEND, 0777))==ERR){
            return ERR;
        }
    } 
    else if (strcmp(redirection,">")==0){
        if((fd=open(command[nbArg-1],O_WRONLY | O_CREAT, 0777))==ERR){
            return ERR;
        }
        if(ftruncate(fd, 0)==ERR){
            return ERR;
        }
    }
    else if((fd=open(command[nbArg-1],O_RDONLY | O_CREAT, 0777))==ERR){
        return ERR;
    }
    for(int i=0;i<2;i++){

        if(stdStreams[i]==-1)
            break;
        dup2(fd,stdStreams[i]);
    }
    free(command[--nbArg]);

    close(fd);
    return 0;
}
static void updateExitCode(){
    if(WIFEXITED(status)){
        exitCode = WEXITSTATUS(status);
    }
    else if(WIFSTOPPED(status)){
        exitCode = WSTOPSIG(status);
    }
    else if(WIFSIGNALED(status)){
        exitCode = WTERMSIG(status);
    }
}


int main(int argc,char *argv[],char *envp[]){
    command = malloc(sizeof(char*));
    arg = malloc(sizeof(char));
    char directory[100];
    bool stopReadInput=false; // on ne lit plus la suite de la commande (&&, ||)
    char c; // caractère courant de la commande
    pid_t pid=getpid();
    char operator[3]="";    // &&, || ...
    char redirection[4]=""; // >, >> ...
    getcwd(directory,100); 
    // gestion d'input
    tcgetattr( STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON);

    pipesFd=malloc(0);
    operator[0]='\0';
    pid_t lastForegroundPid=-1; 
    int lastForegroundExitCode=0;
    int nbProcToWaitFor=0;
    signal(SIGINT, askExitShell);
    initVarLists(); // on initialise les listes chaînées de variables locales et d'environnement
    storeEnv(envp); // on stocke les variables d'env dans la liste
    initJobHead(); // on initialise la liste chaînée de jobs

    while(1){
        printf("%s> ",operator[0] ? "" : directory); //  si la ligne précédente ressemble à "ls |" on attend la suite de la commande donc on n'affiche pas le répertoire
        char stockedChar=0; // on stock le caractère pour plus tard, et on l'ajoute pas dans l'argument courrant directement
        bool canChangeRedirection=true; // on interprète les opérateurs de changement de direction
        bool background=false; // la commande n'est pas lancée en background
        bool readingVarName = false; // on lit le nom d'une variable précédée par un '$'
        char varName[100]; 
        int varNameInc=0;
        while (1 ){
            
            // on clean les espaces
            if(!stockedChar || resetInput)
                while(((c=getchar())  == ' ') && (nbChar==0 ||  arg[nbChar-1]==' '));
            // c prend la valeur stockée
            else{
                c=stockedChar;
                stockedChar=0;
            }
            // on gère la réponse de la demande d'échelle (gérée ici et non dans la fonction de catch du signal, car conflit entre plusieurs fonctions interragissant sur stdin)
            if(resetInput){
                tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
                printf("\n");
                if(c=='y'){
                    killAll();
                    cleanExit(2);
                }
                resetInput=false;
                clearCommandLine();
                break;
            }
            // on interprète le nom de la variable et on récupère sa valeur si elle existe
            if(varNameInc && readingVarName && !((c>=65 && c <= 90) || (c>=97 && c <= 122) || c=='_')){
                varName[varNameInc]='\0';
                char* val = getVarVal(varName);
                if(val){
                    int varValueLength= strlen(val);
                    if(varValueLength){
                        nbChar+=varValueLength;
                        arg = realloc(arg,nbChar+1);
                        for(int i=0;i<varValueLength;i++){
                            arg[nbChar-varValueLength+i]=val[i];
                        }
                        arg[nbChar]='\0';
                    }
                    readingVarName=false;
                    varNameInc=0;
                }
            }
            // caractères spéciaux
            if(charIn(c,"\n; &|><") || charIn(operator[0],"|") || strcmp(operator,"&&")==0 || stopReadInput){
                if(stopReadInput){
                    if(c=='\n'){
                        stopReadInput=false;
                    }
                    else{
                        continue;
                    }
                }
                bool commandEnded=true;
                if(c=='|' && operator[0]==c){
                    operator[1]=c;
                    operator[2]='\0';
                    continue;
                }
                else if(c=='|'){
                    operator[0]=c;
                    operator[1]='\0';
                    continue;
                }
                else if(c=='&' && !background && nbArg){
                    background=true;
                    commandEnded=false;
                }
                else if(c=='&' && background){
                    background=false;
                    operator[0]=c;
                    operator[1]=c;
                    operator[2]='\0';
                    commandEnded=false;
                }
                if(c=='\n' && operator[0]){
                    printf("> ");
                    continue;
                }
                else if(!charIn(c,"\n; &|><")){
                    stockedChar=c;
                }
                else if(canChangeRedirection && charIn(c,"2<>&")){
                    if((c=='&' && strcmp(redirection,">>")==0) || (c=='>' && strcmp(redirection,"2>")==0)){
                        redirection[2]=c;
                        redirection[3]='\0';
                        canChangeRedirection=false;
                        background=false;
                        continue;
                    }
                    else if(charIn(c,">&") && redirection[0]=='>'){
                        redirection[1]=c;
                        redirection[2]='\0';
                        background=false;
                        continue;
                    }
                    else if(c=='>'){
                        int inc=0;
                        if(nbChar && arg[nbChar-1]=='2'){
                            redirection[inc++]='2';
                            arg[--nbChar]='\0';
                            if(!nbChar){
                                nbChar=0;

                            }
                        }
                        redirection[inc++]=c;
                        redirection[inc++]='\0';
                        commandEnded=false;
                    }
                    else if(c=='<' && !redirection[0]){
                        redirection[0]=c;
                        redirection[1]='\0';
                        commandEnded=false;
                        canChangeRedirection=false;
                    }
                    else{
                        canChangeRedirection=false;
                    }
                }
                if(nbChar){
                    arg[nbChar]='\0';
                    if(arg[0]=='$'){
                        char* val;
                        if(nbChar>1){
                            arg++;
                            nbChar--;                            
                            val = getVarVal(arg);
                        }
                        if(!val || nbChar==1){
                            nbChar=0;
                            arg[0]='\0';
                        }
                    }
                    if(arg[0]=='~'){
                        command = realloc(command,sizeof(char*)*++nbArg);
                        char* homeDir = getenv("HOME");
                        command[nbArg-1] = malloc(strlen(homeDir)+1);
                        strcpy(command[nbArg-1],homeDir);
                    }
                    //  on remplace chaque argument par une liste de (1+n) arguments le système de wildcards et la librairie glob
                    else{
                        glob_t globbuf = {0,0,0};
                        glob(arg,GLOB_DOOFFS  ,NULL, &globbuf );
                        if(globbuf.gl_pathc==0){
                            command = realloc(command,sizeof(char*)*++nbArg);
                            command[nbArg-1] = malloc(nbChar+1);
                            strcpy(command[nbArg-1], arg);
                        }
                        for(int i=0;i<globbuf.gl_pathc;i++) {
                            command = realloc(command,sizeof(char*)*++nbArg);
                            command[nbArg-1] = malloc(strlen(globbuf.gl_pathv[i])+1);
                            strcpy(command[nbArg-1],globbuf.gl_pathv[i]);
                        }
                        globfree(&globbuf);
                    } 
                }
                if(c!=' ' && commandEnded){
                    if(nbChar || nbArg){
                        // commandes internes  car non-forkées
                        if(strcmp(command[0],"cd")==0){
                            if((nbArg==1))   {
                                envVar* homeVar = getEnvVar("HOME");
                                if(homeVar &&  (chdir(getEnvVarVal(homeVar->id))==ERR)){
                                    perror(0);
                                }
                            }
                            else if(chdir(command[1])==ERR){
                                perror(0);
                            }
                            getcwd(directory,100);
                        }
                        else if(strcmp(command[0],"exit")==0){
                            cleanExit(2);
                        }
                        else if(strcmp(command[0],"status")==0){
                            if(lastForegroundPid==-1){
                                printf("No process was executed in the foreground\n");
                            }
                            else if(lastForegroundExitCode==2){
                                printf("%d terminated abnormally\n",lastForegroundPid);
                            }
                            else{
                                printf("%d terminated with return code %d\n",lastForegroundPid,lastForegroundExitCode);
                            }
                        }
                        else if(strcmp(command[0],"myjobs")==0){
                            myjobs();
                        }
                        else if(strcmp(command[0],"myfg")==0){
                            if((pid=switchJob((nbArg==2) ? atoi(command[1]) : -1 , true)) != ERR){
                                nbProcToWaitFor++;
                                signal(SIGTTIN, SIG_IGN);   // gestion stdin
                                signal(SIGTTOU, SIG_IGN);   // gestion stdout
                                tcsetpgrp(STDIN_FILENO,pid);
                                kill(pid, SIGCONT); // on envoie un signal au processeur stoppé de se réveiller
                            }
                        }
                        else if(strcmp(command[0],"mybg")==0){
                            switchJob((nbArg==2) ? atoi(command[1]) : -1 , false);
                        }
                        else if((strcmp(command[0],"set")==0)){
                            if(nbArg==1){
                                printLocalVars();
                            }
                            else{
                                addLocalVar(command[1]);
                            }
                        }
                        else if((strcmp(command[0],"setenv")==0)){
                            if(nbArg==1){
                                printEnvVars();
                            }
                            else{
                                addEnvVar(command[1]);
                            }
                        }
                        else if((strcmp(command[0],"unset")==0) && nbArg==2){
                            delLocalVar(command[1]);
                        }
                        else if((strcmp(command[0],"unsetenv")==0) && nbArg==2){
                            delEnvVar(command[1]);
                        }
                        else {
                            if(strcmp(operator,"|")==0){
                                pipesFd=realloc(pipesFd,2*(nbPipe+1)*sizeof(int));
                                if(pipe(pipesFd+nbPipe*2)==ERR){
                                    perror(0);
                                    cleanExit(ERR);
                                }
                            }
                            pid = fork();
                            if(pid==ERR) 
                                cleanExit(ERR);
                            if(!pid) {
                                if(strcmp(operator,"|")==0|| nbPipe){
                                    if(!nbPipe){
                                        dup2(pipesFd[2*nbPipe+1],1);
                                    }
                                    else if(strcmp(operator,"|")==0){
                                        dup2(pipesFd[2*(nbPipe-1)], 0);
                                        dup2(pipesFd[2*nbPipe+1], 1);
                                    }
                                    else{
                                        dup2(pipesFd[2*(nbPipe-1)],0);
                                    }
                                    for(int i=0;i<nbPipe;i++){
                                        close(pipesFd[2*i]);
                                        close(pipesFd[2*i+1]);
                                    }
                                }
                                if(redirection[0]){
                                    int stdStreams[2]={-1,-1};
                                    if(charIn('&',redirection)){
                                        if(redirection[2]){
                                            redirection[2]='\0';
                                        }
                                        else
                                            redirection[1]='\0';
                                        stdStreams[0]=fileno(stdout);
                                        stdStreams[1]=fileno(stderr);
                                    }
                                    else if((redirection[0]=='>'))
                                        stdStreams[0]=fileno(stdout);
                                    else if((redirection[0]=='<'))
                                        stdStreams[0]=fileno(stdin);
                                    else{
                                        stdStreams[0]=fileno(stderr);
                                    }
                                    if(redirectStd(stdStreams,redirection)==ERR){
                                        perror(0);
                                        cleanExit(ERR);
                                    }
                                }
                                command = realloc(command,sizeof(char*)*(nbArg+1));
                                command[nbArg]  =NULL;
                                char path[20];
                                if(strlen(command[0])>=2 && command[0][0] == '.' && command[0][1] =='/'){
                                    strcpy(path,command[0]+2);
                                }
                                else if(strcmp(command[0],"myls")==0 || strcmp(command[0],"myps")==0){
                                    strcpy(path,command[0]);
                                }
                                else{
                                    strcpy(path,"/bin/");
                                    strcat(path,command[0]);
                                }
                                // ayant décidés de gérer les variables d'environnement partagées avec une liste chaînée (plus simple pour gérer les suppressions),
                                // il est nécessaire de mettre à jour le tableau envp à partir de la liste chaînée à l'aide de la méthode avec getUpdatedEnv()
                                char** updatedEnv = getUpdatedEnv();
                                if(execve(path ,command, updatedEnv )==ERR){
                                    for(int i=0;i<nbArg;i++){
                                        fprintf(stderr,"%s",command[i]);
                                    }
                                    fprintf(stderr,": command not found\n");
                                    int i=0;
                                    for(i=0;updatedEnv[i];i++){
                                        free(updatedEnv[i]);
                                    }
                                    free(updatedEnv);
                                    cleanExit(ERR);
                                }
                            }
                            nbProcToWaitFor= !background;
                            if(nbPipe && (strcmp(operator,"|")!=0)){
                                for(int i=0;i<nbPipe;i++){
                                    close(pipesFd[2*i]);
                                    close(pipesFd[2*i+1]);
                                }
                                nbProcToWaitFor+=nbPipe;
                                nbPipe=0;
                            }
                            else if (strcmp(operator,"|")==0)
                                nbPipe++;
                        }
                    }
                    if(!nbPipe){
                        pid_t pid;
                        pid_t* jobPids = malloc(getNbJob()* sizeof(pid_t));
                        int nbJobPid =0;
                        signal(SIGINT, exitForeground);
                        signal(SIGTSTP, stopForeGround);
                        while (nbProcToWaitFor  && (pid = waitpid(-1, &status, WUNTRACED  )) != ERR) // WUNTRACED pour prendre en compte les processus stoppés
                        {
                            job* jobPrec = getJobByPid(pid);
                            updateExitCode();
                            if(jobPrec && (WIFEXITED(status) || WIFSIGNALED(status))){
                                jobPids[nbJobPid++] = pid;
                                jobPrec->exitCode=exitCode;
                            }
                            else if(jobPrec && WIFSTOPPED(status)){
                                jobPrec->stopped = true;
                            }
                            else if(WIFSTOPPED(status)){
                                addJob(command, nbArg, pid,!background, true);
                            }
                            if(!jobPrec || (jobPrec && jobPrec->foreground))
                                nbProcToWaitFor--;
                            if(exitCode && strcmp("&&",operator)==0){
                                stopReadInput=true;
                            }
                            else if(!exitCode && strcmp("||",operator)==0){ 
                                stopReadInput=true;
                            }
                        } 
                        tcsetpgrp(STDIN_FILENO,getpgrp());
                        signal(SIGTTIN, SIG_DFL);
                        signal(SIGTTOU, SIG_DFL); 
                        signal(SIGINT, askExitShell);
                        signal(SIGTSTP, NULL);
                        for(int i=0;i<nbJobPid;i++){
                            delJobByPid(jobPids[i]);
                        }
                        free(jobPids);
                        if(!background){
                            lastForegroundPid= pid;
                            lastForegroundExitCode=exitCode;
                        }
                    }
                    if(background){
                        addJob(command, nbArg, pid,!background, false);
                    }
                    int nbJob = getNbJob();
                    if(nbJob && !(nbArg==1 && strcmp(command[0],"myjobs")==0)){
                        pid_t pid;
                        while (nbJob-- && (pid=waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
                            job* backgroundJob = getJobByPid(pid);
                            updateExitCode();
                            if(WIFEXITED(status)){
                                backgroundJob->exitCode=exitCode;
                                delJobByPid(pid);
                            }
                            else if(WIFSTOPPED(status)){
                                backgroundJob->stopped=true;
                                printf("[%d] %s %s\n",backgroundJob->id,"Stopped     ",backgroundJob->command);
                            }
                        }
                    }
                    // on reset et nettoie l'invite de commande
                    clearCommandLine(&command,&nbArg,&nbChar,pipesFd,nbPipe);
                    canChangeRedirection=true;
                    background=false;
                    operator[0]='\0';
                    redirection[0]='\0';
                    nbProcToWaitFor=0;
                    if(c=='\n'){
                        break;
                    }
                }
                // à chaque fin d'argument
                arg = realloc(arg,0);
                nbChar=0;
            }
            // on s'apprête à lire un nom de variable
            else if(c=='$'){ 
                readingVarName = true;
            }
            // on ne remplace pas $nom_variable par sa valeur, unset a justement besoin de nom_valeur 
            else if(readingVarName && nbArg>=1 && strcmp(command[0],"unset")!=0 && strcmp(command[0],"unsetenv")!=0){
                varName[varNameInc++]=c;
            }
            else if(stopReadInput)
                continue;
            // on ajoute le caractère courant dans l'argument courant
            else{
                arg = realloc(arg,++nbChar+1);
                arg[nbChar-1]=c;
                operator[0]='\0';
            }
        }
    }
}