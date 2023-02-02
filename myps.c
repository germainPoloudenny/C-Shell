#include "utils.h"
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <glob.h>
#include <time.h>
#include <pwd.h>

#define NB_USEFUL_INFO 11 // fait référence au nombre de colonnes affichée par la commande

void updateProcInfos(int* maxLengths, int infoId,char** dst, char* src){
    int length = strlen(src);
    dst[infoId] = malloc(length+1);
    strcpy(dst[infoId],src);
    if(length>maxLengths[infoId])
        maxLengths[infoId]=length;
}

void cleanExit(glob_t globbuf,char*** lines, int code){
    for(int i=0;i<globbuf.gl_pathc+1;i++){
        for(int j=0;j<NB_USEFUL_INFO;j++){
            if(lines[i][j])
                free(lines[i][j]);
        }
        free(lines[i]);
    }
    if(lines)
        free(lines);
    if(code==ERR)
        perror(0);
    globfree(&globbuf);
    exit(code);
}

void fillFileBuffer(char* buffer, int file, char* fileName, glob_t globbuf, char*** lines){
        if((file=open(fileName,O_RDONLY))==ERR){
            cleanExit(globbuf,lines,ERR);
        }
        if(read(file,buffer,500)==ERR) {
            close(file);
            cleanExit(globbuf,lines,ERR);
        }
        if(close(file)==ERR){
            cleanExit(globbuf,lines,ERR);
        }
}

// la proportion de temps CPU utilisée par un processus nécessite 2 calculs écartés d'une certaine période. Ici, 1 seconde
// cette fonction est donc  appelée 2 fois et à 1 seconde d'interval
unsigned long getCpuTime(glob_t globbuf,char*** lines){
    FILE* allProcStatFile;
    char line[100];
    if(!(allProcStatFile=fopen("/proc/stat","r"))){
        cleanExit(globbuf,lines,ERR);
    }
    if(!fgets(line,100,allProcStatFile)){
        fclose(allProcStatFile);
        cleanExit(globbuf,lines,ERR);
    }
    fclose(allProcStatFile);

    char* cpuTime = strtok(line," ");
    unsigned long totalCpuUsage1=0; 
    while(1){
        cpuTime = repeatStrtok(1," ");
        if(!cpuTime)
            break;
        totalCpuUsage1+=strtoul(cpuTime,NULL,10);
    }
    return totalCpuUsage1;
}

//formatage de ligne
void cleanPrintLine(char*** lines, int* maxLengths, int lineId, int  usefulInfoId,int offset, bool reverseAlignment){
    if(reverseAlignment)
        repeatPrintf(maxLengths[usefulInfoId]-strlen(lines[lineId][usefulInfoId])," ");
    printf("%s",lines[lineId][usefulInfoId]);
    repeatPrintf(offset," ");
    if(reverseAlignment)
        return;
    repeatPrintf(maxLengths[usefulInfoId]-strlen(lines[lineId][usefulInfoId])," ");
}

int main(int argc,char *argv[],char *envp[]){
    if(argc>1){
        fprintf(stderr,"error: unsupported option\n");
        exit(ERR);
    }
    glob_t globbuf = {0,0,0};
    int precSmallerProcId=-1;
    int newSmallerProcId=1000;
    int globFileId;
    glob("/proc/[0-9]*",GLOB_DOOFFS  ,NULL, &globbuf );
    int nbProc=globbuf.gl_pathc;
    int nbLines = nbProc+1;
    unsigned long cpuTime = getCpuTime(globbuf,NULL);
    struct sysinfo sys_info;
    sysinfo(&sys_info);
    unsigned long realMemorySize = sys_info.totalram;
    char*** lines= malloc(sizeof(char**)*nbLines);
    for(int i=0;i<nbLines;i++){
        lines[i]=malloc(sizeof(char*)*NB_USEFUL_INFO);
    }
    int maxLengths[NB_USEFUL_INFO];
    int printLineOrder[nbProc];
    for(int i =0;i<NB_USEFUL_INFO;i++)
        maxLengths[i]=0;
    for(int i=0;i<nbProc;i++){
        globFileId=i;
        for(int j=0;j<nbProc;j++){
            char globFile [strlen(globbuf.gl_pathv[j])];
            strcpy(globFile,globbuf.gl_pathv[j]);
            strtok(globFile,"/");
            int dir = atoi(repeatStrtok(1,"/"));
            if(dir < newSmallerProcId && dir > precSmallerProcId){
                newSmallerProcId=dir;
                globFileId=j;
            }
        }
        precSmallerProcId=newSmallerProcId;
        newSmallerProcId=10000000;
        printLineOrder[i]=globFileId;
    }
    char* columnHeaders[NB_USEFUL_INFO] = {   "USER","PID","%CPU","%MEM","VSZ","RSS","TTY","STAT","START","TIME","COMMAND"};
    for(int i=0;i<NB_USEFUL_INFO;i++){
        updateProcInfos(maxLengths,i,lines[0],columnHeaders[i]);
    }
    for(int i=0;i<nbProc;i++){
        globFileId = printLineOrder[i];
        int lengthDirName=strlen(globbuf.gl_pathv[globFileId]);
        char statusFileName[lengthDirName+10];
        int statusFile=0;
        char statusInfo[2000];
        strcpy(statusFileName,globbuf.gl_pathv[globFileId]);
        strcat(statusFileName,"/status");
        fillFileBuffer(statusInfo,statusFile,statusFileName,globbuf,lines);
        strtok(statusInfo,"\n");
        repeatStrtok(4,"\n");
        repeatStrtok(1,"\t");
        updateProcInfos(maxLengths,1,lines[globFileId+1],repeatStrtok(1,"\n"));
        repeatStrtok(2,"\n");
        struct passwd *pw = getpwuid((atoi(repeatStrtok(2,"\t"))));
        updateProcInfos(maxLengths,0,lines[globFileId+1],pw->pw_name);    
        repeatStrtok(9,"\n");
        repeatStrtok(1,"\t");
        updateProcInfos(maxLengths,4,lines[globFileId+1],repeatStrtok(1," ")); 
        repeatStrtok(1,"\n");
        repeatStrtok(1,"\t");
        long vmLck=strtol(repeatStrtok(1," "),NULL,10);
        char statFileName[lengthDirName+10];
        int statFile=0;
        char statInfo[500];
        strcpy(statFileName,globbuf.gl_pathv[globFileId]);
        strcat(statFileName,"/stat");
        fillFileBuffer(statInfo,statFile,statFileName,globbuf,lines);
        strtok(statInfo," ");
        char stateCodes[5];
        int nbStateCode=0;
        stateCodes[nbStateCode++]=*(repeatStrtok(2," "));
        int pgrp=atoi(repeatStrtok(2," "));
        int session=atoi(repeatStrtok(1," "));
        int tty = atoi(repeatStrtok(1," "));
        if(!tty)
            updateProcInfos(maxLengths,6,lines[globFileId+1],"?");
        else{
            char* name = ttyname(tty&(0x7F));
            char extractedName[strlen(name)-5];
            for(int i=0;i<strlen(name);i++)
                extractedName[i]=name[i+5];
            updateProcInfos(maxLengths,6,lines[globFileId+1],extractedName);
        }
        int tpgid = atoi(repeatStrtok(1," "));
        char* stime = repeatStrtok(7," ");
        updateProcInfos(maxLengths,2,lines[globFileId+1],stime);
        long nice =strtol(repeatStrtok(4," "),NULL,10);
        long nbThread=strtol(repeatStrtok(1," "),NULL,10);
        struct stat fileStat;
        if(stat(globbuf.gl_pathv[globFileId], &fileStat) == ERR){
            cleanExit(globbuf,lines,ERR);
        }
        struct  tm startDate;
        struct tm* date = localtime(&fileStat.st_mtime);
        memcpy(&startDate,date,sizeof(*date));
        char formatedStartDate[6];
        strftime(formatedStartDate,6,"%b%d",&startDate);
        time_t t = time(NULL);
        struct  tm currentDate ;
        date = localtime(&t);
        memcpy(&currentDate,date,sizeof(*date));
        char formatedCurrentDate[6];
        strftime(formatedCurrentDate,6,"%b%d",&currentDate);
        if(strcmp(formatedStartDate,formatedCurrentDate)==0)
            strftime(formatedStartDate,6,"%H:%M",&startDate);
        updateProcInfos(maxLengths,8,lines[globFileId+1],formatedStartDate);
        if(nice==-20)
            stateCodes[nbStateCode++]='<';
        else if(nice==19)
            stateCodes[nbStateCode++]='N';
        if(vmLck)
            stateCodes[nbStateCode++]='L';
        if(session==atoi(lines[globFileId+1][1]))
            stateCodes[nbStateCode++]='s';
        if(nbThread>1)
            stateCodes[nbStateCode++]='l';
        if(tpgid==pgrp)
            stateCodes[nbStateCode++]='+';
        stateCodes[nbStateCode++]='\0';
        updateProcInfos(maxLengths,7,lines[globFileId+1],stateCodes);
        time_t cpuTimeUsed =  strtoul(stime,NULL,10) / sysconf(_SC_CLK_TCK) ;
        struct tm* duration = localtime(&cpuTimeUsed);
        char formatedDuration[6];
        strftime(formatedDuration,6,"%M:%S",duration);
        if(formatedDuration[0]=='0')
            for(int i=0;i<5;i++)
                formatedDuration[i]=formatedDuration[i+1];
        updateProcInfos(maxLengths,9,lines[globFileId+1],formatedDuration);
        char cmdLineFileName[lengthDirName+10];
        strcpy(cmdLineFileName,globbuf.gl_pathv[globFileId]);
        strcat(cmdLineFileName,"/cmdline");
        int cmdLineFile;
        char cmdLine[500];
        if((cmdLineFile=open(cmdLineFileName,O_RDONLY))==ERR){
            cleanExit(globbuf,lines,ERR);
        }
        if(read(cmdLineFile,cmdLine,500)==ERR) {
            close(statFile);
            cleanExit(globbuf,lines,ERR);
        }
        if(close(cmdLineFile)==ERR){
            cleanExit(globbuf,lines,ERR);
        }
        updateProcInfos(maxLengths,10,lines[globFileId+1],cmdLine);
    }
    sleep(1);
    unsigned long diffCpuTime = getCpuTime(globbuf,lines) - cpuTime;
    for(int i=0;i<nbProc;i++){
        globFileId = printLineOrder[i];
        int lengthDirName=strlen(globbuf.gl_pathv[globFileId]);
        char statFileName[lengthDirName+10];
        strcpy(statFileName,globbuf.gl_pathv[globFileId]);
        strcat(statFileName,"/stat");
        char statInfo[500];
        int statFile=0;
        fillFileBuffer(statInfo,statFile,statFileName,globbuf,lines);
        strtok(statInfo," ");
        float cpuUsage = (float) (100 * (strtoul(repeatStrtok(14," "),NULL,10) - strtoul(lines[globFileId+1][2],NULL,10)) / diffCpuTime);
        char formatedUsage[4];
        sprintf(formatedUsage,"%0.1f",cpuUsage);
        free(lines[globFileId+1][2]);
        updateProcInfos(maxLengths,2,lines[globFileId+1],formatedUsage);
        long residentSetSize = strtol(repeatStrtok(9," "),NULL,10)*4;
        char stringedResidentSetSize[32];
        sprintf(stringedResidentSetSize,"%ld",residentSetSize);
        updateProcInfos(maxLengths,5,lines[globFileId+1],stringedResidentSetSize);
        float memoryUsage=(float) (residentSetSize /realMemorySize * 100 * 1000);
        sprintf(formatedUsage,"%0.1f",memoryUsage);
        updateProcInfos(maxLengths,3,lines[globFileId+1],formatedUsage);
    }
    for(int i=0;i<nbLines;i++){
        globFileId=printLineOrder[i-1]+1;
        if(i==0)
            globFileId = i;
        else{
            char state = lines[globFileId][7][0];
            switch(state){
                case 'D':
                    printf(BLUE);
                    break;
                case 'R':
                    printf(GREEN);
                    break;
                case 'S':
                    printf(RED);
                    break;
                case 'T':
                    printf(MAGENTA);
                    break; 
                case 't':
                    printf(BOLDMAGENTA);
                    break; 
                case 'Z':
                    printf(WHITE);
                    break; 
                case 'W':
                    printf(YELLOW);
                    break; 
                case 'P':
                    printf(CYAN);
                    break; 
                default :
                    printf(BLACK);
                    
            }
            cleanPrintLine(lines,maxLengths,globFileId,0,3,false);
            cleanPrintLine(lines,maxLengths,globFileId,1,1,true);
            cleanPrintLine(lines,maxLengths,globFileId,2,1,true);
            cleanPrintLine(lines,maxLengths,globFileId,3,2,true);
            cleanPrintLine(lines,maxLengths,globFileId,4,1,true);
            cleanPrintLine(lines,maxLengths,globFileId,5,1,true);
            cleanPrintLine(lines,maxLengths,globFileId,6,4,false);
            cleanPrintLine(lines,maxLengths,globFileId,7,1,false);
            cleanPrintLine(lines,maxLengths,globFileId,8,3,true);
            cleanPrintLine(lines,maxLengths,globFileId,9,1,true);
            cleanPrintLine(lines,maxLengths,globFileId,10,0,false);
            printf(RESET"\n");
        }
    }
    cleanExit(globbuf,lines,0);
}