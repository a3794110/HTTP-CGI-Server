#include "simple-server.h"

using namespace std;



int main(int argc,char** argv){
  int val=1, SelfFd;
  socklen_t socklen;
  
  // Show Help Information
  if(argc < 3){
    cout << "./webserver <portNum> \"/path/to/your/webserver/docroot\"" << endl;
    exit(-1);
  }

  //Initialize
  Port_Binded = atoi(argv[1]);
  chdir(argv[2]);
  MIME_Initial();
  SelfFd = Socket_Initial();


  while(1){
    socklen = sizeof(http_info.addr);
    bzero(&http_info.addr,sizeof(http_info.addr));   
    if((ClientFD = accept(SelfFd, (sockaddr*)&http_info.addr, &socklen)) < 0 ){
      perror("Accept Failure");
      exit(-1);
    }
    int pid = Forkdetacher(); 
    if(pid > 0){ //parent process
      close(ClientFD);
    }
    else{ //child process
      close(SelfFd);
      ClientFP = fdopen(ClientFD,"r+");
      setvbuf(ClientFP, NULL, _IONBF, 0);
      ClientHandler();
      exit(0);
    }
  }
}


