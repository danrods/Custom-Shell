#include "320sh.h"


// Assume no input line will be longer than 1024 bytes


int finished = 0;
int exitCode = EXIT_OK;

/*int argc, char ** argv, char **envp*/
/*envp = array of strings of the form key=value*/
int main (int argc, char ** argv, char **envp) {

  int finished = 0;
  int exitCode = 0;
  const char *prompt = "320sh> ";
  

  /*  pid_t getpid()
      pid_t getppid()
  */



  while (!finished) {

    
    int rv;
    

     argc++;
     argv++;
    printEnvVar(envp, "PATH=");

    // Print the prompt
    rv = write(1, prompt, strlen(prompt)); /*Writes to STDOUT*/
    if (!rv) { 
      finished = 1;
      break;
    }
    
    cmdP executable = (cmdP) calloc(1, sizeof(Com));

    executable = readCommand(rv, executable);

    debug("Validating for Program : %s\n", executable->program);

    for(char** argv = (executable->tacks);*argv!=NULL|| *argv !='\0'; argv++){
        debug("Adding Tacks : %s\n", *argv);
    }
    

    if (!rv) { 
      finished = 1;
      break;
    }


    // Execute the command, handling built-in commands separately 
    // Just echo the command line for now
    //write(1, (executable->program), strnlen((executable->program), MAX_INPUT));

    free(executable);
  }

  return exitCode;
}


pid_t forkProcess(){
  pid_t returnVal;
  if((returnVal = fork()) < 0){
    error("Error trying to fork a process : %s", strerror(errno));
    exit(EXIT_FORKFAIL);
  }

  return returnVal;
}

void printEnv(char **envp){
  printEnvVar(envp, NULL);
  /*for(char** str= envp ;*str != NULL; str++){
    debug("ENV Variable ~~~> : %s\n", *str);
  }
  */
}

void printEnvVar(char** envp, char* var){
  if(var == NULL){ /*Just print all of them*/
    for(char** str= envp ;*str != NULL; str++)
      debug("ENV Variable ~~~> : %s\n", *str);
  }
  else{ /*Else print a specific one*/
    char* str = getPrintVar(envp, var);
    debug("ENV Variable ~~~> : %s\n", str);
  }
}

/**/
char * getPrintVar(char**envp, char* var){
  int index;
  if((index = getPrintVarIndex(envp, var)) > 0){
    return envp[index];
  }

  return NULL;
}


/*Gets the index of the environment variable we're searching for*/
int getPrintVarIndex(char**envp, char* var){
    int i;
    char** str;
   for(i = 0,str= envp ;*str != NULL; str++, i++){
      if(strstr(*str, var) == *str){ 
        return i;
      }
   }

   return -1;
}


/**
* This method will do the hard work of reading commands from the user and forming a structure
*/
cmdP readCommand(int rv, cmdP exec){

    char *cursor;
    char last_char;
    char inputBuf[MAX_INPUT];
    int count;
    const char* delim = " ";

  // read and parse the input
    for(rv = 1, count = 0, cursor = inputBuf, last_char = 1;
        rv && (++count < (MAX_INPUT-1)) && (last_char != '\n');
        cursor++) 
    { 

      rv = read(0, cursor, 1);
      last_char = *cursor;

    } 
    /*TODO : Move cursor back 1 point because null terminates on wrong place*/
    if(count > 1)*(--cursor) = '\0';

    int counter = 0;
    char * token = strtok((cursor=inputBuf), delim);
    debug("Tokenized the input : %s\n", token);

    if(token == NULL) {
      exec->valid = false;
      return exec;
    }
    else {
      strncpy((exec->program), token, ((size_t)MAX_INPUT/2));
      (cursor = cursor + (counter += strlen(token) + 1));
      exec->valid = true;
    }
    char** argv = (exec->tacks);
    for(;counter < count;(cursor = cursor + (counter += strlen(token) + 1))){
      token = strtok(NULL, delim);
      if (token == NULL) break;
      debug("Tokenized the input : %s\n", token);

      *(argv) = token;
      argv++;
    }

    return exec;
}