/**********************************************************
 *  INCLUDES
 *********************************************************/
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include <rtems.h>
#include <rtems/termiostypes.h>
#include <rtems/shell.h>
#include <rtems/untar.h>
#include <bsp.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

/**********************************************************
 *  CONSTANTS
 *********************************************************/
#define NSEC_PER_SEC 1000000000UL

#define DEV_NAME "/dev/com1"
//#define FILE_NAME "/let_it_be_1bit.raw"
#define FILE_NAME "/let_it_be.raw"


#define PERIOD_TASK_SEC    0            /* Period of Task */
//#define PERIOD_TASK_NSEC  512000000    /* Period of Task */
#define PERIOD_TASK_NSEC  64000000    /* Period of Task */
#define SEND_SIZE 256                /* BYTES */

#define TARFILE_START _binary_tarfile_start
#define TARFILE_SIZE _binary_tarfile_size

#define SLAVE_ADDR 0x8

/**********************************************************
 *  GLOBALS
 *********************************************************/
extern int _binary_tarfile_start;
extern int _binary_tarfile_size;

pthread_mutex_t mutex;

/**********************************************************
 * Function: diffTime
 *********************************************************/
void diffTime(struct timespec end,
              struct timespec start,
              struct timespec *diff)
{
    if (end.tv_nsec < start.tv_nsec) {
        diff->tv_nsec = NSEC_PER_SEC - start.tv_nsec + end.tv_nsec;
        diff->tv_sec = end.tv_sec - (start.tv_sec+1);
    } else {
        diff->tv_nsec = end.tv_nsec - start.tv_nsec;
        diff->tv_sec = end.tv_sec - start.tv_sec;
    }
}

/**********************************************************
 * Function: addTime
 *********************************************************/
void addTime(struct timespec end,
              struct timespec start,
              struct timespec *add)
{
    unsigned long aux;
    aux = start.tv_nsec + end.tv_nsec;
    add->tv_sec = start.tv_sec + end.tv_sec +
                  (aux / NSEC_PER_SEC);
    add->tv_nsec = aux % NSEC_PER_SEC;
}

/**********************************************************
 * Function: compTime
 *********************************************************/
int compTime(struct timespec t1,
              struct timespec t2)
{
    if (t1.tv_sec == t2.tv_sec) {
        if (t1.tv_nsec == t2.tv_nsec) {
            return (0);
        } else if (t1.tv_nsec > t2.tv_nsec) {
            return (1);
        } else if (t1.tv_sec < t2.tv_sec) {
            return (-1);
        }
    } else if (t1.tv_sec > t2.tv_sec) {
        return (1);
    } else if (t1.tv_sec < t2.tv_sec) {
        return (-1);
    }
    return (0);
}


struct timespec start_t1,end_t1,diff_t1,cycle_t1;
struct timespec start_t2,end_t2,diff_t2,cycle_t2;
struct timespec start_t3,end_t3,diff_t3,cycle_t3;

char car = '1';
int playing = 1;
struct task_sound_arg_struct {
	int fd_file, fd_serie;
};



//Max 8 bit 15068377
//Max 1 bit  7962721
void *task_sound(void *arguments){
	while(1){
		unsigned char buf[SEND_SIZE];
		struct task_sound_arg_struct *args = arguments;
		int remainder;

		// Retrieve playing status
		pthread_mutex_lock(&mutex);
		int playing_local = playing;
		pthread_mutex_unlock(&mutex);

		// set read buffer to read data if playing, set it to 0 if paused.
		if(playing_local){
			// read from music file
			remainder=read(args->fd_file,buf,SEND_SIZE);
			if (remainder < 0) {
				printf("read: error reading file\n");
				exit(-1);
			}
		}else{
			memset(buf,0,SEND_SIZE);
		}


		// write on the serial/I2C port
		remainder=write(args->fd_serie,buf,SEND_SIZE);
		if (remainder < 0) {
			printf("write: error writting serial\n");
			exit(-1);
		}

		// get end time, calculate lapse and sleep
		clock_gettime(CLOCK_REALTIME,&end_t1);
		diffTime(end_t1,start_t1,&diff_t1);
		if (0 >= compTime(cycle_t1,diff_t1)) {
			printf("ERROR: lasted long than the cycle_t1\n");
			//exit(-1);
		}
		diffTime(cycle_t1,diff_t1,&diff_t1);
		nanosleep(&diff_t1,NULL);
		addTime(start_t1,cycle_t1,&start_t1);
	}
}

//Max time 60970 ns
void task_keyboard(){
	//Task
	while(1){

		// Assign playing state depending on previous read character.
		int playing_local;
		if(car=='1')
			playing_local=1;
		else if (car=='0')
			playing_local=0;

		//Share playing status.
		pthread_mutex_lock(&mutex);
		playing = playing_local;
		pthread_mutex_unlock(&mutex);

		//Read from keyboard.
		while(0>=scanf(" %c",&car));

		clock_gettime(CLOCK_REALTIME,&end_t2);
		diffTime(end_t2,start_t2,&diff_t2);
		diffTime(cycle_t2,diff_t2,&diff_t2);
		nanosleep(&diff_t2,NULL);
		addTime(start_t2,cycle_t2,&start_t2);
	}

}

// Max time 1064352 ns
void *task_state(){
	while(1){
		//Retrieve playing status.
		pthread_mutex_lock(&mutex);
		int playing_local = playing;
		pthread_mutex_unlock(&mutex);

		//Print status
		printf("Reproduction %s\n",playing_local?"resumed":"paused");

		clock_gettime(CLOCK_REALTIME,&end_t3);
		diffTime(end_t3,start_t3,&diff_t3);
		if (0 >= compTime(cycle_t3,diff_t3)) {
			printf("ERROR: lasted long than the cycle_t3\n");
			//exit(-1);
		}
		diffTime(cycle_t3,diff_t3,&diff_t3);
		nanosleep(&diff_t3,NULL);
		addTime(start_t3,cycle_t3,&start_t3);
	}
}
/*****************************************************************************
 * Function: Init()
 *****************************************************************************/
rtems_task Init (rtems_task_argument ignored)
{
    int fd_file = -1;
    int fd_serie = -1;


    printf("Populating Root file system from TAR file.\n");
    Untar_FromMemory((unsigned char *)(&TARFILE_START),
                     (unsigned long)&TARFILE_SIZE);

    rtems_shell_init("SHLL", RTEMS_MINIMUM_STACK_SIZE * 4,
                     100, "/dev/foobar", false, true, NULL);

    /* Open serial port */
    printf("open serial device %s \n",DEV_NAME);
    fd_serie = open (DEV_NAME, O_RDWR);
    if (fd_serie < 0) {
        printf("open: error opening serial %s\n", DEV_NAME);
        exit(-1);
    }

    struct termios portSettings;
    speed_t speed=B115200;

    tcgetattr(fd_serie, &portSettings);
    cfsetispeed(&portSettings, speed);
    cfsetospeed(&portSettings, speed);
    cfmakeraw(&portSettings);
    tcsetattr(fd_serie, TCSANOW, &portSettings);

    /* Open music file */
    printf("open file %s begin\n",FILE_NAME);
    fd_file = open (FILE_NAME, O_RDWR);
    if (fd_file < 0) {
        perror("open: error opening file \n");
        exit(-1);
    }

    // loading cycle_t1 time
    cycle_t1.tv_sec=PERIOD_TASK_SEC;
    cycle_t1.tv_nsec=PERIOD_TASK_NSEC;
    clock_gettime(CLOCK_REALTIME,&start_t1);

    cycle_t2.tv_sec=2;
    cycle_t2.tv_nsec=0;
    clock_gettime(CLOCK_REALTIME,&start_t2);
    cycle_t3.tv_sec=5;
    cycle_t3.tv_nsec=0;
    clock_gettime(CLOCK_REALTIME,&start_t3);


    struct task_sound_arg_struct t1args;
    t1args.fd_file = fd_file;
    t1args.fd_serie = fd_serie;

    pthread_t threadt1, threadt2,threadt3;
    pthread_attr_t attr1, attr2, attr3;

    struct sched_param sched1,sched2,sched3;
    sched1.sched_priority=3;
    sched2.sched_priority=1;
    sched3.sched_priority=2;

    pthread_attr_init(&attr1);
    pthread_attr_setscope(&attr1, PTHREAD_SCOPE_PROCESS);
    pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr1, SCHED_FIFO);
    pthread_attr_setschedparam(&attr1, &sched1);

    pthread_attr_init(&attr2);
    pthread_attr_setscope(&attr2, PTHREAD_SCOPE_PROCESS);
    pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr2, SCHED_FIFO);
    pthread_attr_setschedparam(&attr2, &sched2);

    pthread_attr_init(&attr3);
    pthread_attr_setscope(&attr3, PTHREAD_SCOPE_PROCESS);
    pthread_attr_setinheritsched(&attr3, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr2, SCHED_FIFO);
    pthread_attr_setschedparam(&attr3, &sched3);

    pthread_mutexattr_t mutexattr;
    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_setprotocol(&mutexattr, PTHREAD_PRIO_PROTECT);
    pthread_mutexattr_setprioceiling(&mutexattr, 3);

    pthread_mutex_init(&mutex, &mutexattr);

    pthread_create(&threadt1, &attr1, task_sound, (void*) &t1args);
    pthread_create(&threadt2, &attr2, task_keyboard, (void*) NULL);
    pthread_create(&threadt3, &attr3, task_state, (void*) NULL);

    printf("Starting threads \n");

    pthread_join(threadt1, NULL);
    pthread_join(threadt2, NULL);
    pthread_join(threadt3, NULL);

    exit(0);

} /* End of Init() */

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_APPLICATION_NEEDS_LIBBLOCK
#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 20
#define CONFIGURE_UNIFIED_WORK_AREAS
#define CONFIGURE_UNLIMITED_OBJECTS
#define CONFIGURE_INIT
#include <rtems/confdefs.h>

