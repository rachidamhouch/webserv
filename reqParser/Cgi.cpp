#include "Cgi.hpp"
#include <unistd.h>

Cgi::Cgi(Client &req) : request(req) {}

Cgi::~Cgi() {
  for (int i = 0; env[i] != NULL; i++)
    delete[] env[i];
  delete[] env;
}

char *Cgi::strDup(std::string src) {
  size_t size = src.size();
  char *newStr;

  newStr = new char[size + 1];
  std::strcpy(newStr, src.c_str());
  newStr[size] = 0;
  return newStr;
}

void Cgi::setEnv() {
  int i = 0;

  this->env = new char *[request.env.size() + 1];
  for (std::map<std::string, std::string>::iterator it = request.env.begin();
       it != request.env.end(); it++) {
    this->env[i] = strDup(it->first + "=" + it->second);
    i++;
  }
  this->env[i] = NULL;
}

void Cgi::executeCgi() {
  int pid;
  char *args[3];
  std::stringstream ss;

  setEnv();
  ss << rand() << "_cgi_out.tmp";
  args[0] = new char[request.cgiScript.size() + 1];
  args[1] = new char[request.fullPath.size() + 1];
  args[2] = NULL;
  request.outfile->close();
  request.cgiInFileName = request.cgiFileName;
  request.cgiFileName = ss.str();
  request.outfile->open(request.cgiFileName.c_str());
  request.outfile->close();
  pid = fork();
  if (!pid) {
    freopen(request.cgiInFileName.c_str(), "r", stdin);
    freopen(request.cgiFileName.c_str(), "w", stdout);
    size_t pos = request.fullPath.find_last_of("/");
    if (pos != std::string::npos) {
      chdir(request.fullPath.substr(0, pos).c_str());
      request.fullPath = request.fullPath.substr(pos + 1);
    }
    bzero(args[0], request.cgiScript.size() + 1);
    bzero(args[1], request.fullPath.size() + 1);
    strcpy(args[0], request.cgiScript.c_str());
    strcpy(args[1], request.fullPath.c_str());
    // std::cerr << args[0] << ", " << args[1] << std::endl;
    execve(args[0], args, env);
    exit(100);
  } else {
    request.pid = pid;
    request.is_cgi = 5;
    request.currentTime = std::time(NULL);
    request.is->open(request.cgiFileName.c_str());
  }
}
