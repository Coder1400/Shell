/**
 * Machine Problem: Shell
 * CS 241 - Fall 2016
 */
#include "format.h"
#include "log.h"
#include "shell.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>

void sigint_handler(int num){
    num = 0;
}

void sigchild_handler(int num){
    num = 0;
    /** got this code from Professor Angrave's wikibook! **/
    int status;
    pid_t pid = -1;
    while ( waitpid(pid, &status, WNOHANG) > 0) {}
    
}


int shell(int argc, char *argv[]) {
  
    
    char* history_file = NULL;
    char* script_file = NULL;
    
/*************************************************************************************************/
    
    //GET args manually. GETOPT was the worst documented, most overly complex function
    // I have ever had to deal with.
    
    if ( !(argc == 1 || argc== 3 || argc==5)) {
        print_usage();
        return 0;
    }
    
    if (argc > 1) {
        if ( !strcmp(argv[1], "-h") ) {
            if ( !strcmp(argv[2], "-f") ) {print_usage(); return 0;}
            history_file = argv[2];
            if (argc == 5) {
                 if (!strcmp(argv[3], "-f") ) {
                     if ( argv[4][0] == '-' ) {print_usage(); return 0;}
                      script_file = argv[4];
                 }
                 else{
                     print_usage();
                     return 0;
                 }
            }
        }
        else if ( !strcmp(argv[1], "-f") ) {
            if ( !strcmp(argv[2], "-h") ) {print_usage(); return 0;}
            script_file = argv[2];
            if (argc == 5) {
                if ( !strcmp(argv[3], "-h") ) {
                    if ( argv[4][0] == '-' ) {print_usage(); return 0;}
                    history_file = argv[4];
                }
                else{
                    print_usage();
                    return 0;
                }
            }
        }
        else{print_usage(); return 0;}
    }
/*************************************************************************************************/
    
    Log* log = NULL;
    FILE* stream;
    
    
    if (history_file){
        
        history_file = get_full_path(history_file);
        int exists = access(history_file, F_OK);
        if (exists < 0) {
            print_history_file_error();
            return 0;
        }
        else
            log = Log_create_from_file(history_file);
        
    } else
        log = Log_create();

    
    if (script_file){
        script_file = get_full_path(script_file);
        int exists = access(script_file, F_OK);
        if (exists < 0) {
            print_script_file_error();
            return 0;
        }
        else{
            stream = fopen(script_file, "r");
        }
    } else
        stream = stdin;

    /* signal handling */
    signal(SIGINT, sigint_handler);
    signal(SIGCHLD, sigchild_handler);

    /* getline */
    char *input = NULL;
    size_t len = 0;
    int chars_read;
    /* history command continuation */
    int continuation = 0;
    int mark_input_null = 0;
    char* continuation_text;
    /* shell startup setup */
    int exit = 0;
    char* cwd = 0;
    print_shell_owner("amibrah2");
    while (!exit) {
        
        //continuation signifies if this current command is a continuation of the previous command
        if (!continuation) {
            
            cwd = getcwd(NULL, 0);
            //print prompt before for normal shell
            if (!script_file)
                print_prompt(cwd, getpid());
            
            chars_read = getline(&input, &len, stream);
            if (chars_read == -1) { exit = 1; continue; }
            if (chars_read > 0 && input[chars_read - 1] == '\n') input[chars_read - 1] = 0;
            
            // print prompt after getline for file and print command as if user typed it
            if(script_file){
                print_prompt(cwd, getpid());
                printf("%s\n", input);
            }
            
            
            free(cwd); cwd = 0;
            

        }
        //command triggered by previous command
        else{
            continuation = 0;
            if (input) free(input);
            input = continuation_text;
            mark_input_null = 1;
        }
        
        //If you just pressed enter then just continue to next iteration
        if (!strlen(input)) continue;        
        //We simply split the input by the whitespace delimiter
        size_t num_tokens;
        char** split_input = strsplit(input, " ", &num_tokens);
        //store current command in the history log? Default yes.
        int store_in_history = 1;
        
        //This is where all the real branching will occur!
       
        //built in cd command
        if ( !strcmp(split_input[0], "cd")) {
            int success = chdir(split_input[1]);
            if (success < 0) print_no_directory(split_input[1]);
        }
        else if(!strcmp(split_input[0], "!history")){
            store_in_history = 0;
            size_t size = Log_size(log);
            size_t count = 0;
            while(count< size){
                const char* get = Log_get_command(log, count);
                print_history_line(count, get);
                count++;
            }
        }
        else if(split_input[0][0]=='#'){
            store_in_history = 0;
            if (strlen(split_input[0]) >= 2) {
                
                char* num_c = split_input[0] + 1;
                int num = atoi(num_c);
                
                size_t size = Log_size(log);
                if ((size_t)num >= size) {
                    print_invalid_index();
                }
                else{
                    const char* command = Log_get_command(log, num);
                    print_command(command);
                    continuation = 1;
                    continuation_text = (char*)command;
                }
            }
        }
        else if(split_input[0][0] == '!'){
            store_in_history = 0;
            if (strlen(split_input[0]) > 1 ) {
                
                char* query = split_input[0] + 1;
                char* found_command = NULL;
                size_t size = Log_size(log);
                int count = (int)size - 1;
                while (count >= 0) {
                    char* line = (char*)Log_get_command(log, count);
                    char* substring = strstr(line, query);
                    //If we found a substring at the begenning of the the line
                    if (substring && substring==line) {
                        found_command = line;
                        print_command(found_command);
                        continuation = 1;
                        continuation_text = found_command;
                        break;
                    }
                    count--;
                }
                if (!found_command) print_no_history_match();
            }
        }
        else{
            //External command
            
            //get the last character of the input - whether it is attached to last string or NOT
            char* last_sen  = split_input[num_tokens - 1];
            char last_c = last_sen[strlen(last_sen) - 1];
            int in_background = 0;
            if (last_c == '&') {
                
                in_background = 1;
                last_sen[strlen(last_sen) - 1] = 0;
            }
            
            
            
            fflush(stdout);
            pid_t pid = fork();
            if (pid == -1) { print_fork_failed();}
            else if(pid == 0){
                print_command_executed(getpid());
                execvp(split_input[0], split_input);
                print_exec_failed(input);
                return 0;
            }
            else{
                
                //wait for child if it is not a background process else continue on like nothing even
                //happend and our signal handler will take care of this issue!
                if(!in_background){
                    int status;
                    pid_t res = waitpid(pid, &status, 0);
                    if (res == -1) print_wait_failed();
                }
            }
        }
        //free resources and final steps before next iteration
        
        //add command to log
        if (store_in_history) Log_add_command(log, input);
        //free split_input
        free_args(split_input);
        
        if (input && !mark_input_null) {
            free(input);
        }
        
        //allocate new memory for command evert time. Only way I could solve mem leak
        len = 0;
        input = NULL;
        if (mark_input_null) mark_input_null = 0;
        
    }
    
    //Free resources as we exit loop and get ready to exit the process as whole.
    
    if (input) {
        free(input);
    }
    
    if (cwd) {
        free(cwd);
    }
    if (history_file) {
        Log_save(log, history_file);
        free(history_file);
    }
    if (script_file) {
        free(script_file);
    }
    Log_destroy(log);
    fclose(stream);
    return 0;
}
