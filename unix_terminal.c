#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <inttypes.h>
#include <iso646.h>
#include <sys/wait.h>
#include <unistd.h>
#include <termios.h>
#include <sys/prctl.h>
#include <pwd.h>

#define BUFFER_SIZE 1000

char *getLine(void); // Reads a line from the console
void splitString(char *str, char *substrings[10]); // Splits a string into words
void forceRemove(char *path); // Force delete everything in the path given
int isFile(char *path); // Checks if the path given points to a regular file
int isDirectory(char *path); // Checks if the path given points to a directory
FILE *openFileForInput(char *fileName, char *mode); /*Opens a file for input.
                                                     If mode==">" then we
                                                    write. If it is ">>" we
                                                     abberate.*/
void catFileToFile(char *source, char *dest,
                   char *mode); // Executes cat source mode=">"||mode">>" dest
void catStdinToFile(char *dest,
                    char *mode);     // Executes cat mode=">"||mode=">>" dest
int *pipeLocations(char *args[100]); /* Returns the location of the pipes in the
                                      arguments array*/

pid_t stringToPid(char *pid_string);//Converts a pid in string form to pid_t

void executeCommand(char *args[100],int numOfWords);/*Given an array of a command with arguments and the number of number
                                                  of words in that command, it executes it.*/

/******************************MAIN CODE****************************************************/
int main(void) {
  pid_t child_pid;
  int i,commandCounter,numOfWords,bgCheck;
  struct passwd *pw;
  char *s, *path;
  char *args[100];
  // username=getlogin_r(NULL,0); TO CHECK LATER
  bgCheck=0;
  commandCounter =0; // This will be used to count how many commands the user has entered
  pw=getpwuid(getuid());
  while (1) {
    path = getcwd(NULL, 0);
    printf("%c[4m%s@cs345sh%s/%c[0m$ ", 27,pw->pw_name,path, 27);
    fflush(stdout);
    s = getLine();
    strtok(s, "\n");
    // Initializing the args array with NULL
    for (i = 0; i < 100; i++) {
      args[i] = NULL;
    }
    splitString(s, args);
    i = 0;
		//Counting the number of "words" in the command
    //Checks for "&" in order to send the process to background or not
    while (args[i] != NULL) {

      if(strcmp(args[i],"&")==0){
        args[i]=NULL;
        bgCheck=1;
        break;
      }
      i++;
    }
    numOfWords = i; // This is the number of words given
    if(strcmp(args[0],"cd")==0){/*If the command is cd we don't need to fork off another process,
                                because we need to change the main process's working directory*/
      if(chdir(args[1])!=0){
        printf("Directory not found!\n");
      }
    }else{
      child_pid = fork(); // Time to execute the given command... Time to fork off  a child process!
      if (child_pid!=0&&bgCheck==1) {
        waitpid(-1,NULL,WNOHANG); // Making the parent process wait for the child process to complete.
      }else if(child_pid!=0&&bgCheck==0){
        waitpid(child_pid,NULL,WUNTRACED);
      }else if(child_pid==0&&bgCheck==1){
        prctl(PR_SET_NAME,args[0],NULL,NULL,NULL);//Naming the process
        printf("\nProcess with pid=%d was sent to the background.\n",getpid());
        setpgid(child_pid,0);
        kill(getpid(),SIGSTOP);//Process stops execution and is sent to the background
        executeCommand(args,numOfWords);//When this process receives SIGCONT it resumes execution from here
        kill(getpid(),SIGKILL);
      }else if(child_pid==0&&bgCheck==0){
        executeCommand(args,numOfWords);
        kill(getpid(),SIGKILL);
      }
    }

    bgCheck=0;
    commandCounter++;
  }
  return 0;
}

/****************************************FUNCTIONS**********************************************************************/


void executeCommand(char *args[100],int numOfWords){
  pid_t  tmp_child_pid, tmp_child_pid2;
  int i, k,check, fileDscr, pipeCounter,pipeFds[2], pipeFds2[2],wldCardCheck,filePathCheck,starPosition,
                      currDirCheck,counter,patternSize;
  char *path,*q,*qtmp,*suffix,*full_path,*p,*ptmp;
  struct stat st = {0}; // This will be used to check if a directory exists or not!
  char *argsTmp[100], **command1, **command2, **command3;
  int *pipeLocs;
  FILE *file;
  DIR *d;
  struct dirent *dir;
  wldCardCheck=0;//Used to see if we have a wildcard
  filePathCheck=0;//Used to check if a filepath is given as an argument
  starPosition=0;//If it is 0 the * is in the beginning of the pattern. 1 is in the end

  //Checking for wildcard (*)
  qtmp=NULL;
  q=args[numOfWords-1];
  while(*q!='\0'){
    if(*q=='/'){
      filePathCheck=1;
      qtmp=q;
    }
    q++;
  }


  q=args[numOfWords-1];
  while(*q!='\0'){
	  if(*q=='*'){
      wldCardCheck=1;
      if(filePathCheck==1){//if there is a filepath given
        *qtmp='\0';
        qtmp++;
        if(qtmp==q){//If  *q and qtmp point to the same character then * is the first character of the pattern
          starPosition=0;
          suffix=q+1;
        }else{//If they don't point to the same character then * is the last character of the pattern
          starPosition=1;
          *q='\0';
          suffix=qtmp;
        }
      }else{//if just a pattern is given
        if(args[numOfWords-1]==q){//If  *q and qtmp point to the same character then * is the first character of the pattern
          starPosition=0;
          suffix=q+1;
        }else{//If they don't point to the same character then * is the last character of the pattern
          starPosition=1;
          *q='\0';
          suffix=args[numOfWords-1];
        }
      }
		  break;
	  }
	  q++;
  }

  sleep(0.5);
  pipeLocs = pipeLocations(args);
  if (pipeLocs[0] != -1 || pipeLocs[1] != -1) { // If we have 1 or 2 pipes in the command;
    pipe(pipeFds);
    // Checking how many pipes we have (max 2)
    if (pipeLocs[0] != -1 && pipeLocs[1] == -1) {
      pipeCounter = 1;
    } else if (pipeLocs[0] != -1 && pipeLocs[1] != -1) {
      pipeCounter = 2;
    }
    // Splitting the commands connected with pipes
    // Allocating space for each command
    command1 = malloc((pipeLocs[0] + 1) * sizeof(char *));
    if (pipeCounter == 1) {
      command2 = malloc((numOfWords - pipeLocs[0]) * sizeof(char *));
    } else {
      command2 = malloc((pipeLocs[1] - pipeLocs[0]) * sizeof(char *));
      command3 = malloc((numOfWords - pipeLocs[1]) * sizeof(char *));
    }
    // Splitting each command
    for (i = 0; i < pipeLocs[0]; i++) {
      command1[i] = args[i];
    }
    command1[i + 1] = NULL;
    if (pipeCounter == 1) { // If we have 1 pipe
      i = pipeLocs[0] + 1;
      k = 0;
      while (i < numOfWords + 2) { // We do that because even if the last element of args gets copied as the
                                   // last element of command2 it's ok because it will be NULL and we need that to
                                  // pass command2 to exec()
        command2[k] = args[i];
        i++;
        k++;
      }
    } else { // if we have 2 pipes
      i = pipeLocs[0] + 1;
      k = 0;
      while (strcmp(args[i], "|") != 0) {
        command2[k] = args[i];
        i++;
        k++;
      }
      command2[k] = NULL;
      i++; // args[i] currently is a "|" so we move to the next element in
           // args array
      k = 0;
      while (args[i]!=NULL) { // We do that because even if the last element of args gets copied as the
                                   // last element of command3 it's ok because it will be NULL and we need that
                                   //to pass command3 to exec()
        command3[k] = args[i];
        i++;
        k++;
      }
      command3[k] = NULL;
    }
    // FORK OF 2nd PROCESS TO EXECUTE 2nd COMMAND
    tmp_child_pid = fork(); // Forking off a new process in order to execute the 2 commands (or maybe 3)
    if(tmp_child_pid != 0) {//Parent process always executes last process
      close(pipeFds[1]);
      dup2(pipeFds[0], 0); // Redirecting the stdin to writing to the pipe
      if(pipeCounter==1){//If we have 1 pipe
        //Checking if we want to redirect the output to a file
        k=0;
        while(strcmp(command2[k],">")!=0){
          k++;
        }
        if(strcmp(command2[k],">")==0&&command2[k+1]!=NULL){
          file = openFileForInput(command2[k + 1], command2[k]);
          fileDscr = fileno(file);
          dup2(fileDscr, 1);
          command2[k]=NULL;
          command2[k+1]=NULL;
        }
        //Execute the command
        execvp(command2[0], command2);//Last command will be the 2nd command
      }else{//If we have 2 pipes
        //Checking if we want to redirect the output to a file
        k=0;
        while(strcmp(command3[k],">")!=0){
          k++;
        }
        if(strcmp(command3[k],">")==0&&command3[k+1]!=NULL){
          file = openFileForInput(command3[k + 1], command3[k]);
          fileDscr = fileno(file);
          dup2(fileDscr, 1);
          command3[k]=NULL;
          command3[k+1]=NULL;
        }
        //Execute the command
        execvp(command3[0],command3);//Last command will be the 3rd command
      }
      waitpid(-1, NULL, 0);
    }else{//Child process will execute first command ( or second too if we have 2 pipes)
      close(pipeFds[0]);
      dup2(pipeFds[1], 1); // Redirecting the stdin of the 2nd process to read from
                          //the pipe ( the stdout of the 1st command)
      if(pipeCounter == 1) { // If we only have 1 pipe
        execvp(command1[0], command1);//Execute first command
      }else{ // If we have a second pipe we will need to fork off another  process -_-
        // FORK OF 3rd PROCESS TO EXECUTE 1ST AND 2ND COMMAND
        pipe(pipeFds2); // Creating a second pipe to connect the 1st and 2nd command
        tmp_child_pid2 = fork(); // Forking off a 3rd process
        if (tmp_child_pid2 != 0) {
          close(pipeFds2[1]);
          dup2(pipeFds2[0], 0); // Redirecting the stdout to reading from the
                                // pipe (third command)
          execvp(command2[0], command2); // Executing the second command
          waitpid(-1, NULL, 0); // Waiting for the 1st command to be executed
        } else {                // Time to execute the 1st command
          close(pipeFds2[0]);
          dup2(pipeFds2[1], 1); // Redirecting the stdout of this command to
                                // writing to the pipe (the stdout of the 2nd process)
          execvp(command1[0], command1); // Executing 1st command
          exit(0); // Killing the 3rd process
        }
      }
      exit(0); // Killing the 2nd process
    }
  } else {
    // Time for the child process to execute the command given
    if(strcmp(args[0],"fg")==0){
      //Sending a testing signal to see if the process is active/exists
      if(kill(stringToPid(args[1]),0)==0){//If the process exists
        kill(stringToPid(args[1]),SIGCONT);//Send it a signal to continue
      }else{
        printf("Process does not exist!\n");
      }
    } else if (strcmp(args[0], "mkdir") == 0) {
      if (stat(args[1], &st) == -1) { // If stat returns -1 then it means
                                      // that the directory does not exist
        mkdir(args[1], 0700);
      } else {
        printf("Directory already exists!!\n");
      }
    } else if (strcmp(args[0], "rmdir") == 0) {              // Executing the rmdir command
      if(wldCardCheck!=1){
        if (rmdir(args[1]) != 0) { // Try to delete the directory
          // If not successfull throw an error message
          if (errno == ENOTEMPTY || errno == EEXIST) {
            printf("Directory not empty!! Cannot be deleted.\n");
          } else if (errno == EBUSY) {
            printf("Directory is currently in use by a process or the system! Cannot be deleted!\n");
          } else if (errno == ENOTDIR || errno == ENOENT) {
            printf("Directory does not exist!\n");
          }
        }
      }else{

      }
    } else if (strcmp(args[0], "rm") == 0) { // Executing the rm command
      if (args[2] == NULL) { // If the user has only passed the path to the file to be deleted
        path = args[1];//If there is not a wildcard the path points to a single file , else we check all the files in the directory
        if(wldCardCheck!=1){//If we don't have a wildcard we just delete the pointed file
          if (isFile(path)){ // If the path given points to a file
            unlink(path);     // Then delete the file
          }else{            // Otherwise error... -rf should be used
            printf("This is not a file!\n");
          }
        }else{//If we have a wildcard
          patternSize=strlen(suffix);
          currDirCheck=0;
          printf("Pattern size is %d and pattern is %s\n",patternSize,suffix);
          if(filePathCheck==1){
          //Opening the directory containing the files
            d=opendir(path);
          }else{
            currDirCheck=1;
            d=opendir(getcwd(NULL, 0));
          }
          if(d){
            //Start traversing the directory
            while((dir=readdir(d))!=NULL){
              if(currDirCheck==1){
                full_path=malloc(strlen(getcwd(NULL, 0))+strlen(dir->d_name)+1);
              }else{
                full_path=malloc(strlen(path)+strlen(dir->d_name)+1);
              }

              if(currDirCheck==1){
                strcpy(full_path,getcwd(NULL, 0));
              }else{
                strcpy(full_path,path);
              }

              strcat(full_path,"/");

              strcat(full_path,dir->d_name);
              printf("Full path is:%s\n",full_path);
              if(isFile(full_path)){//If the entry is a file
                counter=1;
                p=dir->d_name;
                if(starPosition==0){//If the wildcard is in the beginning of the pattern
                  printf("Star beginning\n");
                  while(counter<strlen(dir->d_name)-patternSize){
                    p++;
                    counter++;
                  }
                  p++;
                }else{
                  printf("Star ending\n");
                  ptmp=p;
                  while(counter<patternSize){
                    ptmp++;
                    counter++;
                  }
                  ptmp++;
                  *ptmp='\0';
                }
                printf("File pattern is:%s\n",p);
                if(strcmp(p,suffix)==0){
                  unlink(full_path);
                }
              }
              full_path=NULL;
            }
          }
        }
      } else if (strcmp(args[1], "-rf") == 0 && args[2] != NULL) { // If the user has passed the -rf arguments
        path = args[2];
        if (isFile(path)) { // If it is a file just delete it
          unlink(path);
        } else { // If it is a directory delete it recursively
          forceRemove(path);
        }
      }
    } else if (strcmp(args[0], "cat") == 0) { // Executing the cat command
      if(args[1]!=NULL&&args[2]==NULL){
        execvp(args[0], args);
      } else if (strcmp(args[1], "<") == 0 &&  args[3] == NULL) { // If the user has not given any file to read into,
                                                          //then the output is printed in the console
        if (args[2] != NULL) {
          execlp(args[0], "<", args[2], NULL);
        }
        printf("\n\n");
      } else if ((strcmp(args[1], ">") == 0 || strcmp(args[1], ">>") == 0) && args[3] == NULL) { // If the user has given a file to write into a file from the console
        printf("Write to file:\n");
        catStdinToFile(args[2], args[1]);
      } else if ((strcmp(args[2],">") == 0 || strcmp(args[2],">>") == 0)&&args[1]!=NULL) { // If the user wants to write
                                                              // into a file from another file
        catFileToFile(args[1], args[3], args[2]);
      } else if (strcmp(args[1], "<") == 0 &&  (strcmp(args[3], ">") == 0 || strcmp(args[3], ">>") == 0)) {//If we want to print into stdout and a file
        if (args[2] != NULL && args[4] != NULL) {
          // We need to fork off a new process because we execute 2 system  calls
          tmp_child_pid = fork();
          if (tmp_child_pid != 0) {
            //sleep(0.5);
            catFileToFile(args[2], args[4],args[3]); // This system call to output to the second file given
            waitpid(-1, NULL, 0);
          } else {
            execlp(args[0], "<", args[2], NULL); // This system call to output to stdout
            exit(0); // When we are done we kill the new "temporary" process
          }
        } else {
          printf("Invalid format for cat command!\n");
        }
      }
      printf("\n");
    } else if (strcmp(args[0], "exit") == 0) { // Executing the exit command
      kill(getppid(),SIGKILL);
      kill(getpid(),SIGKILL);
    } else if (strcmp(args[0], "clear") ==0) { // Executing the clear command
      system("clear");
    } else {
      i = 0;
      check = 0;
      // Checking if operator ">" exists in the command given
      while (args[i] != NULL) {
        if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
          check = 1;
          k = i;
          break;
        }
        i++;
      }
      if (check == 1) { // If there is a ">" or ">>" operator in the command given
        // Make a new args array without the operator or the file name
        argsTmp[k] = NULL;
        i = 0;
        while (i < k) {
          argsTmp[i] = args[i];
          i++;
        }
        file = openFileForInput(args[k + 1], args[k]);
        fileDscr = fileno(file);
        dup2(fileDscr, 1);
        execvp(argsTmp[0], argsTmp);
        fclose(file);
      } else {
        // Otherwise just execute the command
        execvp(args[0], args);
      }
    }
  }
}






pid_t stringToPid(char *pid_string){
  intmax_t intmax;
  char *tmpString;
  pid_t pid;

  errno=0;
  intmax=strtoimax(pid_string,&tmpString,10);
  if(errno!=0||strcmp(tmpString,pid_string)==0||*tmpString!='\0'||intmax!=(pid_t)intmax){
    fprintf(stderr,"Bad PID!\n");
    return -1;
  }else{
    pid=(pid_t)intmax;
    return pid;
  }

}


int *pipeLocations(char *args[100]) {
  int i, pipeCounter;
  int *pipeLocs=malloc(sizeof(int)*2);
  pipeLocs[0] = -1;
  pipeLocs[1] = -1;
  pipeCounter = 0;
  i=0;
  while (args[i] != NULL) {
    if (strcmp(args[i], "|") == 0) {
      if (pipeCounter == 2) { // We will have at most 2 pipes
        break;
      }
      pipeCounter++;
      pipeLocs[pipeCounter - 1] = i;
    }
    i++;
  }
  return pipeLocs;
}

void forceRemove(char *path) {
  DIR *dir;
  char *path_tmp, *full_path;
  struct dirent *ent;
  if ((dir = opendir(path)) != NULL) { // Opening the directory for reading
    while ((ent = readdir(dir))) { // Iterating the contents of the directory
      path_tmp = malloc(strlen(path) + 1);
      strcpy(path_tmp, path);
      strcat(path_tmp, "/");
      if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
        // Filtering the "." and ".."
      } else {
        full_path = malloc(strlen(path_tmp) + strlen(ent->d_name) + 1);
        sprintf(full_path, "%s%s", path_tmp, ent->d_name);
        if (isFile(full_path)) {
          unlink(full_path);
        } else if (isDirectory(full_path)) {
          forceRemove(full_path);
        }
        free(full_path);
      }
    }
    closedir(dir);
    rmdir(path);
  } else {
    printf("Directory not found!\n");
  }
}

FILE *openFileForInput(char *fileName, char *mode) {
  FILE *file;
  if (strcmp(mode, ">") == 0) {
    file = fopen(fileName, "w+");
  } else if (strcmp(mode, ">>") == 0) {
    file = fopen(fileName, "a");
  } else {
    printf("Invalid mode!\n");
    file = NULL;
  }
  return file;
}

void catFileToFile(char *source, char *dest, char *mode) {
  FILE *output;
  int outputNo;
  sleep(0.5);
  if (source != NULL && dest != NULL) {
    if (access(source, F_OK) == -1) {
      printf("You cannot read from that file! File %s does not exist!\n",source);
    } else {
      output = openFileForInput(dest, mode); // Opening the file we are going to write to
      outputNo = fileno(output); // Getting the file descriptor of the file we are writing to
      dup2(outputNo, 1); // Now cat will write to file instead of stdout
      execlp("cat",mode,source, NULL);
    }
  }
}
//TODO: When calling fg to cat > output, the file is created but cannot write to it... FIX THIS!!
void catStdinToFile(char *dest, char *mode) {
  char *buffer, *tmp;
  char c;
  FILE *output;
  sleep(0.5);
  buffer = malloc(BUFFER_SIZE * sizeof(char));
  tmp = buffer;
  output = openFileForInput(dest, mode);
  // Read the contents to be stored into the file
  while(1) {
    c = getchar();
    if (c == EOF) {
      c = '\0';
      memcpy(tmp, &c, sizeof(c));
      break;
    }
    memcpy(tmp, &c, sizeof(c));
    tmp++;
  }

  fwrite(buffer, 1, strlen(buffer) * sizeof(char),output); // Writing into the file
  fclose(output); // Closing the file
  free(buffer);
  output = NULL;
}

int isFile(char *path) {
  struct stat path_stat;
  stat(path, &path_stat);
  return S_ISREG(path_stat.st_mode);
}

int isDirectory(char *path) {
  struct stat path_stat;
  stat(path, &path_stat);
  return S_ISDIR(path_stat.st_mode);
}

void splitString(char *str, char *substrings[10]) {
  char *substr = NULL;
  int i = 0;
  /*"Initializes" strtok and gets the first substring*/
  substr = strtok(str, " ");
  substrings[0] = substr;
  i++;
  /*Loops until there are no more substrings*/
  while (substr != NULL) {
    /*Gets the next substring*/
    substr = strtok(NULL, " ");
    substrings[i] = substr;
    i++;
  }
}

char *getLine(void) {
  char *line = malloc(100*sizeof(char)), *linePointer;
  size_t lenmax = 100, len = lenmax;
  int c;

  if (line == NULL) {
    return NULL;
  }
  linePointer=line;

  while(1){
    c = fgetc(stdin);
    if (c == EOF) {
      break;
    }

    if (--len == 0) {
      len = lenmax;
      char *linen = realloc(linePointer, lenmax *= 2);
      if (linen == NULL) {
        free(linePointer);
        return NULL;
      }
      line = linen + (line - linePointer);
      linePointer = linen;
    }

    if ((*line++ = c) == '\n') {
      break;
    }
  }
  *line = '\0';
  return linePointer;
}
