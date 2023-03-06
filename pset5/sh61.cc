#include "sh61.hh"
#include <cstring>
#include <cerrno>
#include <vector>
#include <sys/stat.h>
#include <sys/wait.h>
#include <iostream>
#include <unistd.h>  // chdir function 
using namespace std; 


// A command. 
struct command {
    vector<string> args; 
    pid_t pid = -1; // process ID running this command, -1 if none
    
    int make_child(pid_t pgid);
    
    int run(bool builtin);

    struct command* next_cmd = nullptr;
};


// A pipeline.
struct pipeline {
    struct command* first_cmd = nullptr;
    struct pipeline* next_pipeline = nullptr;
    bool is_and = true;

    // Destructor to delete a pipeline
    ~pipeline() {
        struct command* current = this->first_cmd;
        while (current != nullptr) {
            struct command* tmp = current;
            current = current->next_cmd;
            delete tmp;
        }
    }
};


// Describes one element in a chain of conditionals. 
// Indicates whether the chain should be run in the foreground or background
struct cond {
    struct pipeline* first_pipeline = nullptr;
    struct cond* next_cond = nullptr;
    bool is_foreground = true;

    // Destructor to delete a conditional
    ~cond() {
        struct pipeline* current = this->first_pipeline;

        while (current != nullptr) {
            struct pipeline* tmp = current;
            current = current->next_pipeline;
            delete tmp;
        }
    }
};


// A command line 
struct command_line {
    struct cond* first_conditional = nullptr;

    // Destructor to delete a command line
    ~command_line() {
        struct cond* current = first_conditional;
        while (current != nullptr) {
            struct cond* tmp = current;
            current = current->next_cond;
            delete tmp;
        }
    }
};


// Signal handler 
//    Handles SIGINT.

void signal_handler(int signal) {
    if (signal == SIGINT) {
        printf("\n");
        printf("sh61[%d]$ ", getpid());
        fflush(stdout);
    } 
}


// COMMAND EXECUTION

// command::run(builtin)
//    Runs a single command from the current process. 
//    Sets `this->pid` to the pid of the child process and returns `this->pid`.
//    Handles redirection and builtin commands. 
//    Returns 1 if builtin command failed, 0 otherwise. External commands never return.

int command::run(bool builtin) {
    
    this->pid = getpid();

    int stdout_fd = 1, stderr_fd = 2;   // will be changed if have builtin and redirection 

    const char* c_args[this->args.size() + 1];

    int next_arg = 0;

    // iterate through the arguments 
    for (size_t i = 0; i < this->args.size(); i++) {
        string current_arg = this->args[i];
        
        if (current_arg.compare("<") == 0) {
            int fd = open(this->args[i + 1].c_str(), O_RDONLY);

            // if open fails 
            if (fd == -1) { 
                fprintf(stderr, "%s\n", strerror(errno)); 
                if (builtin) return 1;  // if is builtin, do not exit, return 1
                _exit(1); 
            }
            // if open is successful 
            dup2(fd, STDIN_FILENO); // connect fd to stdin of the program
            close(fd);
            i++;
        } else if (current_arg.compare(">") == 0) {
            int fd = open(this->args[i + 1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (fd == -1) { 
                fprintf(stderr, "%s\n", strerror(errno)); 
                if (builtin) return 1;
                _exit(1);
            }
            if (builtin) {
                stdout_fd = fd;  // connect the output of builtin commands to a file
            } else {
                dup2(fd, STDOUT_FILENO);
                close(fd); 
            }
            i++;
        } else if (current_arg.compare("2>") == 0) {
            int fd = open(this->args[i + 1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (fd == -1) { 
                fprintf(stderr, "%s\n", strerror(errno)); 
                if (builtin) return 1;
                _exit(1);
            }
            if (builtin) {
                stderr_fd = fd; // connect the erreo output of builtin commands to a file
            } else {
                dup2(fd, STDERR_FILENO);
                close(fd); 
            }
            i++;
        } 
        else {
            c_args[next_arg++] = this->args[i].c_str();
        }
    }

    c_args[next_arg] = NULL;  // mark the end of a command as NULL 

    if (strcmp(c_args[0], "cd") == 0) {
        // if the only argument is cd 
        if (next_arg == 1) {
            (void) chdir(getenv("HOME")); // go to home directory 
        } else {
            int n = chdir(c_args[1]);  // go to target directory 
            if (n == -1) {
                dprintf(stderr_fd, "%s\n", strerror(errno));  // output error message into a file
                return 1;
            }
        }
        return 0;
    } else if (strcmp(c_args[0], "pwd") == 0) {
        char buffer[4096];  // maximum path length on linux.
        if (!getcwd(buffer, sizeof(buffer))) return 1;  // store current directory in buffer
        dprintf(stdout_fd, "%s\n", buffer); 
        return 0;
    } else {
        if (execvp(c_args[0], (char* const*) c_args) == -1) {   // execute the command 
            _exit(1);
        }
    }
    return 0;
}


// command::make_child(pgid)
//    Recursively creates all commands in a pipeline and assigns them to process group `pgid`. 
//    Returns the exit code of a builtin command. Does not return otherwise.

int command::make_child(pid_t pgid) {
    assert(this->args.size() > 0);
 
    // builtin commands do not use pipe, are guaranteed to enter here 
    // if is the last command in the pipeline 
    if (this->next_cmd == nullptr) {
        // run command 
        return this->run(pgid == -1);   // pgid = -1 indicates running a builtin command
    }

    // create a pipe between the current command and the next command
    int pfd[2];
    assert(pipe(pfd) != -1); 

    pid_t cpid = fork(); 
    
    if (cpid == 0) {
        setpgid(getpid(), pgid);  // assign the process group id of the current process to pgid

        close(pfd[0]);
        dup2(pfd[1], STDOUT_FILENO); 
        close(pfd[1]);
        
        // run child program with no builtin command
        this->run(false);
    }
 
    close(pfd[1]); 
    dup2(pfd[0], STDIN_FILENO); 
    close(pfd[0]); 

    return this->next_cmd->make_child(pgid);
}



// run_pipeline(pipeline, foreground)
//    Waits until the last process in the pipeline finishes. 
//    Returns the exit code of the last process in the pipeline.

int run_pipeline(pipeline* pipeline, bool foreground) {

    int exit_status = 0;

    // if the pipeline only has a single builtin command, execute without forking
    if (pipeline->first_cmd->next_cmd == nullptr) {
        string cmd = pipeline->first_cmd->args[0];
        if (cmd.compare("cd") == 0 || cmd.compare("pwd") == 0) {
            return pipeline->first_cmd->make_child(-1); // run with pgid = -1, indicating a builtin command
        }
    }

    pid_t p = fork();
    if (p == 0) {
        set_signal_handler(SIGINT, SIG_DFL);  // allow child process to receive SIGINT signal 
        setpgid(0, 0);  
        if (foreground) claim_foreground(getpid()); // child claims foreground 
        pipeline->first_cmd->make_child(getpid());
    }
    setpgid(p, p); 
    waitpid(p, &exit_status, 0);
    claim_foreground(0);    // parent claims foreground 

    return exit_status;
}


// run_conditional(cond)
//    Handles `and ` `or` 
//    Waits until the last process in the conditional finishes.  

void run_conditional(cond* cond) {
    struct pipeline* current = cond->first_pipeline;
    bool status = true;
    bool first_time = true;

    while (current != nullptr) {
        // run the current pipeline if: 
        // is first time
        // or revious pipeline is true with `and`
        // or previous pipeline is false with  `or`
        if (first_time || (status && current->is_and) || (!status && !current->is_and)) {
            int exit_code = run_pipeline(current, cond->is_foreground);
            status = WIFEXITED(exit_code) && WEXITSTATUS(exit_code) == 0;
            first_time = false;
        }
        current = current->next_pipeline; 
    }
}


// run(c)
//    Runs a commandline.   

void run(command_line* c) {
    struct cond* current = c->first_conditional;

    while (current != NULL) {
        // if is forground, run conditional 
        if (current->is_foreground) {
            run_conditional(current);
        } 
        // if is background, fork, then run conditional 
        else {
            pid_t cpid = fork();
            if (cpid == 0) {
                setpgid(0,0);
                run_conditional(current);
                _exit(0); 
            } 
        }
        current = current->next_cond;
    }
} 


// parse_line(s)
//    Parse the command list in `s` and return it. Returns `nullptr` if `s` is empty (only spaces). 
//    Handles token types: background, sequence, and, or, pipe.

command_line* parse_line(const char* s) {
    shell_parser parser(s);

    // initialize command line structure 
    command_line* cmd_line = new command_line;
    command* current_cmd = nullptr;
    pipeline* current_pipeline = nullptr;
    cond* current_cond = nullptr;

    // Build the command
    for (shell_token_iterator it = parser.begin(); it != parser.end(); ++it) {

        if (!current_cond) {
            current_cond = new cond;
            cmd_line->first_conditional = current_cond;
        }

        if (!current_pipeline) {
            current_pipeline = new pipeline;
            current_cond->first_pipeline = current_pipeline;
        }

        if (!current_cmd) {
            current_cmd = new command;
            current_pipeline->first_cmd = current_cmd;
        }

        if (it.type() == TYPE_BACKGROUND || it.type() == TYPE_SEQUENCE) {
            cond* next = new cond;
            current_cond->next_cond = next;
            current_cond->is_foreground = it.type() == TYPE_SEQUENCE;
            current_cond = next;
            current_cmd = nullptr;
            current_pipeline = nullptr;
        }
        else if (it.type() == TYPE_AND || it.type() == TYPE_OR) {
            pipeline* next = new pipeline;
            current_pipeline->next_pipeline = next;
            current_pipeline = next;
            current_pipeline->is_and = it.type() == TYPE_AND;
            current_cmd = nullptr;
        } 
        else if (it.type() == TYPE_PIPE) {
            command* next = new command;
            current_cmd->next_cmd = next;
            current_cmd = next;
        }
        else {
            current_cmd->args.push_back(it.str()); 
        }
    }

    return cmd_line;
}


int main(int argc, char* argv[]) {
    FILE* command_file = stdin;
    bool quiet = false;

    // Check for '-q' option: be quiet (print no prompts)
    if (argc > 1 && strcmp(argv[1], "-q") == 0) {
        quiet = true;
        --argc, ++argv;
    }

    // Check for filename option: read commands from file
    if (argc > 1) {
        command_file = fopen(argv[1], "rb");
        if (!command_file) {
            perror(argv[1]);
            exit(1);
        }
    }

    // - Put the shell into the foreground
    claim_foreground(0);
    // - Ignore the SIGTTOU signal, which is sent when the shell is put back
    //   into the foreground
    set_signal_handler(SIGTTOU, SIG_IGN);
    set_signal_handler(SIGINT, signal_handler);   // signal handler for interrupt 

    char buf[BUFSIZ];
    int bufpos = 0;
    bool needprompt = true;

    while (!feof(command_file)) {
        // Print the prompt at the beginning of the line
        if (needprompt && !quiet) {
            printf("sh61[%d]$ ", getpid());
            fflush(stdout);
            needprompt = false;
        }

        // Read a string, checking for error or EOF
        if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == nullptr) {
            if (ferror(command_file) && errno == EINTR) {
                // ignore EINTR errors
                clearerr(command_file);
                buf[bufpos] = 0;
            } else {
                if (ferror(command_file)) {
                    perror("sh61");
                }
                break;
            }
        }

        // If a complete command line has been provided, run it
        bufpos = strlen(buf);
        if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n')) {
            if (command_line* c = parse_line(buf)) {
                run(c);
                delete c;
            }
            bufpos = 0;
            needprompt = 1;
        }

        // Handle zombie processes and/or interrupt requests
        int status;
        while (waitpid(-1, &status, WNOHANG) > 0) {
        }
    }

    return 0;
}
