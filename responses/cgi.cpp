#include "responses.hpp"
#include <string>

char **get_env(Client &client) {
  client.setEnv();
  std::map<std::string, std::string> env = client.env;
  char **keyValueArray = new char *[env.size() + 1];

  int i = 0;
  for (std::map<std::string, std::string>::const_iterator it = env.begin();
       it != env.end(); ++it) {
    std::string keyValue = it->first + "=" + it->second;
    keyValueArray[i] = new char[keyValue.length() + 1];
    std::strcpy(keyValueArray[i], keyValue.c_str());
    i++;
  }
  keyValueArray[env.size()] = NULL;
  return (keyValueArray);
}

void cgi(Client &client) {
  srand(std::time(NULL));
  std::ostringstream convert;
  convert << rand() << "cgi.tmp";
  std::string filename = convert.str();
  client.outfile->open(filename.c_str());
  client.outfile->close();
  if (!(client.pid = fork())) {
    char **env = get_env(client);
    freopen(filename.c_str(), "w", stdout);
   size_t pos = client.fullPath.find_last_of("/");
    if (pos != std::string::npos)
    {
      chdir(client.fullPath.substr(0, pos).c_str());
      client.fullPath = client.fullPath.substr(pos + 1);
    }
    char *argv[] = {
        const_cast<char *>(
            client.location.second.cgi.second[get_ex(client.fullPath)].c_str()),
        const_cast<char *>(client.fullPath.c_str()), NULL};
    execve(argv[0], argv, env);
    exit(100);
  } else {
    client.currentTime = time(NULL);
    client.is_cgi = 5;
    client.cgiFileName = filename;
    client.is->open(filename.c_str());
  }
}
