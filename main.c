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

#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */
struct termios old_termios;
pid_t FOREGROUND_PID=-1;
void catchUserQuit(int sig, siginfo_t * info, void * useless);
void catchCtrlD(int signalNbr);
void catchCtrlZ(int signalNbr);
void execute(char *args[],int background,char inputBuffer[]);
struct background_proc{
    pid_t pid;
    char input[MAX_LINE];
    struct background_proc* next;
    int status;
};
int read_err=0;

typedef struct background_proc* BACKGROUND_PROC_PTR;
typedef struct background_proc BACKGROUND_PROC;
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
if(length==-1){
    read_err=1;
}

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
        } /* end of switch */
    }    /* end of for */
    args[ct] = NULL; /* just in case the input line was > 80 */

   /* for (i = 0; i <= ct; i++)
        printf("args %d = %s\n",i,args[i]);*/

} /* end of setup routine */

int main(void)
{

    struct sigaction action;
    struct sigaction actionZ;
    int status;
    int statusZ;
    action.sa_flags=0;
    action.sa_handler=catchCtrlD;
    status=sigemptyset(&action.sa_mask);

    actionZ.sa_flags=0;
    actionZ.sa_handler=catchCtrlZ;
    statusZ=sigemptyset(&actionZ.sa_mask);

    if(status==-1)
    {
        perror("Failed");
        exit(1);
    }
    status=sigaction(SIGINT,&action,NULL);
    if(status==-1)
    {
        perror("Failed HANDLER");
        exit(1);
    }



    if(statusZ==-1)
    {
        perror("Failed");
        exit(1);
    }
    statusZ=sigaction(SIGTSTP,&actionZ,NULL);
    if(statusZ==-1)
    {
        perror("Failed HANDLER");
        exit(1);
    }
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = catchUserQuit;
    sigaction(SIGCHLD, &sa, NULL);


    char inputBuffer[MAX_LINE]; /*buffer to hold command entered */
    int background; /* equals 1 if a command is followed by '&' */
    char *args[MAX_LINE/2 + 1]; /*command line arguments */

    struct termios  new_termios;
    tcgetattr(0,&old_termios);
    new_termios             = old_termios;
    new_termios.c_cc[VEOF]  = 3;
    new_termios.c_cc[VINTR] = 4;
    tcsetattr(0,TCSANOW,&new_termios);
    pid_t cpid;
    BACKGROUND_PROC_PTR current=NULL;

    int BUILT_IN=0;
    int order=1;


    while (1) {

        background = 0;

        //  setup(inputBuffer, args, &background);
        /*setup() calls exit() when Control-D is entered */

        printf("myshell: ");

            fflush(stdout);


        setup(inputBuffer, args, &background);




        if(args[0]!=NULL ){
            if(strcmp(args[0],"ps_all")==0)
            BUILT_IN=1;
            else if(strcmp(args[0],"exit")==0)
            BUILT_IN=2;
        }


        if(BUILT_IN==0){
            cpid=fork();
            if(cpid==-1)
                perror("Child creation");
            else if(cpid==0 ){
                /*  printf("%d",background);
                  fflush(stdout);*/

                execute(args,background, inputBuffer);}

            else{

                if(background==1){
                    if(BACKGROUND_HEAD==NULL){
                        BACKGROUND_HEAD=(BACKGROUND_PROC_PTR)(malloc(sizeof(BACKGROUND_PROC)));
                        strcpy((char *) BACKGROUND_HEAD->input, (char *) inputBuffer);
                        BACKGROUND_HEAD->pid=cpid;
                        BACKGROUND_HEAD->status=1;
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
                        current->next=NULL; }

                }
                if(background==0 ){
                    FOREGROUND_PID=cpid;
                    while(waitpid(cpid,NULL,WNOHANG)>=0);

                }


            }


        }
        else if(BUILT_IN==1) {
                current = BACKGROUND_HEAD;
                BACKGROUND_PROC_PTR currentFinish;

                BACKGROUND_PROC_PTR old=BACKGROUND_HEAD;
                while (current != NULL) {
                    if(current->status==0){
                        if(FINISH_HEAD==NULL){
                            FINISH_HEAD=(BACKGROUND_PROC_PTR)(malloc(sizeof(BACKGROUND_PROC)));
                            FINISH_HEAD->pid=current->pid;
                            strcpy(FINISH_HEAD->input,current->input);
                            FINISH_HEAD->next=NULL;
                            if(BACKGROUND_HEAD==current)
                                BACKGROUND_HEAD=NULL;
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
                            strcpy(currentFinish->input,current->input);
                            currentFinish->next=NULL;
                            old->next=current->next;

                        }
                        BACKGROUND_PROC_PTR temp=current;
                        current=current->next;
                        free(temp);
                    }
                    else{
                        old=current;
                        current=current->next;
                    }




                }
                fprintf(stderr,"%s","BACKGROUND PROCESSES\n");

                current=BACKGROUND_HEAD;
                while(current!=NULL){
                    printf("%s %d\n",current->input,current->pid);
                    fflush(stdout);
                    current=current->next;
                }
                fprintf(stderr,"%s","------------------\n");
                fprintf(stderr,"%s","FINISHED PROCESSES\n");
                current=FINISH_HEAD;
                while(current!=NULL){
                    printf("FINISHED %s %d\n",current->input,current->pid);
                    fflush(stdout);
                    current=current->next;
                }
                fprintf(stderr,"%s","------------------\n");
                FINISH_HEAD=NULL;
                BUILT_IN=0;

            }
        else if(BUILT_IN==2) {

            fprintf(stderr,"%s\n","There is/are running process(-/es)");
            while(wait(NULL)>0);
            exit(0);
        }




    }




    /** the steps are:
    (1) fork a child process using fork()
    (2) the child process will invoke execv()
    (3) if background == 0, the parent will wait,
    otherwise it will invoke the setup() function again. */






}
void catchUserQuit(int sig, siginfo_t * info, void * useless){
    BACKGROUND_PROC_PTR  current;
    current=BACKGROUND_HEAD;
    while (current!=NULL)
    {
        if(current->pid==info->si_pid){
            current->status=0;
            break;
        }
        current=current->next;}

}


void catchCtrlZ(int signalNbr){
    char message[] = "Ctrl-Z was pressed\n";

    if(FOREGROUND_PID!=-1 &&kill(FOREGROUND_PID,SIGKILL)==0)
    {
        fprintf(stderr,"%s",message);
    }



}

void catchCtrlD(int signalNbr){
    char message[] = "Ctrl-D was pressed\n";
    tcsetattr(0,TCSANOW,&old_termios);
    perror(message);
    exit(1);
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



    while( token != NULL ) {
        strcpy(buff,token);
        if( access( strcat(strcat(buff,"/"),args[0]), F_OK ) == 0  ){

            execv(buff,argument);
        }
        token = strtok(NULL, delim);



    }

}