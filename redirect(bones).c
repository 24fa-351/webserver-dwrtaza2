#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>


void add_char_to_string(char *str, char c){
    int len = strlen(str);
    str[len] = c;
    str[len + 1] = '\0';
}
void split(char *cmd, char *words[], char delim){
    int word_count = 0;
    char *next_char = cmd;
    char current_word[1004];
    strcpy(current_word, "");
    
    while (*next_char != '\0'){
        if (*next_char == delim){
            words[word_count++] = strdup(current_word);
            strcpy(current_word, "");
        }
        else{
            add_char_to_string(current_word, *next_char);
        }
        ++next_char;
    }
    words[word_count++] = strdup(current_word);
    words[word_count] = NULL;
}

//true == found
bool get_abs_path(char *cmd, char abs_path[]){

    char *directory[1000];
    split(getenv("PATH"), directory, ':');

    for (int i = 0; directory[i]!= NULL; i++ )
    {
        char path[1004];
        strcpy(path, directory[i]);
        add_char_to_string(path, '/');
        strcat(path, cmd);
        if (access(path, X_OK) == 0){
            strcpy(abs_path, path);
            return true;
        }
    }
    return false;
}

int main(int argc, char *argv[]){
    char abs_path[1024];
    char *words[1024];
    
    split(argv[2], words, ' ');

  if(words[0]== NULL){
        printf("No command provided\n");
        return 1;
    }
    if ( !get_abs_path(words[0], abs_path)){
        printf( "Command not found: %s\n", words[0]);
        return 1;
    }

    for (int i = 0; words[i] != NULL; i++){
        printf("words[%d] = %s\n", i, words[i]);
    }
    printf("abs_path = %s\n", abs_path);

    execve(abs_path, words, NULL);
    printf("Failed to execute \n");
    return 1;

}