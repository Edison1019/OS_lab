#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "../include/command.h"
#include "../include/builtin.h"

// ======================= requirement 2.3 =======================
/**
 * @brief 
 * Redirect command's stdin and stdout to the specified file descriptor
 * If you want to implement ( < , > ), use "in_file" and "out_file" included the cmd_node structure
 * If you want to implement ( | ), use "in" and "out" included the cmd_node structure.
 *
 * @param p cmd_node structure
 * 
 */
void redirection(struct cmd_node *p)
{
	if (p->in_file) {
        int in_fd = open(p->in_file, O_RDONLY);
        if (in_fd == -1) {
            perror("open in_file");
            exit(EXIT_FAILURE);
        }
        if (dup2(in_fd, STDIN_FILENO) == -1) {
            perror("dup2 in_file");
            exit(EXIT_FAILURE);
        }
        close(in_fd); // Close the file descriptor after redirecting
    }

    // Output redirection
    if (p->out_file) {
        int out_fd = open(p->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd == -1) {
            perror("open out_file");
            exit(EXIT_FAILURE);
        }
        if (dup2(out_fd, STDOUT_FILENO) == -1) {
            perror("dup2 out_file");
            exit(EXIT_FAILURE);
        }
        close(out_fd); // Close the file descriptor after redirecting
    }

    //Pipe redirection if needed
    if (p->in != STDIN_FILENO) {  // STDIN_FILENO Typically defined as 0, this constant represents the standard input. By default, it reads input from the keyboard
        if (dup2(p->in, STDIN_FILENO) == -1) {
            perror("dup2 pipe in");
            exit(EXIT_FAILURE);
        }
        close(p->in); // Close the file descriptor after redirecting
    }

    if (p->out != STDOUT_FILENO) { // STDOUT_FILENO ypically defined as 1, this constant represents the standard output. By default, it outputs data to the screen
        if (dup2(p->out, STDOUT_FILENO) == -1) {
            perror("dup2 pipe out");
            exit(EXIT_FAILURE);
        }
        close(p->out); // Close the file descriptor after redirecting
    }
	
}
// ===============================================================

// ======================= requirement 2.2 =======================
/**
 * @brief 
 * Execute external command
 * The external command is mainly divided into the following two steps:
 * 1. Call "fork()" to create child process
 * 2. Call "execvp()" to execute the corresponding executable file
 * @param p cmd_node structure
 * @return int 
 * Return execution status
 */
int spawn_proc(struct cmd_node *p)
{
	pid_t pid = fork(); /* fork another process */
	int status;
	if (pid < 0) 
	{ /* error occurred */
		fprintf(stderr, "Fork Failed");
		exit(-1);
	}
	else if (pid == 0) 
	{ /* child process */
		redirection(p);	
		if (execvp(p->args[0], p->args) == -1) 
		{
            perror("execvp"); // Print error if execvp fails
            exit(EXIT_FAILURE);
        }
	}
	else 
	{ 	/* parent process */ // ret == child’s pid
		/* parent will wait for the child to complete */
		waitpid(pid, &status, 0);
	}

  	return 1;
}
// ===============================================================


// ======================= requirement 2.4 =======================
/**
 * @brief 
 * Use "pipe()" to create a communication bridge between processes
 * Call "spawn_proc()" in order according to the number of cmd_node
 * @param cmd Command structure  
 * @return int
 * Return execution status 
 */
int fork_cmd_node(struct cmd *cmd)
{
	int pipe_fd[2];
	int in = STDIN_FILENO;
	int out = STDOUT_FILENO;
	struct cmd_node *current = cmd->head;
	while(current != NULL)
	{
		if (pipe(pipe_fd) == -1)
		{
			perror("pipe");
			exit(EXIT_FAILURE);
		}
		current->out = pipe_fd[1];
		if(current->next != NULL)
		{
			current->next->in = pipe_fd[0];
		}
		else
		{
			current->out = STDOUT_FILENO;
		}
		spawn_proc(current);
		close(pipe_fd[1]);
		current = current->next;
	}
	return 1;
}
// ===============================================================


void shell()
{
	while (1) {
		printf(">>> $ ");
		char *buffer = read_line();
		if (buffer == NULL)
			continue;

		struct cmd *cmd = split_line(buffer);
		
		int status = -1;
		// only a single command
		struct cmd_node *current = cmd->head;
		
		if(current->next == NULL){
			status = searchBuiltInCommand(current);
			if (status != -1){
				int in = dup(STDIN_FILENO), out = dup(STDOUT_FILENO);
				if( in == -1 | out == -1)
					perror("dup");
				redirection(current);
				status = execBuiltInCommand(status,current);

				// recover shell stdin and stdout
				if (current->in_file)  dup2(in, 0);
				if (current->out_file){
					dup2(out, 1);
				}
				close(in);
				close(out);
			}
			else{
				//external command
				status = spawn_proc(cmd->head);
			}
		}
		// There are multiple commands ( | )
		else{
			
			status = fork_cmd_node(cmd);
		}
		// free space
		while (cmd->head) {
			
			struct cmd_node *current = cmd->head;
      		cmd->head = cmd->head->next;
			free(current->args);
   	    	free(current);
   		}
		free(cmd);
		free(buffer);
		
		if (status == 0)
			break;
	}
}
