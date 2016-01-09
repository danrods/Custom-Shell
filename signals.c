#include "320sh.h"

volatile sig_atomic_t stop_signal = 0;
volatile sig_atomic_t pause_signal = 0;
volatile sig_atomic_t parent_done_signal = 0;
volatile sig_atomic_t child_done_signal = 0;
 // waitpid(process,&wpid,0);/*Having a 0 as input forces the main thread to wait for all child processes to finish before reexecuting*/
              // int exitCode = WIFEXITED(&wpid);
              // if(WIFEXITED(&wpid))
            // exitCode = WEXITSTATUS(&wpid);

void signal_interrupt(int sig){
	sigset_t mask, prev;
  	synchronizeFull(&mask, &prev);

  	atomic_write(stop_sig_message, stop_message_length);
	stop_signal = 1; //Signal the others~
	child_done_signal = 1;
	endSynchronize(&prev);
	sig++;
}

void signal_pause(int sig){
	sigset_t mask, prev;
  	synchronizeFull(&mask, &prev);

	atomic_write(pause_sig_message, pause_message_length);
	pause_signal = 1;

	endSynchronize(&prev);
	sig++;
}

void signal_continue(int sig){
	sigset_t mask, prev;
  	synchronizeFull(&mask, &prev);

  	atomic_write(continue_sig_message, cont_message_length);
	pause_signal = 0;

	endSynchronize(&prev);
	sig++;
}

void sig_child(int sig){
	sigset_t mask, prev;
	pid_t pid;
	int status;
	//synchronizeFull(&mask, &prev);
	while ((pid = waitpid(-1, &status, 0)) > 0) { /* Reap child */
		if(WIFEXITED(&status) == 0){
			atomic_write(exit_sig_message, exit_message_length);
		}
		else{
			atomic_write(exit_ab_sig_message, exit_ab_message_length);
		}

		if(getpgid(pid) == getpgid(foregroundProcess->pId)){
			atomic_write("Yay\n", 4);
			child_done_signal=1;
		}else{
			atomic_write("Sad\n", 4);
		}
		
	}
	//endSynchronize(&prev);
	sig++;
}

void sig_child_main(int sig){
	sigset_t mask, prev;
	pid_t pid;
	int status, lkstatus;
	

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) { /* Reap child */
		//synchronizeFull(&mask, &prev);
		multiThreadDebug("PID SUCKA %d\n", pid);
		if(foregroundProcess != NULL){
			if(pid == foregroundProcess->pId) {
				atomic_write("Found!\n",7);
				foregroundProcess = NULL;
			}
		}
	
		
		if(WIFEXITED(&status) == 0){
			atomic_write(exit_sig_message, exit_message_length);
			lkstatus = 3;
		}
		else{
			atomic_write(exit_ab_sig_message, exit_ab_message_length);
			lkstatus = 2;
		}

		procP process = removeProcessNode(pid);
		if(process != NULL){
			process->lastKnownStatus = lkstatus;
			if(stop_signal != 1){printProcess(process);}

			freeAllCommands(process->command);
			free(process);
		}

		//endSynchronize(&prev);
	}
	
	sig++;
}

void sig_child_run(int sig){
	//atomic_write("Child can run\n", 14);
	parent_done_signal = 1;
	sig++;
}


void freeCommand(cmdP exec){
	if(exec != NULL){
		free(exec->originalStr);
		free(exec);
	}
}

void freeAllCommands(cmdP exec){
	cmdP nextOne, start = exec;
	while(start != NULL){
		nextOne = start->next;
		//Write(1, "Free!\n", 6);
		freeCommand(start);
		start = nextOne;
	}
}
