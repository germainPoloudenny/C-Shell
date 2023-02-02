typedef struct {
    int id;
    int name;
    int val;
    int next;
} envVar;

typedef struct _localVar{
    char* name;
    char* val;
    struct _localVar* next;
} localVar;

void initVarLists();
void storeEnv(char *envp[]);
char** getUpdatedEnv();
void addLocalVar(char* var);
void addEnvVar(char* var);
localVar* getLocalVar(char* name);
envVar* getEnvVar(char* name);
char* getVarVal(char* name);
void delLocalVar(char* name);
void delEnvVar(char* name);
void printLocalVars();
void printEnvVars();
void clearVars();
envVar* getEnvVarByShId(int shId);
char* getEnvVarName(int shId);
char* getEnvVarVal(int shId);
void setEnvVarName(int shId, char* name);
void setEnvVarVal(int shId, char* val);
envVar* initEnvVar(char* name, char* val);
void clearVars();