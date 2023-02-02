#include "utils.h"
#include <sys/stat.h>
#include <glob.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>

int myls(bool searchHidenFiles, bool searchRecursively, bool printDirContents, char** wildCards, int nbFile){
    glob_t globbuf = {0,0,0};
    struct stat fileStat;
    int maxSizeFile=0; // utile pour le formattage lors de l'affichage
    bool thereIsADir = false; // indique s'il y a au moins un répertoire listé, car la commande "ls" de base change son affichage en fonction
    int returnCode=0;
    if(searchHidenFiles && !printDirContents)
        glob(".*", GLOB_DOOFFS ,NULL, &globbuf );
    if(!nbFile)
        glob("*", GLOB_DOOFFS | GLOB_APPEND ,NULL, &globbuf );
    for(int i=0;i<nbFile;i++){
        if(glob(wildCards[i], GLOB_DOOFFS | GLOB_APPEND ,NULL, &globbuf ) == GLOB_NOMATCH && printDirContents){
            printf("myls: canot access \'%s\': No such file or directory\n",wildCards[i]);
            returnCode=ERR;
        }
    }
    for(int i=0;i<globbuf.gl_pathc;i++){
        if( (returnCode= stat(globbuf.gl_pathv[i], &fileStat)) == ERR){
            globfree(&globbuf);
            exit(ERR);
        }
        if((!nbFile || !S_ISDIR(fileStat.st_mode) || printDirContents) && fileStat.st_size>maxSizeFile)
            maxSizeFile=fileStat.st_size;
        if(S_ISDIR(fileStat.st_mode))
            thereIsADir=true;
    }
    int maxNbDigit= getNbDigit(maxSizeFile);
    for(int i=0;i<globbuf.gl_pathc;i++){
        stat(globbuf.gl_pathv[i], &fileStat);
        if(!(S_ISDIR(fileStat.st_mode) && printDirContents)){
            int length=strlen(globbuf.gl_pathv[i]);
            int localOffset=0;
            if(charIn('/',globbuf.gl_pathv[i]) ){
                for(int j=length;j>=0;j--){
                    if(globbuf.gl_pathv[i][j]=='/'){
                        localOffset=j+1;
                        break;
                    }
                }
            }
            printf("%s",S_ISDIR(fileStat.st_mode) ? ((globbuf.gl_pathv[i][localOffset]== '.')? BLUE : GREEN) : YELLOW);
            printf( (S_ISDIR(fileStat.st_mode)) ? "d" : "-" );
            printf( (fileStat.st_mode & S_IRUSR) ? "r" : "-");
            printf( (fileStat.st_mode & S_IWUSR) ? "w" : "-");
            printf( (fileStat.st_mode & S_IXUSR) ? "x" : "-");
            printf( (fileStat.st_mode & S_IRGRP) ? "r" : "-");
            printf( (fileStat.st_mode & S_IWGRP) ? "w" : "-");
            printf( (fileStat.st_mode & S_IXGRP) ? "x" : "-");
            printf( (fileStat.st_mode & S_IROTH) ? "r" : "-");
            printf( (fileStat.st_mode & S_IWOTH) ? "w" : "-");
            printf( (fileStat.st_mode & S_IXOTH) ? "x" : "-");
            struct passwd *pw = getpwuid(fileStat.st_uid);
            struct group  *gr = getgrgid(fileStat.st_gid);
            printf(" %ld %s %s " ,fileStat.st_nlink, pw->pw_name, gr->gr_name);
            int nbSpaces = maxNbDigit-getNbDigit(fileStat.st_size);
            repeatPrintf(nbSpaces," ");
            printf("%ld ",fileStat.st_size);
            char printedDate[13];
            struct  tm* date  = localtime(&fileStat.st_mtime);
            strftime(printedDate, 13,"%b %d %H:%M",date);
            printf("%s ",printedDate);
            printf("%s\n",globbuf.gl_pathv[i]+localOffset);
            printf(RESET);
        }
        else
            thereIsADir=true;
    }
    if(!nbFile || !thereIsADir || !(printDirContents || searchRecursively)){
        globfree(&globbuf);
        return returnCode;
    }

    char** newWildCards=malloc(sizeof(char*));
    char** fileNames = (searchRecursively)? globbuf.gl_pathv : wildCards;
    int length=(searchRecursively)? globbuf.gl_pathc : nbFile;
    int nbWildCards=0;
    for(int i=0;i<length;i++){
        stat(fileNames[i], &fileStat);
        if((!printDirContents && (strcmp(fileNames[i],".")==0 || strcmp(fileNames[i],"..")==0))  )
            continue;
        if(S_ISDIR(fileStat.st_mode)){
            bool isRootFile = strlen(fileNames[i])==1 && fileNames[i][0]=='/';
            newWildCards[0]=malloc(strlen(fileNames[i])+ 3);
            strcpy(newWildCards[0],fileNames[i]);
            strcat(newWildCards[0], isRootFile ? "*" : "/*");
            nbWildCards++;

            if(searchHidenFiles){
                newWildCards = realloc(newWildCards,sizeof(char*)*2);
                newWildCards[1]=malloc(strlen(fileNames[i])+4);
                strcpy(newWildCards[1],fileNames[i]);
                strcat(newWildCards[1], isRootFile ? ".*" : "/.*");
                nbWildCards++;
            }
            DIR *d;
            struct dirent *dir;
            if(!(d = opendir(fileNames[i]))){
                returnCode=ERR;
            }
            if (d){ 
                while ((dir = readdir(d)) != NULL)
                {
                    if(i)
                        printf("\n");
                    if(nbFile>1 || searchRecursively)
                        printf("%s:\n",globbuf.gl_pathv[i]);
                    returnCode= myls(searchHidenFiles,searchRecursively, false,newWildCards,1) ? ERR : returnCode;
                    break;
                }
                closedir(d);
            }
        }
        for(int i=0;i<nbWildCards;i++)
            free(newWildCards[i]);
        nbWildCards=0;
    } 
    for(int i=0;i<nbWildCards;i++)
        free(newWildCards[i]);
    free(newWildCards);
    globfree(&globbuf);
    return returnCode;
}

int main(int argc,char *argv[],char *envp[]){
    bool searchHidenFiles=false;
    bool searchRecursively = false;
    bool thereMayStillBeOptions=true;
    char** fileNames=malloc(sizeof(char*));
    int nbFile=0;
    int exitCode;
    for(int i=1;i<argc;i++){
        if(argv[i][0]=='-' && !thereMayStillBeOptions){
            fprintf(stderr,"Wrong syntax\n");
            exitCode=ERR;
            break;
        }
        if(argv[i][0]=='-'){
            if(strcmp(argv[i],"-a")==0){
                searchHidenFiles=true;
            }
            else if(strcmp(argv[i],"-aR")==0 || strcmp(argv[i],"-Ra")==0 ){
                searchHidenFiles=true;
                searchRecursively=true;
            }
            else if(strcmp(argv[i],"-R")==0){
                searchRecursively=true;
            }
            else{
                fprintf(stderr,"Wrong syntax\n");
                exitCode=ERR;
                break;
            }
        }
        else{
            thereMayStillBeOptions=false;
            fileNames[nbFile] = malloc(strlen(argv[i])+1);
            strcpy(fileNames[nbFile],argv[i]);
            fileNames = realloc(fileNames,sizeof(char*)* (++nbFile+1));
        }

    }
    if(!nbFile){
        fileNames[nbFile] = malloc(2);
        strcpy(fileNames[nbFile],".");
        fileNames = realloc(fileNames,sizeof(char*)* (++nbFile+1));
    }
    exitCode = myls(searchHidenFiles, searchRecursively, true, fileNames, nbFile) ? ERR : 0;
    for(int i=0;i<nbFile;i++){
        free(fileNames[i]);
    }
    free(fileNames);
    exit(exitCode);
}