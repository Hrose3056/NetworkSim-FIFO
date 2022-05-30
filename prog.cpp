#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <map>
#include <vector>
#include <sys/time.h>
#include <signal.h>

#define MAX_NSW 7
#define MAX_LEN 100
#define MAX_IP 1000
#define SRCIP_LO 0
#define SRCIP_HI 1000
#define READ 0
#define WRITE 1

using namespace std;

/*
* Author: Hannah Desmarais
* CCID: hdesmara
* 
* This program simulates a set of TOR packet switches with a master switch to control information
* flow. Each packet switch is connected to the master through two FIFO's, one for reading and one
* for writing, as well as it's neighbours. Port one and two of the switch is for it's neighbours,
* while zero is for the master and three is for the blades. A user can provide a file for the 
* program to read and it will process the lines based on which switch has been activated. A line
* should have the switch being activated first, then the source IP, and lastly, the destination
* IP. A user may type info to be shown a table which changes based on whether the program has been
* invoked as the master, or a packet switch. They may type exit to end the program.
*/
typedef struct pkt{
	char type[MAX_LEN];
	char action[MAX_LEN];
	int swt;
	int neighbours[2];
	char rangeIP[MAX_LEN];
	int srcIP;
	int destIP;
} pkt;

typedef struct forwardingRow{
	int srcIP_lo;
	int srcIP_hi;
	int destIP_lo;
	int destIP_hi;
	string actionType;
	int actionVal = 0;
	int pktCount = 0;
} forwardingRow;

/*
* These global variables are needed in order to process signals sent to the program. Originally,
* they were declared in the main function, which is why they have been passed around to different
* functions, but I had to move them in the end.
*/
bool alarmOn = false;
vector <forwardingRow> forwardTable;
vector <pkt> attachedSwitches;
map <string, int> typeCount;

/*
* The addRow function will add and check any incoming rule to make sure it is valid.
* It can only hold a maximum of 100 rules.
* 
* Arguments:
* 	table- The forwarding table containing all rules as a vector of forwardingRow structs.
*	srcIP_lo- The lower bound of the sources IP as an integer
*	srcIP_hi- The upper bound of the sources IP as an integer
*	destIP_lo- The lower bound of the destinations IP as an integer
*	destIP_hi- The upper bound of the destinations IP as an integer
*	actionType- The rule instruction being added as a string
*	actionVal- The port number for the instruction as an integer
*	pktCount- The number of packets matching the rule taken in as an integer 
* Returns:
*	Returns 0 if one of the arguments has been found to be unacceptable
*/
int addRow(vector<forwardingRow> &table, int srcIP_lo, int srcIP_hi, int destIP_lo,int destIP_hi,
	string actionType, int actionVal, int pktCount){
	struct forwardingRow row;
	
	if (srcIP_lo > srcIP_hi){
		printf("Error: srcIP_lo must be less than srcIP_hi.\n");
		return 0;
	}
	
	if (srcIP_lo < 0) {
		printf("Error: srcIP_lo must be larger than -1.\n");
		return 0;
	}
	row.srcIP_lo = srcIP_lo;
	
	if(srcIP_hi > MAX_IP){
		printf("Error: srcIP_hi must be under 1001.\n");
		return 0;
	} 
	row.srcIP_hi = srcIP_hi;

	if (destIP_lo > destIP_hi){
		printf("Error: destIP_lo must be less than destIP_hi.\n");
		return 0;
	}
	
	if (destIP_lo < 0) {
		printf("Error: destIP_lo must be larger than -1.\n");
		return 0;
	}
	row.destIP_lo = destIP_lo;
	
	if(destIP_hi > MAX_IP){
		printf("Error: destIP_hi must be under 1001.\n");
		return 0;
	} 
	row.destIP_hi = destIP_hi;
	
	row.actionType = actionType;
	row.actionVal = actionVal;
	row.pktCount = pktCount;
	
	if(table.size() < 101) table.push_back(row);
	else {
		printf("The forwarding table is full!\n");
	}
}

/*
* The startPacket checks the arguments provided to the packet switch that has been activated and 
* constructs a HELLO packet to be sent to the master so it can add it to the vector of active 
* switches.
* 
* Arguments:
* 	startpkt- The HELLO packet which information is being added to as a pkt struct
*	argc- The number of arguments provided to the program from the command line as an integer
*	argv- The parsed arguments as an array of C-strings
* Returns:
*	Returns 0 if an argument provided is deemed unacceptable.
*/
int startPacket(struct pkt &startpkt, int argc, char *argv[]){
	strcpy(startpkt.type,"HELLO");
	
	string arg(argv[1]);
	if (arg.substr(0,3) == "psw" && 0 < atoi(&arg.substr(3,1)[0]) < 8){
		startpkt.swt = atoi(&arg.substr(3,1)[0]);
	}
	else{
		printf("Incorrect Argument: Argument 3 must be pswi, where i is the switch you wish to");
		printf(" run and does not exceed the maximum number of switches (7)\n");
		return 0;
	}
	
	//get neighbour in port 1
	string arg3(argv[3]);
	if (arg3 == "null") startpkt.neighbours[0] = -1;
	else if(arg3.substr(0,3) == "psw" && 0 < atoi(&arg3.substr(3,1)[0]) > 0){
		if(atoi(&arg3.substr(3,1)[0]) == startpkt.swt-1){
			startpkt.neighbours[0] = atoi(&arg3.substr(3,1)[0]);
		}
	}
	else{
		printf("Incorrect Argument: Argument 4 may either be null or pswi,");
		printf(" where i is the port 1 neighbour.\n");
		return 0;		
	}
		
	//get neighbour in port 2
	string arg4(argv[4]);
	if (arg4 == "null") startpkt.neighbours[1] = -1;
	else if(arg4.substr(0,3) == "psw" && 0 < atoi(&arg4.substr(3,1)[0]) < 8){
		if(atoi(&arg4.substr(3,1)[0]) == startpkt.swt+1){
			startpkt.neighbours[1] = atoi(&arg4.substr(3,1)[0]);
		}
	}
	else{
		printf("Incorrect Argument: Argument 5 may either be null or pswi,");
		printf(" where i is the port 2 neighbour.\n");
		return 0;
	}
	
	string arg5(argv[5]);
	size_t found = arg5.find("-");
	if (found != string::npos && 0 < found < arg5.length()-1){
		strcpy(startpkt.rangeIP, &(arg5)[0]);
	}
	else {
		printf("Invalid Argument: Argument 6 must be in the form IPlow-IPhigh.\n");
		return 0;
	}
}

/*
* The infoSwitch function will print out the forwarding table and a list of packet types and how
* many the switch has recieved and sent.
* 
* Arguments:
* 	table- The forwarding table containing all rules as a vector of forwardingRow structs.
*	typeCount- The types of packets and how often the switch has processed one of them as 
*			   a map with strings (types) as the keys and integers as the values.
*/
void infoSwitch(vector<forwardingRow> &table, map <string, int> typeCount){
	printf("\nForwarding table:\n");
	for (int i = 0; i < table.size(); i++){
		printf("[%d]  (srcIP= %d-%d, destIP= ", i, table[i].srcIP_lo, table[i].srcIP_hi);
		printf("%d-%d, action= %s:", table[i].destIP_lo, table[i].destIP_hi, &table[i].actionType[0]);
		printf("%d, pktCount= %d)\n", table[i].actionVal, table[i].pktCount);
	}
	
	printf("\nPacket Stats:\n");
	printf("	Recieved:     ADMIT:%d, HELLO_ACK:%d, ", typeCount["ADMIT"], typeCount["HELLO_ACK"]);
	printf("ADD:%d, RELAYIN:%d\n", typeCount["ADD"], typeCount["RELAYIN"]);
	printf("	Transmitted:  HELLO:%d, ASK:%d,", typeCount["HELLO"], typeCount["ASK"]);
	printf(" RELAYOUT:%d\n\n", typeCount["RELAYOUT"]);
}

/*
* The infoMaster function will print a list of switches that have been activated and
* a list of packet types and how often they have been processed by the master switch.
*
* Arguments:
* 	switches- The switches that have been activated as a vector of forwardingRow structs.
*	typeCount- The types of packets and how often the switch has processed one of them as 
*			   a map with strings (types) as the keys and integers as the values.
*/
void infoMaster(vector<pkt> &switches, map <string, int> typeCount){
	printf("\nSwitch information:\n");
	for (int i = 0; i < switches.size(); i++){
		//check to make sure that the pkt at the switch has sent info
		if (switches[i].swt != NULL){
			printf("[psw%d] port1= %d, ", switches[i].swt, switches[i].neighbours[0]);
			printf("port2= %d, port3= %s\n", switches[i].neighbours[1], switches[i].rangeIP);
		}
	}
	
	printf("\nPacket Stats:\n");
	printf("	Received:     HELLO:%d, ASK:%d\n", typeCount["HELLO"], typeCount["ASK"]);
	printf("	Transmitted:  HELLO_ACK:%d, ADD:%d\n\n",typeCount["HELLO_ACK"],typeCount["ADD"]);
}

/*
* The pollOut function polls a FIFO to check it is available to be written to and then
* writes the packet provided.
* 
* Arguments:
* 	pfd- The pollfd struct containing the poll parameters
*	outgoingpkt- The packet being written to the FIFO as a pkt struct
* Returns:
*	Returns 0 if either poll or write fails.
*/
int pollOut(pollfd pfd, pkt outgoingpkt){
	if(poll(&pfd, 1, 0) < 0){
		printf("Poll outgoing FIFO error: %s\n", strerror(errno));
		return 0;
	}
	else if(pfd.revents & POLLOUT){
		if(write(pfd.fd, &outgoingpkt, sizeof(outgoingpkt)) == -1){
			printf("Write error: %s\n", strerror(errno));
			return 0;
		}
	}
}

/*
* The searchTable function searches the forwarding table for a rule matching the header packet
* being processed. If one is found and it is not a FORWARD instruction, it simply increases the
* packet count. However, if it matches a FORWARD rule and the port number indicated in actionVal
* does not equal 3, meaning it has not yet reached it's destination, it sends the packet to the
* switch indicated in the port. If the actionVal does equal 3, it simply increases the packet 
* count as well. If it does not find a rule, it will send an ASK packet to master and then waits 
* for it to send back a rule to add as a ADD packet. If the action is FORWARD, it will then 
* send the header to the switch indicated in the ADD packet. 
* 
* Arguments:
* 	dest- The destination IP of the packet that needs instruction as an integer
*	pipeNo- The pipe number of the switch asking for instruction as an integer
*	fd- The file descriptors of the FIFO's as a 2-d array of integers
*	typeCount- The types of packets and how often the switch has processed one of them as 
*			   a map with strings (types) as the keys and integers as the values.
*	table- The forwarding table containing all rules as a vector of forwardingRow structs.
*/
void searchTable(int dest, int pipeNo, int fd[][2],
	map<string,int> &typeCount,vector<forwardingRow> &table){
	for  (int i = 0; i < table.size(); i++){
		//if a matching rule is found, increase packet count
		if (table[i].destIP_lo <= dest && dest <= table[i].destIP_hi){
			/* 
			* If the type is forward and the value isn't 3, we need to forward the
			* header to the specified neighbour.
			*/
			if (table[i].actionType == "FORWARD" && table[i].actionVal != 3){
				struct pkt relayOut;
				strcpy(relayOut.type,"RELAYOUT");
				relayOut.destIP = dest;
				
				//find out if we are writing to lower or upper neighbour
				string writing = "/tmp/fifo-" + to_string(pipeNo) + "-";
				if(table[i].actionVal == 2) {
					writing += to_string(pipeNo+1);
					relayOut.swt = pipeNo+1;
				}
				else {
					writing += to_string(pipeNo-1);
					relayOut.swt = pipeNo-1;
				}
				
				fd[table[i].actionVal][WRITE] = open(&writing[0], O_WRONLY | O_NONBLOCK);
				if (fd[table[i].actionVal][WRITE] == -1){
					printf("Open relayout search FIFO error: %s\n", strerror(errno));
					return;
				}
				
				struct pollfd relaypfd;
				relaypfd.fd = fd[table[i].actionVal][WRITE];
				relaypfd.events = POLLOUT;
				
				if(pollOut(relaypfd, relayOut) == 0) return;
				close(relaypfd.fd);
				typeCount["RELAYOUT"]++;
			}
			table[i].pktCount++;
			return;
		}
	}
	
	//Not in table so we need to ask master what to do
	struct pkt ask;
	strcpy(ask.type,"ASK");
	ask.swt = pipeNo;
	ask.destIP = dest;
	
	string writing = "/tmp/fifo-" + to_string(pipeNo) + "-0";
	
	fd[0][WRITE] = open(&writing[0], O_WRONLY | O_NONBLOCK);
	if (fd[0][WRITE] == -1){
		printf("Open ask FIFO error: %s\n", strerror(errno));
		return;
	}
		
	struct pollfd askpfd;
	askpfd.fd = fd[0][WRITE];
	askpfd.events = POLLOUT;
	
	if(pollOut(askpfd, ask) == 0) return;
	close(askpfd.fd);
	typeCount["ASK"]++;
	
	//Wait for master to respond with a new rule
	struct pkt incomingPkt;
	vector<pkt> attachedSwitches;
	for(;;){
		struct pollfd addpfd;
		addpfd.fd = fd[0][READ];
		addpfd.events = POLLIN;
		
		if(poll(&addpfd, 1, 0) == -1){
			printf("Poll incoming FIFO error: %s\n", strerror(errno));
			return;
		}
		else if(addpfd.revents & POLLIN){
			if (read(addpfd.fd, &incomingPkt, sizeof(pkt)) == -1){
				printf("Read error: %s\n", strerror(errno));
				return;
			}
			if(strcmp(incomingPkt.type, "ADD") == 0){
				//Add the incoming rule to the table
				typeCount["ADD"]++;
				string actionType(incomingPkt.action);
				
				/*
				* Here we are using swt as the port number the packet is to be directed to.
				* If the port number does not equal 1 or 2, then the packet is to be dropped.
				*/
				if(incomingPkt.swt == 1 || incomingPkt.swt == 2){
					string range = incomingPkt.rangeIP;
				
					int low, high;
					int found = range.find("-");
					if (found != string::npos){
						low = atoi(&range.substr(0, found)[0]);
						high = atoi(&range.substr(found+1)[0]);
					}
			
					if(addRow(table, SRCIP_LO, SRCIP_HI, low, high, 
						actionType, incomingPkt.swt, 1) == 0){
						return;
					}
				}
				else if(addRow(table, SRCIP_LO, SRCIP_HI, incomingPkt.destIP, incomingPkt.destIP, 
						actionType, 0, 1) == 0){
						return;
				}
				
				//If the action type is FORWARD, relay it to the corresponding neighbour
				if (actionType == "FORWARD"){
					struct pkt relayOut;
					strcpy(relayOut.type,"RELAYOUT");
					relayOut.destIP = incomingPkt.destIP;
					
				
					//write packet to corresponding neighbour
					string writing = "/tmp/fifo-" + to_string(pipeNo) + "-";
					
					//find neighbour we are writing to
					int neighbour;
					if(incomingPkt.swt == 2) neighbour = pipeNo+1;
					else neighbour = pipeNo-1;
					relayOut.swt = neighbour;
					
					writing += to_string(neighbour);
					
					fd[incomingPkt.swt-1][WRITE] = open(&writing[0], O_WRONLY | O_NONBLOCK);
					if (fd[incomingPkt.swt-1][WRITE] == -1){
						printf("Open relayout after recieving add FIFO error: %s\n", strerror(errno));
						return;
					}
					
					struct pollfd relaypfd;
					relaypfd.fd = fd[incomingPkt.swt-1][WRITE];
					relaypfd.events = POLLOUT;
					
					if(pollOut(relaypfd, relayOut) == 0) return;
					close(relaypfd.fd);
					typeCount["RELAYOUT"]++;
					return;
				}
			}
			return;
		}	
	}
	return;
}

/*
* The process packet function will find out which type of switch we are processing packets for
* and follow certain actions depending on which type of packet it recieves.
* 
* Arguments:
*	i- The switch number we are processing packets for as an int
* 	incomingPkt- The packet being processed as a pkt struct
*	fd- The file descriptors of the FIFO's connected to the switch as a 2-d array of integers
*	typeCount- The types of packets and how often the switch has processed one of them as 
*			   a map with strings (types) as the keys and integers as the values.
*	table- The forwarding table containing all rules as a vector of forwardingRow structs
*	switches- The switches that have been activated as a vector of forwardingRow structs
*/
void processPacket(pkt incomingPkt, int fd[][2], map<string,int> &typeCount, 
	vector<pkt> &switches, vector<forwardingRow> &table){
	/*
	* If the table of rules is not empty does not come to this method, we know it is a packet
	* switch calling it.
	*/
	if (!table.empty()){
		// If receiving HELLO_ACK, simply increase the packet count
		if(strcmp(incomingPkt.type, "HELLO_ACK") == 0){
			typeCount["HELLO_ACK"]++;
			return;
		}
		/*
		* If receiving a RELAYOUT, search the table to see if there is an existing rule
		* and increment RELAYIN.
		*/
		if(strcmp(incomingPkt.type, "RELAYOUT") == 0){
			typeCount["RELAYIN"]++;

			searchTable(incomingPkt.destIP, incomingPkt.swt, fd, typeCount, table);
			return;
		}
	}
	else{
		// If receiving HELLO, add the new switch to the list and send back HELLO_ACK
		if(strcmp(incomingPkt.type, "HELLO") == 0){
			switches[incomingPkt.swt-1] = incomingPkt;
			typeCount["HELLO"]++; //increase count of pkt type
			string writing = "/tmp/fifo-0-" + to_string(incomingPkt.swt);
				
			struct pkt hello_ack;
			strcpy(hello_ack.type,"HELLO_ACK");
			
			fd[incomingPkt.swt-1][WRITE] = open(&writing[0], O_WRONLY | O_NONBLOCK);
			if (fd[incomingPkt.swt-1][WRITE] == -1){
				printf("Open write FIFO error: %s\n", strerror(errno));
				return;
			}
			
			struct pollfd pfdSending;
			pfdSending.fd = fd[incomingPkt.swt-1][WRITE];
			pfdSending.events = POLLOUT;

			int send = pollOut(pfdSending, hello_ack);
			if (send == 0) {
				return;
			}
			
			close(fd[incomingPkt.swt-1][WRITE]);
			typeCount["HELLO_ACK"]++;
			return;
		}
		// If receiving ASK, figure out what instructions to send back
		if(strcmp(incomingPkt.type, "ASK") == 0){
			typeCount["ASK"]++;
			
			struct pkt add;
			strcpy(add.type, "ADD");
			add.destIP = incomingPkt.destIP;
			
			/*
			* Search the switches list to find out if there is a switch that the header's 
			* destination IP belongs to. If there is, set exists to true and break from the loop
			*/
			int low, high, pos = 0;
			bool exists = false;
			char rangeC[MAX_LEN];
			while(pos < switches.size()){
				if(switches[pos].swt != NULL){
					string range(switches[pos].rangeIP);
					strcpy(rangeC, &range[0]);
					int found = range.find("-");
					if (found != string::npos){
						low = atoi(&range.substr(0, found)[0]);
						high = atoi(&range.substr(found+1)[0]);
					}
					
					if (low <= incomingPkt.destIP && incomingPkt.destIP <= high) {
						exists = true;
						break;
					}
				}
				pos++;
			}
			
			//If a switch was found, figure out if we are sending it left, right, or dropping it
			if(exists == true){
				
				//if it is found in a smaller position, send to lower packet if one exists
				if(switches[incomingPkt.swt-2].swt != NULL && pos < incomingPkt.swt-1){
					strcpy(add.action, "FORWARD");
					add.swt = 1;
					strcpy(add.rangeIP, rangeC);
				}
				//if it is found in a larger position, send it to upper neighbour if exists
				else if(switches[incomingPkt.swt].swt != NULL && pos > incomingPkt.swt-1){
					strcpy(add.action, "FORWARD");
					add.swt = 2;
					strcpy(add.rangeIP, rangeC);
				}
				else{
					//If there is a gap in between, we must drop the packet
					strcpy(add.action, "DROP");
					add.swt = 0;
				}
			}
			else {
				//If there doesn't exist a switch, we must drop the packet
				strcpy(add.action, "DROP");
				add.swt = 0;
			}
			
			//send an add packet to answer ask
			string writing = "/tmp/fifo-0-" + to_string(incomingPkt.swt);
			
			fd[incomingPkt.swt-1][WRITE] = open(&writing[0], O_WRONLY | O_NONBLOCK);
			if (fd[incomingPkt.swt-1][WRITE] == -1){
				printf("Open write add FIFO error: %s\n", strerror(errno));
				return;
			}
			
			struct pollfd pfdSending;
			pfdSending.fd = fd[incomingPkt.swt-1][WRITE];
			pfdSending.events = POLLOUT;
			
			int send = pollOut(pfdSending, add);
			if (send == 0) {
				close(fd[incomingPkt.swt-1][WRITE]);
				return;
			}
			close(pfdSending.fd);
			typeCount["ADD"]++;
			return;
		}
	}
	return;
}

/*
* This function will take in a string and split it based on the field delimiters provided. It will
* fill the outToken array with the seperated strings and return the total number of tokens created.
* 
* Arguments:
*	inStr- the string being parsed
*	token- the container for the parsed string
*	fieldDelim- the characters used to split the string
* Returns:
*	0 if no tokens are found or the number of tokens found
*/
int split(string inStr, char token[][MAX_LEN], char fieldDelim[]){
	int i, count;
	char *tokenp;
	
	count = 0;
	
	for (i = 0; i < MAX_LEN; i++)
		memset(token[i], 0 , sizeof(token[i]));
	
	string inStrCpy = inStr; //create a copy of the string passed to the function
	if((tokenp = strtok(&inStr[0], fieldDelim)) == NULL){
		return 0; //return 0 if no token is found
	}
	//store first token if found in if statement above
	strcpy(token[count], tokenp);
	count++;
	
	// This loop captures each token in the string and stores them in token. 
	while((tokenp = strtok(NULL, fieldDelim))!= NULL) {
		strcpy(token[count], tokenp);
		count++;
	}
	
	inStr = inStrCpy;
	return count;
}

/*
* The user1_handle function is a signal handler which will display the information of the current 
* switch.
*
* Arguments: 
*	sigint- The signal recieved as an integer
*/
void user1_handler(int sigint){
	if(attachedSwitches.empty()){
		infoSwitch(forwardTable, typeCount);
	}
	else infoMaster(attachedSwitches, typeCount);
	return;
}

/*
* The alarm_handler function is a signal handler which will set the alarm flag to false.
*
* Arguments:
*	signum- The signal recieved as an integer
*/
void alarm_handler(int signum){
	alarmOn = false;
	return;
}

/*
* The processLine function will figure out what to do with the line read from the file. If it is a 
* header packet, it will call search table. If it is a delay, it will delay reading the file 
* until a SIGALRM signal is recieved.
*
* Arguments:
*	pipeNo- The switch number we are processing packets for as an int
*	fd- The file descriptors of the FIFO's connected to the switch as a 2-d array of integers
*	typeCount- The types of packets and how often the switch has processed one of them as 
*			   a map with strings (types) as the keys and integers as the values.
*	table- The forwarding table containing all rules as a vector of forwardingRow structs
*	switches- The switches that have been activated as a vector of forwardingRow structs
*	line- The line of the file to be processed as a C-string
*/
void processLine(int pipeNo, char *line, vector<forwardingRow> &table, 
	int fd[][2], map<string,int> &typeCount){
	char token [MAX_LEN][MAX_LEN];
	char delim [1] = {' '};
		
	int tokenNum = split((string)line, token, delim);
	
	if ((string)token[1] != "delay"){
		//if the source IP isn't in the range return without processing the packet
		if (atoi(&(token[1])[0]) < table[0].srcIP_lo || 
			atoi(&(token[1])[0]) > table[0].srcIP_hi) return;
		//check forward table for matching rule
		searchTable(atoi(&(token[2])[0]), pipeNo, fd, typeCount, table);
	}
	else{
		struct itimerval timer;
		timer.it_value.tv_sec= atoi(&(token[2][0])) / 1000;
		
		// Catch alarm signal
		if (signal(SIGALRM, alarm_handler) == SIG_ERR){
			printf("SIGALRM error");
			return;
		}
		
		//Set alarm flag and delay reading the file further
		alarmOn = true;
		if (setitimer(ITIMER_REAL,&timer, NULL) == -1){
			printf("Error calling setitimer: %s\n", strerror(errno));
			return;
		}
	}
	return;
}

/*
* This function starts the program and runs the main loop in which the user enters commands. 
* It will decide which function to call based on the command entered. 
*
* Arguments: 
*	argc- The number of arguments provided as an integer
*	argv- The parsed arguments as an array of C-strings
* Returns:
*	0 once the user uses the exit command.
*/
int main(int argc, char *argv[]){
	string arg1(argv[1]);
	
	//using a map to keep counts of types of packets recieved to send to other functions easily
	typeCount["HELLO"] = 0;
	typeCount["HELLO_ACK"] = 0;
	typeCount["ASK"] = 0;
	typeCount["ADD"] = 0;
	typeCount["ADMIT"] = 0;
	typeCount["RELAYIN"] = 0;
	typeCount["RELAYOUT"] = 0;
	
	//check to see which arguments were provided
	if (arg1 == "master"){
		if (argc != 3) {
			cout<< "Invalid argument: Incorrect number of arguments provided\n";
			return 0;
		}
		
		string arg2(argv[2]);
		int nsw;
		nsw = stoi(arg2);
		//size the vector to match the argument provided
		attachedSwitches.resize(nsw);
		
		if (nsw > MAX_NSW || nsw < 1){
			cout<< "Invalid argument: Number of switches can range from 1-7\n";
			return 0;
		}
		
		int fd[nsw][2];
		char command[MAX_LEN];
		
		//get fd's of FIFO's for reading
		for (int i = 1; i < nsw+1; i++){
			string reading = "/tmp/fifo-" + to_string(i) + "-0";
			
			fd[i-1][READ] = open(&reading[0], O_RDONLY | O_NONBLOCK);
			if (fd[i-1][READ] == -1){
				printf("Open read FIFO error: %s\n", strerror(errno));
				return 0;
			}
		}
		
		struct pollfd pfd;
		pfd.fd = STDIN_FILENO;
		pfd.events = POLLIN;
		
		for (;;){
			//check if the prog has been sent a USER1 signal
			if (signal(SIGUSR1, user1_handler) == SIG_ERR){
				printf("Catch USR1 signal error");
				return 0;
			}
			
			//poll for input
			if (poll(&pfd, 1, 0) == -1){
				if (errno == EINTR) continue;
				printf("Poll keyboard error: %s\n", strerror(errno));
				return 0;
			}
			else if(pfd.revents & POLLIN){
				size_t readChar = read(pfd.fd, command, sizeof(command)-1);
				if(readChar == -1){
					printf("Read Error: %s\n", strerror(errno));
					return 0;
				}
				if (readChar >0) {
					command[readChar] = '\0';
					//if exit is entered, end the program
					if (strcmp(command, "exit\n") == 0){
						for (int i = 1; i < nsw+1; i++){
							close(fd[i-1][READ]);
						}
						break;
					}
					//if info is entered, print information for the master switch
					else if (strcmp(command, "info\n") == 0){
						infoMaster(attachedSwitches, typeCount);
					}	
				}
			}
			
			//poll FIFO's
			for (int i = 1; i < nsw+1; i++){
				struct pkt incomingPkt;
				struct pollfd pfdIncoming; //using poll to make sure we aren't blocking
				
				pfdIncoming.fd = fd[i-1][READ];
				pfdIncoming.events = POLLIN;
				if(poll(&pfdIncoming, 1, 0) == -1){
					//If the signal for USR1 are interrupting poll, ignore the error
					if (errno == EINTR) continue;
					printf("Poll incoming FIFO error: %s\n", strerror(errno));
					return 0;
				}
				//
				else if(pfdIncoming.revents & POLLIN){
					//If something has been written to master, read and call processPacket
					if (read(pfdIncoming.fd, &incomingPkt, sizeof(pkt)) == -1){
						printf("Read error: %s\n", strerror(errno));
						return 0;
					}
					processPacket(incomingPkt, fd, typeCount, attachedSwitches, forwardTable);
				}
			}

		}
		
	}
	
	if (arg1.substr(0, 3) == "psw"){
		if (argc != 6) {
			cout<< "Invalid argument: Incorrect number of arguments provided\n";
			return 0;
		}
		
		//Don't actually need to open port 3 because we aren't sending anything there so 3 rows
		int fd[3][2];
		
		struct pkt startpkt;
		int start = startPacket(startpkt, argc, argv);
		if (start == 0) return 0;
		
		//Open master FIFO for reading
		string openNeighbour = "/tmp/fifo-0-" + arg1.substr(3, 1);
		fd[0][READ] = open(&openNeighbour[0], O_RDONLY | O_NONBLOCK);
		if (fd[0][READ] == -1){
			printf("Open master read error: %s\n", strerror(errno));
			return 0;
		}
		
		//Open neighbours for reading
		string port1(argv[3]);
		string port2(argv[4]);
		
		if (port1 != "null"){
			openNeighbour = "/tmp/fifo-"+ port1.substr(3, 1)+ "-" + arg1.substr(3, 1);
			fd[1][READ] = open(&openNeighbour[0], O_RDONLY | O_NONBLOCK);
			if (fd[1][READ] == -1){
				printf("Open port1 read error: %s\n", strerror(errno));
				return 0;
			}
		}
		if (port2 != "null"){
			openNeighbour = "/tmp/fifo-"+ port2.substr(3, 1)+ "-" + arg1.substr(3, 1);
			fd[2][READ] = open(&openNeighbour[0], O_RDONLY | O_NONBLOCK);
			if (fd[2][READ] == -1){
				printf("Open port2 read error: %s\n", strerror(errno));
				return 0;
			}
		}
		
		struct pollfd pfdMasterRead;
		pfdMasterRead.fd = fd[0][READ];
		pfdMasterRead.events = POLLIN;
		
		//send hello packet to master
		string masterOut = "/tmp/fifo-"+ arg1.substr(3, 1) + "-0";
		fd[0][WRITE] = open(&masterOut[0], O_WRONLY); //Have to open first as O_WRONLY
		fcntl(fd[0][WRITE], F_SETFL, O_NONBLOCK); //Set flag for non-blocking
		if (fd[0][WRITE] == -1){
			printf("Open master write error: %s\n", strerror(errno));
			return 0;
		}
		
		struct pollfd pfdHello;
		pfdHello.fd = fd[0][WRITE];
		pfdHello.events = POLLOUT;
		pollOut(pfdHello, startpkt);
		close(fd[0][WRITE]);
		typeCount["HELLO"]++;
		
		string ipRange(argv[5]);
		size_t found = ipRange.find("-");
		int IPlow = atoi(&ipRange.substr(0, found+1)[0]);
		int IPhigh = atoi(&ipRange.substr(found+1)[0]);
		
		int startTable = addRow(forwardTable, 0, MAX_IP, IPlow, IPhigh, "FORWARD", 3, 0);
		if (startTable == 0) return 0; 
		
		//open file for reading
		FILE *file;
		if ((file = fopen(argv[2], "r")) == NULL){
			printf("FOPEN FAILED! %s\n", strerror(errno));
			return 0;
		}
		
		struct pollfd pfd;
		pfd.fd = STDIN_FILENO;
		pfd.events = POLLIN;
		
		char command[MAX_LEN];
		for (;;){
			//check if the prog has been sent a USER1 signal
			if (signal(SIGUSR1, user1_handler) == SIG_ERR){
				printf("Catch USR1 signal error");
				return 0;
			}
			
			//Get line in file
			if (!alarmOn){
				char unparsed[MAX_LEN], *result;
				if ((result = fgets(unparsed, MAX_LEN, file)) != NULL){
					if(result[0] != '#'){
						/*
						* If the header packet specifies the current switch as the source, admit
						* the packet and call processLine
						*/
						if (((string)result).substr(0,4) == arg1){
							typeCount["ADMIT"]++;
							processLine(atoi(&((string)result).substr(3,1)[0]), result, 
										forwardTable, fd, typeCount);
						}
					}
				}
			}
			
			//poll for input
			if (poll(&pfd, 1, 0) == -1){
				if (errno == EINTR) continue;
				printf("Poll keyboard error: %s\n", strerror(errno));
				return 0;
			}
			else if(pfd.revents & POLLIN){
				size_t readChar = read(pfd.fd, command, sizeof(command)-1);
				if(readChar == -1){
					printf("Read Error: %s\n", strerror(errno));
					return 0;
				}
				if (readChar >0) {
					command[readChar] = '\0';
					if (strcmp(command, "exit\n") == 0){
						fclose(file);
						break;
					}
					else if (strcmp(command, "info\n") == 0){
						infoSwitch(forwardTable, typeCount);
					}	
				}
			}
			
			//poll FIFO's
			for (int i = 0; i < 3; i++){
				struct pkt incomingPkt;
				struct pollfd pfdIncoming; //using poll to make sure we aren't blocking
				
				pfdIncoming.fd = fd[i][READ];
				pfdIncoming.events = POLLIN;
				if(poll(&pfdIncoming, 1, 0) == -1){
					// If the SIGALRM is active, ignore the error
					if (errno == EINTR) continue;
					printf("Poll incoming FIFO error: %s\n", strerror(errno));
					return 0;
				}
				else if(pfdIncoming.revents & POLLIN){
					if (read(pfdIncoming.fd, &incomingPkt, sizeof(pkt)) == -1){
						printf("Read error: %s\n", strerror(errno));
						return 0;
					}
					processPacket(incomingPkt, fd, typeCount, attachedSwitches, forwardTable);
				}
			}
		}
		
	}		
	return 0;
}
