#include "utils.h"

int getNbDigit(int nb){
    int nbDigit=1;
    while(nb/10>0){
        nb/=10;
        nbDigit++;
    }
    return nbDigit;
}

char* repeatStrtok(int nb, char* separators){
    char* strToken;
    for(int i=0;i<nb;i++){
        strToken = strtok(NULL, separators);
    }
    return strToken;
}

void repeatPrintf(int nb, char* string){
    for(int i=0;i<nb;i++){
        printf("%s",string);
    }
}

bool charIn(char c, char* string){
    int length = strlen(string);
    for(int i=0;i<length;i++){
        if(c==string[i])
            return true;
    }
    return false;
}



