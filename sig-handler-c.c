#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <time.h>
#include <signal.h> //sigaction()
#include <string.h>
#include <stdlib.h>
/*
We will be using the inotify function in Linux to monitor changes in a directory, and store them in a log.
Changes we will monitor: file creation, deletions, and, renamees under the directory named in its command-line argument.
Program will run in the background and monitor/write events to log.

http://man7.org/linux/man-pages/man7/inotify.7.html
https://stackoverflow.com/questions/13351172/inotify-file-in-c
http://man7.org/linux/man-pages/man2/inotify_add_watch.2.html

*/

#define EVENT_SIZE	(sizeof(struct inotify_event))
#define BUFFER_LEN	(1024 * (EVENT_SIZE + 16))

int transactions = 0;
int sighup_caught = 0;
int sigusr1_caught = 0;


//https://stackoverflow.com/questions/16443780/how-to-return-a-char-array-from-a-function-in-c/16444218
char *timestamp(){
    time_t ltime; /* calendar time */
    ltime=time(NULL); /* get current cal time */
    return asctime( localtime(&ltime) );
}

// Receives a path and data
int log_output(char *path, char *content){
	FILE * file_pointer; // pointer to a file, in this case, our log file.
	file_pointer = fopen(path, "a+");
	printf("%s\n", path);
	printf("%s\n", content);
	if(file_pointer==NULL) {
    	perror("Error opening log file.");
		return -1;
	}
	
	fprintf(file_pointer, "%s", content);
	fclose(file_pointer);
	
	return 0;	
}

// 
void handle_signal(int signal) {

    switch(signal){

        case SIGHUP:
            //printf("Caught SIGHUP!"); // I know this isn't safe in signal handlers
            sighup_caught = 1;
            break;
        case SIGUSR1:
            //printf("Transactions reset to 0.");	
            sigusr1_caught = 1;
            break;
        default:
            return;
        return;
              
    }
}
int notify(char *directory, int buffer_length, char *log_directory){


	char buffer[buffer_length];
	int file_descriptor = inotify_init();
	
	if(file_descriptor < 0){
		perror("inotify_init");
	}
	int watch_descriptor = inotify_add_watch(file_descriptor,
					   	   ".", IN_MODIFY | IN_CREATE | IN_DELETE);// http://man7.org/linux/man-pages/man2/inotify_add_watch.2.html
															   	   // IN events are defined in inotify.h - https://code.woboq.org/qt5/include/sys/inotify.h.html
	int data_length = read(file_descriptor, buffer, buffer_length);// length of the data read for file or directory
	
	if(data_length < 0) {
		perror("read");	
	}
	
	int i = 0;
	while(i < data_length) {
		struct inotify_event *event = (struct inotify_event *) &buffer[i];
		if(event->len) {
			if(event->mask & IN_CREATE) {
				printf("%d - %s was created.\n", transactions, event->name);
				log_output(log_directory, strcat(timestamp(), strcat(event->name," was created.\n")));
                transactions++;
                

			} else if(event->mask & IN_DELETE) {
				printf("%d - %s was deleted.\n", transactions, event->name);
				log_output(log_directory, strcat(timestamp(),strcat(event->name," was deleted.\n")));
                transactions++;

			} else if(event->mask & IN_MODIFY) {
				printf("%d - %s was modified.\n", transactions, event->name);	
				log_output(log_directory, strcat(timestamp(), strcat(event->name," was modified.\n")));
                transactions++;

			}
		}
		i += EVENT_SIZE + event->len;
	}
	
	(void) inotify_rm_watch(file_descriptor, watch_descriptor);
	(void) close(file_descriptor);
	return 0;
}

// Arg 0 = program name
// Arg 1 = monitoring directory
// Arg 2 = log directory
// 1 and 2 cannot be equal.
int main(int argc, char **argv){
    // set up the signal handler.
    signal(SIGUSR1, handle_signal);
    signal(SIGHUP, handle_signal);

	// Handle the arguments.
	if(argc < 3){
		printf("Not enough args. See usage: inotify monitor_directory log_directory_incl_filename\n");
		printf("Another example: ./inotify \"home/user/directory\" \"home/user/log/log.txt\"");
		return -1;
	} else if(argc == 3){
		if(strcmp(argv[1],argv[2])==0){ // if the monitor directory is the same as the log directory, don't allow!
			printf("You cannot have the log directory be in the directory you are monitoring! :c\n");
			return -1;
		}
		while(1){
            if(sighup_caught == 1){
                printf("Sighup caught!\n");
                sighup_caught = 0;
            }
            if(sigusr1_caught == 1){
                printf("Sigusr1 caught! Resetting transactions to 0.\n");
                transactions = 0;
                sigusr1_caught = 0;
            }
			notify(argv[1], BUFFER_LEN, argv[2]);
		}
	} else{
		printf("Too many args. See usage: inotify monitor_directory log_directory\n");
		printf("Another example: ./inotify \"home/user/directory\" \"home/user/log/log.txt\"");
		return -1;
	}
	
}
