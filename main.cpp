/**************************************************************************************************************
Written by: Alan Doose
Oct 8 2018

Purpose: This program simulates a unix shell that can execute unix shell scripts. This is accomplished
  by parsing the string entered by the user and passing that command to the execute function which forks
  a child process which in turn executes the command with the execvp() system call.
  Special commands included are:
   'history' - modified from the unix command, this will now print only the 10 most recent commands entered
   '!!' - will execute the most recent command in the history, if one exists
   '!#' - will execute command number # in the history list, if such a command exists
   'exit' - will exit this shell. Alternatively can press ctrl-c

Assumptions: It is assumed that this program will be compiled and executed in a unix 
 environment. It is also assumed that the user will have some familiarity with unix commands. 
*************************************************************************************************************/

#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_LINE 80 //max length of a command
#define MAX_HISTORY 100 //max slots in the history array

/* Prototypes */
bool string_compare(const char* s1, const char* s2);
void execute(char* cmd[], bool waiting);
void parse_input(char input[], char* args[], bool& shouldWait);
void print_history(char* history[], int hc);


//This is the main process that serves as a driver for my unix shell.
// Main algorithm: inside of an infinite loop, this program displays a shell prompt 'dsh>' into which a user can enter a unix shell script.
//  Any input is first checked for the special commands '!!' '!n' and 'history' :
//   '!!' - executes the most recent command in the history (if one exists, otherwise displays an error message)
//   '!n' - executes the nth command in the history, if such a command exists (otherwise displays an error message)
//   'history' - overrules the built-in unix history command and only displays the 10 most recent commands that have been entered by the 
//     user, and shows the order they were entered in
// If no special command was entered, the input string is parsed by the parse_input() function and then executed as a child process
//  in the execute() function. Any command entered is logged in the history[] array as a raw string. If a command is recalled from history, that
//  command is parsed and executed again, then added to the history again.
// To exit the shell, simply type 'exit'
int main()
{
  char* args[MAX_LINE/2 + 1]; //Command line args (specified from book)
  char input[MAX_LINE]; //Holds the input from the user before being parsed into the command args array
  char* history[MAX_HISTORY]; //array holding up to 10 commands entered by the user
  int history_count = -1; //counter for numbering the commands in the history list

  int should_run = 1; //Flag to determine when to exit
  bool should_wait = true; //Flag to signal whether the parent process will wait for the child to complete
  bool need_fork = true; //Flag to signal if the command needs to fork a child process (e.g. 'history' does not need to fork)

  while(should_run)
    {
      printf("dsh>"); //shell prompt
      
      //read user input
      fflush(stdout);
      std::cin.clear(); //clear any error flags
      std::cin.getline(input, MAX_LINE); //read input

      if(strlen(input) == 0) //if user enters a blank string, i.e. just presses enter, don't process anything just restart the loop (prevent storing blank  commands in the history)
	{}
      else if(string_compare(input, "exit")) //check if command is exit, and set flag to end program if so
	{
	  should_run = 0;
	}
      else //else some other command string was entered so process it
	{
	  need_fork = true; //by default, any command will need to fork a child process
	  should_wait = true; //by default, the parent will wait on the child process unless the user enters & at the ned of a command

	  //check special commands before parsing string for a unix shell command
	  if(string_compare(input, "!!"))
	    {
	      //execute last command in history if at least one command has already been entered
	      if(history_count != -1)
		{
		  strcpy(input, history[history_count]); //copy the most recent command into input[] to be parsed again by the parse_input function
		  printf("%s\n", input); //echo the command fetched from history before executing it
		}
	      else //if no commands are in the history, notify user
		{
		  printf("No commands in history!\n");
		  need_fork = false; //do not fork a child for a null command
		}
	    }
	  else if(input[0] == '!') //if user is trying to re-enter a specific command from the history
	    {
	      int num = 0;
	      //execute specified command from history if it exists
	      if(input[1]) //if the user entered a number after the !, e.g. !4
		{
		  num = atoi(&input[1]); //convert the number to an int to use to fetch the command from history
		  num--; //decrement num by one since array access is 0 thru MAX-1 not 1 thru MAX as shown in 'history'
		}
	      
	      if(num <= history_count && num > -1) //if the number corresponds to a valid entry in the history
		{
		  strcpy(input, history[num]); //copy the command from history to input[] so it can be parsed in parse_input
		  printf("%s\n", input); //echo the command fetched from history before executing it
		}
	      else //else the entered number does not match a command in the list
		{
		  printf("There is no command numbered %d in the history.\n", num+1);	      
		  input = NULL;
		}
	    }
	  
	  // If user wants to see history, show last 10 and swap flag to not fork a process
	  if(string_compare(input, "history"))
	    {
	      if(history_count == -1) //history empty
		{
		  printf("No commands in history!\n");
		}
	      else
		{
		  //print last 10 commands in history
		  print_history(history, history_count);
		  need_fork = false; //no need to fork a process to display history
		}
	    }
	  else //no special history command, so parse string normally as a unix shell command
	    { 
	      parse_input(input, args, should_wait);
	    }     
	  
	  //add command to the history array with wraparound on the array
	  if(history_count >= MAX_HISTORY)
	    history_count = -1; //if hit max history count, reset and overwrite old commands
	  history[(++history_count % MAX_HISTORY)] = strdup(input); //duplicate the command string and store a pointer to it in the history array
	  
	  if(need_fork) //will not fork a child if the command entered was 'history'
	    {
	      //fork a child process to execute the command
	      execute(args, should_wait);
	    }
	}
    }
  return 0;
}//end main

// Forks a child process to execute the given command
//
// Params:
//  char* cmd[] - the string array of command arguments that will be passed to execvp()
//  bool waiting - the flag to signal whether the parent process should wait for the child to complete the given command before returning
void execute(char* cmd[], bool waiting)
{
  pid_t child = fork();

  if(child < 0) //error creating child process
    {
      printf("Fork failed!\n\n");
      exit(1);
    }
  else if(child == 0) //this is the child process
    {
      //child executes the command
      if(!execvp(cmd[0], cmd) < 0) //execvp returns -1 on failure
	{
	  printf("Error executing command!\n"); 
	}
    }
  else //this is the parent process
    {
      if(waiting) //if command does not include &, waiting flag will be set to true
        {
          int status; //kept this in case need to read a return code later
	  while(wait(&status) !=  child) //parent waits for child to complete
	    ;
        }
    }
}

// Parses an input string into tokens and places them into the args string array which is passed by reference
//   Utilizes char* strtok(char* newstring, const char* delimiters) from string.h, see documentation: http://www.gnu.org/software/libc/manual/html_node/Finding-Tokens-in-a-String.html
//
// Params:
//  char input[] - the input string that will be parsed into individual string tokens.
//  char* args[] - the array of string to hold the parsed tokens. Each args[0] will be the command, args[1] thru args[n-1] will be the arguments
//    that accompany the command, and the last slot args[n] will be NULL ( as required by execvp() ).
//  int& shouldRun - the flag passed by reference that tells the main loop if the user entered 'exit' (in which case shouldRun will be set to 0).
//  bool& shouldWait - the flag passed by reference that tells the main loop if the user entered '&' at the end of the string, in which case
//    the execute() function will have the parent NOT invoke wait() after a child is forked.
void parse_input(char input[], char* args[], bool& shouldWait)
{    
  //parse string
  const char delimiters[] = " \n"; //delimiters array for strtok function, tells strtok where to split the tokens
  char* copy; //copy of input string (strtok modifies the original string)
  int k = 0; //index to step through the args array
  
  copy = strdup(input); //copy the input string so it is not overwritten
    
  if((args[k++] = strtok(copy, delimiters))) //grab first token which will be the first characters in the string until a delimiter from the delimiters array is hit
    {
      while((args[k] = strtok(NULL, delimiters))) //grabs subsequent tokens until it hits the end of the string, returns NULL at the end so the last slot of the args array will contain NULL (which is how execvp wants its argument array parameter terminated)
	{
	  if(string_compare(args[k], "&")) //check for & to set should_wait flag
	    {
	      shouldWait = false; //change parent wait flag
	      *args[k] = ' '; //replace & with a blank space so it is overwritten with a null terminator next loop (flag is set so we don't want this in the command when it's executed because it will read a garbage argument)
	      k--; //cancel out moving up an index since this slot will now get a null terminator instead of &
	    }
	  k++; //move to next slot in the args array
	}    
    }
}

// Prints the 10 most recent commands in the command history
// Params:
//  char* history[] - the array of strings from which the 10 most recent will be printed
//  int hc - the history_count which should be passed from main() so that the print loop can count 10 entries down from the most recent entry
void print_history(char* history[], int hc)
{
  //print last 10 commands in history
  printf("Command history:\n");
  for(int count = 10; hc > -1 && count > 0; count--, hc--)
    {
      printf("%d %s\n", hc+1, history[hc]); //history entries are stored normally in the array, so hc+1 will display 1 thru history_count while printing the corresponding commands
    }
}

// Utility function - Compares 2 c strings and returns true if they match.
//
// Params:
//  s1 & s2 are the C-style strings that will be compared for equality
// Return value:
//  true if the strings are equal, false otherwise
bool string_compare(const char* s1, const char* s2)
{
  //check lengths of strings
  int a, b;
  a = b = 0;
  while(s1[a] != '\0' || s2[b] != '\0')
    {
      if(s1[a] != '\0')
        a++;
      if(s2[b] != '\0')
        b++;
    }
  //if the strings are not the same length they cannot be equal so return false
  if(a != b)
    return false;

  //if equal length, check char by char for equality
  for(int i = 0; s1[i] != '\0' && s2[i] != '\0'; i++)
    {
      if(s1[i] != s2[i])
        return false;
    }
  return true;
}
