/**==============================================
 * *                   INFO
 *   
 * @author      : Danielle DuChene
 * @createdOn   : 2/5/2021
 * @description : This is supposed to smallsh
 *=============================================**/
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/wait.h>

struct commandStuff{
    char*   command;
    char*   args[515];
    char*   inputFile;
    char*   outputFile;
    int     bgProcess;
};

static int latestStatus = 0;
static int numRunning = 0;
int runningPIDS[200];
static int runForeground = 1;

//catchSIGINT: from exploration on signals
void catchSIGINT(int signo){
	write(STDOUT_FILENO, "terminated by \n", 21);
    exit(0);
}

//when user hits ctrl+z, will alter between foreground only &
//both foreground and background processes
void catchSIGTSTP(int signo){
    if(runForeground == 1){
        char* message = "Entering foreground only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, 50);
        fflush(stdout);
        runForeground = 0;
    }

    else{
        char* message = "Exiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 30);
        fflush(stdout);
        runForeground = 1;
    }
}

//varExpanion: if the variable has two $$ in a row, write a new
//string replacing it with the $$ with the pid
char* varExpansion(char *inVar, int inPID){
    int length = strlen(inVar) + 1;
    int replacePoints[1024];
    int pointCounter = 0;
    char newVar[2048] = "";
    char pidBuf[24] = "";
    snprintf(pidBuf, 64, "%d", inPID);
    int dollarSign = 0;


    for(int i = 0; i < length; i++){
        if(inVar[i] == '$' && inVar[i + 1] == '$'){
            replacePoints[pointCounter] = i;
            //increase spot in point array
            pointCounter++;
            //advance i so we don't double count i + 1
            i++;
            dollarSign = 1;
        }
    }

    if(dollarSign){
        pointCounter = 0;
        for(int i = 0; i < length; i++){
            if(i == replacePoints[pointCounter]){
                strcat(newVar, pidBuf);
                pointCounter++;
                //dont double count i
                i++;
            }
            else{
                char toAdd = inVar[i];
                strncat(newVar, &toAdd, 1);
            }
        }
        return newVar;
    }

    else{
        return inVar;
    }

}

//cdCommand: will direct to HOME if there is no
//argument provided. Or else it will direct to the
//argument. not sure what to do if it doesn't exist
//not sure if i need to worry about it
void cdCommand(struct commandStuff inCommand){
    // if there are no arguments
    if(!(inCommand.args[1])){
        char* homeDir = getenv("HOME");
        chdir(homeDir);
    }

    else{
        chdir(inCommand.args[1]);
    }
}

//exitCommand: kills all the current running commands
void exitCommand(struct commandStuff inCommand){
    int i = 0;

    while(runningPIDS[i]){
        kill(runningPIDS[i], SIGTERM);
        i++;
    }


}

//otherCommand: this will handle commands other than exit, cd, and status
//by using execvp to create the child process. It hopefully updates the
//list of currently running PIDs. this is mostly from the class notes
//regarding exec() functions
void otherCommand(struct commandStuff inCommand){
    int childStatus;
    signal(SIGINT, SIG_IGN);

    //Fork a new process
    int spawnPid = fork();
    
    //from exploration on child and parent processes    
    switch(spawnPid){
        case -1:
            perror("fork()\n");
            exit(1);
            break;
        case 0:
            //in child process
            signal(SIGINT, catchSIGINT);
            //recommend i/o redirection be done in the child process
            //you got it
            if(inCommand.outputFile){
                //from replit https://repl.it/@cs344/54redirectc
                //and https://repl.it/@cs344/54sortViaFilesc
                int targetFD = open(inCommand.outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (targetFD == -1) { 
                    perror("target open()"); 
                    fflush(stdout);
                    latestStatus = 1;
                    exit(1); 
                }
                // Use dup2 to point FD 1, i.e., standard output to targetFD
                int result = dup2(targetFD, 1);
                if(result == -1){
                    perror("dup2\n");
                    fflush(stdout);
                    latestStatus = 2;
                    exit(2);
                }
            }
            if(inCommand.inputFile){
                //from https://repl.it/@cs344/54sortViaFilesc
                // Open source file
                int sourceFD = open(inCommand.inputFile, O_RDONLY);
                if (sourceFD == -1) { 
                    perror("source open()"); 
                    fflush(stdout);
                    latestStatus = 1;
                    exit(1); 
                }

                // Redirect stdin to source file
                int result = dup2(sourceFD, 0);
                if (result == -1) { 
                    perror("source dup2()"); 
                    fflush(stdout);
                    latestStatus = 2;
                    exit(2); 
                }
            }

            //if user doesn't redirect standard input and its in the bg, std in
            //and out should be "/dev/null"
            //I COULDn"T FIGURE THIS OUT so I gave up
            // if(inCommand.bgProcess){
            //     if(!inCommand.inputFile){
            //         int nullFile = open("dev/null", O_RDONLY);
            //         if(nullFile == -1){
            //             perror("open null file\n");
            //             fflush(stdout);
            //             latestStatus = 1;
            //             exit(1);
            //         }

            //         int result = dup2(nullFile, 0);
            //         if (result == -1) { 
            //             perror("source dup2() in null\n"); 
            //             fflush(stdout);
            //             latestStatus = 2;
            //             exit(2); 
            //         }                    
            //     }

            //     if(!inCommand.outputFile){
            //         int nullFile = open("dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            //         if(nullFile == -1){
            //             perror("open null file\n");
            //             fflush(stdout);
            //             latestStatus = 1;
            //             exit(1);
            //         }

            //         int result = dup2(nullFile, 1);
            //         if (result == -1) { 
            //             perror("source dup2() out null\n"); 
            //             fflush(stdout);
            //             latestStatus = 2;
            //             exit(2); 
            //         }  
            //     }
            // }

            execvp(inCommand.command, inCommand.args);
            //exec only returns if there is an error
            perror("execvp\n");
            latestStatus = 2;
            exit(2);
            break;
        default:
            //in parent process
            if(inCommand.bgProcess && runForeground){                
                    //run it in the background, from explorations on processes
                    waitpid(spawnPid, &childStatus, WNOHANG);
                    printf("background pid is %d\n", spawnPid);
                    fflush(stdout);
                    //add it to our global variables if it lets us
                    runningPIDS[numRunning] = spawnPid;
                    numRunning++;
            }
            else{
                //wait for child termination
                waitpid(spawnPid, &childStatus, 0);
            }

            //from brewster video. have a coffee
            //https://www.youtube.com/watch?v=VwS3dx3uyiQ&list=PL0VYt36OaaJll8G0-0xrqaJW60I-5RXdW&index=18&ab_channel=BenjaminBrewster
            if(WIFEXITED(childStatus)){
                latestStatus = WEXITSTATUS(childStatus);
            }
            else{
                //child was terminated by a signal
                latestStatus = WTERMSIG(childStatus);
            }

            //look for finished processes....
            for(int i = 0; i < numRunning; i++){
                //https://www.youtube.com/watch?v=1R9h-H2UnLs&list=PL0VYt36OaaJll8G0-0xrqaJW60I-5RXdW&index=16&ab_channel=BenjaminBrewster
                //"check if process specified has completed, return immediately with 0 if it hasn't"
                int childExitMethod = -5;
                int childPID = waitpid(runningPIDS[i], &childExitMethod, WNOHANG);

                if(childPID != 0){
                    printf("backgound pid %d is done: ", runningPIDS[i]);
                    fflush(stdout);
                    //i'm not sure if this is needed or if i can just use latestStatus variable
                    if(WIFEXITED(childExitMethod)){
                        printf("exit value %d\n", WEXITSTATUS(childExitMethod));
                        fflush(stdout);
                    }
                    else{
                        //was term'd by a signal
                        printf("terminated by signal %d\n", WTERMSIG(childExitMethod));
                        fflush(stdout);
                    }

                    //remove from array
                    for(int j = i; j < numRunning; j++){
                        runningPIDS[j] = runningPIDS[j+1];
                    }
                    runningPIDS[numRunning] = 0;
                    numRunning--;
                }
            }

            break;
    }

}

//nextLine: prints a colon and gets the command from the user and directs
//it as appropriate to a command
void nextLine(int inpid){
    char inString[2048];
    char* token;
    struct commandStuff newCommand;
    int continueBool = 1;
    while(continueBool){
        printf(": ");
        fflush(stdout);
        
        fgets(inString, 2048, stdin);
        char *saveStr = inString;

        if(saveStr[0] == '#'){
            continueBool = 1;
            continue;
        }

        else if(saveStr[0] == ' ' || saveStr[0] == '\n'){
            continueBool = 1;
            continue;
        }

        else{
            // its safe to grab the tokens
            token = strtok_r(saveStr, " \n", &saveStr);
            newCommand.bgProcess = 0;
            int argCount = 0;
            
            //set command and first argument to command
            newCommand.command = token;
            newCommand.args[0] = token;
            argCount++;
            newCommand.inputFile = NULL;
            newCommand.outputFile = NULL;


            while((token = strtok_r(saveStr, " \n", &saveStr))){
                //these are wrong bc they are not caring about the order
                if(token[0] == '<'){
                    //process in file
                    token = strtok_r(saveStr, " \n", &saveStr);
                    newCommand.inputFile = token;
                }
                else if(token[0] == '>'){
                    //process out file
                    token = strtok_r(saveStr, " \n", &saveStr);
                    newCommand.outputFile = token;
                }
                else if(token[0] == '&'){
                    newCommand.bgProcess = 1;
                }
                else{
                    //add to argruments
                    fflush(stdout);
                    token = varExpansion(token, inpid);
                    newCommand.args[argCount] = token;
                    argCount++;
                }
            }

            //add null to the end of args?
            newCommand.args[argCount] = NULL;
        }

        //direct to correct functions
        if((!strcmp(newCommand.command, "cd"))){
            cdCommand(newCommand);
        }

        else if((!strcmp(newCommand.command, "exit"))){
            exitCommand(newCommand);
            continueBool = 0;
        }

        else if((!strcmp(newCommand.command, "status"))){
            printf("terminated by signal %d\n", latestStatus);
            fflush(stdout);
        }

        else{
            otherCommand(newCommand);
        }
    }
}

//main: this is where it all happens... good luck main
int main(){
    signal(SIGTSTP, catchSIGTSTP);
    signal(SIGINT, SIG_IGN);
  
    int myPid = getpid();
    nextLine(myPid);

    return 0;
}
