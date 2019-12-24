#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/mutex.h>


MODULE_LICENSE("GPL");


MODULE_DESCRIPTION("Elevator Module");

#define ENTRY_NAME "elevator"
#define PERMS 0644
#define PARENT NULL
#define IDLE 0
#define UP 1
#define LOADING 2
#define OFFLINE 3
#define DEACTIVATING 4
#define DOWN 5
#define ENTRY_SIZE 1000

static struct file_operations fops;
static char *tempMessage1;//
static char *tempMessage2;

static char *message;
static int read_p;
int elevator_proc_open(struct inode *sp_inode, struct file *sp_file);
ssize_t elevator_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset);
int elevator_proc_release(struct inode *sp_inode, struct file *sp_file);
int elevator_dot_exe(void * data);//the loop for the elevator
int queue_to_elevator(void); // moves from waiting on floor to elevator
void empty_elevator(void); //removes from the elevator
static struct mutex lock;

typedef struct elevatorPassenger
{
	int type;
	int size;
	int weight;
	int startFloor;
	int destinationFloor;	
	struct list_head lift;
	struct list_head queue;
} elevatorPassenger;

//currentLoad is the weight
//currentPassengerUnits is the people units

struct elevatorType
{
	int currentFloor, numPassengers, currentLoad, state, currentPassengerUnits;
	struct list_head lift;
};

struct queueType
{
	int length;
	struct list_head queue;
};


static struct task_struct *thread1;


struct elevatorType elevator;
struct queueType elevator_queue;
int queueWeight = 0;
int numServiced = 0;
int ended = 0;
int weightArr[11] = {0,0,0,0,0,0,0,0,0,0,0};
int servicedArr[11] = {0,0,0,0,0,0,0,0,0,0,0};
int passengerArr[11] = {0,0,0,0,0,0,0,0,0,0,0};


extern long (*STUB_start_elevator)(void);
long our_start_elevator(void){
	printk(KERN_NOTICE "Elevator Start\n");
	if (elevator.state != 3){
		return 1;
	}
	elevator.state = IDLE;
	elevator.currentFloor = 1;
	elevator.numPassengers = 0;
	elevator.currentLoad = 0;
	elevator.currentPassengerUnits = 0;
	//printk(KERN_NOTICE "%d %d %d %d", elevator.state, elevator.currentFloor, elevator.numPassengers, elevator.currentLoad);
	return 0;
}


extern long (*STUB_issue_request)(int,int,int);
long our_issue_request(int passenger, int start, int destination){
    //printk(KERN_NOTICE "%d %d %d", passenger, start, destination);
	elevatorPassenger *temp;
	
	temp = kmalloc(sizeof(elevatorPassenger) * 1, __GFP_RECLAIM);
	

	//create a passenger,intialize, then put into the queue
	
	if (temp == NULL)
		return -ENOMEM;
	if (passenger == 0 || passenger > 4 || start < 1 || start > 10 || destination < 1 || destination > 10)
		return 1;
	
	mutex_lock_interruptible(&lock);
	if (passenger == 1){
		temp->type = 1;
		temp->size = 1;
		temp->weight = 2;
		queueWeight = queueWeight + 2;
		temp->startFloor = start;
		temp->destinationFloor = destination;
	}
	else if (passenger == 2){
		temp->type = 2;
		temp->size = 1;
		temp->weight = 1;
		queueWeight = queueWeight + 1;
		temp->startFloor = start;
		temp->destinationFloor = destination;
	}
	else if (passenger == 3){
		temp->type = 3;
		temp->size = 2;
		temp->weight = 4;
		queueWeight = queueWeight + 4;
		temp->startFloor = start;
		temp->destinationFloor = destination;
	}
	else{
		temp->type = 4;
		temp->size = 2;
		temp->weight = 8;
		queueWeight = queueWeight + 8;
		temp->startFloor = start;
		temp->destinationFloor = destination;
	}
	weightArr[start] = weightArr[start] + temp->weight;
	passengerArr[start] = passengerArr[start] + temp->size;
	//printk(KERN_NOTICE "%d\n",passengerArr[start]);

	list_add_tail(&temp->queue, &elevator_queue.queue);
	elevator_queue.length += 1;
	
	mutex_unlock(&lock);
	//printk(KERN_NOTICE "%d\n",passengerArr[start]);
	//list_del(elevator_queue.queue.next);
	//printk(KERN_NOTICE "%d\n",list_first_entry(&elevator_queue.queue, elevatorPassenger, queue)->type);
	
    return 0;
}


extern long (*STUB_end_elevator)(void);
long our_end_elevator(void){
	if (ended == 1){
		return 1;
	}
	else{
		ended = 1;
   	 	printk(KERN_NOTICE "Elevator end\n");
    	return 0;
	}
	
}

static int elevatormod_init(void){
	STUB_start_elevator = our_start_elevator;
	STUB_issue_request = our_issue_request;
	STUB_end_elevator = our_end_elevator;

	////////////
	printk(KERN_NOTICE "/proc/%s create\n", ENTRY_NAME);
	fops.open = elevator_proc_open;
	fops.read = elevator_proc_read;
	fops.release = elevator_proc_release;
	elevator.state = 3;
	elevator.currentFloor = 1;
	elevator_queue.length = 0;
	mutex_init(&lock);
	INIT_LIST_HEAD(&elevator_queue.queue);
	INIT_LIST_HEAD(&elevator.lift);

	if (!proc_create(ENTRY_NAME, PERMS, NULL, &fops)){
		printk(KERN_WARNING "proc create\n");
		remove_proc_entry(ENTRY_NAME, NULL);
		return -ENOMEM;
	}

	//thread_init_parameter(&thread1);
	thread1 = kthread_run(elevator_dot_exe,NULL, "elevator thread");
	if (IS_ERR(thread1)){
		printk(KERN_WARNING "error spwaning thread");
		remove_proc_entry(ENTRY_NAME, NULL);
		return PTR_ERR(thread1);
	}


	return 0;
}

module_init(elevatormod_init);

static void elevatormod_exit(void){
	STUB_start_elevator = NULL;
	STUB_issue_request = NULL;
	STUB_end_elevator = NULL;
	mutex_destroy(&lock);
	/////////////////////
	kthread_stop(thread1);
	remove_proc_entry(ENTRY_NAME, NULL);
	printk(KERN_NOTICE "Removing /proc/%s\n", ENTRY_NAME);

}

module_exit(elevatormod_exit);

////////////////////////////////////////////////////////
////////////////////////////////////////////////////////
///////////////////////////PROC/////////////////////////
////////////////////////////////////////////////////////
////////////////////////////////////////////////////////





int elevator_proc_open(struct inode *sp_inode, struct file *sp_file){

	int nextFloor;
	mutex_lock_interruptible(&lock);
	if (elevator.currentFloor == 1)
		nextFloor = 2;
	else if (elevator.currentFloor == 2){
		nextFloor = 3;
	}
	else if (elevator.currentFloor == 3){
		nextFloor = 4;
	}
	else if (elevator.currentFloor == 4){
		nextFloor = 5;
	}
	else if (elevator.currentFloor == 5){
		nextFloor = 6;
	}
	else if (elevator.currentFloor == 6){
		nextFloor = 7;
	}
	else if (elevator.currentFloor == 7){
		nextFloor = 8;
	}
	else if (elevator.currentFloor == 8){
		nextFloor = 9;
	}
	else if (elevator.currentFloor == 9){
		nextFloor = 10;
	}
	else{
		nextFloor = 1;
	}
		

	mutex_unlock(&lock);
	printk(KERN_INFO "proc called open\n");
	read_p=1;
	
	message = kmalloc(sizeof(char)* ENTRY_SIZE, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	tempMessage1 = kmalloc(sizeof(char)* 100, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	tempMessage2 = kmalloc(sizeof(char)* 100, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	
	if (message == NULL){
		printk(KERN_WARNING "elevator_proc_open");
		return -ENOMEM;
	}

	
	
	if (elevator.state == UP){
		sprintf(message, "UP | FLOOR: %d | NEXT: %d | # OF PASSENGERS: %d | PASSENGER UNITS: %d | ", elevator.currentFloor, nextFloor, elevator.numPassengers, elevator.currentPassengerUnits);
	}	
	else if (elevator.state == DOWN){
		sprintf(message, "DOWN | FLOOR: %d | NEXT: %d | # OF PASSENGERS: %d | PASSENGER UNITS: %d | ", elevator.currentFloor, nextFloor, elevator.numPassengers, elevator.currentPassengerUnits);

	}
	else if (elevator.state == IDLE){
		sprintf(message, "IDLE | FLOOR: %d | NEXT: %d | # OF PASSENGERS: %d | PASSENGER UNITS: %d | ", elevator.currentFloor, nextFloor, elevator.numPassengers, elevator.currentPassengerUnits);

	}
	else if (elevator.state == OFFLINE){
		sprintf(message, "OFFLINE | FLOOR: %d | NEXT: %d | # OF PASSENGERS: %d | PASSENGER UNITS: %d | ", elevator.currentFloor, nextFloor, elevator.numPassengers, elevator.currentPassengerUnits);
	
	}
	else if (elevator.state == LOADING){
		sprintf(message, "LOADING | FLOOR: %d | NEXT: %d | # OF PASSENGERS: %d | PASSENGER UNITS: %d | ", elevator.currentFloor, nextFloor, elevator.numPassengers, elevator.currentPassengerUnits);
	
	}
	
	if (elevator.currentLoad % 2 == 0){
		sprintf(tempMessage1, "ELEVATOR WEIGHT: %d\n", elevator.currentLoad/2);
	}
	else{
		sprintf(tempMessage1, "ELEVATOR WEIGHT: %d 1/2\n", (elevator.currentLoad-1)/2);
	}
	
	strcat(message,tempMessage1);

	sprintf(tempMessage2, "Floor 1:		PASSENGER UNITS ->  %d		PASSENGER WEIGHT ->  ", passengerArr[1]);

	strcat(message,tempMessage2);
	if (weightArr[1]%2 == 0){
		sprintf(tempMessage2, "%d			# SERVICED  ->  %d\n", weightArr[1]/2, servicedArr[1]);
	}
	else{
		sprintf(tempMessage2, "%d 1/2		# SERVICED  ->  %d\n", (weightArr[1]-1)/2, servicedArr[1]);
	}
	strcat(message,tempMessage2);
//
//
	sprintf(tempMessage2, "Floor 2:		PASSENGER UNITS ->  %d		PASSENGER WEIGHT ->  ", passengerArr[2]);

	strcat(message,tempMessage2);
	if (weightArr[2]%2 == 0){
		sprintf(tempMessage2, "%d			# SERVICED  ->  %d\n", weightArr[2]/2, servicedArr[2]);
	}
	else{
		sprintf(tempMessage2, "%d 1/2		# SERVICED  ->  %d\n", (weightArr[2]-1)/2, servicedArr[2]);
	}
	strcat(message,tempMessage2);
//
//
	sprintf(tempMessage2, "Floor 3:		PASSENGER UNITS ->  %d		PASSENGER WEIGHT ->  ", passengerArr[3]);

	strcat(message,tempMessage2);
	if (weightArr[3]%2 == 0){
		sprintf(tempMessage2, "%d			# SERVICED  ->  %d\n", weightArr[3]/2, servicedArr[3]);
	}
	else{
		sprintf(tempMessage2, "%d 1/2		# SERVICED  ->  %d\n", (weightArr[3]-1)/2, servicedArr[3]);
	}
	strcat(message,tempMessage2);
//
//

sprintf(tempMessage2, "Floor 4:		PASSENGER UNITS ->  %d		PASSENGER WEIGHT ->  ", passengerArr[4]);

	strcat(message,tempMessage2);
	if (weightArr[4]%2 == 0){
		sprintf(tempMessage2, "%d			# SERVICED  ->  %d\n", weightArr[4]/2, servicedArr[4]);
	}
	else{
		sprintf(tempMessage2, "%d 1/2		# SERVICED  ->  %d\n", (weightArr[4]-1)/2, servicedArr[4]);
	}
	strcat(message,tempMessage2);
//
//

sprintf(tempMessage2, "Floor 5:		PASSENGER UNITS ->  %d		PASSENGER WEIGHT ->  ", passengerArr[5]);

	strcat(message,tempMessage2);
	if (weightArr[5]%2 == 0){
		sprintf(tempMessage2, "%d			# SERVICED  ->  %d\n", weightArr[5]/2, servicedArr[5]);
	}
	else{
		sprintf(tempMessage2, "%d 1/2		# SERVICED  ->  %d\n", (weightArr[5]-1)/2, servicedArr[5]);
	}
	strcat(message,tempMessage2);
//
//

sprintf(tempMessage2, "Floor 6:		PASSENGER UNITS ->  %d		PASSENGER WEIGHT ->  ", passengerArr[6]);

	strcat(message,tempMessage2);
	if (weightArr[6]%2 == 0){
		sprintf(tempMessage2, "%d			# SERVICED  ->  %d\n", weightArr[6]/2, servicedArr[6]);
	}
	else{
		sprintf(tempMessage2, "%d 1/2		# SERVICED  ->  %d\n", (weightArr[6]-1)/2, servicedArr[6]);
	}
	strcat(message,tempMessage2);
//
//
sprintf(tempMessage2, "Floor 7:		PASSENGER UNITS ->  %d		PASSENGER WEIGHT ->  ", passengerArr[7]);

	strcat(message,tempMessage2);
	if (weightArr[7]%2 == 0){
		sprintf(tempMessage2, "%d			# SERVICED  ->  %d\n", weightArr[7]/2, servicedArr[7]);
	}
	else{
		sprintf(tempMessage2, "%d 1/2		# SERVICED  ->  %d\n", (weightArr[7]-1)/2, servicedArr[7]);
	}
	strcat(message,tempMessage2);
//
//
sprintf(tempMessage2, "Floor 8:		PASSENGER UNITS ->  %d		PASSENGER WEIGHT ->  ", passengerArr[8]);

	strcat(message,tempMessage2);
	if (weightArr[8]%2 == 0){
		sprintf(tempMessage2, "%d			# SERVICED  ->  %d\n", weightArr[8]/2, servicedArr[8]);
	}
	else{
		sprintf(tempMessage2, "%d 1/2		# SERVICED  ->  %d\n", (weightArr[8]-1)/2, servicedArr[8]);
	}
	strcat(message,tempMessage2);
//
//
sprintf(tempMessage2, "Floor 9:		PASSENGER UNITS ->  %d		PASSENGER WEIGHT ->  ", passengerArr[9]);

	strcat(message,tempMessage2);
	if (weightArr[9]%2 == 0){
		sprintf(tempMessage2, "%d			# SERVICED  ->  %d\n", weightArr[9]/2, servicedArr[9]);
	}
	else{
		sprintf(tempMessage2, "%d 1/2		# SERVICED  ->  %d\n", (weightArr[9]-1)/2, servicedArr[9]);
	}
	strcat(message,tempMessage2);
//
//
sprintf(tempMessage2, "Floor 10:		PASSENGER UNITS ->  %d		PASSENGER WEIGHT ->  ", passengerArr[10]);

	strcat(message,tempMessage2);
	if (weightArr[10]%2 == 0){
		sprintf(tempMessage2, "%d			# SERVICED  ->  %d\n", weightArr[10]/2, servicedArr[10]);
	}
	else{
		sprintf(tempMessage2, "%d 1/2		# SERVICED  ->  %d\n", (weightArr[10]-1)/2, servicedArr[10]);
	}
	strcat(message,tempMessage2);

	return 0;
}
	
ssize_t elevator_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset){
	int len = strlen(message);

	read_p = !read_p;
	if (read_p)
	return 0;

	printk(KERN_INFO "proc called read\n");
	copy_to_user(buf, message, len);
	return len;
}

int elevator_proc_release(struct inode *sp_inode, struct file *sp_file){
	printk(KERN_NOTICE "proc called release\n");
	kfree(message);
	return 0;
}

////////////////////////////////////////////
////////////////////////////////////////////



int elevator_dot_exe(void * data){
	
	while (!kthread_should_stop()){
		
		if (elevator.state != 3){
			
			ssleep(2);
			

			//if stop_elevator has been called then stop loading
			if (ended == 0){
				
				queue_to_elevator();
				
			}
			
			empty_elevator();

			//no one on elevator or in queue...idle
			if (elevator.numPassengers == 0 && elevator_queue.length == 0 && ended == 0){
				mutex_lock_interruptible(&lock);
				elevator.state = IDLE;
				mutex_unlock(&lock);
			}
			//stop elevator has been called and empty
			else if (elevator.numPassengers == 0 &&  ended == 1){
				mutex_lock_interruptible(&lock);
				elevator.state = OFFLINE;
				mutex_unlock(&lock);
			}
			//increases the floor
			if (elevator.state != IDLE && elevator.state != OFFLINE){
				mutex_lock_interruptible(&lock);
				elevator.currentFloor++;
				mutex_unlock(&lock);
			}	
			//when at top go back to bottom
			if (elevator.currentFloor == 11){
				mutex_lock_interruptible(&lock);
				elevator.state = 5;
				elevator.state = 1;
				elevator.currentFloor = 1;
				mutex_unlock(&lock);
			}
			
		}
		
	}

	
	
	return 0;


}
//adds to the elevator based on the start floor and the type
int queue_to_elevator(void){
	struct list_head *temp;
	struct list_head *dummy;
	elevatorPassenger *person;

	list_for_each_safe(temp,dummy,&elevator_queue.queue){
		person = list_entry(temp,elevatorPassenger,queue);
		if (person->startFloor == elevator.currentFloor){
			if (person->type == 1){
				if (elevator.currentLoad + 2 <= 30 && elevator.currentPassengerUnits + 1 <= 10){
					mutex_lock_interruptible(&lock);
					elevator.state = 2;
					elevator_queue.length--;
					elevator.currentLoad = elevator.currentLoad + 2;
					passengerArr[person->startFloor] = passengerArr[person->startFloor] - 1;
					weightArr[person->startFloor] = weightArr[person->startFloor] - 2;
					servicedArr[person->startFloor] = servicedArr[person->startFloor] + 1;
					queueWeight = queueWeight - 2;
					elevator.currentPassengerUnits = elevator.currentPassengerUnits + 1;
					elevator.numPassengers++;	
					list_add_tail(&person->lift,&elevator.lift);
					list_del(temp);
					numServiced++;
					mutex_unlock(&lock);
					ssleep(1);				
				}
				
			}
			else if(person->type == 2){
				if (elevator.currentLoad + 1 <= 30 && elevator.currentPassengerUnits + 1 <= 10){
					mutex_lock_interruptible(&lock);
					elevator.state = 2;
					elevator_queue.length--;
					elevator.currentLoad = elevator.currentLoad + 1;
					servicedArr[person->startFloor] = servicedArr[person->startFloor] + 1;
					queueWeight = queueWeight - 1;
					weightArr[person->startFloor] = weightArr[person->startFloor] - 1;
					passengerArr[person->startFloor] = passengerArr[person->startFloor] - 1;
					elevator.currentPassengerUnits = elevator.currentPassengerUnits + 1;
					elevator.numPassengers++;
					list_add_tail(&person->lift,&elevator.lift);
					list_del(temp);
					numServiced++;
					mutex_unlock(&lock);
					ssleep(1);				
				}
			}
			else if(person->type == 3){
				if (elevator.currentLoad + 4 <= 30 && elevator.currentPassengerUnits + 2 <= 10){
					mutex_lock_interruptible(&lock);
					elevator.state = 2;
					elevator_queue.length--;
					elevator.currentLoad = elevator.currentLoad + 4;
					weightArr[person->startFloor] = weightArr[person->startFloor] - 4;
					passengerArr[person->startFloor] = passengerArr[person->startFloor] - 2;
					servicedArr[person->startFloor] = servicedArr[person->startFloor] + 1;
					queueWeight = queueWeight - 4;
					elevator.currentPassengerUnits = elevator.currentPassengerUnits + 2;
					elevator.numPassengers++;
					list_add_tail(&person->lift,&elevator.lift);
					list_del(temp);
					numServiced++;
					mutex_unlock(&lock);
					ssleep(1);				
				}
			}
			else{
				if (elevator.currentLoad + 8 <= 30 && elevator.currentPassengerUnits + 2 <= 10){
					mutex_lock_interruptible(&lock);
					elevator.state = 2;
					elevator_queue.length--;
					elevator.currentLoad = elevator.currentLoad + 8;
					servicedArr[person->startFloor] = servicedArr[person->startFloor] + 1;
					queueWeight = queueWeight - 8;
					weightArr[person->startFloor] = weightArr[person->startFloor] - 8;
					passengerArr[person->startFloor] = passengerArr[person->startFloor] - 2;
					elevator.currentPassengerUnits = elevator.currentPassengerUnits + 2;
					elevator.numPassengers++;
					list_add_tail(&person->lift,&elevator.lift);
					list_del(temp);
					numServiced++;
					mutex_unlock(&lock);
					ssleep(1);				
				}
			}
			
		}
		mutex_lock_interruptible(&lock);
		elevator.state = 1;
		mutex_unlock(&lock);
	}
	return 0;
}
//removes from the elevator based on the destination floor
void empty_elevator(void){
	
	struct list_head *temp;
	struct list_head *dummy;
	elevatorPassenger *item;

	list_for_each_safe(temp,dummy,&elevator.lift){
		item = list_entry(temp,elevatorPassenger,lift);
		if (item->destinationFloor == elevator.currentFloor){
			if (item->type == 1){
				mutex_lock_interruptible(&lock);
				elevator.currentLoad = elevator.currentLoad - 2;
				elevator.currentPassengerUnits = elevator.currentPassengerUnits - 1;
				elevator.numPassengers--;
				mutex_unlock(&lock);
			}
			else if (item->type == 2){
				mutex_lock_interruptible(&lock);
				elevator.currentLoad = elevator.currentLoad - 1;
				elevator.currentPassengerUnits = elevator.currentPassengerUnits - 1;
				elevator.numPassengers--;
				mutex_unlock(&lock);
			}
			else if (item->type == 3){
				mutex_lock_interruptible(&lock);
				elevator.currentLoad = elevator.currentLoad - 4;
				elevator.currentPassengerUnits = elevator.currentPassengerUnits - 2;
				elevator.numPassengers--;
				mutex_unlock(&lock);
			}
			else{
				mutex_lock_interruptible(&lock);
				elevator.currentLoad = elevator.currentLoad - 8;
				elevator.currentPassengerUnits = elevator.currentPassengerUnits - 2;
				elevator.numPassengers--;
				mutex_unlock(&lock);
			}
			list_del(temp);
			kfree(item);
		}
	}
}
