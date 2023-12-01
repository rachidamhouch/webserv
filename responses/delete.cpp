#include "responses.hpp"
#include <dirent.h>

int delete_dir(std::string name, int *n) {
  DIR *dir;
  int isdir;
  std::string path;

  dir = opendir(name.c_str());
  if (dir) {
    struct dirent *entry;
    while ((entry = readdir(dir))) {
      if (!std::string(entry->d_name).compare("..") ||
          !std::string(entry->d_name).compare("."))
        continue;
      path = name + "/" + std::string(entry->d_name);
      isdir = is_dir(path);
      if (isdir == 1) {
        delete_dir(path, n);
      } else if (isdir == 0) {
        if (remove(path.c_str()))
          *n = 0;
      }
      if (!*n)
        break;
    }
    closedir(dir);
  } else
    *n = 0;
  if (*n && remove(name.c_str()))
    *n = 0;
  return 0;
}

void ft_delete(Client &client) {
  int n = 1;
  client.statusCode = 204;

  if (is_dir(client.fullPath) == 0) {
    if (remove(client.fullPath.c_str()))
      n = 0;
  } else if (is_dir(client.fullPath) == 1)
  {
    delete_dir(client.fullPath, &n);
  }
  else {
    client.statusCode = 404;
    return;
  }
  if (!n)
    client.statusCode = 403;
}
