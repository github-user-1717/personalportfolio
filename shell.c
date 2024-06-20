#include "process.h"


typedef struct DirNode {
    char* directory;
    struct DirNode* next;
} DirNode;

DirNode* dir_stack_head = NULL; // head of the stack

//note about error codes: return errno for syscalls, EXIT_FAILURE for user error checks 
//dont forget to wrap!

int simple_cmd(const CMD *cmd);

int cmd_setup(const CMD *cmd);

int cd_builtin(const CMD *cmd);

int pushd_builtin(const CMD *cmd);

int popd_builtin(const CMD *cmd);

int handle_pipe(const CMD *cmd);

int do_sep_and(const CMD *cmd);

int do_sep_or(const CMD *cmd);

int background(const CMD *cmdList);

int subcommand(const CMD *cmd);

//setting env with status
void wrapper(int status)
{
    char str[64];
    sprintf(str, "%d", status);
    setenv("?", str, 1);
}


int process(const CMD *cmdList)
{
    int zombie_status;

    pid_t check;

    do
    {
        check = waitpid(-1, &zombie_status, WNOHANG);
        if (check > 0)
        {
            fprintf(stderr, "Completed: %d (%d)\n", check, STATUS(zombie_status));
        }
    } while (check > 0);


    if(!cmdList) return 0;

    //keep track of return code to make sure you are doing the right thing when something goes wrong 
    int status = 0; //default to success

    // builtins!

    //cd
    if (cmdList->argv[0] != NULL) {

            if (strcmp("cd", cmdList->argv[0]) == 0)
            {
                // execute cd_builtin and return its exit status directly
                return cd_builtin(cmdList); 
            }

            if (strcmp("pushd", cmdList->argv[0]) == 0)
            {
                return pushd_builtin(cmdList); 
            }

            if (strcmp("popd", cmdList->argv[0]) == 0)
            {
                return popd_builtin(cmdList); 
            }
        }


        switch(cmdList->type)
        {
            case SIMPLE:
                status = simple_cmd(cmdList);
                break;

            case PIPE:
                status = handle_pipe(cmdList);
                break;

            //EQUAL PRECEDENCE?
            case SEP_AND:
                status = do_sep_and(cmdList);
                break;

            case SEP_OR:
                status = do_sep_or(cmdList);
                break;

            case SUBCMD:
                status = subcommand(cmdList);
                break;

            //END AND BG SAME PRECEDENCE?
            case SEP_END:
                status = process(cmdList->left);
                wrapper(status);
                status = process(cmdList->right);
                wrapper(status);
                break;

            //For a subcommand, the status is that of the command within the parentheses.
            //For a backgrounded command, the status in the invoking shell is 0.
            case SEP_BG:
                status = background(cmdList->left);
                if(cmdList->right != NULL)
                {
                    process(cmdList->right);
                }
                break;
    }

    wrapper(status);

    return status;

}


int simple_cmd(const CMD *cmd)
{    
	pid_t pid = fork(); //fork returns program id

    int status = 0;

	if (pid < 0) 
    {
        status = errno;
		perror("fork");

        wrapper(status);
		return status;
    }

	//run code for both parent and child code
	else if (pid == 0)
    {

		//in the child
        //set up local variables if necessary
        if ((status = cmd_setup(cmd)) != 0)
        {
            //redirection didn't work correctly
            //ALWAYS CHECK IF IT FAILED
            status = errno;

            perror("bad setup");

            wrapper(status);

            exit(status);
        }

        //CHECK REDIRECTION
        if (cmd->toType == RED_OUT)
        {
            //check flags
            int fd1 = open(cmd->toFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (fd1 < 0) {
                status = errno;

                perror("open");

                wrapper(status);

                exit(status);
            }
            //change stdin of current thing
            //close old, replace w new open
            dup2(fd1, STDOUT_FILENO);

            close(fd1);
        }

        //append standard out
        if (cmd->toType == RED_OUT_APP)
        {
            //changing flags for append
            int fd1 = open(cmd->toFile, O_WRONLY | O_CREAT | O_APPEND, 0644);

            if (fd1 < 0) {
                status = errno; 

                perror("open");

                wrapper(status);

                exit(status);
            }

            dup2(fd1, STDOUT_FILENO);

            close(fd1);

        }

        // CHECK REDIRECTION for input
        if (cmd->fromType == RED_IN) {

            int fd2 = open(cmd->fromFile, O_RDONLY);

            if (fd2 < 0) {
                status = errno;

                perror("open");

                wrapper(status);

                exit(status);
            }

            dup2(fd2, STDIN_FILENO);

            close(fd2);
        }


        // CHECK heredoc
        if (cmd->fromType == RED_IN_HERE) {

            char tempFilePath[] = "/tmp/heredocXXXXXX";
            
            int fd2 = mkstemp(tempFilePath);
            if (fd2 < 0) {
                status = errno;

                perror("mkstemp");

                wrapper(status);

                exit(status);
            }

            write(fd2, cmd->fromFile, strlen(cmd->fromFile));

            lseek(fd2, 0, SEEK_SET);

            dup2(fd2, STDIN_FILENO);

            unlink(tempFilePath);

            close(fd2);
        }

        execvp(cmd->argv[0], cmd->argv);

        //ERRNO FOR SYSCALLS
        status = errno;

        perror("execvp");
        wrapper(status);

        exit(status);

    }
		
	else
    {
        //in the parent
        //wait for child to finish
        waitpid(pid, &status, 0);

                wrapper(STATUS(status));

        return STATUS(status);
    }

    status = EXIT_SUCCESS;

    wrapper(status);

    return status; 

}


int cmd_setup(const CMD *cmd)
{
    //write a command and specify local variables
    int ret_code = 0;

    for (int i = 0; i < cmd->nLocal; i++)
    {
        setenv(cmd->locVar[i], cmd->locVal[i], 1);
    }

    return ret_code;
}

//ENSURE this properly handles input redirection!! -- completed 4/2
int cd_builtin(const CMD *cmd)
{
    int status = 0;

    if (cmd->argc > 2)
    {
        //error
        status = EXIT_FAILURE;
        perror("too many arguments (cd)");

        wrapper(status);

        return status;
    }

    //change present working directory
    char *pathname = NULL;

    if (cmd->argc == 1)
    {
        pathname = getenv("HOME"); 

        if (!pathname) {
            // handling NULL return from getenv, indicating HOME is not set
            status = errno;
            perror("cd: HOME environment variable not set");

            wrapper(status);

            return status;
        }
    }
    else 
    {
        pathname = cmd->argv[1];

        if (pathname[0] == '\0') {
            // handling empty pathname as an error
            status = EXIT_FAILURE;
            perror("cd: empty pathname not allowed");

            wrapper(status);

            return status;
        }
    }

    if(cmd->toType == RED_OUT)
    {
        int fd = open(cmd->toFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (fd < 0) {
            status = errno;

            perror("open");

            wrapper(status);

            return status;
        }    

        close(fd);   
    }

    //ALWAYS CHECK ERRORS FOR SYSCALLS
    if (chdir(pathname) < 0)
    {
        //error
        status = errno;
        perror("chdir");

        wrapper(status);

        return status; 
    }

    //should default to 0 at this point
    status = EXIT_SUCCESS;
    wrapper(status);

    return status;
}


int pushd_builtin(const CMD *cmd) {
    int status = EXIT_SUCCESS;

    int fd = STDOUT_FILENO;
    //default print to stdout
    
    if (cmd->argc != 2) {
        status = EXIT_FAILURE;
        perror("pushd: exactly one argument required");
        wrapper(status);
        return status;
    }
    
    char currentDir[PATH_MAX];
    if (getcwd(currentDir, sizeof(currentDir)) == NULL) {
        status = errno;
        perror("Error getting current directory");
        //added this -- fixed a write error (even if might not be called here)
        if (fd != STDOUT_FILENO) close(fd);
        wrapper(status);
        return status;
    }


    if(cmd->toType == RED_OUT)
    {
        fd = open(cmd->toFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (fd < 0) {
            status = errno;

            perror("open");

            if (fd != STDOUT_FILENO) close(fd);

            wrapper(status);

            return status;
        }       
    }
    else if(cmd->toType == RED_OUT_APP)
    {
        fd = open(cmd->toFile, O_WRONLY | O_CREAT | O_APPEND, 0644);

        if (fd < 0) {
            status = errno;

            perror("open");

            if (fd != STDOUT_FILENO) close(fd);

            wrapper(status);

            return status;
        }
    }

    if (chdir(cmd->argv[1]) < 0) {
        // if changing directory fails then clean up and close fd if opened
        status = errno;
        perror("pushd: error changing directory");
        if (fd != STDOUT_FILENO) close(fd);
        wrapper(status);
        return status;
    }

    // allocate a new node in stack
    DirNode* newNode = (DirNode*)malloc(sizeof(DirNode));
    if (!newNode) {
        status = errno;
        perror("Failed to allocate node for directory stack");
        if (fd != STDOUT_FILENO) close(fd);
        wrapper(status);
        return status;
    }

    newNode->directory = strdup(currentDir);
    if (!newNode->directory) {
        free(newNode);
        status = errno;
        perror("Failed to allocate directory string");
        if (fd != STDOUT_FILENO) close(fd);
        wrapper(status);
        return status;
    }

    // push the new node onto the stack
    newNode->next = dir_stack_head;
    dir_stack_head = newNode;

    char newCurrentDir[PATH_MAX];
    if (getcwd(newCurrentDir, sizeof(newCurrentDir)) != NULL) {
        write(fd, newCurrentDir, strlen(newCurrentDir)); // print the new current directory
    }
    for (DirNode* current = dir_stack_head; current != NULL; current = current->next) {
        //iterate through and print directories
        write(fd, " ", 1);
        write(fd, current->directory, strlen(current->directory));
    }
    write(fd, "\n", 1);

    // close the file descriptor if needed
    if(cmd->toType == RED_OUT || cmd->toType == RED_OUT_APP)
    {
        close(fd);
    }

    wrapper(status);

    return status;
}

int popd_builtin(const CMD *cmd) {
    int status = EXIT_SUCCESS;

    int fd = STDOUT_FILENO;
    //default print to stdout

    // popd takes no arguments
    if (cmd->argc != 1) {
        status = EXIT_FAILURE;
        perror("popd: no arguments required");
        if (fd != STDOUT_FILENO) close(fd);
        wrapper(status);
        return status;
    }

    // check if the stack is empty
    if (dir_stack_head == NULL) {
        status = EXIT_FAILURE;
        perror("popd: directory stack empty");
        if (fd != STDOUT_FILENO) close(fd);
        wrapper(status);
        return status;
    }

    if(cmd->toType == RED_OUT)
    {
        fd = open(cmd->toFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (fd < 0) {
            status = errno;

            perror("open");
            if (fd != STDOUT_FILENO) close(fd);

            wrapper(status);

            return status;
        }       
    }
    else if(cmd->toType == RED_OUT_APP)
    {
        fd = open(cmd->toFile, O_WRONLY | O_CREAT | O_APPEND, 0644);

        if (fd < 0) {
            status = errno;

            perror("open");
            if (fd != STDOUT_FILENO) close(fd);

            wrapper(status);

            return status;
        }

    }

    // change to the directory at the top of the stack
    if (chdir(dir_stack_head->directory) != 0) {
        status = errno;
        perror("popd: error changing directory");
        if (fd != STDOUT_FILENO) close(fd);
        return status;
    }

    // store the next directory before freeing the current top
    DirNode* toRemove = dir_stack_head;

    dir_stack_head = dir_stack_head->next; // update stack head
    free(toRemove->directory); // clean up the popped node
    free(toRemove);

    // print the stack after pop occurs
    char newCurrentDir[PATH_MAX];
    if (getcwd(newCurrentDir, sizeof(newCurrentDir)) != NULL) {
        write(fd, newCurrentDir, strlen(newCurrentDir)); // print the current directory first
        for (DirNode* current = dir_stack_head; current != NULL; current = current->next) {
            write(fd, " ", 1);
            write(fd, current->directory, strlen(current->directory)); // iterate to print each directory in the stack
        }
    }
    write(fd, "\n", 1);

    if(cmd->toType == RED_OUT || cmd->toType == RED_OUT_APP)
    {
        close(fd);
    }

    wrapper(status);

    return status;
}


int handle_pipe(const CMD *cmd)
{
    //as sketched out in the spec
    int pipe_fd[2]; // array to hold the file descriptors for the pipe
    pid_t pid_left, pid_right;
    int status_left = 0;
    int status_right = 0;

    //errno placeholder for this function
    int status = 0;

    // create a pipe
    if (pipe(pipe_fd) == -1) {
        status = errno;

        perror("pipe");

        wrapper(status);
        //CHECK
        return status;
    }

    // fork for the left command
    if ((pid_left = fork()) == -1) {
        status = errno;
        perror("fork (left)");
        wrapper(status);
        return status;
    }

    if (pid_left == 0) { // left child process

        // redirect stdout to the write end of the pipe USING DUP2
        dup2(pipe_fd[1], STDOUT_FILENO);

        // close the read end of the pipe, not needed in this LEFT child
        close(pipe_fd[0]);
        close(pipe_fd[1]); // close the original write end fd

        //THIS IS A SUBSHELL!!!!
        status_left = process(cmd->left);

        wrapper(status_left);

        exit(status_left);
    }

    // fork for the right command
    if ((pid_right = fork()) == -1) {
        status = errno;
        perror("fork (right)");
        wrapper(status);
        return status;
    }

    if (pid_right == 0) { // right child process

        // redirect stdin to the read end of the pipe
        dup2(pipe_fd[0], STDIN_FILENO);

        // close the write end of the pipe, not needed in this child
        close(pipe_fd[1]);

        close(pipe_fd[0]); // close the original read end fd

        // recurse the right command
        status_right = process(cmd->right);

        wrapper(status_right);

        exit(status_right);
    }

    //parent process
    //close both ends of the pipe
    close(pipe_fd[0]);
    close(pipe_fd[1]);

    // wait for both child processes to complete
    waitpid(pid_left, &status_left, 0);
    waitpid(pid_right, &status_right, 0);

    //this ugly lol but it works
    if(status_right == 0 && status_left == 0)
    {
        wrapper(STATUS(0));
        return STATUS(0);
    }
    else if(status_right != 0 && status_left == 0)
    {
        wrapper(STATUS(status_right));
        return STATUS(status_right);
    }
    else if(status_right == 0 && status_left != 0)
    {
        wrapper(STATUS(status_left));
        return STATUS(status_left);
    }
    else
    {
        wrapper(STATUS(status_right));
        return STATUS(status_right);
    }
}

int do_sep_and(const CMD *cmd) {
    int status_left = 0;

    // process the left command
    status_left = process(cmd->left);
    int final_status = status_left;

    // if left command succeeds then proceed with right command
    if (STATUS(status_left) == 0) { 
        int status_right = process(cmd->right);
        final_status = status_right; // the final status is that of the right command
    }

    wrapper(final_status);

    return final_status; 
}

int do_sep_or(const CMD *cmd) {
    int status_left = 0;

    // process the left command
    status_left = process(cmd->left);
    int final_status = status_left;

    // if left command fails then proceed with right command
    if (STATUS(status_left) != 0) {
        int status_right = process(cmd->right);
        final_status = status_right; 
    }

    wrapper(final_status);

    return final_status;
}


int background(const CMD *cmdList)
{
    if(cmdList->type == SEP_BG)
    {
        background(cmdList->left);
        if(cmdList->right != NULL)
        {
            background(cmdList->right);
        }

    }
    else if(cmdList->type == SEP_END)
    {
        process(cmdList->left);
        if(cmdList->right != NULL)
        {
            background(cmdList->right);
        }
    }
    else
    {
        //SIMPLE STUFF, just dont wait in parent
        //had to mostly copy paste as i didnt modularize
        //backgrounding logic
        pid_t pid = fork(); //fork returns program id

        int status = 0;

        if (pid < 0) 
        {
            status = errno;
            perror("fork");

            wrapper(status);

            return status;
        }
        else if (pid == 0)
        {
            //need to exit in child!
            //subshell
            exit(process(cmdList));

        }
        else
        {
            //print backgrounded 
            fprintf(stderr, "Backgrounded: %d\n", pid);
            //DONT WAITPID
            //RETURN 0
            wrapper(0);

            return 0;
        }

        return status; 
    }

    return 0;
}

int subcommand(const CMD *cmd)
{
    pid_t pid = fork(); //fork returns program id

    int status = 0;

	if (pid < 0) 
    {
        status = errno;
		perror("fork");
        wrapper(status);
		return status;
    }
	else if (pid == 0)
    {
        if ((status = cmd_setup(cmd)) != 0)
        {
            status = errno;
            perror("bad");
            wrapper(status);
            exit(status);
        }

        if (cmd->toType == RED_OUT)
        {
            int fd1 = open(cmd->toFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (fd1 < 0) {
                status = errno;
                perror("open");
                wrapper(status);
                exit(status);
            }
            dup2(fd1, STDOUT_FILENO);

            close(fd1);
        }

        if (cmd->toType == RED_OUT_APP)
        {
            int fd1 = open(cmd->toFile, O_WRONLY | O_CREAT | O_APPEND, 0644);

            if (fd1 < 0) {
                status = errno;
                perror("open");
                wrapper(status);
                exit(status);
            }

            dup2(fd1, STDOUT_FILENO);

            close(fd1);
        }

        if (cmd->fromType == RED_IN) {

            int fd2 = open(cmd->fromFile, O_RDONLY); 

            if (fd2 < 0) {
                status = errno;
                perror("open");
                wrapper(status);
                exit(status);
            }

            dup2(fd2, STDIN_FILENO);

            close(fd2); // Close the original file descriptor
        }

        if (cmd->fromType == RED_IN_HERE) {

            char tempFilePath[] = "/tmp/heredocXXXXXX";
            
            int fd2 = mkstemp(tempFilePath);
            if (fd2 < 0) {
                status = errno;

                perror("mkstemp");

                wrapper(status);

                exit(status);
            }

            write(fd2, cmd->fromFile, strlen(cmd->fromFile));

            lseek(fd2, 0, SEEK_SET);

            dup2(fd2, STDIN_FILENO);

            unlink(tempFilePath);

            close(fd2);
        }

        exit(process(cmd->left));
    }
		
	else
    {
        //in the parent
        //wait for child to finish
        waitpid(pid, &status, 0);
        wrapper(STATUS(status));
        return STATUS(status);
    }
}
