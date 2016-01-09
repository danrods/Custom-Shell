#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <sched.h>

#define MAX_INPT    1024
#define MAX_PATH    128
#define MAX_HISTORY 
#define BUFFER_SIZE 512
#define MAX_PROC    64
#define ALPHABET 128 /*Alphabet consist of [a-z,A-Z,0-9]*/
#define BYTE 1
#define RETURN_VAR "?"
#define PATH_DELIM ":"
#define LT "<"
#define GT ">"
#define LINE "----------------------------------------------"
#define PATH_VAR "PATH"
#define WD_VAR "PWD"
#define HOME_VAR "HOME"
#define OLDPWD_VAR "OLDPWD"
#define history_fileName ".320sh_history"
#define Total_BuiltIns 3
#define Total_solo_builtin 8
#define INTERFILE ".inter_comm"
#define OUTERFILE ".outer_comm"
#define CTRLC '\003'
#define CTRLZ '\032'
#define DEL '\177'
#define BACKSPACE '\b'
#define NEWLINE '\n'
#define TAB '\t'
#define isArrow1 '\033'
#define HOMEPROGRAM "./"
#define isArrow2 '['
#define UP_ARROW 'A'
#define DOWN_ARROW 'B'
#define RIGHT_ARROW 'D'
#define LEFT_ARROW 'C'
#define continue_sig_message "Continuing...\n" 
#define cont_message_length 14
#define stop_sig_message "Stopping...\n" 
#define stop_message_length 12
#define pause_sig_message "Pausing...\n" 
#define pause_message_length 11
#define exit_sig_message "Exit Success!\n" 
#define exit_message_length 14
#define exit_ab_sig_message "Exit Abnormal!\n" 
#define exit_ab_message_length 15


typedef const enum {DEV, NONE} VERBOSE_LEVEL;
typedef const enum {SHOW, NO} TIME_DISPLAY;
typedef const enum {EXIT_OK, EXIT_FAIL, EXIT_FORKFAIL, EXIT_FNF} RETURN_CODES;
typedef const enum {SIG_HUP=1,SIG_INT=2, SIG_KILL=9, SIG_USR1=10,SIG_ALARM=14, SIG_CHLD=17,SIG_CONT=18, SIG_TSTP=20} Signals;
typedef void lambda_t(int);


typedef struct Command
    {
        char program[MAX_INPT/2];
        char* tacks[MAX_INPT/2];
        char *originalStr;
        int tackCount, in_fd, out_fd;
        bool valid, isInfinite;
        struct Command* next;
        
    } Command, *cmdP;

typedef struct Process
    {
        int jobId;
        pid_t pId;
        int lastKnownStatus;
        cmdP command;
        struct Process* prev;
        struct Process* next;

    } Process, *procP;

typedef struct HistoryList
    {
        char* string;
        struct HistoryList* prev;
        struct HistoryList* next;
    }HistoryList, *histListP;

typedef struct Trie
    {
        char sym;//root will not have a character here 
        int amountOfChildren;// number of chilren spinoffed
        struct Trie *children[ALPHABET];
    } Trie, *trieP;

extern procP head;
extern procP foregroundProcess;
extern bool STDERR;
extern VERBOSE_LEVEL DEBUG;
extern volatile sig_atomic_t stop_signal;
extern volatile sig_atomic_t pause_signal;
extern volatile sig_atomic_t parent_done_signal;
extern volatile sig_atomic_t child_done_signal;
extern const char* STATUS[]; //{"Running", "Stopped", "Killed", "Done"};


#define debug(fmt, ...) if(DEBUG == DEV) printf("DEBUG: %s ==> (%s:%d) --> " fmt, __FILE__,__FUNCTION__, __LINE__, ##__VA_ARGS__) 
#define error(fmt, ...) if(DEBUG == DEV) fprintf(stderr, "**ERROR: %s:%s:%d " fmt, __FILE__,__FUNCTION__, __LINE__, ##__VA_ARGS__) 

#define output(fmt, ...) fprintf(stdout, "" fmt, ##__VA_ARGS__)

/*Returns 0 for success otherwise returns a non zero number*/
#define procDebug(fmt, ...) if(STDERR)  output("" fmt, ##__VA_ARGS__)
#define procError(fmt, ...) fprintf(stderr, "" fmt, ##__VA_ARGS__)

#define multiThreadDebug(fmt, ...) debug("\n-------{P:#%d-->[C:#%d::G:#%d]}-------\n" fmt "%s\n", getppid(),getpid(),getpgid(getpid()),##__VA_ARGS__,LINE)

#define atomic_write(msg, count) if(DEBUG==DEV) Write(1, msg, count);

#define signalChild(id, signal) kill(id, signal)
#define signalGroup(signal) kill(0, signal)//0 --> sig shall be sent to all processes whose process gID is equal to the process group ID of the sender
#define signalGroupPid(id, signal) kill( ((~id) + 1), signal)// -pid  --> sig shall be sent to all processes whose process group ID is equal to the absolute value of pid

#define eraseLine() output("\r\033[K"); fflush(stdout);

#define synchronizeFull(new, old) sigfillset(new); sigprocmask(SIG_BLOCK, new, old)
#define synchronize_single(new, old, Type) sigemptyset(new); sigaddset(new, Type); sigprocmask(SIG_BLOCK, new, old)

#define endSynchronize(old) sigprocmask(SIG_SETMASK, old, NULL)

#define GET_CHAR_INDEX(c) ((int)(c)-48))

/*int argc, char ** argv, char **envp*/
int main (int argc, char ** argv, char **envp);

trieP BuildTrie(char *directory);


pid_t forkProcess();
int parse_options(int argc, char** argv);
void batchMode(int rv, int batch_index, char** argv);


void printEnv(char **envp);
void printEnvVar(char** envp, char* var);
char* getPrintVar(char** envp, char* var);
int getPrintVarIndex(char** envp, char* var);
void setVariable(const char* key, char* newValue);
char * getValueFromKey(const char * key);
void replaceVariables(cmdP exe);


cmdP readCommand(int rv, cmdP exec, char* buffer);
int ReadBuffer(int rv,char inputBuf[]);
cmdP readCommand_buffer(int rv, cmdP exec, char* buffer);
cmdP readCommand_stdin(int rv, cmdP exec);
cmdP parseBuffer(char* inputBuf, cmdP exec, int count);
char** parsePipes(int totalMatches, char* inputBuf);
void freeCommand(cmdP exec);
void freeAllCommands(cmdP exec);

void launchExecutable(cmdP exe);
int executeExternalProgram(cmdP exe);
int isTypeBuiltin(cmdP exe);
int isSoloBuiltin(cmdP exe);
void executeAndFree(cmdP executable);
/*Copies the file from the current fd and creates a new File descriptor with the contents of the old file*/
int copyFiletoNewFile();

bool checkIfProgramExists(char* path, cmdP command);
bool checkIfPathExists(char* path);
bool areCommandsInValidFormat(cmdP exe,bool isFirstProgram);


char * getProgramFromPath(cmdP command);

void executePwd(void);

void echo(cmdP exe);
void setVar(cmdP exe);
void cd(cmdP exec);
void writeHistory(cmdP exec);
void History();
void clearHistory();
void readHistory();
int getLineCount(int fd);
void switchSTDFileDescriptors(int in_fd,int out_fd);
bool glob(const cmdP exe);
void setForeground(cmdP exe);
void setBackground();
void killProcess();
procP findProcess(char* argument);
void addProcessNode(procP newProcess);
procP removeProcessNode(pid_t processID);
void addHistoryNode(histListP newNode, char *buffer);
histListP popHistoryNode(histListP node);
void freeHistory();
char *displayHistory(char sym);
/********WRAPPER CLASSES*******************/
int OpenFileCurrentDirIgnoreMode(const char *file, int flags);
int OpenFileCurrentDir(const char* file, int flags, mode_t mode);
int OpenIgnoreMode(const char* file, int flags);
int Open(const char* file, int flags, mode_t mode);
ssize_t Read(int fd, void* buf, size_t count);
ssize_t Write(int fd, const void* buf, ssize_t count);
int Close(int fd);
void CloseAll(int in_fd, int out_fd, int backFile, int inputFile);
off_t Lseek(int fd, off_t offset, int whence);/*This will move the cursor back*/
/******************************************/


int distinctCount(char* haystack, char* needle);
void printCommand(cmdP command);

void printProcess(procP process);
void printAllProcesses();
void printJobs();


/********************Signal functions****************/
void signal_interrupt(int sig);

void signal_pause(int sig);

void signal_continue(int sig);

void sig_child(int sig);

void sig_child_main(int sig);

lambda_t* SignalFunc(int signal, lambda_t* handler);

void sig_child_run(int sig);

void createBackgroundProcess();

/***************************************************/
