#include "parser.h"
#include <iostream>
#include <string>
#include <vector>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>


using namespace std;



parsed_input parsed;
single_input inp;
int *status;
int pid;



//helper function definitions:
void execCommand(char *args[]);
void execSubShell(char subshell[]);
void execPipe(parsed_input par);
void execPipeFromSeqOrPara(int numComms, command commands[], bool seq);
void execSeq(parsed_input par);
void execPipeFromParallSub(int numComms, command commands[], int repPipes[][2], int m, int number);
void subParallel(parsed_input par);
void execPara(parsed_input par);




int main() {

  char line[INPUT_BUFFER_SIZE];
  signal(SIGPIPE, SIG_IGN);
  
  // MAIN LOOP
  
  while (true) {
  
  cout << "/> ";
  if (!cin.getline(line, INPUT_BUFFER_SIZE))
    break;
  
  // if input is quit
  if (!strcmp(line, "quit"))
    break;
    
  parse_line(line, &parsed);
  
  // depends if single instruction, pipe, seq or parallel
  switch (parsed.separator) {
    
    case SEPARATOR_NONE: // DONE
      inp = parsed.inputs[0];
        if (inp.type == INPUT_TYPE_COMMAND) {
          int f = fork();
          if (f) //parent
            waitpid(f, status, 0);
          
          else //child
            execCommand(inp.data.cmd.args);
        }
        
        
        else if (inp.type == INPUT_TYPE_SUBSHELL) {
          int f = fork();
          if (f) //parent
            waitpid(f, status, 0);
          
          else //child
            execSubShell(inp.data.subshell);
            
        }
      break;
    
    
    
    
    case SEPARATOR_PIPE:
      execPipe(parsed);
      break;
    
    
    
    case SEPARATOR_SEQ:
      execSeq(parsed);
      break;
    
    case SEPARATOR_PARA:
      execPara(parsed);
      break;
    
  }  
  
  
  
  } //end of main while loop
  return 0;
}
















//helper functions

void execCommand(char *args[]) {
    execvp(args[0], args);
    exit(0); // incase of error
}
  

void execSubShell(char subshell[]) {
  
  parsed_input par;
  parse_line(subshell, &par);
  int f;
  switch (par.separator) {
    
    case SEPARATOR_NONE: // input is a single command
      f = fork();
      if (f) //parent
       waitpid(f, status, 0);
       
      else //child
        execCommand(par.inputs[0].data.cmd.args);
        
      break;
    
    
    case SEPARATOR_PIPE: // input is commands seperated with pipes
        execPipe(par);
      break;
    
    
    
    case SEPARATOR_SEQ: // input is commands or pipes(with only commands)
      execSeq(par);
      break;
    
    case SEPARATOR_PARA: // input is commands or pipes (with only commands)
      subParallel(par);
      break;
      
  }
  
  exit(0);
  
}

  


void execPipe(parsed_input par) {
  
  int numComms = par.num_inputs;
  int pipes[numComms - 1][2];
  int f[numComms];
  
  for (int i = 0; i < numComms; i++) {
  
    if (i == numComms - 1) { // special case for last command
      f[i] = fork();
      if (!f[i]) {
      dup2(pipes[i - 1][0], 0);
      close(pipes[i - 1][0]);
      close(pipes[i - 1][1]);
      if (par.inputs[i].type == INPUT_TYPE_COMMAND)
        execCommand(par.inputs[i].data.cmd.args);
      else {
        execSubShell(par.inputs[i].data.subshell);
      }
      exit(0);
      
      }
      //parent:
      close(pipes[i - 1][0]);
      close(pipes[i - 1][1]);
      continue;
    }
    
    pipe(pipes[i]);
    
    if (i == 0) { //  special case for 1st command
      f[i] = fork();
      if (!f[i]) {
      
        dup2(pipes[i][1], 1);
        close(pipes[i][0]);
        close(pipes[i][1]);
        if (par.inputs[i].type == INPUT_TYPE_COMMAND)
          execCommand(par.inputs[i].data.cmd.args);
        else {
          execSubShell(par.inputs[i].data.subshell);
        }
        exit(0);
      }
      //parent:
      continue;
    }
    
    // rest of the middle commands:
    
    f[i] = fork();
    if (!f[i]) {
      dup2(pipes[i - 1][0], 0);
      dup2(pipes[i][1], 1);
      close(pipes[i - 1][0]);
      close(pipes[i - 1][1]);
      close(pipes[i][0]);
      close(pipes[i][1]);
      if (par.inputs[i].type == INPUT_TYPE_COMMAND)
        execCommand(par.inputs[i].data.cmd.args);
      else
        execSubShell(par.inputs[i].data.subshell);
      exit(0);
    }
    
    // parent:
    close(pipes[i - 1][0]);
    close(pipes[i - 1][1]);

  }
  
  for (int i = 0; i < numComms; i++)
    waitpid(f[i], status, 0);
}




void execPipeFromSeqOrPara(int numComms, command commands[], bool seq) {
  
  int pipes[numComms - 1][2];
  int f[numComms];
  
  for (int i = 0; i < numComms; i++) {
    if (i == numComms - 1) { // special case for last command
      f[i] = fork();
      if (!f[i]) {
      dup2(pipes[i - 1][0], 0);
      close(pipes[i - 1][0]);
      close(pipes[i - 1][1]);
      execCommand(commands[i].args);
      exit(0);
      }
      //parent:
      close(pipes[i - 1][0]);
      close(pipes[i - 1][1]);
      continue;
    }
    
    pipe(pipes[i]);
    
    if (i == 0) { //  special case for 1st command
      f[i] = fork();
      if (!f[i]) {
        dup2(pipes[i][1], 1);
        close(pipes[i][0]);
        close(pipes[i][1]);
        execCommand(commands[i].args);
        exit(0);
      }
      //parent:
      continue;
    }
    
    // rest of the middle commands:
    
    f[i] = fork();
    if (!f[i]) {
      dup2(pipes[i - 1][0], 0);
      dup2(pipes[i][1], 1);
      close(pipes[i - 1][0]);
      close(pipes[i - 1][1]);
      close(pipes[i][0]);
      close(pipes[i][1]);
      execCommand(commands[i].args);
      exit(0);
    }
    
    // parent:
    close(pipes[i - 1][0]);
    close(pipes[i - 1][1]);

  }
  
  if (seq) {
  for (int i = 0; i < numComms; i++)
    waitpid(f[i], status, 0);
  }
  
}




// input is commands or pipes(with only commands)
void execSeq(parsed_input par) {
  
  int num = par.num_inputs;
  
  for (int i = 0; i < num; i++) {
    
    if (par.inputs[i].type == INPUT_TYPE_PIPELINE) {
      execPipeFromSeqOrPara(par.inputs[i].data.pline.num_commands, par.inputs[i].data.pline.commands, true);
    }
    
    else {  //  normal command
      int f = fork();
      if (f)
        waitpid(f, status, 0);
      
      else
        execCommand(par.inputs[i].data.cmd.args);
    }
    
  }
  
}


void execPipeFromParallSub(int numComms, command commands[], int repPipes[][2], int m, int number) {

  int pipes[numComms - 1][2];
  
  for (int i = 0; i < numComms; i++) {
    if (i == numComms - 1) { // special case for last command
      if (!fork()) {
      dup2(pipes[i - 1][0], 0);
      close(pipes[i - 1][0]);
      close(pipes[i - 1][1]);
      for (int j = 0; j < number; j++) { //closing all pipes for child
        close(repPipes[j][0]);
        close(repPipes[j][1]);
      }
      execCommand(commands[i].args);
      exit(0);
      }
      //parent:
      close(pipes[i - 1][0]);
      close(pipes[i - 1][1]);
      continue;
    }
    
    pipe(pipes[i]);
    
    if (i == 0) { //  special case for 1st command
      if (!fork()) {
        dup2(repPipes[m][0], 0);
        dup2(pipes[i][1], 1);
        close(pipes[i][0]);
        close(pipes[i][1]);
        for (int j = 0; j < number; j++) { //closing all pipes for child
          close(repPipes[j][0]);
          close(repPipes[j][1]);
        }
        execCommand(commands[i].args);
        exit(0);
      }
      //parent:
      continue;
    }
    
    // rest of the middle commands:
    
    if (!fork()) {
      dup2(pipes[i - 1][0], 0);
      dup2(pipes[i][1], 1);
      close(pipes[i - 1][0]);
      close(pipes[i - 1][1]);
      close(pipes[i][0]);
      close(pipes[i][1]);
      for (int j = 0; j < number; j++) { //closing all pipes for child
        close(repPipes[j][0]);
        close(repPipes[j][1]);
      }
      execCommand(commands[i].args);
      exit(0);
    }
    
    // parent:
    close(pipes[i - 1][0]);
    close(pipes[i - 1][1]);

  }
  
}



void subParallel(parsed_input par) {
  
  int count = 1;
  int pipes[par.num_inputs][2];
  for (int i = 0; i < par.num_inputs; i++)
    pipe(pipes[i]);
  
  for (int i = 0; i < par.num_inputs; i++) {
  
    if (par.inputs[i].type == INPUT_TYPE_COMMAND) {
      count++;
      if (!fork()) {
        dup2(pipes[i][0], 0);
        for (int j = 0; j < par.num_inputs; j++) { //closing all pipes for child
          close(pipes[j][0]);
          close(pipes[j][1]);
        }
        execCommand(par.inputs[i].data.cmd.args);
        exit(0);
      }
    }
    
    else if (par.inputs[i].type == INPUT_TYPE_PIPELINE) {
      count += par.inputs[i].data.pline.num_commands;
      execPipeFromParallSub(par.inputs[i].data.pline.num_commands, par.inputs[i].data.pline.commands, pipes, i, par.num_inputs);
    }
    
  }
  
  // CLOSING ALL INPUT PIPES
  for (int j = 0; j < par.num_inputs; j++)
    close(pipes[j][0]);
  
  // NOW FOR THE REPEATER
  if (!fork()) {
    char c;
    while (read(STDIN_FILENO, &c, 1)) {
      for (int i = 0; i < par.num_inputs; i++)
        write(pipes[i][1], &c, 1);
    }
    
    for (int j = 0; j < par.num_inputs; j++) {
      close(pipes[j][1]);
    }
    
    exit(0);
  }
  
  for (int j = 0; j < par.num_inputs; j++)
    close(pipes[j][1]);
  
  //wait for all
  for (int j = 0; j < count; j++)
    wait(status);
  
}



void execPara(parsed_input par) {
  
  int count = 0;
  int num = par.num_inputs;
  
  for (int i = 0; i < num; i++) {
    
    if (par.inputs[i].type == INPUT_TYPE_PIPELINE) {
      count += par.inputs[i].data.pline.num_commands;
      execPipeFromSeqOrPara(par.inputs[i].data.pline.num_commands, par.inputs[i].data.pline.commands, false);
    }
    
    else {  //  normal command
      count++;
      if (!fork())
        execCommand(par.inputs[i].data.cmd.args);
    }
      
  }
  
  for (int i = 0; i < count; i++)
      wait(status);
  
}
