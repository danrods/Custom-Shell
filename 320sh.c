#include "320sh.h"


// Assume no input line will be longer than 1024 bytes


int finished = 0;
int exitCode = EXIT_OK;
char pathVal[MAX_PATH]; 
const char *prompt = "320sh> ";

/*int argc, char ** argv, char **envp*/
/*envp = array of strings of the form key=value*/
int main (int argc, char ** argv, char **envp) {

  int finished = 0;
  int exitCode = 0;
  int opt;
  memset(pathVal, 0, MAX_PATH);
  strcat(pathVal, prompt);
  signal (SIGTTIN, SIG_IGN);
  

    while((opt = getopt(argc, argv, "d")) != -1) {
        switch(opt) {
                break;
            case 'd':
                 STDERR = true;
                break;
            default:
                /* A bad option was provided. */;
                debug("-d has been added \n");
                break;
        }
    }

  /*  pid_t getpid()
      pid_t getppid()
  */
  

  while (!finished) {

    
    int rv;
    
    //printEnv(envp);
    
    // Print the prompt
    rv = write(1, pathVal, strlen(pathVal)); /*Writes to STDOUT*/
    if (!rv) { 
      finished = 1;
      break;
    }
    
    cmdP executable = (cmdP) calloc(1, sizeof(Com));

    executable = readCommand(rv, executable);

   // debug("Validating for Program : %s\n", executable->program);

    for(char** argv = (executable->tacks);*argv!=NULL|| *argv !='\0'; argv++){
 //       debug("Adding Tacks : %s\n", *argv);
    }

    /* char* path = getProgramFromPath(envp, executable->program);*/
  //  debug("Before Launch\n");
    launchExecutable(envp,executable);
    //debug("After Launch\n");
    

    if (!rv) { 
      finished = 1;
      break;
    }


    // Execute the command, handling built-in commands separately 
    // Just echo the command line for now
    //write(1, (executable->program), strnlen((executable->program), MAX_INPUT));

    //free(executable->program);
    free(executable);
  }

  return exitCode;
}



/***************************END MAIN*********************************/




/**
* This method will do the hard work of reading commands from the user and forming a structure
*/
cmdP readCommand(int rv, cmdP exec){

    char *cursor;
    char last_char;
    //char inputBuf[MAX_INPUT];
    char* inputBuf = malloc(MAX_INPUT);
    int count;
    const char* delim = " ";

    memset(inputBuf,0,MAX_INPUT);
  // read and parse the input
    for(rv = 1, count = 0, cursor = inputBuf, last_char = 1;
        // rv && (++count < (MAX_INPUT-1))
         (last_char != '\n');
        cursor++,count++) 
    { 


      
      last_char = Read(cursor,inputBuf);
      
      // rv = read(0, cursor, 1);
      // last_char = *cursor;


    }
    if(count > 1)*(--cursor) = '\0';
    int counter = 0;
    char * token = strtok((cursor=inputBuf), delim);
//    debug("Tokenized the input : %s\n", token);

    if(token == NULL) {
      exec->valid = false;
      return exec;
    }
    else {
      strncpy((exec->program), token, ((size_t)MAX_INPUT/2)); //Copys the program token into the struct
      *(exec->tacks) = token; //Copies the program into the argv array
      exec->valid = true;
      exec->tackCount = 1;
    }

    char** argv = (exec->tacks);
    argv++; /*Skip the first line because we've added the program name*/
    for(;counter < count;){
      token = strtok(NULL, delim);
      if (token == NULL) break;
 //     debug("Tokenized the input : %s\n", token);

      *(argv) = token;
      (exec->tackCount)++;
      argv++;
    }

    return exec;
}


char Read(char *cursor, char inputBuf[]) {
      char symbol = getchar();
      switch(symbol)
      {
        case '\177': /* Ascii number in octal for the delete key Macs rule*/
        case '\b': 
          // if(cursor != inputBuf)
            *(--cursor) = '\0';
          printf("\r\033[K%s%s", prompt,inputBuf);
          return *cursor;
        case '\t':
          //TODO: add tab functionatlity
          break;
      }
      putchar(symbol);
      *cursor = symbol;
      return *cursor;
}




/***************************ENV VARIABLE FUNCTIONS*******************************/


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
      if(strstr(*str, var) == *str){ /*Is the substring var in str == start of str*/
        return i;
      }
   }

   return -1;
}

char * getValueFromKey(char ** envp, char * key){
    //int index = getPrintVarIndex(envp, key);
    bool nothing;
    if(envp == envp + 1){
      nothing = false;
    }

    
  return getenv(key);
}


void setVariable(char* key, char* newValue){
    debug("Current -> %s variable :::: %s\n", key, getenv(key));
    unsetenv(key);
    debug("After unset -> %s variable :::: %s\n", key, getenv(key));
    setenv(key, newValue, 1);
    debug("After set env ->  %s variable :::: %s\n", key, getenv(key));
}



/**
* This method will parse the PATH env variable path's until it finds the program that was passed in.
* Do not call if program is using a relative path.
*/
char* getProgramFromPath(char** envp, cmdP command){

    //debug("Entering Program");
    bool isFound = false;
    char* value = getValueFromKey(envp, PATH_VAR);

    char* path = (char*) malloc(sizeof(char) * strlen(value) + 1);
    strcpy(path, value);

    const char* original = path;

    path = strtok(path,PATH_DELIM);

    if(path !=NULL){
        while(path != NULL){

 //         debug("Checking Path : %s\n", path);
          if(checkIfProgramExists(path, command)){
            isFound = true;
            break;  
          }
          else{
            path = strtok(NULL,PATH_DELIM);
          }   
          
        }
        
    } 

    free((void *)original);
    if(isFound) return command->program;

    error("Error getting program from Path\n");
    return NULL;
}



/***************************ENV VARIABLE FUNCTIONS END*******************************/







/***************************SHELL BUILT-IN FUNCTIONS *******************************/

void cd(char** envp, cmdP exec){
    bool isChange = false;
    char* newPath = "";
    char buffer[MAX_PATH];
    char *oldwd = getcwd(buffer,MAX_PATH);


    char** argv = exec->tacks;
    //char* oldwd = getValueFromKey(envp, WD_VAR);
    if(oldwd == NULL){
      error("Error %d getting current working directory : %s\n", errno, strerror(errno));
    }

    if(argv[1] == NULL){ /*The only argument is the program, i.e should be the command 'Cd'  with no arguments*/
        newPath = getValueFromKey(envp, HOME_VAR);
        isChange = true;
    }
    else{
        switch(argv[1][0]){ /*Get the second argument in argv, the first element should be a . , .. , -  */
          case '-':
              if(argv[1][1] == '\0'){
                debug("Executing Command Cd -\n");
                newPath = getValueFromKey(envp, OLDPWD_VAR);
                isChange = true;
              }
              else{
                error("Invalid input : %s" , argv[1]);
              }
              break;
          case '.':

              switch(argv[1][1]){

                  case '.':
                  /*We have a Cd ..*/
                    debug("Executing Command Cd ..\n");
                    char* oldPath = getValueFromKey(envp, WD_VAR);

                    if(strlen(oldPath) == 1){/*We're at the root*/
                      error("Can't go beyond root\n");
                    } 
                    else
                    {
                        char* index = strrchr(oldPath, '/');

                        if(index > 0 && oldPath!= index){ /*Keep removing values until there's only one slash left*/
                          *(index) ='\0';
                        }
                        else if(oldPath == index){/*If there's one slash left just remove everything after it*/
                          if(strlen(oldPath) > 1){
                            *(index + 1) = '\0';
                          }
                        }
                    }
                      newPath = oldPath;
                      debug("Changing path to %s\n from %s\n", newPath, oldwd);
                      isChange = true;
                    break;
                

                  case '\0':
                    debug("Executing Command Cd .\n");
                      //newPath = getValueFromKey(envp, WD_VAR);
                      isChange = false;
                      break;
                  case '/': //Relative Paths
                      newPath = argv[1];
                      isChange = true;
                      break;
                  default:
                    error("Invalid path : %s" , argv[1]);
                    break;
              }
  
              break;
          
          case '/':
              /*This is an absolute path*/

              if(! checkIfPathExists(argv[1])){
                 procError("cd : %s -> No such file or directory\n", argv[1]);
              }
              else{
                newPath = argv[1];
                isChange = true;
              }

              break;
          default:
          {
            //if(argv[1][1] > 'a' && argv[1][1] < 'z')
            char newStr[(strlen(newPath) + strlen(oldwd) + 1)];
            strcat(newStr, oldwd);
            strcat(newStr, "/");
            strcat(newStr, newPath);
            newPath = newStr;
            isChange= true;       
            break;
          }
        }
  
    }

    if(isChange){
      if(chdir(newPath) < 0){
        procError("cd : %s -> No such file or directory\n", newPath);
        error("There was a problem changing the internal directory to %s\n", newPath);
        return;
      }
      else{
        setVariable(WD_VAR ,newPath); /*Reset the current working directory variable*/
        setVariable(OLDPWD_VAR ,oldwd); /*Reset the current working directory variable*/
      }

      char *newPath = getcwd(buffer,MAX_PATH);
      
      memset(pathVal, 0, MAX_PATH);
      strcpy(pathVal, "[");
      strcat(pathVal, newPath);
      strcat(pathVal, "]:");  
      strcat(pathVal, prompt);
    }

}

char* replaceHome(char** envp, char** oldPath){
    char* home = getValueFromKey(envp, HOME_VAR);
    home = "";
    oldPath++;
    return home;
}




void executePwd(void) {
  size_t size = MAX_INPUT;
  char buffer[size + 1];
  char *cwd = getcwd(buffer,size+1);
  if(cwd != NULL)
      printf("%s\n", buffer);
  else
      perror("Bad directory");
}


/***************************SHELL BUILT-IN FUNCTIONS END*******************************/









/***************************PARSING/EXECUTING************************************/


pid_t forkProcess(){
  pid_t returnVal;
  if((returnVal = fork()) < 0){
    error("Error trying to fork a process : %s", strerror(errno));
    exit(EXIT_FORKFAIL);
  }

  return returnVal;
}




bool isTypeBuiltin(char *program)
{
  /*Check if command is a builtin command*/
  /* Have a list of functions we support 
  * this method will check the list to see if program name is supported
  *
   */
    int i = 0;
    while(builtInMethods[i] != '\0')
    {
      if(!strcmp(program,builtInMethods[i++]))
      {
        return 1;
      }
    }
  return 0;
}




void launchExecutable(char** envp, cmdP exe){
    /*In this section of code I'm going to be taking my exe and checking
    * if it is not a child process then checking using the execve if the program is in
    * built in or not if it for now i only care if it isn't
    */
    
    if(!exe->valid){
        procError("%s : Command Not found\n", exe->program);
        return;
      }

    stderrRun(exe->program); 
    if(isTypeBuiltin(exe->program))/*This will return 1 for the time being*/
    {
 //         debug("Check if function is built in \n");
          executeInternalProgram(envp,exe);
    }
    /*function is not built-in and should be called using a child process*/
    else
    {
//      debug("Execute non built-in program\n");
      executeExternalProgram(envp, exe);
    }
    stderrEnd(exe->program,EXIT_OK);
}




void executeInternalProgram(char** envp, cmdP exe){
    if(!strcmp(exe->program,"cd"))
    { 
      if(! envp)
      {
        error("Cd may need this\n");
      }
      else{
//        debug("Executing cd\n");
        cd(envp, exe);
      }
      
    }
    else if(!strcmp(exe->program,"pwd"))
    { 
      debug("Executing pwd\n");
      executePwd();
    }
    else if(!strcmp(exe->program,"echo"))
    { 
      debug("Executing echo\n");
    }
    else if(!strcmp(exe->program,"exit"))
    {
      stderrEnd(exe->program,EXIT_OK);
      exit(EXIT_OK);
    }
    else
      debug("This should never happen");
}





void executeExternalProgram(char** envp, cmdP exe){
    int wpid = 0;
    pid_t process = forkProcess();

    if(process == 0) /* This means it is a child process and i should laundh some stuff*/
    {
        /*Should probably use stat to check if the file exist*/
           // char *args[] = {"ls", "",NULL}; 
           /*Figure out how to take into consideration they providing their own directory*/
           char * programPath = getProgramFromPath(envp,exe);
           debug("Starting program %s with program path %s\n", programPath, exe->program);
           if(execvp(programPath,exe->tacks) == -1) { 
                procError("%s : Command Not found\n", exe->program);
                stderrEnd(exe->program,EXIT_FAILURE);
                exit(EXIT_FAILURE);
              }
           //debug("You should do this smoothly\n");
           exit(EXIT_OK);
    }
    else
    {
      wait(0);/*Having a 0 as input forces the main thread to wait for all child processes to finish before reexecuting*/
      if(WIFEXITED(wpid))
      {
  //      debug("did it work\n");
      }
     /* else
         error("Shit out of luck");*/
    }
}




bool checkIfProgramExists(char* path, cmdP command){
    char *programPath = malloc(strlen(path) + strlen(command->program) + 2);

    strcat(programPath, path);
    strcat(programPath,"/");
    strcat(programPath, command->program);

    
    bool isContained = checkIfPathExists(programPath);

    if(isContained){
      strcpy(command->program, programPath);
    }
    
    free(programPath);

    return isContained;
}

bool checkIfPathExists(char* path){
  struct stat *inbuf = malloc(sizeof(struct stat)); 
  memset(inbuf, 0, sizeof(struct stat));
  int input_stat = stat(path, inbuf);

  bool isContained = input_stat >= 0 ? true : false;
  free(inbuf);
  debug("Does Path Exist --> %d\n", isContained);
  return isContained;
}



