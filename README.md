*This project has been created as part of the 42 curriculum by bkiskac, raydogmu.*

# Webserv

## Description

Webserv is an HTTP server written in **C++98**.

The purpose of the project is to understand what happens behind a web server by building one from scratch. It handles socket connections, parses HTTP requests, creates responses, and manages clients using non-blocking I/O.

We use `epoll` to handle multiple connections at the same time. The server supports both HTTP/1.0 and HTTP/1.1.

Main features:

* GET, POST and DELETE methods
* Static file serving
* File uploads
* CGI execution
* HTTP redirects
* Custom error pages
* Autoindex
* Multiple ports and server configurations

## Instructions

### Requirements

Webserv is written in **C++98** and relies on Linux networking APIs such as `epoll`.

Because of this, the project needs to be compiled and run on Linux.

Useful tools:

* `c++` for compilation
* `make` for building the project
* `curl` for sending HTTP requests
* `php-cgi` for CGI testing
* `valgrind` for optional memory checks

### Compilation

Clone the repository and enter the project directory:

```bash
git clone https://github.com/batuhankiskac/webserv.git
cd webserv
```

Compile the project with:

```bash
make
```

The Makefile uses the following flags:

```text
-Wall -Wextra -Werror -std=c++98
```

Other available Makefile commands are:

```bash
make clean
make fclean
make re
```

* `clean` removes the object files.
* `fclean` removes the object files and the executable.
* `re` rebuilds the project from scratch.

### Execution

Start the server by giving it a configuration file:

```bash
./webserv config/example.conf
```

The general syntax is:

```bash
./webserv [configuration_file]
```

The configuration file controls things like:

* listening ports
* server names
* maximum request body size
* custom error pages
* allowed HTTP methods
* root directories
* default index files
* autoindex
* upload locations
* CGI settings
* HTTP redirects

After starting the server, you can access it from a browser or send requests with `curl`.

For example:

```bash
curl http://localhost:8080/
```

The port depends on the configuration file being used.

## Resources

Some references we used during the project:

* [RFC 9110 - HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110)
* [RFC 9112 - HTTP/1.1](https://www.rfc-editor.org/rfc/rfc9112)
* [MDN HTTP Documentation](https://developer.mozilla.org/en-US/docs/Web/HTTP)
* [NGINX Documentation](https://nginx.org/en/docs/)
* [Linux epoll Documentation](https://man7.org/linux/man-pages/man7/epoll.7.html)
* https://www.youtube.com/watch?v=L0jMBrCEQNQ&t=29s
* https://www.youtube.com/watch?v=hWyBeEF3CqQ
* https://www.youtube.com/watch?v=WuwUk7Mk80E&t=88s

### AI Usage

We used AI mainly as a support tool during development. It helped us with:

* understanding some HTTP and networking concepts
* thinking about possible edge cases
* organizing the documentation
* creating test scripts for debugging
