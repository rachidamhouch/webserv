#include "Cgi.hpp"
#include "reqParse.hpp"
#include <algorithm>
#include <string>

void	checkValidCharacters(const std::string & uri) {
    std::string validCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~:/?# []@!$&'()*+,;=%");

    for (size_t i = 0; i < uri.size(); i++) {
        if (validCharacters.find(uri[i]) == std::string::npos)
            throw 400;
    }
}
void    checkPath(std::string & path) {
    std::string part;

    for (size_t i = 0; i < path.size(); i++) {
        if (path[i] != '/')
            part += path[i];
        if (path[i] == '/' || i == path.size() - 1) {
            if (part == "..")
                throw 400;
            part.clear();
        }
    }
}

void	parseUri( Client & request, std::string & path, std::string & query ) {
	size_t		pos;

  pos = request.target.find('?');
	if (pos != std::string::npos) {
		path = request.target.substr(0, pos);
		query = request.target.substr(pos + 1);
		Tools::decodeUri(path);
	}
	else
		path = request.target;
  checkPath(path);
}

void	targetChecker( Client & request ) {
	std::string	path, query;
	bool		r(false), w(false);

	parseUri(request, path, query);
	request.path = path;
	request.query = query;
	request.fullPath = request.location.second.root + path.substr(request.location.first.size());
    if (!Tools::pathExists(request.fullPath.c_str(), request.isDir, r, w))
        throw 404;
	else if (!r)
		throw 403;
	if (request.method == "POST" && request.isDir && request.location.second.index.size()) {
		std::string	newPath;
		bool		isDir;

		for (size_t	i = 0; i < request.location.second.index.size(); i++) {
			isDir = false;
			r = false;
			w = false;
			newPath = request.fullPath + request.location.second.index[0];
			if (Tools::pathExists(newPath.c_str(), isDir, r, w) && r) {
				request.fullPath = newPath;
				request.isDir = isDir;
				break;
			}
		}
	}
}

void    skipSpace(Client & request) {
    if (request.buf[request.position] != ' ') {
        if (!request.crlf.empty()) {
            request.pState = request.pNext;
            request.crlf.clear();
        }
        else if (request.pNext == HEADER)
            request.pState = CRLF;
        else
            request.pState = request.pNext;
    }
    else
        request.position++;
}

bool    isEnd(Client & request) {     
    if (request.crlf == "\r\n\r\n" || request.crlf == "\n\n" || request.crlf == "\r\n\n" || request.crlf == "\n\r\n")
        return true;
    return false;
}

void    checkCrlf(Client & request) {
    if (request.buf[request.position] == '\r' || request.buf[request.position] == '\n')
        request.crlf += request.buf[request.position++];
    if (!isEnd(request) && request.position == request.buffSize)
        return;
    if (request.crlf.size() > 4)
        throw 400;
    if (request.buf[request.position] != '\r' && request.buf[request.position] != '\n'){
        if (request.crlf == "\r\n" || request.crlf == "\n")
            request.pState = SPACE;
        else if (request.crlf == "\r\n\r\n" || request.crlf == "\n\n" || request.crlf == "\r\n\n" || request.crlf == "\n\r\n")
            request.pState = CHECK_ERROR;
        else
            throw 400;
    }
}

void    getMethod(Client & request) {
    if (!std::isalpha(request.buf[request.position]) && request.buf[request.position] != ' ')
        throw 400;
    if (request.buf[request.position] == ' ') {
        if (request.method != "GET" && request.method != "POST" && request.method != "DELETE")
            throw 405;
        request.pState = SPACE;
        request.pNext = TARGET;
    }
    else
        request.method += request.buf[request.position++];
}

void    getTarget(Client & request) {
    if  (request.buf[request.position] == ' ' || request.buf[request.position] == '\r' || request.buf[request.position] == '\n') {
        if (request.target.empty() || request.target[0] != '/')
            throw 400;
        checkValidCharacters(request.target);
        Tools::decodeUri(request.target);
        request.pState = SPACE;
        request.pNext = VERSION;
    }
    else {
        request.target += request.buf[request.position++];
        if (request.target.size() > 1024)
            throw 414;
    }
}

void    getVersion(Client & request) {
    if (request.buf[request.position] == ' ' || request.buf[request.position] == '\r' || request.buf[request.position] == '\n') {
        if (request.version != "HTTP/1.1")
            throw 505;
        request.pState = SPACE;
        request.pNext = HEADER;
    }
    else {
        request.version += request.buf[request.position++];
        if (request.version.size() > 8)
            throw 400;
    }

}

void    checkHeader(Client & request) {
    bool        isKey = true;
    std::string key, value;

    for (size_t i = 0; i < request.header.size(); i++) {
        if (isKey && request.header[i] != ':')
            key += std::tolower(request.header[i]);
        else if (isKey && request.header[i] == ':') {
            isKey = false;
            i++;
            while (i < request.header.size() && request.header[i] == ' ')
                i++;
        }
        if (!isKey && i < request.header.size())
            value += request.header[i];
    }
    if (!request.headers.empty() && request.headers.find(key) != request.headers.end())
        throw 400;
    if (!key.empty() && !value.empty()) {
        if (key == "host")
            request.host = value;
        request.headers[key] = value;
    }
}

void    getHeader(Client & request) {
    if (request.buf[request.position] == '\r' || request.buf[request.position] == '\n') {
        checkHeader(request);
        request.header.clear();
        request.pState = SPACE;
        request.pNext = HEADER;
    }
    else
        request.header += request.buf[request.position++];
}

void    checkErrors(Client & request, std::vector<Server>& serv) {
    findServ(request, serv, request.host, request.target);
    if (std::find(request.location.second.allow_method.begin(), request.location.second.allow_method.end(), request.method) == request.location.second.allow_method.end())
        throw 405;
    if (request.host.empty())
        throw 400;
    targetChecker(request);

    if (request.method != "POST")
        throw 200;
    if (request.headers["content-type"].find("multipart/form-data; boundary=") == 0) {
        request.isBound = true;
        request.boundary = "--" + request.headers["content-type"].substr(30);
        if (request.headers.find("transfer-encoding") != request.headers.end())
    {
        throw 501;
      }
  }
    else if (request.headers.find("transfer-encoding") != request.headers.end()) {
        if (request.headers["transfer-encoding"] != "chunked")
    {
      throw 501;
      }
    }
    else if (request.headers.find("content-length") == request.headers.end())
        throw 400;
    if (request.headers.find("content-length") != request.headers.end()) {
        request.contentLength = std::strtol(request.headers["content-length"].c_str(), NULL, 10);
        if (request.contentLength > request.maxBodySize)
            throw 413;
    }
    request.crlf.clear();
    request.pState = BODY;
    postHandler(request);
}

void    hunting(Client & request, std::vector<Server>& serv) {
    switch (request.pState) {
        case SPACE:
            skipSpace(request);
            break;
        case CRLF:
            checkCrlf(request);
            break;
        case METHOD:
            getMethod(request);
            break;
        case TARGET:
            getTarget(request);
            break;
        case VERSION:
            getVersion(request);
            break;
        case HEADER:
            getHeader(request);
            break;
        case CHECK_ERROR:
            checkErrors(request, serv);
            break;
        case BODY:
            postHandler(request);
            break;
    }
}

void    reqParser(Client & request, int sock, std::vector<Server>& serv) {
    try {
		int amount = 1024;

        amount = read(sock, request.buf, BUFF_SIZE);
        request.position = 0;
        if (amount == 0 || amount == -1) {
			if (!request.uploadFile.empty())
				std::remove(request.uploadFile.c_str());
			request.state = CLOSE;
			throw 200;
		}
		request.buffSize = amount;
		request.buf[request.buffSize] = 0;
    while (request.position < request.buffSize || request.pState == CHECK_ERROR) {
      hunting(request, serv);
    }
	}
	catch (int status) {
		    request.statusCode = status;
        request.setEnv();
        if (request.method == "POST" && (status == 200 || status == 201) && request.isCgi) {
			Cgi	cgi(request);

			cgi.executeCgi();
      request.statusCode = 200;
		}
        request.outfile->close();
        if (request.state != CLOSE)
		    request.state = DONE;
	}
}
