#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#define MAX_NSW 7

using namespace std;

int main(int argc, char *argv[]){
	int nsw;
	string arg1(argv[1]);
	
	//check to see which arguments were provided
	if (arg1 == "master"){
		if (argc != 3) {
			cout<< "Invalid argument: Incorrect number of arguments provided\n";
			return 0;
		}
		
		string arg2(argv[2]);
		nsw = stoi(arg2);
		
		if (nsw > MAX_NSW){
			cout<< "Invalid argument: Max number of switches is 7\n";
			return 0;
		}
	}
	
	
	
	return 0;
}