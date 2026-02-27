#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "sh.h"
#include <glob.h>

extern char **environ;
extern char *path;

int sh(int argc, char **argv, char **envp) {
  char *prompt = calloc(PROMPTMAX, sizeof(char));
//  char *commandline = calloc(MAX_CANON, sizeof(char));
  char *command, *arg, *commandpath, *p, *pwd, *owd;
  char **args = calloc(MAXARGS, sizeof(char*));
  int uid, i, status, argsct, go = 1;
  struct passwd *password_entry;
  char *homedir;
  struct pathelement *pathlist;

  uid = getuid();
  password_entry = getpwuid(uid);               /* get passwd info */
  homedir = password_entry->pw_dir;		/* Home directory to start
						  out with */

  if((pwd = getcwd(NULL, PATH_MAX + 1)) == NULL) {
    perror("getcwd");
    exit(2);
  }

  owd = calloc(strlen(pwd) + 1, sizeof(char));
  memcpy(owd, pwd, strlen(pwd));
  prompt[0] = ' '; prompt[1] = '\0';

  /* Put PATH into a linked list */
  pathlist = get_path();

  FILE *fptr;
  int readingFile = 0;
  if(argc == 2) {
    fptr = fopen(argv[1], "r");
    if(!fptr) {
       printf("File could not be found or doesn't exist \n");
    }
    else {
      readingFile = 1;
    }
  }

  int last_exit_code = 0;
  while(go) {
    /* print your prompt */
    if(!readingFile) {
      printf("[%s] $ ", pwd);
    }

    if(readingFile) {
       if(fgets(prompt, PROMPTMAX, fptr)) {
          prompt[strcspn(prompt, "\n")] = 0;

          if(strcmp(prompt, "prompt") == 0) {
             continue;
          }
          else {
            pid_t pid = fork();
            if(pid == -1) {
                perror("fork failed");
                continue;
            }

            if(pid == 0) {
//	        printf("PROMPT: %s \n", prompt);
                execvp(args[0], args);
                perror("execvp failed");
                exit(EXIT_FAILURE);
            }
	    else {
                int status;
                waitpid(pid, &status, 0);

                if(WIFEXITED(status)) {
                    last_exit_code = WEXITSTATUS(status);
                    printf("Command '%s' exited with code %d \n", prompt, last_exit_code);
                }
		else {
                    printf("Command '%s' did not terminate normally \n", prompt);
                }
            }
          }
       }
       else {
          if(feof(fptr)) {
              fclose(fptr);
              readingFile = 0;
	      go = 0;
	      printf("EC: %d \n", last_exit_code);
	      continue;
           }
       }
    }
    else {
        if(!fgets(prompt, PROMPTMAX, stdin)) {
          if(feof(stdin)) {
              clearerr(stdin);
              printf("\n");
              continue;
          }
       }
    }

    /* get command line and process */
    prompt[strcspn(prompt, "\n")] = '\0';
    if(prompt[0] != '\0') {
      command = strtok(prompt, " ");
      i = 0;
      args[0] = command;

      char* token = command;
      while(token) {
         args[i] = token;
    	 i++;
    	 token = strtok(NULL, " ");
      }
      args[i] = NULL;

      /* check for each built in command and implement */
      if(strcmp(command, "exit") == 0) {
        if(!checkEnvValExistence()) {
            printf("Executing built-in 'exit' command \n");
        }
	int exit_code = args[1] ? atoi(args[1]) : 0;
        printf("EC: %d \n", exit_code);
	free(prompt);
        free(owd);
	free(pwd);
	struct pathelement *current = pathlist;
	while(current != NULL) {
    	   struct pathelement *next = current->next;
    	   free(current);
    	   current = next;
        }
	free(path);
 	free(args);
        go = 0;
      }
      else if(strcmp(command, "which") == 0) {
        if(!checkEnvValExistence()) {
            printf("Executing built-in 'which' command \n");
        }
        if(args[1]) {
          for(int i = 1; args[i]; i++) {
            char *full_path = which(args[i], pathlist);
            if(full_path) {
 	      printf("%s \n", full_path);
            }
          }
        }
      }
      else if(strcmp(command, "where") == 0) {
        if(!checkEnvValExistence()) {
            printf("Executing built-in 'where' command \n");
        }
        if(args[1]) {
          for(int i = 1; args[i]; i++) {
            char *full_path = where(args[1], pathlist);
            if(full_path) {
              printf("%s \n", full_path);
            }
          }
        }
    }
    else if(strcmp(command, "list") == 0) {
      if(!checkEnvValExistence()) {
         printf("Executing built-in 'list' command \n");
      }
      if(args[1]) {
        for(int i = 1; args[i]; i++) {
          list(args[i]);
        }
      }
      else {
         list(pwd);
      }
    }
    else if(strcmp(command, "ls") == 0) {
      if(!checkEnvValExistence()) {
         printf("Executing built-in 'ls' command \n");
      }
      int globbing_handled = 0;
      for(int i = 1; args[i]; i++) {
        if(strpbrk(args[i], "*?[]")) {
           glob_t globbuf;
           int ret;

           ret = glob(args[i], 0, NULL, &globbuf);

           if(ret == 0) {
              for(size_t i = 0; i < globbuf.gl_pathc; i++) {
                 printf("%s\n", globbuf.gl_pathv[i]);
              }
              globfree(&globbuf);
      	      globbing_handled = 1;
           }
           else {
              fprintf(stderr, "Error using glob: %d \n", ret);
           }
        }
      }
      if(!globbing_handled) {
 	 pid_t pid = fork();
      	 if(pid == 0) {
            int status_code = execvp(command, args);
            if(status_code == -1) {
               printf("Terminated Incorrectly \n");
            }
            printf("\n");
         }
         else if(pid > 0) {
            wait(NULL);
         }
         else {
           perror("Fork failed");
         }
      }
    }
    else if(strcmp(command, "echo") == 0) {
      if(!checkEnvValExistence()) {
          printf("Executing built-in 'echo' command \n");
      }
      for(int i = 1; args[i]; i++) {
          if(args[i][0] == '$') {
             if(strcmp(args[i], "$0") == 0) {
                printf("%s \n", argv[0]);
             }
             else {
                char *varName = &args[i][1];
                char *envVal = getenv(varName);
                if(envVal) {
                   printf("%s ", envVal);
                }
		else {
                  printf("\"\" ");
                }
             }
          }
	  else {
            if(strpbrk(args[i], "*?[]")) {
                glob_t globbuf;
                int ret;

                ret = glob(args[i], 0, NULL, &globbuf);

                if(ret == 0) {
                    for(size_t j = 0; j < globbuf.gl_pathc; j++) {
                        printf("%s ", globbuf.gl_pathv[j]);
                    }
                    globfree(&globbuf);
                } else {
                    fprintf(stderr, "Error using glob: %d \n", ret);
                }
            }
	    else {
                printf("%s ", args[i]);
            }
         }
      }
      printf("\n");
    }
    else if(strcmp(command, "pwd") == 0) {
      if(!checkEnvValExistence()) {
         printf("Executing built-in 'pwd' command \n");
      }
      printf("%s \n", pwd);
    }
    else if(strcmp(command, "cd") == 0) {
      if(!checkEnvValExistence()) {
         printf("Executing built-in 'cd' command \n");
      }
      char *oldpwd = strdup(pwd);
      int numArgs = 0;
      for(int i = 1; args[i]; i++) {
         numArgs++;
      }

      if(numArgs > 1) {
         printf("Too many arguments \n");
      }
      else {
        if(numArgs == 0) {
            if (chdir(homedir) == -1) {
                perror("Failed to change to home directory");
            }
            else {
                char *newpwd = getcwd(NULL, 0);
                if(newpwd) {
                    free(pwd);
                    pwd = newpwd;
                } else {
                    perror("getcwd failed");
                }
            }
        }
        else if(strcmp(args[1], "-") == 0) {
            if(chdir(owd) == -1) {
                perror("Failed to change to previous directory");
            }
            else {
                char *newpwd = getcwd(NULL, 0);
                if(newpwd) {
                    free(pwd);
                    pwd = newpwd;
                }
		else {
                    perror("getcwd failed");
                }
            }
        }
        else {
            if(chdir(args[1]) == -1) {
                perror("Directory not found");
            }
            else {
                char *newpwd = getcwd(NULL, 0);
                if(newpwd) {
                    free(pwd);
                    pwd = newpwd;
                }
		else {
                    perror("getcwd failed");
                }
            }
        }
    }
    if(oldpwd) {
        free(owd);
        owd = oldpwd;
    }
   }
   else if(strcmp(command, "prompt") == 0) {
      if(!checkEnvValExistence()) {
         printf("Executing built-in 'prompt' command \n");
      }
      if(args[1]) {
         printf("%s ", args[1]);
      }
      else {
        char new_prompt[PROMPTMAX];
        printf("Enter prompt prefix: ");
        fgets(new_prompt, sizeof(new_prompt), stdin);
        strcpy(prompt, new_prompt);
        char* prefix = strtok(prompt, " ");
        printf("%s ", prefix);
      }
   }
   else if(strcmp(command, "kill") == 0) {
      if(!checkEnvValExistence()) {
         printf("Executing built-in 'kill' command \n");
      }
      if(!args[1]) {
         fprintf(stderr, "Missing PID \n");
      }
      else {
        pid_t pid = atoi(args[1]);
        int signal = SIGTERM;

        if(args[2]) {
          signal = atoi(args[2]);
          if(signal <= 0 || signal > 31) {
             fprintf(stderr, "Invalid signal number: %d \n", signal);
          }
        }

        int res = kill(pid, 0);
        if(res == -1) {
           perror("Error checking process");
        }

        if(kill(pid, signal) == -1) {
           perror("Error sending signal");
        }
     }
   }
   else if(strcmp(command, "pid") == 0) {
      if(!checkEnvValExistence()) {
         printf("Executing built-in 'pid' command \n");
      }
      pid_t pid = getpid();
      printf("%d\n", pid);
   }
   else if(strcmp(command, "more") == 0) {
      char full_command[PROMPTMAX];
      snprintf(full_command, PROMPTMAX, "more %s", args[1]);
      system(full_command);
   }
   else if(strcmp(command, "setenv") == 0) {
//      if(!checkEnvValExistence()) {
//        printf("Executing built-in 'setenv' command \n");
//      }
      if(!args[1]) {
	char **envp = environ;
	while(*envp) {
	   printf("%s \n", *envp++);
	}
      }
      else if(args[3]) {
        fprintf(stderr, "Too many arguments \n");
      }
      else {
	printf("Created environment variable \n");
        setenv(args[1], args[2] ? args[2] : "", 1);
      }
   }
   else if(strcmp(command, "printenv") == 0) {
      if(!checkEnvValExistence()) {
         printf("Executing built-in 'printenv' command \n");
      }
      char **envp = environ;
      if(!args[1]) {
        while(*envp) {
            printf("%s \n", *envp++);
        }
      }
      else if(args[3]) {
	fprintf(stderr, "Too many arguments \n");
      }
      else {
        char *envVal = getenv(args[1]);
        if(envVal) {
            printf("%s=%s \n", args[1], envVal);
        }
        else {
            printf("No variable: %s \n", args[1]);
        }
      }
   }
   else if(strcmp(command, "addacc") == 0) {
      if(!checkEnvValExistence()) {
         printf("Executing built-in 'addacc' command \n");
      }
      char *envVal = getenv("ACC");
      int currentVal = 0;

      if(envVal) {
        currentVal = atoi(envVal);
      }

      int valueToAdd = 0;
      if(args[1]) {
         int argValue = atoi(args[1]);
         if(argValue != 0 || strcmp(args[1], "0") == 0) {
             valueToAdd = argValue;
         }
      }
      else {
	 valueToAdd = 1;
      }

      currentVal += valueToAdd;

      char newValue[12];
      snprintf(newValue, sizeof(newValue), "%d", currentVal);

      setenv("ACC", newValue, 1);

      printf("New ACC value: %s\n", newValue);
   }
   else {
     char *path_to_command = which(command, pathlist);
     DIR *dir = opendir(command);
     if(dir) {
    	if(!checkEnvValExistence()) {
            printf("Executing %s \n", command);
        }
        printf("%s: IS a directory \n", command);
        closedir(dir);
     }
     else {
        pid_t pid = fork();
        if(pid == 0) {
           int status_code = execvp(command, args);
           if(status_code == -1) {
              if(path_to_command) {
                 if(execve(path_to_command, args, environ) == -1) {
                    perror("execve failed");
                    exit(EXIT_FAILURE);
                 }
              }
              else if(strchr(command, '/') && !dir) {
                 printf("%s: directory does not exist \n", command);
              }
           else {
             fprintf(stderr, "%s: Command not found.\n", command);
           }
          // free(path_to_command); // added
           exit(EXIT_FAILURE);
        }
       }
       else if(pid > 0) {
         int status;
         waitpid(pid, &status, 0);
       }
       else {
         perror("Fork failed");
       }
      }
      free(path_to_command);
     } // else
   } // parent else
 }
 return 0;
} /* sh() */

int checkEnvValExistence() {
    char *envVal = getenv("NOECHO");
    return envVal && strlen(envVal) > 0;
}

char *which(char *command, struct pathelement *pathlist) {
   /* Loop through pathlist until finding command and return it. Return
   NULL when not found. */
   // RETURNS FIRST OCCURANCE OF WHERE A COMMAND IS LOCATED

   while(pathlist) {
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", pathlist->element, command);

    if(access(full_path, F_OK) == 0) {
        return strdup(full_path);
    }

    pathlist = pathlist->next;
   }

   return NULL;

} /* which() */

char *where(char *command, struct pathelement *pathlist) {
  /* Similarly loop through finding all locations of command */
  // RETURNS ALL OCCURANCES OF WHERE A COMMAND IS LOCATED

  char path[PATH_MAX];
  struct pathelement *current = pathlist;
  while(current) {
    snprintf(path, sizeof(path), "%s/%s", current->element, command);

    if(access(path, F_OK) == 0) {
       printf("%s\n", path);
    }

    current = current->next;
  }

  return NULL;
} /* where() */

void list(char *directory) {
  /* see man page for opendir() and readdir() and print out filenames for
  the directory passed */
  printf("\n%s:\n", directory);
    struct dirent *dir;
    DIR *d = opendir(directory);
    if(d) {
      while((dir = readdir(d)) != NULL) {
        printf("%s\n", dir->d_name);
      }
      closedir(d);
    }
    else {
      printf("Directory does not exist. Make sure you're including '/' and that your directory path is correct! \n");
    }
} /* list() */
