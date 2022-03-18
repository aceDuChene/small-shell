To compile, type into the linux terminal
gcc -g -std=gnu99 main.c -o smallsh

Then to run, type in the terminal
./smallsh

To run the test script, you may have to 
change the permissions, then run it:
chmod +x ./testscript
./testscript 2>&1

This is smallsh, an assignment done for CS344
   Operating Systems I for Oregon State University Ecampus.
   This specific implementation has been edited down from
   the original requirements.
   A shell that implements a subset of features of well-known
   shells. As listed on the assignment description, it will
      1. Provide a prompt for running commands
      2. Handle blank lines and comments (beginning with #)
      3. Execute exit, cd, and status via code built into shell
      4. Execute other commands by creating new processes using
          a function from the exec family of functions
      5. Support input and output redirection
      6. Can run foreground commands & background processes
      7. Implement custom handlers for 2 signals SIGINT & SIGTSTP