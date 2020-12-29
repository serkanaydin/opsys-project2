#include <stdio.h>
#include <unistd.h>
#include <errno.h>
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
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */

/* The setup function below will not return any value, but it will just: read
in the next command line; separate it into distinct arguments (using blanks as
delimiters), and set the args array entries to point to the beginning of what
will become null-terminated, C-style strings. */
void standartOutput(char *args[]);
int getArgumentCount(char *args[]);
int getArgumentCount(char *args[]){
    int i= 0;
    while(args[i]!=NULL)
        i++;
    return i;
}


#define CREATE_FLAGS (O_WRONLY | O_CREAT | O_APPEND)
#define CREATE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH )



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

    printf(">>%s<<",inputBuffer);
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

    for (i = 0; i <= ct; i++)
        printf("args %d = %s\n",i,args[i]);
} /* end of setup routine */

int main(void)
{
    char inputBuffer[MAX_LINE]; /*buffer to hold command entered */
    int background; /* equals 1 if a command is followed by '&' */
    char *args[MAX_LINE/2 + 1]; /*command line arguments */
    while (1){
        background = 0;
        printf("myshell: ");
        fflush(stdout);
        /*setup() calls exit() when Control-D is entered */
        setup(inputBuffer, args, &background);
        standartOutput(args);
    }
}



void standartOutput(char *args[]){
    pid_t cpid;
    cpid = fork();
    int argCount = getArgumentCount(args);
    char *path= getenv("PATH");
    path = strtok(strdup(path),":");
    int fd;
    char output[100];

    if(cpid == -1)
    {
        perror( "child is not created");
        return;
    }
    else if(cpid == 0)
    {
        strcpy(output,args[argCount-1]);
        if((fd = open(output,CREATE_FLAGS,CREATE_MODE)) == -1){
            perror("dsoya açılamadı");
            return;
        }
        dup2(fd,STDOUT_FILENO);
        close(fd);
        args[argCount-2] = NULL;
        //dirent kütüphanesinde file oluşturuyoruz
        struct dirent *file;

        while(path != NULL)
        {
            DIR *dir;
            dir = opendir(path);
            while((file  = readdir(dir) ) != NULL)
            {
                if( strcmp(file->d_name, args[0]) == 0 )
                {
                    strcat(path,"/");
                    strcat(path,args[0]);
                    execv(path,args);
                }
            }
            path = strtok(NULL,":");
        }
    }
    else if(cpid>0)
    {
        waitpid(cpid, NULL , 0);
        return;
    }
}