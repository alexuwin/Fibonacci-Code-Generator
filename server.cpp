#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <istream>
#include <ostream>
#include <vector>
#include <algorithm>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <semaphore.h>
#include <fcntl.h>

using namespace std;

struct data{

    char letter;
    int freq;
    bool used;
    int rank;
    string code;
};

struct arguments{
    pthread_mutex_t* bsem;
    pthread_cond_t* waitTurn;

    char letter;
    int freq;
    int rank;
    string* code;
};

//called by child thread to communicate with server and get fibbonacci code
void * getFibbo(void *data_void_ptr){
    arguments data_ptr = *((arguments *)data_void_ptr);
    pthread_cond_signal(data_ptr.waitTurn);
    vector<int> fib; //create vector storing fib sequence
    fib.push_back(1);
    fib.push_back(2);
    int index = 0;
    int num1 = 1;
    int num2 = 2;

    while(num2 <= data_ptr.rank){
        num1 = fib[index];
        fib.push_back(num1+num2);
        num2 = num1+num2;
        index++;
    }

    int current = data_ptr.rank;

    for(int k = fib.size()-2; k >= 0; k--){
        if(fib[k] == current){
            *(data_ptr.code) = "1" + *(data_ptr.code);
            current = current - fib[k];
        } else if (fib[k] > current){
            *(data_ptr.code) = "0" + *(data_ptr.code);
        } else if (fib[k] < current){
            *(data_ptr.code) = "1" + *(data_ptr.code);
            current = current - fib[k];
        }
    }

    cout << "Symbol: " << data_ptr.letter << ", ";
    cout << "Frequency: " << data_ptr.freq << ", ";
    cout << "Code: " << *(data_ptr.code) << endl;
    return NULL;
}

void decompressFile(vector<data> mydata, string fileName){
    ifstream compFile;
    compFile.open(fileName);
    
    int subsize = 1;
    string message = "";
    string content = "";
    string substring = "";
    compFile >> content;
    
    //while content has remaining codes.. continue deciphering/decompressing
    while(content.length() > 0){
        bool match = false;
        substring = content.substr(0,subsize);
        for(int i = 0; i < mydata.size(); i++){
            if(substring == string(mydata[i].code)){
                message += mydata[i].letter;    //add the letter/symbol to deciphered message
                content = content.substr(subsize); //remove the corresponding code from file content
                match = true;
                break;
            }
        }
        if(match){
            subsize = 1;
        } else {
            subsize++;
        }
    }
    cout << "Decompressed message = " << message;
}

int main(int argc, char *argv[]){
    int numSymbols;
    cin >> numSymbols;

    //vectors storing needed info
    vector<data> myData;
    vector<data> sortedData;

    //for synchronization in output
    pthread_mutex_t bsem;
    pthread_mutex_init(&bsem, NULL);
    pthread_cond_t waitTurn = PTHREAD_COND_INITIALIZER;

    //initialize/define contents of myData/sortedData
    //for each symbol in input/file... get the symbol and its frequency
    string line = "";
    getline(cin, line);
    for(int i = 0; i < numSymbols; i++){
        getline(cin, line);
        data newData;
        
        newData.letter = line[0];
        newData.freq = stoi(line.substr(2));
        newData.used = false;
        newData.rank = 0;
        newData.code = "1";

        myData.push_back(newData);
        sortedData.push_back(newData);
    }

    string compFileName;
    cin >> compFileName;

    //SORT DATA
    //sort the letters based on ascii order (least to greatest)
    for(int i = 0; i < myData.size(); i++){
        int j = 0;
        int least = 256; //no ascii char greater than 255
        int leastIndex = -1;
        for(j = 0; j < myData.size(); j++){
            if(!myData[j].used && (int)myData[j].letter < least){
                least = myData[j].letter;
                leastIndex = j;
            }
        }
        myData[leastIndex].used = true;
        sortedData[i] = myData[leastIndex];
    }
    //reset used for later
    for(int i = 0; i < sortedData.size(); i++){
        sortedData[i].used = false;
    }
    
    //array storing threads to use (1 for each letter in alphabet)
    pthread_t threads[sortedData.size()];

    //assign rank of each letter based on its freq
    for(int i = 1; i <= sortedData.size(); i++){
        int biggest = 0;
        int biggestIndex = -1;
        int j = 0;
        for(j = 0; j < sortedData.size(); j++){
            if(!sortedData[j].used && biggest < sortedData[j].freq){
                biggest = sortedData[j].freq;
                biggestIndex = j;
            } else if(!sortedData[j].used && biggest == sortedData[j].freq){
                if(sortedData[biggestIndex].letter < sortedData[j].letter){
                    biggest = sortedData[j].freq;
                    biggestIndex = j;
                }
            }
        }
        sortedData[biggestIndex].used = true;
        sortedData[biggestIndex].rank = i;
    }
    //copy ranks to corresponding letters from sorted to mydata
    for(int i = 0; i < myData.size(); i++){
        for(int j = 0; j < sortedData.size(); j++){
            if(myData[i].letter == sortedData[j].letter){
                myData[i].rank = sortedData[j].rank;
                break;
            }
        }
    }

    arguments args;
    args.bsem = &bsem;
    args.waitTurn = &waitTurn;

    for(int i = 0; i < myData.size(); i++){
        /*create a thread for each symbol in address &threads[i] location
        located NULL (anywhere in stack)
        upon creation, execute the getFibbo function with args as an argument which contains myData[i] contents*/

        args.letter = myData[i].letter;
        args.freq = myData[i].freq;
        args.rank = myData[i].rank;
        args.code = &(myData[i].code);
        if (pthread_create(&threads[i], NULL, getFibbo, &args))
		{
			cout << "Error creating thread" << endl;
			return 1;
		}
        pthread_cond_wait(&waitTurn, &bsem);
    }
    
    //terminate threads when getFibbo is completed
    for (int i = 0; i < myData.size(); i++){
		pthread_join(threads[i], NULL);
    }
    decompressFile(myData, compFileName);
}

/*
EXAMPLE

"COSC 3360"

 's code: 01011
0's code: 10011
3's code: 011
6's code: 00011
C's code: 11
O's code: 1011
S's code: 0011

Fibonacci Code: 111011001111010110110110001110011
*/