#include <iostream>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <regex>
#include <map>
#include "unistd.h"
#include "arpa/inet.h"
#include "netinet/in.h"
#include "sys/wait.h"
#include "sys/socket.h"
#include "sys/select.h"
#include "sys/stat.h"
#include "sys/types.h"
#include "dirent.h"

#define BUF_SIZE 5096

using namespace std;

FILE* ClientFP;
int ClientFD;
unsigned short Port_Binded;
map<string,string> MappingMIME;

struct {
  sockaddr_in addr;
  string uri;
  string verb;
  string path; 
  string query; 
  string req_line;
  string body;
  map<string,string> headerInfo;  
} http_info;

void MIME_Initial();
int Socket_Initial();
int ClientHandler();
void RequestHandler();
void StaticFileHandler(const string&, struct stat&);
void StaticNormalFileHandler(const string&, struct stat&, const string&);
void StaticExecutableFileHandler(const string&);
void DirectoryHandler(const string&);
void DirectoryHandlerWithRedirect(const string&);
void DirectoryHandlerWithIndex(const string&);
void DirectoryHandlerWithoutIndex(const string&);
void ServerErrorHandler();
int Forkdetacher();
void http_info_Printer(FILE*);
void CommonHeaderMsger(int code);
string StringStriper(const string&);
string GetMimeTypeByExtention(const string& extension);
string GetExtension(const string& filename);
bool BoolExtensionExecutable(const string& ext);
bool BoolFileExecutable(const string& ext);
bool CmdExec(const string& filename,const string& arg_list);
char EnvStrFormTransfer(char);


void MIME_Initial(){
  MappingMIME.insert(make_pair("jpg","image/jpeg"));
  MappingMIME.insert(make_pair("gif","image/gif"));
  MappingMIME.insert(make_pair("png","image/png"));
  MappingMIME.insert(make_pair("txt","text/plain"));
  MappingMIME.insert(make_pair("htm","text/html"));
  MappingMIME.insert(make_pair("html","text/html"));
  MappingMIME.insert(make_pair("bmp","image/x-ms-bmp"));
  MappingMIME.insert(make_pair("pdf","application/pdf"));
  MappingMIME.insert(make_pair("doc","application/msword"));
  MappingMIME.insert(make_pair("swf","application/x-shockwave-flash"));
  MappingMIME.insert(make_pair("swfl","application/x-shockwave-flash"));
  MappingMIME.insert(make_pair("bz2","application/x-bzip2"));
  MappingMIME.insert(make_pair("gz","application/x-gzip"));
  MappingMIME.insert(make_pair("mp4","video/mp4"));
  MappingMIME.insert(make_pair("ogg","audio/ogg"));
}


int Socket_Initial(){
    sockaddr_in addr_self;
    int val, SelfFd;

    addr_self.sin_port = htons(Port_Binded);
    addr_self.sin_family = PF_INET;
    addr_self.sin_addr.s_addr = INADDR_ANY;

    SelfFd = socket(AF_INET,SOCK_STREAM,0);
    if(setsockopt(SelfFd ,SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0){
      perror("setsockopt failed");
      exit(-1);
    }
    if(SelfFd < 0){
        cout << "create socket failed." << endl;
        exit(1);
    }
    if(bind(SelfFd,(sockaddr*)&addr_self,sizeof(addr_self))<0){
        cout << "bind failed." << endl;
        exit(1);
    }
    if(listen(SelfFd,10)<0){
        cout << "listen failed" << endl;
        exit(1);
    }

    return SelfFd;
}


int ClientHandler(){
  char buf[BUF_SIZE];
  smatch match;

  // get http request
  fgets(buf,BUF_SIZE,ClientFP);
  http_info.req_line = string(buf);

  // request line parsing
  if(regex_search(http_info.req_line,match,regex("(\\S+) (\\S+)"))){
    http_info.verb = match[1];
    http_info.uri = match[2];
    if(regex_search(http_info.uri,match,regex("\\?"))){
      http_info.path = match.prefix();
      http_info.query = match.suffix();
    }
    else{
      http_info.path = http_info.uri;
    }
  }
  // get other http header
  while(fgets(buf,BUF_SIZE,ClientFP)){
    string line(buf);
    if(line == "\r\n"){
      break;
    }
    else if(regex_search(line, match, regex("(\\S+): (.*)\r\n"))){
      http_info.headerInfo.insert(make_pair(match[1],match[2]));
    }
  }
  http_info_Printer(stdout);
  RequestHandler();
  return 0;
}


void RequestHandler(){
  // check if we can access that file
  string file_path = string(".") + http_info.path;
  if(access(file_path.c_str(),R_OK) == 0){
    // check directory or not
    struct stat stat_result;
    if(stat(file_path.c_str(),&stat_result) == 0){
      if (S_ISDIR(stat_result.st_mode)){
        DirectoryHandlerWithRedirect(file_path);
      }
      else{
        StaticFileHandler(file_path,stat_result);
      }
    }
    else{
      CommonHeaderMsger(500);
      fprintf(ClientFP,"Content-Type: text/plain\r\n");
      fprintf(ClientFP,"\r\n");
      fprintf(ClientFP,"Error code = %d\r\n",errno);
      perror("stat");
    }
  }
  else{
    if(errno == EACCES ){
      // permission
      CommonHeaderMsger(404);
      fprintf(ClientFP,"Content-Type: text/html\n\n<h1>404 Not Found</h1>\n");
      fprintf(ClientFP,"\r\n");
      fprintf(ClientFP,"Access denied for %s\r\n",file_path.c_str());
    }
    else if(errno == ENOTDIR || errno == ENOENT){
      // not found
      CommonHeaderMsger(403);
      fprintf(ClientFP,"Content-Type: text/html\n\n<h1>403 Forbidden</h1>\n");
      fprintf(ClientFP,"\r\n");
      fprintf(ClientFP,"Not found for %s\r\n",file_path.c_str());
    }
    else{
      // other error
      ServerErrorHandler();
      perror("stat");
    }
  }
}


void StaticNormalFileHandler(const string& file_path, struct stat& statbuf, const string& extension){
  
  CommonHeaderMsger(200);
  fprintf(ClientFP,"Content-Type: %s\r\n",GetMimeTypeByExtention(extension).c_str());
  fprintf(ClientFP,"Content-Length: %d\r\n",statbuf.st_size);
  fprintf(ClientFP,"\r\n");
  // read bytes and send to client
  char buf[BUF_SIZE];
  int remain = statbuf.st_size;
  ifstream fin(file_path, ifstream::binary); 
  while(remain > 0){
    int n = fin.readsome(buf,min(remain,BUF_SIZE));
    fwrite(buf,1,n,ClientFP);
    remain -= n;
  }
}
void StaticExecutableFileHandler(const string& file_path){
  int read_fd[2], write_fd[2];
  pipe(read_fd);
  pipe(write_fd);
  int pid = fork();
  if(pid < 0){
    perror("fork in CGI");
    ServerErrorHandler();
  }
  else if(pid == 0){ 
    // child
    // set up pipe for stdin and stdout
    // use read_fd to write data, write_fd to read data
    dup2(write_fd[0],STDIN_FILENO);
    dup2(read_fd[1],STDOUT_FILENO);
    close(write_fd[0]);
    close(read_fd[0]);    
    close(write_fd[1]);
    close(read_fd[1]);   
    close(ClientFD);

    // setup env
    clearenv();
    setenv("REQUEST_METHOD",http_info.verb.c_str(),1);
    setenv("REQUEST_URI",http_info.uri.c_str(),1);
    setenv("CONTENT_LENGTH",http_info.headerInfo["Content-Length"].c_str(),1);
    setenv("CONTENT_TYPE",http_info.headerInfo["Content-Type"].c_str(),1);
    setenv("SCRIPT_NAME",http_info.path.c_str(),1);
    setenv("QUERY_STRING",http_info.query.c_str(),1);
    setenv("GATEWAY_INTERFACE","CGI/1.1",1);
    // address
    char addr_buf[20] = {0};
    inet_ntop(AF_INET,&http_info.addr.sin_addr,addr_buf,sizeof(addr_buf));
    setenv("REMOTE_ADDR",addr_buf,1);
    // port
    char port_buf[6] = {0};
    snprintf(port_buf,5,"%u",ntohs(http_info.addr.sin_port));
    setenv("REMOTE_PORT",port_buf,1);
    setenv("PATH","/bin:/usr/bin:/usr/local/bin",1);

    // other env variable
    for(auto& data :  http_info.headerInfo){
      string key = data.first; 
      string value = data.second; 
      if(key != "Content-Length" && key != "Content-Type"){
        transform(key.begin(), key.end(), key.begin(), EnvStrFormTransfer);
        string new_key = string("HTTP_") + key;
        setenv(new_key.c_str(),value.c_str(),1);
      }
    }
    // exec CGI
    if(!CmdExec(file_path,"")){
      fprintf(stdout,"Exec CGI Failed: %s\r\n",file_path.c_str());
      exit(-1);
    } 
  }
  else{
    // parent
    int size_left = atoi(http_info.headerInfo["Content-Length"].c_str());
    close(read_fd[1]);
    close(write_fd[0]);
    // send header
    CommonHeaderMsger(200);
    // start forwarding data
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(read_fd[0],&rset);
    close(write_fd[1]);
  
    int max_fd = max(ClientFD, read_fd[0]);
    char buf[BUF_SIZE];
    while(true){
        fd_set rs = rset;
        int nready = select(max_fd+1,&rs,NULL,NULL,NULL);
        if(nready < 0){
          perror("select failed");
          ServerErrorHandler();
          break;
        }
        else{
          if(FD_ISSET(read_fd[0],&rs)){
            // from cgi
            bzero(buf,BUF_SIZE);
            int n = read(read_fd[0], buf, BUF_SIZE);
            if(n < 0){
              perror("From CGI Falied");
              break;
            }
            else if(n == 0){
              cout << "CGI terminated" << endl;
              break;
            }
            else{
              cout << "CGI:" << buf <<  endl;
              write(ClientFD,buf,n);
            }
          }
          else if(FD_ISSET(ClientFD, &rs)){
            // from socket
            bzero(buf,BUF_SIZE);
            int n = read(ClientFD, buf, min(size_left,BUF_SIZE));
            if(n < 0){
              perror("From Socket Falied");
              break;
            }
            else if(n == 0){
              cout << "Socket closed" << endl;
              FD_CLR(ClientFD,&rset);
              close(write_fd[1]);
            }
            else{
              cout << "Socket:" << buf <<  endl;
              write(write_fd[1],buf,n);
              size_left -= n;
              if(size_left == 0){
                cout << "content size too large" << endl;
                FD_CLR(ClientFD,&rset);
                close(write_fd[1]);
              }
            }
          }
        }
    }
    waitpid(pid,NULL,0);
  }
}


void StaticFileHandler(const string& file_path, struct stat& statbuf){
  // get extension
  string extension = GetExtension(file_path);  
  if(BoolExtensionExecutable(extension) && BoolFileExecutable(file_path) ){
    //check cgi validation
    StaticExecutableFileHandler(file_path);
  }
  else{
    StaticNormalFileHandler(file_path, statbuf, extension);
  }
}


void DirectoryHandlerWithRedirect(const string& file_path){
  smatch match;
  if(regex_search(file_path, match, regex("/$"))){
    DirectoryHandler(file_path);
  }
  else{
    // no slash
    cout << "Redirect from :" << file_path << endl;
    CommonHeaderMsger(301);
    fprintf(ClientFP,"Location: http://%s%s/\r\n",http_info.headerInfo["Host"].c_str(),http_info.uri.c_str());
    fprintf(ClientFP,"\r\n");
  }
}
void DirectoryHandler(const string& file_path){
  cout << "Open directory for :" << file_path << endl; // scan for index.html
  DIR* dir = opendir(file_path.c_str());
  struct dirent entry;
  struct dirent* result;
  bool hasIndex = false;
  while(true){
    if( readdir_r(dir, &entry, &result ) == 0 ){
      if(result == nullptr){
        break;
      }
      if(strcmp(entry.d_name,"index.html")==0 ){
        hasIndex = true;
        break;
      }
    }
    else{
      perror("readdir_r");
      break;
    }
  }
  if(hasIndex){
    DirectoryHandlerWithIndex(file_path);
  }
  else{
    DirectoryHandlerWithoutIndex(file_path);
  }
}
void DirectoryHandlerWithIndex(const string& dir_path){
  string file_path = dir_path + "index.html";
  struct stat stat_result;
  if(stat(file_path.c_str(),&stat_result) == 0){
    // check index.html is accessible 
    if(access(file_path.c_str(),R_OK) == 0){
      StaticFileHandler(file_path,stat_result);
    }
    else{
      CommonHeaderMsger(403);
      fprintf(ClientFP,"Content-Type: text/html\n\n<h1>403 Forbidden</h1>\n");
      fprintf(ClientFP,"\r\n");
      fprintf(ClientFP,"default index not readable for %s\r\n",file_path.c_str());
    }
  }
  else{
    CommonHeaderMsger(500);
    fprintf(ClientFP,"Content-Type: text/plain\r\n");
    fprintf(ClientFP,"\r\n");
    fprintf(ClientFP,"Error code = %d\r\n",errno);
    perror("stat");
  }
  
}
void DirectoryHandlerWithoutIndex(const string& file_path){
  CommonHeaderMsger(200);
  fprintf(ClientFP,"Content-Type: text/html\r\n");
  fprintf(ClientFP,"\r\n");
  DIR* dir = opendir(file_path.c_str());
  struct dirent entry;
  struct dirent* result;
  fprintf(ClientFP,"<h1>%s:</h1>\r\n",http_info.path.c_str());
  while(true){
    if( readdir_r(dir, &entry, &result ) == 0 ){
      if(result == nullptr){
        break;
      }
      fprintf(ClientFP,"<li><a href=%s%s>%s</a></li><br />\r\n",http_info.path.c_str(),entry.d_name,entry.d_name);
    }
    else{
      perror("readdir_r");
      break;
    }
  }
  closedir(dir);
}

int Forkdetacher(){
  int pid1, pid2;
  if((pid1 = fork()) < 0){
    perror("fork 1 failed");
    exit(-1);
  }
  else if(pid1 == 0){
    // child 1
    if((pid2 = fork())<0){
      perror("fork 2 failed");
      exit(-1);
    }
    else if(pid2 == 0){
      // child 2 return as child
      return pid2;
    }
    else{
      // parent 2 exit immediately (not needed)
      exit(0);
    }
  }
  else{
    // parent 1 return as parent
    waitpid(pid1,NULL,0);
    return pid1;
  }
}

void http_info_Printer(FILE* fp){
  fprintf(fp,"HTTP/1.1 200 OK\r\n");
  fprintf(fp,"Connection: close\r\n");
  fprintf(fp,"Content-Type: text/plain\r\n");
  fprintf(fp,"\r\n");
  fprintf(fp,"Http verb: %s\r\n", http_info.verb.c_str());
  fprintf(fp,"Http path: %s\r\n", http_info.path.c_str());
  fprintf(fp,"Http query: %s\r\n", http_info.query.c_str());
  fprintf(fp,"Other Headers:\r\n");
  for(const auto& pair : http_info.headerInfo){
    fprintf(fp,"%s: %s\r\n", pair.first.c_str(), pair.second.c_str());
  }
  fflush(fp);
}

void CommonHeaderMsger(int report){
  switch(report){
    case 404:
      fprintf(ClientFP,"HTTP/1.1 404 Not Found\r\n");
      break;
    case 403:
      fprintf(ClientFP,"HTTP/1.1 403 Forbidden\r\n");
      break;
    case 301:
      fprintf(ClientFP,"HTTP/1.1 301 Moved Permanently\r\n");
      break;  
    case 200:
      fprintf(ClientFP,"HTTP/1.1 200 OK\r\n");
      break;     
  }
  fprintf(ClientFP,"Connection: close\r\n");
}

string StringStriper(const string& original){
  if(original.empty()){
    return original;
  }
  else{
    string::size_type front = original.find_first_not_of(" \t\n\r");
    if(front==string::npos){
      return "";
    }
    string::size_type end = original.find_last_not_of(" \t\n\r");
    return original.substr(front,end - front + 1);
  }
}

string GetExtension(const string& filename){
  smatch match;
  if(regex_search(filename,match,regex("\\.(\\w+)$"))){
    return match[1];
  }
  else{
    return "";
  }
}

string GetMimeTypeByExtention(const string& extension){
  string ext = extension;
  transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  cout << "Search MIME Extension:" << ext << endl;
  auto it = MappingMIME.find(ext);
  if(it != MappingMIME.end()){
    cout << " MIME Found:" << it->second << endl;
    return it->second;
  }
  else{
    cout << " MIME not found" << endl;
    return "text/plain";
  }
}
bool BoolExtensionExecutable(const string& ext){
  return ext == "sh" || ext == "php" || ext == "cgi"; 
}


bool BoolFileExecutable(const string& filename){
  do{
    if(access(filename.c_str(),R_OK | X_OK) != 0){
      break;
    }
    return true;
  }while(false);

  return false;
}

void ServerErrorHandler(){
  CommonHeaderMsger(500);
  fprintf(ClientFP,"Content-Type: text/plain\r\n");
  fprintf(ClientFP,"\r\n");
  fprintf(ClientFP,"Internal Server Error, Error code = %d\r\n",errno);
}

char** convert_string_to_argv(const string& arg_list){ 
  stringstream ss(arg_list);
  string arg;
  vector<string> arg_array;
  while(ss >> arg){
    arg_array.push_back(arg);
  }
  char** argv = new char*[arg_array.size()+1];
  for(int i=0;i<arg_array.size();i++){
    string& arg = arg_array[i];
    argv[i] = new char[arg.length()+1];
    strncpy(argv[i],arg.c_str(),arg.length()+1);
  }
  argv[arg_array.size()] = nullptr;
  return argv;
}

void delete_argv(char** argv){
  char **ptr = argv;
  while(*ptr){
    delete[] *ptr;
    ++ptr;
  }
  delete[] argv;
    
}

bool CmdExec(const string& filename,const string& arg_list){
  char** argv = convert_string_to_argv(filename + " " + arg_list);
  if(execvp(filename.c_str(),argv)){
    delete_argv(argv);
    return false;
  }
  return true;
}
char EnvStrFormTransfer(char normal_char){
  normal_char = toupper(normal_char);
  if(normal_char == '-'){
    normal_char = '_';
  }
  return normal_char;
}
