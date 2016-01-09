#include "320sh.h"



#ifdef DEBUGFLAG
    VERBOSE_LEVEL DEBUG = DEV;
#else 
    VERBOSE_LEVEL DEBUG = NONE;
#endif



// Assume no input line will be longer than 1024 bytes

bool STDERR = false;
bool isBatchMode = false;
char pathVal[MAX_PATH];
procP headProcess = NULL;
procP foregroundProcess = NULL;
int currentJobId = 0;
histListP historyHead = NULL; 
histListP lastKnownhistListPSpot = NULL;
const char *prompt = "320sh> ";
const char* soloBuiltInMethods[Total_solo_builtin] = {"cd", "kill", "bg", "fg", "exit", "jobs","set","clear-history"};
const char *builtInMethods[Total_BuiltIns] = {"pwd","echo", "history"};
const char* STATUS[] = {"Running", "Stopped", "Killed", "Done"};
volatile sig_atomic_t flag = 0;

/*int argc, char ** argv, char **envp*/
/*envp = array of strings of the form key=value*/
int main (int argc, char ** argv, char **envp) {

  int finished = 0;
  int exitCode = EXIT_OK;
  int batch_index;
  memset(pathVal, 0, MAX_PATH);
  strcat(pathVal, prompt);
  
    struct sigaction new_action;
    new_action.sa_handler = &signal_interrupt;
    sigaction(SIGINT,&new_action,NULL);
      // signal(SIGINT,signal_interrupt);
      // signal(SIGTSTP,signal_pause);

    //int index = getPrintVarIndex(envp, key);
    bool nothing;
    if(envp == envp + 1){
      nothing = false;
    }

    batch_index = parse_options(argc, argv);
    readHistory();
    // char buffer[MAX_INPT];
    // char *cwd = getcwd(buffer,MAX_INPT);
    //  BuildTrie(cwd);

  while (!finished) {

    
    int rv = 1;
    
    //printEnv(envp);
    
    // Print the prompt
    if(! foregroundProcess){
        rv = Write(1, pathVal, strlen(pathVal)); /*Writes to STDOUT*/
    }
    
    if (!rv) { 
      finished = 1;
      break;
    }

    if(isBatchMode){
        batchMode(rv, batch_index, argv);
        finished = true;
    }
    else
    {

      cmdP executable = (cmdP) calloc(1, sizeof(Command));
      executable = readCommand_stdin(rv, executable);
      executeAndFree(executable);
    }
      
      

      if (!rv) { 
        finished = 1;
        break;
      }


    // Execute the command, handling built-in commands separately 
    // Just echo the command line for now
    //write(1, (executable->program), strnlen((executable->program), MAX_INPT));


    
  }//End While(!finished)

  /*************************************/
  /*          FREE DATA SECTION        */
  /*************************************/

  //freeHistory();




  return exitCode;
}

void executeAndFree(cmdP executable){  
    if(! executable) return; // If Executable is null, we're done 
    launchExecutable(executable);
    if(executable->isInfinite)
      printProcess(foregroundProcess);
    
    //free(executable->tacks[0]);
    //free(executable);

}

void batchMode(int rv, int batch_index, char** argv){
        if(batch_index < 0) return;
        char* fileName = argv[batch_index];
        int batch_fd = OpenIgnoreMode(fileName, O_RDONLY);
        
        if(batch_fd > 0){

          bool continueRunning = true;
          ssize_t readBytes;
          char buffer[BUFFER_SIZE];
          while((readBytes = Read(batch_fd, buffer, BUFFER_SIZE)) > 0 && continueRunning){ //Stop when read bytes == 0

              if(readBytes == 0){
                break;
              }
              else if(readBytes < BUFFER_SIZE)
              {
                  if(strstr(buffer, "\n") == NULL){
                    buffer[++readBytes] = '\n';
                  }
              }

              char last_char = '\0';
              int i;
              for(i = 0; readBytes > 0 && (last_char != '\n'); i++, readBytes--) 
              { 
                last_char = buffer[i]; 
              }

              *(buffer + i - 1) = '\0';
              Lseek(batch_fd, (~readBytes) + 1, SEEK_CUR); //Push the amount of bytes we didn't use back so we can re-read them

              if(strstr(buffer, "#") == buffer){ //If it starts with a # ignore that shit
                  //debug("Skipping the crunchbang\n");
                  continue;
              }

            
            cmdP executable = (cmdP) calloc(1, sizeof(Command));
            executable = readCommand_buffer(rv, executable, buffer);
            executeAndFree(executable);
          }//End While loop


          Close(batch_fd);
        } 

}



int parse_options(int argc, char** argv){
    int opt;
    int returnValue = -1;

    while((opt = getopt(argc, argv, "d")) != -1) {
        switch(opt) {
            case 'd':
                  STDERR = true;
                  debug("-d has been added \n");
                  break;
            case 't':
                  debug("-t has been added \n");
                  break;
            case '?':
                  debug("Passed value -> ?\n");
                  debug("Option Argument : %s\n",optarg);
            default:
                break;
        }
    }

    if(optind < argc && (argc - optind) == 1) { /*Check to see if there are arguments not corresponding to tacks*/
        isBatchMode = true;
        returnValue = optind;
        debug("Found Batch File : %s\n", argv[returnValue]);
    } 

    return returnValue;
}




/***************************END MAIN*********************************/




/**
* This method will do the hard work of reading commands from the user and forming a structure
*/
cmdP readCommand(int rv, cmdP exec, char* buffer){

    int count;
    
    char* inputBuf = malloc(MAX_INPT);
    memset(inputBuf,0,MAX_INPT);


    if(buffer){
      strcpy(inputBuf, buffer);
      count = strlen(buffer);
    }
    else{

        count = ReadBuffer(rv,inputBuf);
       
    }

    if(count > 0) {
        if(count == 1 && inputBuf[0] == '\n') return NULL; //Ignore empty lines

        char* val = calloc(1 ,count + 1);//One for null terminator
        strncpy(val, inputBuf, count + 1);
        exec->originalStr = val;

        parseBuffer(inputBuf, exec, count);
        writeHistory(exec);
        replaceVariables(exec); //TODO : Fix replace Variables to accept multiple Commands
        glob(exec); //TODO : Fix Glob to accept multiple Commands
    }
    else{
        return NULL;
    }

    return exec;
}

cmdP readCommand_buffer(int rv, cmdP exec, char* buffer){
  return readCommand(rv, exec, buffer);
}

cmdP readCommand_stdin(int rv, cmdP exec){
  return readCommand(rv, exec, NULL);
}


cmdP parseBuffer(char* inputBuf, cmdP exec, int count){
   
    char* cursor;
    //int counter = 0;
    const char* delim = " ";
    char * needle = "&";

 /***************Step 0 : Check for Ampersands ****************/
    char* ampersand;
    bool isEnd = true;
    if((ampersand = strstr(inputBuf, needle)) ){ //Theres an ampersand
        char* start = ampersand;
        int counter = 0;
        for(start++; *start != '\0';start++){
            if(*start != ' '){
              counter++;
              isEnd = false;
            } 
        }
        if(isEnd){
          debug("Yayy it's infinite\n");
          exec->isInfinite = true;
          for(;*ampersand !='\0'; ampersand++){
            *(ampersand) ='\0';
          }
        }

        
    }

    /***********Step 1 : Check for Pipes **************/
    int totalPipes;
    if((totalPipes = distinctCount(inputBuf, "|"))){ ///Piped Input
      char ** sections = parsePipes(totalPipes, inputBuf);

      int i=0;
      cmdP head = parseBuffer(sections[i++], exec, count);
      //printCommand(head);
      cmdP current = head;
      for(;i<(totalPipes + 1);i++){ //There should be one more than the amount of pipes we found
          cmdP newPtr = calloc(1, sizeof(Command));
          newPtr = parseBuffer(sections[i], newPtr, count);
          current->next=newPtr;
          //printCommand(newPtr);
          current = newPtr;
      }

      return exec; //Already got what I needed just return
    }//End Check for piped input
    /****************End Step 1 *************************/


    /************Step 2 : Check for Redirection**********/
    int input_fd=-1,
        output_fd=-1;

    //const char * original = "Hello > World < outtie";

    char* lt=NULL, *gt=NULL;
    lt=strstr(inputBuf, LT);//TODO : check if multiple
    gt=strstr(inputBuf, GT);

    bool failure = false;

    if(lt !=NULL && gt != NULL){
      *lt='\0'; 
      *gt='\0';
          
      while(*(++lt) == ' ');
      int length = strlen(lt);
      char* end = lt + length;
      while(*(--end) == ' ') *end ='\0';

      while(*(++gt) == ' ');
      length = strlen(gt);
      end = gt + length;
      while(*(--end) == ' ') *end ='\0';
      
      input_fd = OpenFileCurrentDirIgnoreMode(lt, O_RDONLY);
      output_fd = OpenFileCurrentDir(gt, O_CREAT|O_TRUNC|O_WRONLY|O_APPEND, (S_IWUSR| S_IRUSR | S_IXUSR));

      if(input_fd < 0 || output_fd < 0) failure = true;
    }
    else if(lt != NULL || gt != NULL){
     if(lt != NULL){
          *lt='\0';
          while(*(++lt) == ' ');

          int length = strlen(lt);
          char* end = lt + length;
          while(*(--end) == ' ') *end ='\0';

          input_fd = OpenFileCurrentDirIgnoreMode(lt, O_RDONLY);
          if(input_fd < 0) failure = true;
          
      }
      else{
          *gt='\0';
          while(*(++gt) == ' ');

          int length = strlen(gt);
          char* end = gt + length;
          while(*(--end) == ' ') *end ='\0';

          output_fd = OpenFileCurrentDir(gt, O_CREAT|O_TRUNC|O_WRONLY|O_APPEND, (S_IWUSR| S_IRUSR | S_IXUSR));
          if(output_fd < 0) failure = true;

      } 
        
    }

    exec->in_fd = input_fd;
    exec->out_fd = output_fd;
    //multiThreadDebug("InputFD --> %s(%d)\n OutputFD --> %s(%d)\n",lt,input_fd,gt,output_fd);


    if(failure){
      exec->valid = false;
      return exec;
    }



    /*******************End Step 2***********************/




    /****************Step 3 : Proceed normally***********/
    char * token = strtok((cursor=inputBuf), delim);
    //debug("Tokenized the input : %s\n", token);

    //check to see if program has ./
    char buffer[MAX_PATH];
    char *cwd;

    if(token == NULL) {
      exec->valid = false;
      return exec;
    }
    else 
    {

      if(strstr(token,HOMEPROGRAM) !=NULL)
      {
        cwd = getcwd(buffer,MAX_PATH);
        char *newToken = calloc(1,strlen(cwd) + strlen((cursor +1)) + 1);

        strcat(newToken,cwd);
        strcat(newToken,(cursor+1));

        strncpy(exec->program,newToken,((size_t)MAX_INPT/2));
      }
      else
      {
        strncpy((exec->program), token, ((size_t)MAX_INPT/2)); //Copys the program token into the struct
      }

      *(exec->tacks) = token; //Copies the program into the argv array
      exec->valid = true;
      exec->tackCount = 1;
    }

    char** argv = (exec->tacks);
    argv++; /*Skip the first line because we've added the program name*/

    while((token = strtok(NULL, delim))){ 
      //debug("Tokenized the input : %s\n", token);
        *(argv++) = token;
        (exec->tackCount)++;
    }

    /****************** Finished Parsing **********************/

    //printCommand(exec);
    return exec;
}


char** parsePipes(int totalMatches, char* inputBuf){
    char* cursor;
    char* delim = "|";

    char** totalArgs = malloc(totalMatches + 1); //TODO : Don't forget to free this later


    char * token = strtok((cursor=inputBuf), delim);
    //debug("Parsed Section : %s\n", token);

    if(token == NULL) {
      return NULL;
    }
    else 
    { 
        int i = 0;
        totalArgs[i++] = token;
        while((token = strtok(NULL, delim))){ //Keep looping until NULL
          //debug("Parsed Section : %s\n", token);
            totalArgs[i++] = token;
        }
    }

    return totalArgs;
}


/**
* ReadBuffer returns -1 if reading command failed or if there was CTRL-C/CTRL-Z
*/
int ReadBuffer(int rv,char inputBuf[]) {

      char *cursor = inputBuf;
      int count;
      char last_char;
      bool printToScreen;
      char symbol;
      char *strFromHist;

       for(rv = 1, count = 0, cursor = inputBuf, last_char = 1;
          // rv && (++count < (MAX_INPT-1))
           (last_char != '\n');
          cursor++) 
        { 
            printToScreen = true;
            symbol = getchar();   
            // rv = read(0, cursor, 1);
            // last_char = *cursor;
            last_char = symbol;

            switch(symbol)
            {
              case EOF:
                return -1;
                break;
              case CTRLC://ctrl+c
                if(foregroundProcess) {
                   //errno=0;
                   multiThreadDebug("Signaling Foreground to die : %d -->\n", foregroundProcess->pId);
                   foregroundProcess->lastKnownStatus=2;
                   signalChild(foregroundProcess->pId,SIG_INT);
                   multiThreadDebug("DONE : child died successfully \n");
                   //debug("Returning Errno : %s", strerror(errno));
                 }
                eraseLine();
                return -1; //Ignore everything, we outtie
              case CTRLZ: // ctrl + z
                if(foregroundProcess){ 
                    //errno=0;
                    multiThreadDebug("Signaling Foreground to pause : %d -->\n", foregroundProcess->pId);
                    foregroundProcess->lastKnownStatus=1;
                    signalGroupPid(foregroundProcess->pId,SIG_TSTP);
                    multiThreadDebug("DONE : child paused successfully \n");
                }
                eraseLine();
                return -1; //Ignore everything, we outtie
                 //pause();
              case DEL: /* Ascii number in octal for the delete key Macs rule*/
              case BACKSPACE: 
                if(cursor == inputBuf){
                    eraseLine();
                    return -1; //If we're at the beginning print that prompt again and start over
                }
                //debug("Got Backspace\n");
                --count;
                *(--cursor) = '\0';
                output("\r\033[K%s%s",pathVal, inputBuf);
                --cursor; //Once for the loop
                printToScreen = false;
                
                break;
              case TAB:
                //TODO: add tab functionatlity
                break;
              case isArrow1:
                symbol = getchar();
                if(symbol == isArrow2)
                {
                  symbol = getchar();
                    if(symbol == UP_ARROW || symbol == DOWN_ARROW)
                    {
                      if( (strFromHist = displayHistory(symbol) ) != NULL )
                        { 
                          eraseLine();
                          strcpy(inputBuf,strFromHist);
                          output("\r\033[K%s%s",pathVal, inputBuf);
                          cursor = inputBuf;
                          count = strlen(inputBuf);
                          cursor+=count;
                        }
                        --cursor;
                        printToScreen = false;
                    }
                    else if(symbol == RIGHT_ARROW || symbol == LEFT_ARROW)
                    {
                      --cursor;
                      printToScreen = false;
                    }
                }
                break;
            }
          
          if(printToScreen){
            putchar(symbol);
            *cursor = symbol;
            count++;
          }
     

    }


      if(count > 1) *(--cursor) = '\0';
      return count;
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

char * getValueFromKey(const char * key){
  return getenv(key);
}


void setVariable(const char* key, char* newValue){
    //debug("Current -> %s variable :::: %s\n", key, getenv(key));
    unsetenv(key);
    //debug("After unset -> %s variable :::: %s\n", key, getenv(key));
    setenv(key, newValue, 1);
    //debug("After set env ->  %s variable :::: %s\n", key, getenv(key));
}



/**
* This method will parse the PATH env variable path's until it finds the program that was passed in.
* Do not call if program is using a relative path.
*/
char* getProgramFromPath(cmdP command){

    //debug("Entering Program");
    bool isFound = false;
    char* value = getValueFromKey(PATH_VAR);
    char* path = (char*) malloc(sizeof(char) * strlen(value) + 1);
    strcpy(path, value);

    const char* original = path;


    path = strtok(path,PATH_DELIM);
    if(strstr(command->program,"/") != NULL && checkIfPathExists(command->program))
    {
      isFound = true;
    }
    else if(path !=NULL){
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

void cd(cmdP exec){
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
        newPath = getValueFromKey(HOME_VAR);
        isChange = true;
    }
    else{
        switch(argv[1][0]){ /*Get the second argument in argv, the first element should be a . , .. , -  */
          case '-':
              if(argv[1][1] == '\0'){
                debug("Executing Command Cd -\n");
                newPath = getValueFromKey(OLDPWD_VAR);
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
                    char* oldPath = getValueFromKey(WD_VAR);

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
            int newStrLength = (strlen(argv[1]) + strlen(oldwd) + 1); 
            char *newStr = malloc(newStrLength); //TODO : Mem Leak
            memset(newStr, 0, newStrLength);

            strcat(newStr, oldwd);
            strcat(newStr, "/");
            strcat(newStr, argv[1]);
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

      //char *newPath = getcwd(buffer,MAX_PATH);
      debug("Found newPath : %s\n", newPath);
      memset(pathVal, 0, MAX_PATH);
      strcpy(pathVal, "[");
      strcat(pathVal, newPath);
      strcat(pathVal, "]:");  
      strcat(pathVal, prompt);
    }

}

char* replaceHome(char** oldPath){//TODO : Implememnt replacing home with ~
    char* home = getValueFromKey(HOME_VAR);
    home = "";
    oldPath++;
    return home;
}


/**
* Replaces all Variables that start with a '$' with their corresponding value
*/
void replaceVariables(cmdP exe){
    char**argv = exe->tacks;

    char* needle;
    while(*argv != NULL){
      //debug("Checking Value %s\n",  *argv);
      if((needle = strstr(*argv,"$")) == *argv){ //If the $ is at the start
          *argv = getValueFromKey(needle + 1); //Replace the pointer to this argument with the variable value
          debug("Replaced %s with %s", needle, *argv);
      }
      argv++;
    }

}


void executePwd(void) {
  size_t size = MAX_INPT;
  char buffer[size + 1];
  char *cwd = getcwd(buffer,size+1);
  if(cwd != NULL)
      printf("%s\n", buffer);
  else
      procError("Bad directory");
}


void echo(cmdP exe){
  char** argv = exe->tacks;
  argv++; //Skip the first one, which is echo

  while(*argv != NULL){
    output("%s ",*(argv++));
  }
  output("\n");
}


void setVar(cmdP exe){
  char* var = exe->tacks[1];
  char* eq = exe->tacks[2];
  char* assignment = exe->tacks[3];

  

  char* needleLoc;

  //If the needle is found, is of the form "Var=Value"
  if((needleLoc = strstr(var, "=")) && (!eq) && (! assignment)){ 
    debug("Removing '=' Before : %s\n", var);
    *(needleLoc)= '\0';
    assignment = needleLoc + 1; //The Value is right after the '='
    debug("After : %s\t%s\n", var,assignment);
  }

  debug("Assigning Variable : %s to %s Value %s\n\n", var, eq, assignment);

  setVariable(var, assignment);
  getValueFromKey(var);
}




void writeHistory(cmdP exec){
    char **argv = exec->tacks;   

    if(argv==NULL || *argv == NULL) return;

    char* home = getValueFromKey(HOME_VAR);
    
    int totalLength = strlen(home) + strlen(history_fileName) + 1;
    char file[totalLength];
    memset(file, 0, totalLength);
    strcat(file, home);
    strcat(file, "/");
    strcat(file, history_fileName);
    /* file should not be created here it should be created when the program is fun runned*/
    int history_fd = Open(history_fileName, O_CREAT| O_WRONLY | O_APPEND, (S_IWUSR| S_IRUSR | S_IXUSR));
    if((history_fd < 0)){
      return;
    }

    histListP historyList = (histListP) calloc(1, sizeof(HistoryList));
    //creates a new string array to hold the value to be copied over to the struct
    size_t length;

      length = strlen(exec->originalStr);
      Write(history_fd, exec->originalStr, length);
      Write(history_fd, "\n", 1);
    size_t len = strlen(exec->originalStr)+1;
    debug("The length of the newString is %zu \n",len);
    char *newString = malloc(len);
    strncpy(newString,exec->originalStr,len);

    //grabs the last item in the linkedList and appends the new item and makes it the new tail;
    addHistoryNode(historyList,newString);
    Close(history_fd);
}

void History(){
    char* home = getValueFromKey(HOME_VAR);
    
    int totalLength = strlen(home) + strlen(history_fileName) + 1;
    char file[totalLength];
    memset(file, 0 , totalLength);
    strcat(file, home);
    strcat(file, "/");
    strcat(file, history_fileName);


    int history_fd = OpenIgnoreMode(history_fileName, O_RDONLY);
    if(history_fd < 0) return;

    output("--------History Dump---------\n");

    ssize_t readBytes;
    char buffer[BUFFER_SIZE];
    while((readBytes = Read(history_fd, buffer, BUFFER_SIZE)) > 0){ //Stop when read bytes == 0
      output("%s\n", buffer);
    }
    output("----------------------------\n");

    Close(history_fd);
}

void clearHistory(){
    char* home = getValueFromKey(HOME_VAR);
    
    int totalLength = strlen(home) + strlen(history_fileName) + 1;
    char file[totalLength];
    memset(file, 0 , totalLength);
    strcat(file, home);
    strcat(file, "/");
    strcat(file, history_fileName);

    freeHistory();

    int history_fd = OpenIgnoreMode(history_fileName, O_TRUNC | O_WRONLY);
    Close(history_fd);
}

void readHistory(){
  // returns true if successful f if failure
  /*puts all the history into a string array for iteration*/ 
  int history_fd = Open(history_fileName, O_CREAT| O_RDONLY | O_APPEND, (S_IWUSR| S_IRUSR | S_IXUSR));
  int bytesRead;
  int count = 0;
  char *delim = "\n";
  char *buffer;
  //replace new line with null terminator
  // histListP historyList = (histListP) calloc(1, sizeof(HistoryList));
  // addHistoryNode(historyList);
  // histListP currentHistory = historyHead;
  //lseek
  histListP historyList;
  while( ( count = getLineCount(history_fd) ) > 0)// this will read and then move the cursor back
  {
    if(count == -1) break;
    Lseek(history_fd, (~count) + 1, SEEK_CUR);
    buffer = malloc(count);
    bytesRead = Read(history_fd, buffer, count);
    buffer = strtok(buffer, delim);//replace the newline character with a null terminator
    //(currentHistory->string)=buffer;

    histListP historyList = (histListP) calloc( 1, sizeof(HistoryList));


    addHistoryNode(historyList, buffer);
   // addHistoryNode(historyList);

    // historyList->prev=currentHistory;
    // currentHistory->next = historyList;

   // currentHistory= currentHistory->next;

    //update head and tail
    // historyHead->prev=currentHistory;
    // currentHistory->next=historyHead;

  }
  //for the last node that is blank
  // historyHead->prev = currentHistory->prev;
  // currentHistory->prev->next = historyHead;
  // currentHistory->next= NULL;

  // free(currentHistory->string);
  // free(currentHistory);
  Close(history_fd);
}
void addHistoryNode(histListP historyList, char *buffer)
{
    historyList->string = buffer;

    if(historyHead == NULL){ //None have been added
        historyHead = historyList;
        historyHead-> next = historyHead->prev = historyHead;
    }
    else if(historyHead->prev == historyHead){ //There's only one
        historyList-> next = historyList-> prev = historyHead;
        historyHead->next = historyHead->prev = historyList;
    }
    else{ //At least 2
        histListP oldTail = historyHead->prev; //Temporarily store the old tail
        oldTail-> next = historyList;
        historyList-> prev = oldTail; 
        historyList-> next = historyHead;
        historyHead-> prev = historyList;
    }
    lastKnownhistListPSpot = historyList->prev;

}
void freeHistory()
{
  histListP cursor;

  while( (cursor = popHistoryNode(historyHead)) != NULL)
  {
    free(cursor->string);
    free(cursor);
  }
  // historyHead = NULL;removeHistoryNode itself should make it NULL
}

histListP popHistoryNode(histListP node)
{
      histListP returnHistory = NULL,start;
      if((start = historyHead) == NULL) return NULL;

    do
    {
      if(start == node)
      {
        returnHistory = start;

        if(start -> next == start) //There's only one
        {
            if(start == historyHead)
            {
              debug("Deleting Head %s\n",start->string);
              historyHead = NULL;
            }
            else
            {
              debug("Something got messed up\n");
            }
        }
        else //2 or more
        {
          debug("Deleting Node %s\n",start->string);
          histListP nextOne = returnHistory->next;
          returnHistory->prev->next = nextOne;
          nextOne->prev = returnHistory->prev;
          returnHistory-> next = returnHistory-> prev = NULL;

          historyHead = nextOne;
        }
        
        break;
      }
      start = start-> next;
    }while(returnHistory != historyHead);

    debug("Returning Node\n");
    // printProcess(returnProcess);
    return returnHistory;

}
char *displayHistory(char sym)
{
  if(historyHead ==  NULL)
    return NULL;
  if(lastKnownhistListPSpot == NULL)
    lastKnownhistListPSpot = historyHead;

  if(UP_ARROW == sym)
    lastKnownhistListPSpot = lastKnownhistListPSpot->next;
  else if(DOWN_ARROW == sym)
    lastKnownhistListPSpot = lastKnownhistListPSpot->prev;
  else
  {
    debug("This shouldnt happen");
  }
  return (lastKnownhistListPSpot->string);


}
int getLineCount(int fd){
  int bytesRead;
  int ctr = 0;
  char *buffer = malloc(sizeof(char));
  while( (bytesRead = Read(fd, buffer, BYTE)) > 0)
  {
    ctr++;
    if(*buffer == NEWLINE)
      {   
        if(ctr == 1)
        {
          free(buffer);
          return -1;
        }
        break;
     }
        

  }
  free(buffer);
  return ctr;
}


/****************** Threading ******************************/
void setForeground(cmdP exe){
    procP newProcess;
    //sigset_t mask, prev;

    //synchronizeFull(&mask, &prev);
    procP oldProc = foregroundProcess;

    if((newProcess = findProcess(exe->tacks[1]))){ //Not null
        if(foregroundProcess){
          foregroundProcess->command->isInfinite = false;  //Put in BG
          foregroundProcess->lastKnownStatus = 0;         // Set it to run status
        }

        foregroundProcess = newProcess;
        newProcess->command->isInfinite=true;
        newProcess->lastKnownStatus=0;
      //endSynchronize(&prev);
    }
    else {
      if(foregroundProcess){
        foregroundProcess->lastKnownStatus = 0; //Run it again
        printProcess(foregroundProcess);
        multiThreadDebug("Signaling child : %d --> \n", foregroundProcess->pId);
      }
      
      //endSynchronize(&prev);
    }
    if(oldProc) signalGroupPid(oldProc->pId,SIG_CONT);
    if(newProcess) signalGroupPid(newProcess->pId,SIG_CONT);
    printProcess(foregroundProcess);
}

void setBackground(){
   // sigset_t mask, prev;

    procP oldProc = foregroundProcess;
    //synchronize_single(&mask, prev, SIG_);
    
    foregroundProcess->command->isInfinite = true;
    foregroundProcess->lastKnownStatus=0;
    
    printProcess(foregroundProcess);
    foregroundProcess = NULL;
    
    //endSynchronize(&prev);

    if(oldProc)signalGroupPid(oldProc->pId,SIG_CONT);
}


procP findProcess(char* argument){
    bool isJob = false;
    char* thread = argument;
    multiThreadDebug("Starting the find process...\n");

    if(! thread) {
      procError("Can't Set foreground process without ID\n");
      return NULL;
    }


    if((thread = strstr(thread,"%"))){ //If there's a % then it's a Job ID
        ++thread; // Skip the %
        isJob = true;
    }

    multiThreadDebug("Is it a job ID ? : %d\n", isJob);

    int length =strlen(thread);
    int id = 0;
    for(int i = 0; i< length;i++){
      if(thread[i] < '0' || thread[i] > '9') return NULL; //Can't accept non digits 
      id += id * 10 + (thread[i] - '0'); 
    }

    multiThreadDebug("Found Job ID :  %d\n", id);

      procP returnValue = NULL;
      procP start = headProcess;
      multiThreadDebug("JOB ID : %016x\n", (int)start);
      do
      {
        if(isJob){ //Use Job ID
            if(id == start->jobId){
              returnValue = start;
              break;
            }
        }
        else{ //Else use Process ID
            if(id == start->pId){
                returnValue = start;
                break;
            }
        }
        
        start = start->next;
      }while(start != headProcess);

    return returnValue;
}


lambda_t* SignalFunc(int signalNum, lambda_t* handler){
  
  struct sigaction action, old_action;

  action.sa_handler = handler;
  sigemptyset(&action.sa_mask); /* Block sigs of type being handled */
  action.sa_flags = SA_NODEFER;

  if(sigaction(signalNum, &action, &old_action) < 0){
      multiThreadDebug("Error signaling\n");
  }

  return (old_action.sa_handler);
}

void killProcess(){
  if(foregroundProcess){
    kill(foregroundProcess->pId,SIGKILL);
  }
}

void printJobs(){

  procP start = headProcess;

  if(! start) return;

  do{

    //if(start->command->isInfinite) 
        printProcess(start);
       
    start = start->next;
  }while(start && start != headProcess); //While there are more processes
    
}

/****************** END Threading ****************************/



/***************************SHELL BUILT-IN FUNCTIONS END*******************************/






/**************************** WRAPPER CLASSES *****************************************/

int OpenFileCurrentDirIgnoreMode(const char *file, int flags){
    
    char* current = (char*) file;
    if(! strstr(file, "/")){
        char buffer[MAX_PATH];
        memset(buffer, 0, MAX_PATH);
        char * current = getcwd(buffer,MAX_PATH);
        strcat(current, "/");
        strcat(current, file);
    }

    return OpenIgnoreMode(current, flags);
}

int OpenFileCurrentDir(const char* file, int flags, mode_t mode){
     char* current = (char*) file;
    if(! strstr(current, "/")){
      char buffer[MAX_PATH];
      memset(buffer, 0, MAX_PATH);
      current = getcwd(buffer,MAX_PATH);
      strcat(current, "/");
      strcat(current, file);
    }
    return Open(current, flags, mode);
}

int OpenIgnoreMode(const char *file, int flags){

  int returnVal;
  if((returnVal = open(file, flags)) < 0){
      error("Error Opening File : %s --> %s\n", file, strerror(errno));
  }

  return returnVal;
}

int Open(const char* file, int flags, mode_t mode){

  int returnVal;
  if((returnVal = open(file, flags, mode)) < 0){
      error("Error Opening File : %s --> %s\n", file, strerror(errno));
  }

  return returnVal;
}

ssize_t Read(int fd, void* buf, size_t count){
  ssize_t returnVal;
  if((returnVal = read(fd, buf,count)) < 0){
      error("Error Reading from File %s\n", strerror(errno));
  }
  return returnVal;
}

ssize_t Write(int fd, const void* buf, ssize_t count){
  ssize_t returnVal;
  if((returnVal = write(fd, buf,count)) < 0){
      error("Error Writing to File %s\n", strerror(errno));
  }
  else if(returnVal != count){ 
      error("Write to file failed. Expected %zu bytes but got %zd\n", count, returnVal);
  }
  return returnVal;
}

int Close(int fd){
  int returnVal;
  if((returnVal = close(fd)) < 0){
      error("Error closing File %s\n", strerror(errno));
  }
  return returnVal;
}

void CloseAll(int in_fd, int out_fd, int backFile, int inputFile){
    if(backFile != -1)
      Close(backFile);
    if(inputFile != -1)
      Close(inputFile);
    if(in_fd != -1)
      Close(in_fd);
    if(out_fd != -1)
      Close(out_fd);
}

off_t Lseek(int fd, off_t offset, int whence){

    off_t totalOffset;
    /* Set the file position*/
    if((totalOffset = lseek(fd, offset, whence)) < 0) {
        /* failed to move the file pointer */
        error("Problem moving the cursor forward/back %lu bytes", offset);
        exit(EXIT_FAILURE);       
    }

    return totalOffset;
}



/*****************************END WRAPPER CLASSES********************************/




/***************************PARSING/EXECUTING************************************/
trieP BuildTrie(char *directory)
{
  trieP root = (trieP)calloc(1, sizeof(Trie));
  DIR *dir;
  struct dirent *dp;
  dir = opendir(directory);
  int len;
  int i;
  trieP iterator = root;
  while ((dp=readdir(dir)) != NULL) {
        output("looking for string files %s\n", dp->d_name);
        len = strlen(dp->d_name);//length of string to spinoff
        for(i = 0; i < len;i++)
        {
          int pos = (int)dp->d_name[i];
          if( (iterator->children[pos]) == NULL) // we have to hash it the new child
          {
            trieP newChild = (trieP)calloc(1, sizeof(Trie));
            newChild->sym = dp->d_name[i];
            iterator->children[pos] = newChild;
            (iterator->amountOfChildren)++;
          }
          // child exist in the heap so we got to make it real
          iterator = iterator->children[pos];

        }

        iterator = root;
      }

    //build 
    closedir(dir);

    return root;
}

bool glob(const cmdP exe){

    debug("Checking if Globs\n");
    char* needle = NULL; //Find the needle in the haystack
    char **argv = exe->tacks;

    int preGlobCounter = 0;
    while(*argv != NULL){
      if((strstr(*argv, "*."))){
          needle = *argv + 1;//+1 to Skip the *
          break;
      }
      preGlobCounter++;
      argv++;
    }

    if(needle == NULL){
      debug("No Globs found\n");
      return false;
    }


    DIR *dir;
    struct dirent *dp;
    int matchCounter= 0;

    char buffer[MAX_PATH];
    char *current_working_dir = getcwd(buffer,MAX_PATH);

    dir = opendir(current_working_dir);

    while ((dp=readdir(dir)) != NULL) {
      if((strstr(dp->d_name,needle))) matchCounter++;
    }
    closedir(dir);



    dir = opendir(current_working_dir);

    char** matches = calloc(1, (matchCounter + preGlobCounter) * sizeof(char * )); // Add 1 to match counter for program
    char** original = matches;
    argv = exe->tacks;

    for(int i = 0; i < preGlobCounter;i++){
      *(matches++)=argv[i]; //Assign the program to the first element and then increment it with our matches
    }


    char* str;
    while ((dp=readdir(dir)) != NULL) {
        debug("Found file : %s\n", dp->d_name);

        if((strstr(dp->d_name,needle))){
            str = calloc(1, strlen(dp->d_name));
            *(matches++) = strcpy(str,dp->d_name);
        }

    }
    closedir(dir);


    memcpy(exe->tacks, original, ((matchCounter + preGlobCounter) * sizeof(char * ))); // Copy that sucka
    return true;
}


pid_t forkProcess(){
  pid_t returnVal;
  //multiThreadDebug("Forking!\n");

  if((returnVal = fork()) < 0){
    error("Error trying to fork a process : %s\n", strerror(errno));
    exit(EXIT_FORKFAIL);
  }
  else{
    //multiThreadDebug("Forked Successfully!\n");
  }

  return returnVal;
}


void createBackgroundProcess(){


  pid_t pid;
  sigset_t mask, prev;

  //sigaddset(&mask, SIG_INT);

  
  SignalFunc(SIG_INT,signal_interrupt);
  SignalFunc(SIG_TSTP,signal_pause);
  SignalFunc(SIG_CONT,signal_continue);
  SignalFunc(SIG_CHLD,sig_child);
  sigemptyset(&mask);
  synchronizeFull(&mask, &prev);
  
  setpgid(0,0);

  if((pid = forkProcess())){
    endSynchronize(&prev);
    volatile sig_atomic_t signaled = 0;
    pause_signal = stop_signal = 0;
    //multiThreadDebug("In parent process!\n");
    while(!stop_signal){
      sigsuspend(&prev);
      signaled = 0;
      while(pause_signal){
        signaled = 1;
        //signalGroup(SIG_TSTP);
        multiThreadDebug("Pausing!\n");
        sigsuspend(&prev);
      }
      /*if(signaled){
        multiThreadDebug("Continuing!\n");;
        signalGroup(SIG_CONT);
        signaled = 0;
      }
      */

    }  

    //multiThreadDebug("Yayy returned successfully!\n"); 
    //multiThreadDebug("Grandparent Signaling all children : \n");   
    signalGroup(SIG_INT);
    //multiThreadDebug("DONE : Grandparent Signaled children \n" );
    //endSynchronize(&prev);
    exit(EXIT_OK);
  }
  endSynchronize(&prev);

}


/*Check if command is a builtin command*/
/* Have a list of functions we support 
* this method will check the list to see if program name is supported
*
*/
int isTypeBuiltin(cmdP exe)
{
    //multiThreadDebug("Checking if is built in! \n");

    for(int i = 0; i < Total_BuiltIns; i++)
    {
      if(!strcmp(exe->program,builtInMethods[i]))
      {

            if(!strcmp(exe->program,"pwd"))
            { 
              debug("Executing pwd\n");
              executePwd();
            }
            else if(!strcmp(exe->program,"echo"))
            { 
              debug("Executing echo\n");
              echo(exe);
            }
            
            else if(!strcmp(exe->program,"history"))
            {
              debug("Executing History\n");
              History();
            }
            
            else
              debug("This should never happen\n"); 
               
           return 0;     
        }
    }
  return -1;
}

int isSoloBuiltin(cmdP exe){

  //multiThreadDebug("Running Solo builtins now!\n");

  sigset_t mask,prev;
  int returnValue = (foregroundProcess!= NULL)? 0 : -1; //If fg process: return 0 so we don't fork process
  for(int i = 0; i < Total_solo_builtin; i++)
  {
    if(!strcmp(exe->program,soloBuiltInMethods[i]))
    {

        if(!strcmp(exe->program,"clear-history"))
        {
          debug("Executing Clear History\n");

          
          if(foregroundProcess== NULL)  clearHistory();
          
        }
        else if(!strcmp(exe->program,"cd"))
        {
          debug("Executing Clear History\n");
          if(foregroundProcess== NULL)  cd(exe);
        }
        else if(!strcmp(exe->program,"jobs"))
        {
          debug("Executing Jobs\n");
          printJobs();
        }
        else if(!strcmp(exe->program,"fg"))
        {
          debug("Executing FG\n");
          setForeground(exe);
        }
        else if(!strcmp(exe->program,"set"))
        {
          debug("Executing Set\n");
          if(foregroundProcess== NULL)  setVar(exe);
        }
        else if(!strcmp(exe->program,"bg"))
        {
          debug("Executing BG\n");
          setBackground();
        }
        else if(!strcmp(exe->program,"kill"))
        {
          debug("Executing Killl\n");
          killProcess();
        }else if(!strcmp(exe->program,"exit"))
        {
          debug("Executing Killl\n");
          if(foregroundProcess== NULL) exit(EXIT_OK);
        }
        else{
          debug("Hmm\n");
        }
        //multiThreadDebug("Returning success from solo!\n");
        returnValue = 0;
      }

  }
  //multiThreadDebug("Returning failure from solo!\n");
  return returnValue;
}

/*In this section of code I'm going to be taking my exe and checking
* if it is not a child process then checking using the execve if the program is in
* built in or not if it for now i only care if it isn't
*/
void launchExecutable(cmdP exe){
    
    cmdP head = exe;
    bool isFirstProgram = true; 
    int inputPipeFile = -1, backEndFile = -1;
    int wpid,exitCode = 0, exitStatus = 0;
    pid_t process;
    procP newForeground = NULL;


        SignalFunc(SIG_CHLD,sig_child_main);
        SignalFunc(SIG_USR1,sig_child_run);

        if((exitCode = isSoloBuiltin(exe)) == 0){
            //multiThreadDebug("Is a solo builtin!\n");
        }
        else if((process = forkProcess()) == 0) /* This means it is a child process and i should launch some stuff*/
        {
          //multiThreadDebug("Running program now!\n");
          sigset_t mask,prev;
          exitCode = 0;
          SignalFunc(SIG_CHLD,sig_child);
          SignalFunc(SIGXCPU,SIG_IGN);
          synchronizeFull(&mask, &prev);
          endSynchronize(&prev);

          while(! parent_done_signal)
            sigsuspend(&prev);

          //multiThreadDebug("Done Yielding\n");
          createBackgroundProcess(); //Launch another fork, only child returns
          
          pid_t new_process;
          while(head != NULL){
          
              //multiThreadDebug("Looping! head = %s\n", head->program);
              
 
              //multiThreadDebug("Forking another child now!\n");
              if(!(new_process = forkProcess())){//For child
                      // This line will open a new file if it is new otherwise set it to -1 
                      backEndFile = (head->next != NULL) ? 
                          Open(OUTERFILE, O_CREAT | O_RDWR | O_TRUNC, (S_IWUSR| S_IRUSR | S_IXUSR)) : -1;

                      if(!areCommandsInValidFormat(exe,isFirstProgram)) {
                          multiThreadDebug("Not in valid format!\n");
                          exit(EXIT_FAILURE);
                      }

                      int input = (inputPipeFile == -1) ? head->in_fd : inputPipeFile;
                      int output = (backEndFile == -1) ? head->out_fd : backEndFile; 
                      switchSTDFileDescriptors(input,output);


                      procDebug("RUNNING: %s\n", head->program); 
              
                      //multiThreadDebug("Running program now!\n");
                      if((exitCode = isTypeBuiltin(head)) == -1)
                      {
                        //debug("Execute non built-in program\n");
                        exitCode = executeExternalProgram(head);
                      }

                      
                      procDebug("ENDED: %s (ret=%d)\n", head->program,exitCode);
                      
                      //multiThreadDebug("Child Dying with exit code %d!\n", exitCode);
                      exit(exitCode);
              }
              else{
                  while(! child_done_signal){
                    sigsuspend(&prev);//waitpid(-1,&wpid,0);/*Having a 0 as input forces the main thread to wait for all child processes to finish before reexecuting*/
                  }

                  multiThreadDebug("Parent Done waiting! Goodbye!\n");
                   //exitCode = WIFEXITED(&wpid);
                  if(WIFEXITED(&wpid))
                      exitCode = WEXITSTATUS(&wpid);
                  //multiThreadDebug("Child Died with exit code %d, Error : %s!\n", exitCode, strerror(errno));
                  isFirstProgram = false;
                  exitStatus = exitStatus | exitCode;
                  if(exitStatus) {
                    //multiThreadDebug("Parent Dying from failure! \n");
                    exit(EXIT_FAIL);
                  }
              }


              CloseAll(head->in_fd, head->out_fd, backEndFile, inputPipeFile);

              if((head = head->next) != NULL)
              {
                //debug("Moving to next position !\n");
                inputPipeFile = copyFiletoNewFile();
              }

            }//END WHILE LOOP

            //multiThreadDebug("Parent Dying! --> \n");
            signalGroup(SIG_INT);
            //multiThreadDebug("DONE : Parent Signaled child --> Dying \n");
            exit(exitStatus);
        }
        else
        {
              
            //Convert int to String
              // int counter = 0;
              // for(int temp = exitCode; temp > 0; temp = temp/10) counter++;
              // char buffer[counter + 1];
              // sprintf(buffer, "%d",exitCode);
              // ///////////////////////
              // setVariable("?", buffer);
            
              multiThreadDebug("Parent setting processID to : %d\n", process);
              
              newForeground = (procP) calloc(1, sizeof(Process)); 
              newForeground->jobId = ++currentJobId;
              newForeground->command = exe;
              newForeground->pId = process;
              newForeground->lastKnownStatus = 0; //Set status to running
              addProcessNode(newForeground);

              if(!exe->isInfinite && foregroundProcess == NULL){
                  foregroundProcess = newForeground;
              }
              
              signalChild(process,SIG_USR1); //Child can go now

        }

}


int executeExternalProgram(cmdP exe){

       char * programPath = getProgramFromPath(exe);

       if(programPath == NULL || execv(programPath, exe->tacks) == -1)
       { 
          procError("%s : Command Not found\n", exe->program);
          exit(EXIT_FAIL);
       }
       else{
          debug("Starting program %s with program path %s\n", programPath, exe->program);
       }
       
       exit(EXIT_OK);
    }

bool areCommandsInValidFormat(cmdP exe,bool isFirstProgram)
{
    bool isValid = true;
      if(!exe->valid)
      {
        procError("%s : Command Not found\n", exe->program);
        isValid = false;
      }
      if( (isFirstProgram && exe->out_fd != -1 && exe->next != NULL && strcmp(exe->program,"cd") != 0) 
          || (!isFirstProgram && exe->next == NULL && exe->in_fd != -1 && strcmp(exe->program,"cd") != 0) 
          || (!isFirstProgram && exe->next != NULL && exe->out_fd != -1 && exe->in_fd != -1 && strcmp(exe->program,"cd") != 0) )
      { 
         procError("can't continue execution, format is incorrect\n");
         isValid = false;
      }
      //multiThreadDebug("Returning exit code : %d for program %s!\n", isValid, exe-> program);
      return isValid;
}
void switchSTDFileDescriptors(int in_fd,int out_fd){

  if(in_fd !=  -1)
    dup2(in_fd,STDIN_FILENO);
  if(out_fd != -1)
    dup2(out_fd,STDOUT_FILENO);

}

int copyFiletoNewFile(){
 int new_fd = Open(INTERFILE, O_CREAT | O_RDWR | O_TRUNC, (S_IWUSR| S_IRUSR | S_IXUSR));
 int old_fd =  OpenIgnoreMode(OUTERFILE, O_RDONLY);
 char buffer[BYTE];
 int bytesRead;
 while( ( bytesRead = Read(old_fd, buffer, BYTE) ) > 0)
 {
  Write(new_fd,buffer,BYTE);
 }
 Close(new_fd);
 Close(old_fd);
 return OpenIgnoreMode(INTERFILE, O_RDONLY);
}

bool checkIfProgramExists(char* path, cmdP command){
    //char *programPath = malloc(strlen(path) + strlen(command->program) + 2);

    char programPath[strlen(path) + strlen(command->program) + 2];
    strcat(programPath, path);
    strcat(programPath,"/");
    strcat(programPath, command->program);

    
    bool isContained = checkIfPathExists(programPath);

    if(isContained){
      strcpy(command->program, programPath);
    }
    //free(programPath);

    return isContained;
}

bool checkIfPathExists(char* path){
  sigset_t mask, prev;
  
  //  debug("init stat buffer\n");
    struct stat status[sizeof(struct stat)];// *inbuf = (struct stat *) malloc(sizeof(struct stat)); 
    //debug("after stat init\n");
    memset(status, 0, sizeof(struct stat));
    int input_stat = stat(path, status);

    bool isContained = input_stat >= 0 ? true : false;
    //free(inbuf);

  ///debug("Does Path Exist --> %d\n", isContained);
  return isContained;
}



int distinctCount(char* haystack, char* needle){
  int count = 0;
  const char *arg = haystack;
  while((arg = strstr(arg, needle)) != NULL)
  {
     count++;
     arg++;
  }
  return count;
}

void addProcessNode(procP newProcess){
    if(headProcess == NULL){ //None have been added
        headProcess = newProcess;
        headProcess-> next = headProcess->prev = headProcess;
    }
    else if(headProcess->prev == headProcess){ //There's only one
        newProcess-> next = newProcess-> prev = headProcess;
        headProcess->next = headProcess->prev = newProcess;
    }
    else{ //At least 2
        procP oldTail = headProcess->prev; //Temporarily store the old tail
        oldTail-> next = newProcess;
        newProcess-> prev = oldTail; 
        newProcess-> next = headProcess;
        headProcess-> prev = newProcess;
    }
    debug("Successfully added Node\n");
    //printProcess(newProcess);
}

procP removeProcessNode(pid_t processID){
      procP returnProcess = NULL,start;
      if((start = headProcess) == NULL) return NULL;

      multiThreadDebug("Removing process Nodes : %d\n",processID);
    do
    {
      if(start->pId == processID){
        returnProcess = start;

        if(start -> next == start){ //There's only one
          multiThreadDebug("Only one : %d\n",processID);
            if(start == headProcess){
                headProcess = NULL;
            }
            else{
              debug("Something got messed up\n");
            }
        }
        else{//2 or more
          if(start == headProcess) headProcess = headProcess->next;
          multiThreadDebug("2 or more : %d\n",processID);
          procP nextOne = returnProcess->next;
          returnProcess->prev->next = nextOne;
          nextOne->prev = returnProcess->prev;
          returnProcess-> next = returnProcess-> prev = NULL;
        }
        
        break;
      }

      start = start-> next;
    }while(start != headProcess);

    debug("Returning Node\n");
    //printProcess(returnProcess);
    return returnProcess;
}


void printCommand(cmdP command){
  debug("Command -> %d\n", ((int)command->next));
  debug("Program -> %s\n",command->program);
  
  char** argv = command->tacks;
  int i = -1;
  while(argv[++i] != NULL){
    debug("Tacks[%d] -> %s\n",i,argv[i]);
  }
  debug("Tack Count -> %d\n", command->tackCount);
  debug("IsValid -> %d\n", command->valid);
  debug("Input FD -> %d\n", command->in_fd);
  debug("Output FD -> %d\n", command->out_fd);
  debug("Next -> %d\n", ((int)command->next));
}


void printAllProcesses(){
    procP start;
    if((start = headProcess) == NULL) return;

    debug("----------Print processes----------------\n");
    do
    {
      printProcess(start);
      output("|\n");
      output("v\n");
      start = start-> next;
    }while(start != headProcess);

    debug("-------------End Print processes----------------\n");
}

void printProcess(procP process){
    if(process == NULL) return;

    output("[%d] (%d) %s\t",process->jobId, process->pId, STATUS[process->lastKnownStatus]);
    //char**argv = process->command->tacks;
    if(process->command != NULL){
      //for(int i=0; i < process->command->tackCount;i++){
      output("%s ", process->command->originalStr);
      //}
    }
    if(process->command->isInfinite) output(" &");
    output("\n");
} 



