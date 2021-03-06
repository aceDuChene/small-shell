/**============================================================
 * *                   INFO
 *
 * @author      : Danielle DuChene
 * @createdOn   : 2/5/2021
 * @editedOn    : 3/17/2022
 * @description : This is smallsh, an assignment done for CS344
 *   Operating Systems I for Oregon State University Ecampus.
 *   This specific implementation has been edited down from
 *   the original requirements.
 *   A shell that implements a subset of features of well-known
 *   shells. As listed on the assignment description, it will
 *      1. Provide a prompt for running commands
 *      2. Handle blank lines and comments (beginning with #)
 *      3. Execute exit, cd, and status via code built into shell
 *      4. Execute other commands by creating new processes using
 *          a function from the exec family of functions
 *      5. Support input and output redirection
 *      6. Can run foreground commands & background processes
 *      7. Implement custom handlers for 2 signals SIGINT & SIGTSTP
 *============================================================**/
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

// information from the command line
struct commandInfo
{
    char *command;
    char *args[515];
    char *inputFile;
    char *outputFile;
    int bgProcess;
};

static int latestStatus = 0;
static int numRunning = 0;
int runningPIDS[200];
static int runForeground = 1;

// catchSIGINT:
// from exploration on signals
// sent when user types interrupt character, Ctrl+C
void catchSIGINT(int signo)
{
    write(STDOUT_FILENO, "terminated by \n", 21);
    exit(0);
}

// catchSIGTSTP:
// when user hits ctrl+z, will alter between foreground only &
// both foreground and background processes
void catchSIGTSTP(int signo)
{
    if (runForeground == 1)
    {
        char *message = "Entering foreground only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, 50);
        fflush(stdout);
        runForeground = 0;
    }

    else
    {
        char *message = "Exiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 30);
        fflush(stdout);
        runForeground = 1;
    }
}

// cdCommand:
// will direct to HOME if there is no argument
// provided. Or else it will direct to the argument.
void cdCommand(struct commandInfo inCommand)
{
    // if there are no arguments
    if (!(inCommand.args[1]))
    {
        char *homeDir = getenv("HOME");
        chdir(homeDir);
    }

    else
    {
        chdir(inCommand.args[1]);
    }
}

// exitCommand:
// kills all the current running commands before the shell terminates
void exitCommand(struct commandInfo inCommand)
{
    int i = 0;

    while (runningPIDS[i])
    {
        kill(runningPIDS[i], SIGTERM);
        i++;
    }
}

// otherCommand:
// this will handle commands other than exit, cd, and status
// by using execvp to create the child process. It updates the
// list of currently running PIDs.
// adapted from class notes regarding exec() functions
void otherCommand(struct commandInfo inCommand)
{
    int childStatus;
    signal(SIGINT, SIG_IGN);

    // Fork a new process
    int spawnPid = fork();

    // from exploration on child and parent processes
    switch (spawnPid)
    {
    case -1:
        perror("fork()\n");
        exit(1);
        break;
    case 0:
        // in child process
        signal(SIGINT, catchSIGINT);
        // i/o redirection be done in the child process
        if (inCommand.outputFile)
        {
            // from provided replit https://repl.it/@cs344/54redirectc
            // and https://repl.it/@cs344/54sortViaFilesc
            int targetFD = open(inCommand.outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (targetFD == -1)
            {
                perror("target open()");
                fflush(stdout);
                latestStatus = 1;
                exit(1);
            }
            // Use dup2 to point FD 1, i.e., standard output to targetFD
            int result = dup2(targetFD, 1);
            if (result == -1)
            {
                perror("dup2\n");
                fflush(stdout);
                latestStatus = 2;
                exit(2);
            }
        }
        if (inCommand.inputFile)
        {
            // from https://repl.it/@cs344/54sortViaFilesc
            // Open source file
            int sourceFD = open(inCommand.inputFile, O_RDONLY);
            if (sourceFD == -1)
            {
                perror("source open()");
                fflush(stdout);
                latestStatus = 1;
                exit(1);
            }

            // Redirect stdin to source file
            int result = dup2(sourceFD, 0);
            if (result == -1)
            {
                perror("source dup2()");
                fflush(stdout);
                latestStatus = 2;
                exit(2);
            }
        }

        execvp(inCommand.command, inCommand.args);
        // exec only returns if there is an error
        perror("execvp\n");
        latestStatus = 2;
        exit(2);
        break;
    default:
        // in parent process
        if (inCommand.bgProcess && runForeground)
        {
            // run it in the background, from explorations on processes
            waitpid(spawnPid, &childStatus, WNOHANG);
            printf("background pid is %d\n", spawnPid);
            fflush(stdout);
            // add it to our global variables
            runningPIDS[numRunning] = spawnPid;
            numRunning++;
        }
        else
        {
            // wait for child termination
            waitpid(spawnPid, &childStatus, 0);
        }

        // from video by Prof Brewster
        // https://www.youtube.com/watch?v=VwS3dx3uyiQ&list=PL0VYt36OaaJll8G0-0xrqaJW60I-5RXdW&index=18&ab_channel=BenjaminBrewster
        if (WIFEXITED(childStatus))
        {
            latestStatus = WEXITSTATUS(childStatus);
        }
        else
        {
            // child was terminated by a signal
            latestStatus = WTERMSIG(childStatus);
        }

        // look for finished processes
        for (int i = 0; i < numRunning; i++)
        {
            // https://www.youtube.com/watch?v=1R9h-H2UnLs&list=PL0VYt36OaaJll8G0-0xrqaJW60I-5RXdW&index=16&ab_channel=BenjaminBrewster
            //"check if process specified has completed, return immediately with 0 if it hasn't"
            int childExitMethod = -5;
            int childPID = waitpid(runningPIDS[i], &childExitMethod, WNOHANG);

            if (childPID != 0)
            {
                printf("backgound pid %d is done: ", runningPIDS[i]);
                fflush(stdout);
                if (WIFEXITED(childExitMethod))
                {
                    printf("exit value %d\n", WEXITSTATUS(childExitMethod));
                    fflush(stdout);
                }
                else
                {
                    // was term'd by a signal
                    printf("terminated by signal %d\n", WTERMSIG(childExitMethod));
                    fflush(stdout);
                }

                // remove from array
                for (int j = i; j < numRunning; j++)
                {
                    runningPIDS[j] = runningPIDS[j + 1];
                }
                runningPIDS[numRunning] = 0;
                numRunning--;
            }
        }

        break;
    }
}

// nextLine:
// prints a colon and gets the command from the user and directs
// it as appropriate to a command
void nextLine(int inpid)
{
    char inString[2048];
    char *token;
    struct commandInfo newCommand;
    int continueBool = 1;
    while (continueBool)
    {
        printf(": ");
        fflush(stdout);

        fgets(inString, 2048, stdin);
        char *saveStr = inString;

        if (saveStr[0] == '#')
        {
            continueBool = 1;
            continue;
        }

        else if (saveStr[0] == ' ' || saveStr[0] == '\n')
        {
            continueBool = 1;
            continue;
        }

        else
        {
            // its safe to grab the tokens
            token = strtok_r(saveStr, " \n", &saveStr);
            newCommand.bgProcess = 0;
            int argCount = 0;

            // set command and first argument to command
            newCommand.command = token;
            newCommand.args[0] = token;
            argCount++;
            newCommand.inputFile = NULL;
            newCommand.outputFile = NULL;

            while ((token = strtok_r(saveStr, " \n", &saveStr)))
            {
                if (token[0] == '<')
                {
                    // process in file
                    token = strtok_r(saveStr, " \n", &saveStr);
                    newCommand.inputFile = token;
                }
                else if (token[0] == '>')
                {
                    // process out file
                    token = strtok_r(saveStr, " \n", &saveStr);
                    newCommand.outputFile = token;
                }
                else if (token[0] == '&')
                {
                    newCommand.bgProcess = 1;
                }
                else
                {
                    // add to arguments
                    fflush(stdout);
                    newCommand.args[argCount] = token;
                    argCount++;
                }
            }

            // add null to the end of args
            newCommand.args[argCount] = NULL;
        }

        // direct to correct functions
        if ((!strcmp(newCommand.command, "cd")))
        {
            cdCommand(newCommand);
        }

        else if ((!strcmp(newCommand.command, "exit")))
        {
            exitCommand(newCommand);
            continueBool = 0;
        }

        else if ((!strcmp(newCommand.command, "status")))
        {
            printf("exit value %d\n", latestStatus);
            fflush(stdout);
        }

        else
        {
            otherCommand(newCommand);
        }
    }
}

int main()
{
    signal(SIGTSTP, catchSIGTSTP);
    signal(SIGINT, SIG_IGN);

    int myPid = getpid();
    nextLine(myPid);

    return 0;
}
