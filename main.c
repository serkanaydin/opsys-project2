#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include<termios.h>
#include<wait.h>
#include <stdio_ext.h>
#include <dirent.h>
#include <fnmatch.h>

#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */
struct background_proc{
    pid_t pid;
    char input[MAX_LINE];
    int order;
    struct background_proc* next;
    int status;
};
typedef struct background_proc* BACKGROUND_PROC_PTR;
typedef struct background_proc BACKGROUND_PROC;
typedef struct File File;

struct termios old_termios;
pid_t FOREGROUND_PID=-1;
int order=0;

void catchUserQuit(int sig, siginfo_t * info, void * useless);
void catchCtrlD(int signalNbr);
void catchCtrlZ(int signalNbr);
void execute(char *args[],int background,char inputBuffer[]);
void search(char *args[]);
void searchDir(char* path,char *args[] );
int isBackground(BACKGROUND_PROC_PTR head,pid_t pid);

void standartWrite(char *args[]);
void standartAppend(char *args[]);
void standartInput(char *args[]);
void standartError(char *args[]);
void stdoutCommand(char *args[]);
char* getPath();
int getArgumentCount(char *args[]);
int getArgumentCount(char *args[]){
    int i= 0;
    while(args[i]!=NULL)
        i++;
    return i;
}


#define CREATE_FLAGS (O_WRONLY | O_CREAT | O_TRUNC)
#define CREATE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH )
#define CREATE_APPEND (O_WRONLY | O_APPEND | O_CREAT )
#define CREATE_INPUTFLAGS (O_RDWR)


BACKGROUND_PROC_PTR BACKGROUND_HEAD=NULL;
BACKGROUND_PROC_PTR FINISH_HEAD = NULL;
void setup(char inputBuffer[], char *args[],int *background)
{


    int length, /* # of characters in the command line */
    i,      /* loop index for accessing inputBuffer array */
    start,  /* index where beginning of next command parameter is */
    ct;     /* index of where to place the next parameter into args[] */

    ct = 0;

    /* read what the user enters on the command line */

    length = read(STDIN_FILENO,inputBuffer,MAX_LINE);

    /* 0 is the system predefined file descriptor for stdin (standard input),
       which is the user's screen in this case. inputBuffer by itself is the
       same as &inputBuffer[0], i.e. the starting address of where to store
       the command that is read, and length holds the number of characters
       read in. inputBuffer is not a null terminated C-string. */

    start = -1;
    if (length == 0)
        exit(0);            /* ^d was entered, end of user command stream */

/* the signal interrupted the read system call */
/* if the process is in the read() system call, read returns -1
  However, if this occurs, errno is set to EINTR. We can check this  value
  and disregard the -1 value */
    if ( (length < 0) && (errno != EINTR) ) {
        perror("error reading the command");
        exit(-1);           /* terminate with error code of -1 */
    }



  //  printf(">>%s<<",inputBuffer);
    for (i=0;i<length;i++){ /* examine every character in the inputBuffer */

        switch (inputBuffer[i]){
            case ' ':
            case '\t' :               /* argument separators */
                if(start != -1){
                    args[ct] = &inputBuffer[start];    /* set up pointer */
                    ct++;
                }
                inputBuffer[i] = '\0'; /* add a null char; make a C string */
                start = -1;
                break;

            case '\n':                 /* should be the final char examined */
                if (start != -1){
                    args[ct] = &inputBuffer[start];
                    ct++;
                }
                inputBuffer[i] = '\0';
                args[ct] = NULL; /* no more arguments to this command */
                break;

            default :             /* some other character */
                if (start == -1)
                    start = i;
                if (inputBuffer[i] == '&'){
                    *background  = 1;
                    inputBuffer[i-1] = '\0';

                }
        }
    }
    args[ct] = NULL;



}

int main(void)
{
    struct sigaction action;
    struct sigaction actionZ;
    struct sigaction sa;

    int status;
    int statusZ;
    int statusUserQuit;

    action.sa_flags=0;
    action.sa_handler=catchCtrlD;
    status=sigemptyset(&action.sa_mask);

    actionZ.sa_flags=0;
    actionZ.sa_handler=catchCtrlZ;
    statusZ=sigemptyset(&actionZ.sa_mask);

    statusUserQuit=sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = catchUserQuit;


    if(status==-1)
    {
        perror("Failed: CTRL-D");
        exit(1);
    }
    status=sigaction(SIGINT,&action,NULL);
    if(status==-1)
    {
        perror("Failed HANDLER CTRL-D");
        exit(1);
    }

    if(statusZ==-1)
    {
        perror("Failed CTRL-Z");
        exit(1);
    }
    statusZ=sigaction(SIGTSTP,&actionZ,NULL);
    if(statusZ==-1)
    {
        perror("Failed HANDLER CTRL-Z");
        exit(1);
    }

    if( statusUserQuit==-1)
    {
        perror("Failed USER QUIT");
        exit(1);
    }
    statusUserQuit=sigaction(SIGCHLD, &sa, NULL);
    if( statusUserQuit==-1)
    {
        perror("Failed HANDLER USER QUIT");
        exit(1);
    }

    struct termios  new_termios;
    tcgetattr(0,&old_termios);
    new_termios             = old_termios;
    new_termios.c_cc[VEOF]  = 3;
    new_termios.c_cc[VINTR] = 4;
    tcsetattr(0,TCSANOW,&new_termios); //TO TERMINATE MAIN PROGRAM WITH CTRL-D

    char inputBuffer[MAX_LINE]; /*buffer to hold command entered */
    int background; /* equals 1 if a command is followed by '&' */
    char *args[MAX_LINE/2 + 1]; /*command line arguments */
    pid_t cpid;
    BACKGROUND_PROC_PTR current=NULL;
    int BUILT_IN=0;
    while (1) {
        background = 0;
        printf("myshell: ");
        fflush(stdout);
        setup(inputBuffer, args, &background);
        if(args[0]!=NULL ){
            if(strcmp(args[0],"ps_all")==0)
            BUILT_IN=1;
            else if(strcmp(args[0],"exit")==0)
            BUILT_IN=2;
            else if(strcmp(args[0],"search")==0) {
                search(args);
                continue;}

        }

        int argumentCount = getArgumentCount(args);
        if( argumentCount >= 4 && strcmp(args[argumentCount-2],">") == 0 && strcmp(args[argumentCount-4],"<") == 0)
            stdoutCommand(args);
        else if(strcmp(args[argumentCount-2],">") == 0)
            standartWrite(args);
        else if(strcmp(args[argumentCount-2],">>") == 0)
            standartAppend(args);
        else if(strcmp(args[argumentCount-2],"<") == 0)
            standartInput(args);
        else if(strcmp(args[argumentCount-2],"2>") == 0)
            standartError(args);
    }


        if(BUILT_IN==0){ //IF PROCESS IS NOT BUILT-IN PROGRAM FORKS TO EXECUTE PROGRAM
            cpid=fork();
            if(cpid==-1)
                perror("Child creation error");
            else if(cpid==0 ){ //CHILD EXECUTES PROGRAM
                execute(args,background, inputBuffer);}
            else{ // PARENT (MAIN PROGRAM) RUNS
                if(background==1){ //IF NEW PROCESS IS BACKGROUND THEN PROGRAM ADDS TO BACKGROUND PROCESSES'S LINKEDLIST
                    if(BACKGROUND_HEAD==NULL){
                        order=1;
                        BACKGROUND_HEAD=(BACKGROUND_PROC_PTR)(malloc(sizeof(BACKGROUND_PROC)));
                        strcpy((char *) BACKGROUND_HEAD->input, (char *) inputBuffer);
                        BACKGROUND_HEAD->pid=cpid;
                        BACKGROUND_HEAD->status=1;
                        BACKGROUND_HEAD->order=order;
                        BACKGROUND_HEAD->next=NULL;
                    }
                    else {
                        current=BACKGROUND_HEAD;
                        while (current->next!=NULL)
                            current=current->next;
                        current->next=(BACKGROUND_PROC_PTR)(malloc(sizeof(BACKGROUND_PROC)));
                        current=current->next;
                        strcpy((char *) current->input, (char *) inputBuffer);
                        current->pid=cpid;
                        current->status=1;
                        order++;
                        current ->order=order;
                        current->next=NULL; }
                }
                if(background==0 ){ //IF NEW PROCESS IS FOREGROUND THAN PROGRAM WAITS UNTIL THE PROCESS WILL BE TERMINATED
                    FOREGROUND_PID=cpid;
                    while(waitpid(cpid,NULL,WNOHANG)>=0);
                }
            }
        }
        else if(BUILT_IN==1) { //BUILT_IN=1 MEANS USER PROMPTED ps_all COMMAND
                current = BACKGROUND_HEAD;
                BACKGROUND_PROC_PTR currentFinish;
                BACKGROUND_PROC_PTR old=BACKGROUND_HEAD;
                while (current != NULL) { //status=0 means the background process was terminated
                    if(current->status==0){  // if process was terminated then ps_all adds the process to finished process
                        if(FINISH_HEAD==NULL){
                            FINISH_HEAD=(BACKGROUND_PROC_PTR)(malloc(sizeof(BACKGROUND_PROC)));
                            FINISH_HEAD->pid=current->pid;
                            strcpy(FINISH_HEAD->input,current->input);
                            FINISH_HEAD->order=current->order;
                            FINISH_HEAD->next=NULL;
                            if(BACKGROUND_HEAD==current) {
                                if(BACKGROUND_HEAD->next==NULL)
                                BACKGROUND_HEAD = NULL;
                                else BACKGROUND_HEAD=BACKGROUND_HEAD->next;} //ps_all deletes finished process from background processes's linked list
                            else
                            old->next=current->next;
                        }
                        else{
                            currentFinish=FINISH_HEAD;
                            while(currentFinish->next!=NULL){
                                currentFinish=currentFinish->next;
                            }
                            currentFinish->next=(BACKGROUND_PROC_PTR)(malloc(sizeof(BACKGROUND_PROC)));;
                            currentFinish=currentFinish->next;
                            currentFinish->pid=current->pid;
                            currentFinish->order=current->order;
                            strcpy(currentFinish->input,current->input);
                            currentFinish->next=NULL;
                            old->next=current->next;
                        }
                        BACKGROUND_PROC_PTR temp=current;
                        current=current->next;
                        free(temp); //freed finished background process's space
                    }
                    else{
                        old=current;
                        current=current->next;
                    }
                }
                fprintf(stderr,"%s","BACKGROUND PROCESSES\n"); //ps_all lists background processes
                current=BACKGROUND_HEAD;
                while(current!=NULL){
                    printf("[%d] %s %d\n",current->order,current->input,current->pid);
                    fflush(stdout);
                    current=current->next;
                }
                fprintf(stderr,"%s","------------------\n");
                fprintf(stderr,"%s","FINISHED PROCESSES\n");

                current=FINISH_HEAD; //ps_all lists finished background processes
                while(current!=NULL){
                    printf("[%d] %s %d\n",current->order,current->input,current->pid);
                    fflush(stdout);
                    current=current->next;
                }
                fprintf(stderr,"%s","------------------\n");
                FINISH_HEAD=NULL; //ps_all clears finished background processes's list
                BUILT_IN=0;
            }
        else if(BUILT_IN==2) { //BUILT_IN=2 MEANS USER PROMPTED exit COMMAND
            if(BACKGROUND_HEAD!=NULL){
            fprintf(stderr,"%s\n","There is/are running process(-/es)"); //program warns users about existence of background process
            while(wait(NULL)>0); //program waits until all the background processes will be terminated
            }
            fprintf(stderr,"%s","PROGRAM EXITED\n");
            exit(0);
        }
    }
}
void catchUserQuit(int sig, siginfo_t * info, void * useless){
    BACKGROUND_PROC_PTR  current;
    current=BACKGROUND_HEAD;
    while (current!=NULL)
    {
        if(current->pid==info->si_pid){
            fprintf(stderr,"%s %d %s CLOSED BY USER\n","PROCESS: ",current->pid,current->input);
            current->status=0; //says to the main program the process was terminated
            break;
        }
        current=current->next;}
}
void catchCtrlZ(int signalNbr){
    char message[] = "Ctrl-Z was pressed\n";
    if(FOREGROUND_PID!=-1 &&FOREGROUND_PID!=0 && isBackground(BACKGROUND_HEAD,FOREGROUND_PID) )
    {    if(kill(FOREGROUND_PID,SIGKILL)==0)
        fprintf(stderr,"%s",message); //if foreground program exists then handler kills the program
    }
    else{
        perror("THERE IS NO FOREGROUND PROCESS");
    }
}
void catchCtrlD(int signalNbr){ //terminates myshell
    char message[] = "Ctrl-D was pressed\n";
    tcsetattr(0,TCSANOW,&old_termios);
    perror(message);
    exit(1);
}

int isBackground(BACKGROUND_PROC_PTR head,pid_t pid){
    BACKGROUND_PROC_PTR current= head; //check the pid whether is background process
    while(current!=NULL){
        if(current->pid==pid)
            return 0;
        current=current->next;
    }
    return 1;
}
void execute(char *args[],int background,char inputBuffer[]){
    char* s = getenv("PATH");
    char delim[]=":\n";
    char * token = strtok(s, delim);
    char buff[50];
    int i=0;
    while(args[i]!=NULL){
        i++;
    }
    if(strcmp(args[i-1],"&")==0)
        i=i-1;
    char* argument[i+1];
    for(int k=0;k<i;k++) {
        argument[k] = args[k];
    }
    argument[i]=NULL;
    while( token != NULL ) { //tries all the environment paths until executable was found
        strcpy(buff,token);
        if( access( strcat(strcat(buff,"/"),args[0]), F_OK ) == 0  ){
            execv(buff,argument);
        }
        token = strtok(NULL, delim);
    }
    fprintf(stderr,"%s","Command not found\n");
exit(1);
}
void getSubDir(char *name, int indent,char *args[]){
    DIR *dir; //get subdirectories of current path recursively and calls searchDir to find argument
    struct dirent *dp;
    if (!(dir = opendir(name)))
        return;
    while ((dp = readdir(dir)) != NULL) {
        if (dp->d_type == DT_DIR) {
            char path[250];
            if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
                continue;
            strcpy(path,name);
            strcat(path,"/");
            strcat(path,dp->d_name);
            searchDir(path,args);
            getSubDir(path, indent + 2,args);
        }
    }
    closedir(dir);
}
void search(char *args[]){ //makes error control and calls functions depending on "-r" argument
    if(args[1]==NULL)
        perror("No argument was given to search command");
    if(strcmp(args[1],"-r")==0){
        if(args[2]==NULL)
            perror("No argument was given to search -r command");
        int len;
        len=strlen(args[2]);
        char args2[len-2];
        for(int i=1;i<(len-1);i++){
            args2[i-1]= args[2][i];
        }
        args2[len-2] ='\0';
        strcpy(args[2],args2);
        getSubDir(".", 0,args);
        searchDir(".",args);
    }
    else{
        int len;
        len=strlen(args[1]);
        char args1[len-2];
        for(int i=1;i<len-1;i++){
            args1[i-1]= args[1][i];
        }
        args1[len-2] ='\0';
        strcpy(args[1],args1);
        searchDir(".",args);
    }
}
void searchDir(char* path,char *args[] ){ //searchs current path for source codes.
    DIR *dirp=opendir(path);
    struct dirent entry;
    char buff[250];
    struct dirent *dp=&entry;
    while(dp = readdir(dirp)){
        if((fnmatch("*.c", dp->d_name,0)) == 0 ||(fnmatch("*.h", dp->d_name,0)) == 0
           ||(fnmatch("*.C", dp->d_name,0)) == 0 ||(fnmatch("*.H", dp->d_name,0)) == 0 )
        {
            int lineNum=0;
            char pathBuff[250];
            strcpy(pathBuff,path);
            strcat(pathBuff,"/");
            strcat(pathBuff,dp->d_name);
            File* file= (File *) fopen(pathBuff, "r");
            while(fgets(buff, 250, (FILE *) file)){
                if(strcmp(args[1],"-r")==0) {
                    if ((strstr(buff, (args[2]))) != NULL) {
                        fprintf(stderr, "\t%d: %s -> %s", lineNum, pathBuff, buff);
                    }
                }
                else  {
                    if ((strstr(buff, (args[1]))) != NULL) {
                        fprintf(stderr, "\t%d: %s -> %s", lineNum, pathBuff, buff);
                    }
                }
                lineNum++;
            }
        }
    }
}


void standartWrite(char *args[]) {
    pid_t cpid;
    cpid = fork();
    int argCount = getArgumentCount(args);
    if(cpid == -1)
    {
        fprintf(stderr,"%s","child is not created");
        return;
    }
    else if(cpid == 0) {
        int fd;
        if ((fd = open(args[argCount - 1], CREATE_FLAGS, CREATE_MODE)) == -1) {
            fprintf(stderr,"%s","dosya açılamadı");
            return;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
        char *path;
        path = getPath(args[0]);
        if(path != NULL){
            args[argCount-1] = NULL;
            args[argCount-2] = NULL;
            execv(path,args);
        }


    }
    else if(cpid>0)
    {
        waitpid(cpid, NULL , 0);
        return;
    }
}


void standartAppend(char *args[]){
    pid_t cpid;
    cpid = fork();
    int argCount = getArgumentCount(args);
    if(cpid == -1)
    {
        fprintf(stderr,"%s", "child is not created");
        return;
    }
    else if(cpid == 0) {
        int f1;
        if ((f1 = open(args[argCount - 1], CREATE_APPEND, CREATE_MODE)) == -1) {
            fprintf(stderr,"%s", "dosya açılamadı");
            return;
        }
        args[argCount] = NULL;
        dup2(f1, STDOUT_FILENO);
        close(f1);
        char *path;
        path = getPath(args[0]);
        if( path != NULL)
        {
            args[argCount-2] = NULL;
            execv(path,args);
        }

    }
    else if(cpid>0) {
        waitpid(cpid, NULL, WNOHANG);
        return;
    }
}

void standartInput(char *args[]){

    int argumentCount = getArgumentCount(args);
    pid_t cpid;
    cpid = fork();
    char *path= getenv("PATH");
    path = strtok(strdup(path),":");
    if(cpid == -1)
    {
        fprintf(stderr,"%s", "child is not created");
        exit(0);
    }
    else if(cpid == 0) {
        int fd;
        if ((fd = open(args[argumentCount - 1], CREATE_INPUTFLAGS, CREATE_MODE)) == -1) {
            fprintf(stderr, "%s", "file is not opened");
            exit(0);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);

        char *path;
        path = getPath(args[0]);
        if (path != NULL) {
            args[argumentCount - 2] = NULL;
            execv(path, args);
        }
    }

    else if(cpid>0)
    {
        waitpid(cpid, NULL , 0);
        return;
    }
}

void standartError(char *args[]) {
    int argumentCount = getArgumentCount(args);
    pid_t cpid;
    cpid = fork();
    if (cpid == -1) {
        fprintf(stderr,"%s","child is not created");
        return;
    }
    else if (cpid == 0) {
        int fd;
        if (fd = open(args[argumentCount - 1], CREATE_FLAGS, CREATE_MODE) == -1) {
            fprintf(stderr,"%s","file is not opened");
            return;
        }
        dup2(fd, STDERR_FILENO);
        close(fd);
        char *path;
        path = getPath(args[0]);

        if(path == NULL)
        {
            fprintf(stderr,"%s","path is not accessible");
            return;
        }
        args[argumentCount-1] = NULL;
        args[argumentCount-2] = NULL;
        if(execv(args[0], args) == -1)
            fprintf(stderr,"%s","path is not accessible");
    }
    else if (cpid > 0) {
        waitpid(cpid, NULL, 0);
        return;
    }
}

void stdoutCommand(char *args[]){
    int argumentCount = getArgumentCount(args);
    pid_t cpid;
    cpid = fork();
    if (cpid == -1) {
        fprintf(stderr,"%s","child is not created");
        return;
    }
    else if (cpid == 0) {
        int file1;
        int file2;
        file1 = open(args[argumentCount - 3], CREATE_INPUTFLAGS, CREATE_MODE);
        file2 = open(args[argumentCount - 1], CREATE_FLAGS, CREATE_MODE);

        dup2(file1, STDIN_FILENO);
        close(file1);
        dup2(file2, STDOUT_FILENO) ;
        close(file2);

        char *path;
        path = getPath(args[0]);
        if(path == NULL)
        {
            fprintf(stderr,"%s","path is not accessible");
            return;
        }
        args[argumentCount-5] = NULL;
        args[argumentCount-4] = NULL;
        args[argumentCount-3] = NULL;
        args[argumentCount-2] = NULL;
        args[argumentCount-1] = NULL;
        if(execv(args[0], args) == -1)
            fprintf(stderr,"%s","not executed");

    }
    else if (cpid > 0) {
        waitpid(cpid, NULL, 0);
        return;
    }

}
char* getPath(char* fileName){
    char *path= getenv("PATH");
    path = strtok(strdup(path),":");
    struct dirent *file;
    while(path != NULL)
    {
        DIR *dir;
        dir = opendir(path);
        while((file = readdir(dir) ) != NULL)
        {
            if( !strcmp(file->d_name, fileName))
            {
                strcat(path,"/");
                strcat(path,fileName);
                return path;
            }
        }
        path = strtok(NULL,":");
    }
    return NULL;
}

