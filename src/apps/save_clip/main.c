#include <stdio.h>
#inlcude <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#inlcude <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>


static char output_dir[] = "/home/james/ComputerVision/CV_PI5_data/clips";

void activate_cam(const char* path_temp, int duration_ms, bool verbose){
    
}

bool ensure_output_dir(const char* path){
    /* 

        Checks whether output directory exsists, if not creates it. 
        If successful returns true
        else returns false and an errno 

    */

    if (!path || !*path) return false; // Checking for NULL pointers
    
    char *buffer = strdup(path); // Copies path to a buffer for modifying
    if (!buffer){return false;} // Checking NULL return from strdup()
    
    size_t path_length = strlen(buffer);
    if (path_length > 1 && buffer[path_length-1] == '/')buffer[path_length-1]='\0'; // Removes trailing '/'

    // Checking each part of the path to ensure it exsists if it doesnt it creates it using mkdir()
    for(char *p = buffer + (buffer[0]=='/' ? 1:0); *p;  ++p){
        if(*p=='/'){
            *p = '\0';
            if(mkdir(buffer,0775) !=  0 && errno != EEXIST){free(buffer);return false;} 
            *p = '/';
        }
    }
    
    if(mkdir(buffer,0775) != 0 && errno != EEXIST) {free(buffer);return false;} 

    struct stat statbuffer; // Ensures that the final node is a directory not a file itself
    if (stat(buffer,&statbuffer)!=0){free(buffer);return false;}
    if (!S_ISDIR(statbuffer.st_mode)){free(buffer);return false;}

    free(buffer);
    return true; 
}

bool ensure_output_dir_writeable(const char* path){
    /*
        Returns true only if a temp file can be created and deleted in the output directory 
    */
    if(!path ||!*path)return false;

    size_t need = (size_t)snprintf(NULL,0,"%s/.write_check-XXXXXX",path) + 1; // Checks number of bytes needed for the final string
    char *temp_path = malloc(need);
    if (!temp_path)return false;

    snprintf(temp_path,need,"%s/.write_check-XXXXXX",path);

    int fd = mkstemp(temp_path);
    if (fd<0){free(temp_path);return false;}
    close(fd);
    
    if(unlink(temp_path)!=0){free(temp_path);return false;}

    free(temp_path);
    return true;
}

int check_storage(const char* path){
    
    // if storage space is sufficient
    // else create space by replacing the oldest 

}

char* create_filename(){


}


int main(){

    if(!ensure_output_dir(output_dir)){puts("output dir not found");return 0;}
    if(!ensure_output_dir_writeable(output_dir)){puts("output dir not writeable");return 0;}


    activate_cam();

    // Create a temp file 
    // Create the file that you want to save the clip in 

    while(1); 
    return 0;
}