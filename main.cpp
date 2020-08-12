extern "C"
{
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netdb.h>
    #include <unistd.h>
    //#include <stdio.h>
    #include <signal.h>
    #include <poll.h>
    #include <arpa/inet.h>
}

#include <iostream>
//#include <string>
#include <fstream>
#include <vector>
#include <thread>
//#include <mutex>
#include <map>

std::string addr_in_2str(const struct sockaddr_in *addr)
{
    return std::string(inet_ntoa(addr->sin_addr)) + ':' + std::to_string(addr->sin_port);
}

struct client_addr_t
{
    struct sockaddr data;
    socklen_t len;
};

int main(int argc, char *argv[])
{
//    std::cout << "pid = " << getpid() << std::endl;
//    std::cout << "Введите pid принимающей стороны: ";

//    pid_t target_pid;
//    std::cin >> target_pid;
//    std::cin.get();

//    std::ofstream out(std::string("/proc/") + std::to_string(target_pid) + std::string("/fd/1"));

//    for (;;)
//    {
//        char msg[1024];
//        std::cin.getline(msg, 1024);
//        out << getpid() << "> " << msg << std::endl;
//    }



    signal(SIGPIPE, SIG_IGN);

    std::cout << "Быть сервером? [y/n]: ";
    bool server_mode = std::cin.get() == 'y';
    int sockfd;

    if (server_mode)
    {
        struct addrinfo *servinfo;
        const struct addrinfo hints = {
            .ai_flags = AI_PASSIVE,
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
        };

        (std::cout << "Какой порт будет у сервера? [0-65535]: ").flush();
        unsigned short port;
        std::cin >> port;
        int r = getaddrinfo(0, std::to_string(port).data(), &hints, &servinfo);
        if (r != 0)
        {
            std::cerr << "getaddrinfo() error " << r << '.' << std::endl;
            exit(-1);
        }
        if (!servinfo)
        {
            std::cerr << "addrinfo list is empty." << std::endl;
            exit(-2);
        }

        for (struct addrinfo *p = servinfo; p; p = p->ai_next)
        {
            sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                // TODO: close(sockfd)
            if (sockfd == -1) continue;

            // мб пригодится setsockopt SO_REUSEADDR

            if (bind(sockfd, p->ai_addr, p->ai_addrlen) != 0)
            {
                close(sockfd);
                sockfd = -1;
                continue;
            }

            break;
        }
        if (sockfd == -1)
        {
            std::cerr << "socket() or bind() error." << std::endl;
            std::cerr << "Failed to find address to bind." << std::endl;
            exit(-3);
        }

        freeaddrinfo(servinfo);

        if (listen(sockfd, 10000) != 0)
        {
            std::cerr << "listen() error." << std::endl;
            exit(-4);
        }


//        std::mutex sockfds_mtx;
        std::vector<pollfd> sockfds{{sockfd, POLLIN, 0}};
        std::map<int, client_addr_t> client_addrs;

        auto send_to_all = [&sockfds](std::string out_str)
        {
            out_str = std::string("\n") + out_str + "\n>";
            (std::cout << out_str).flush();
            for (auto client_it = sockfds.begin() + 1; client_it != sockfds.end(); ++client_it)
            {
                if (write(client_it->fd, out_str.data(), out_str.size() + 1) < 0)
                {
                    if (errno == EPIPE)
                    {
                        std::cerr << "write(" << client_it->fd << ") => EPIPE -- странно??" << std::endl;
                    }
                    else
                    {
                        std::cerr << "write(" << client_it->fd << ") error." << std::endl;
                    }
                    errno = 0;
                }
            }
        };


        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        (std::cout << '>').flush();
        std::thread{[&send_to_all/*, &sockfds_mtx*/] // TODO: потокобезопасность
        {
            for (;;)
            {
                std::string out_str;
                std::getline(std::cin, out_str);
//                sockfds_mtx.lock();
                send_to_all(std::string("server: ") + std::move(out_str));
//                sockfds_mtx.unlock();
            }
        }}.detach();


        for (;;)
        {
            for (auto& sockfd : sockfds) sockfd.revents = 0; // need?
//            sockfds_mtx.lock();
            int poll_res = poll(sockfds.data(), sockfds.size(), -1 /*20*/);
//            sockfds_mtx.unlock();

            if (poll_res < 0)
            {
                std::cerr << "poll() error!" << std::endl;
                errno = 0;
            }

            if (sockfds[0].revents & (POLLHUP | POLLERR | POLLNVAL))
            {
                std::cerr << "server_socket error!" << std::endl;
                exit(0);
            }
            else if (sockfds[0].revents & POLLIN)
            {
                struct sockaddr client_addr;
                socklen_t client_addr_len = sizeof(client_addr);
                int client_sockfd = accept(sockfd, &client_addr, &client_addr_len);
                if (client_sockfd < 0)
                {
                    std::cerr << "accept() error." << std::endl;
//                    continue; // ? exit()
                }
                else
                {
                    client_addrs[client_sockfd] = {client_addr, client_addr_len};

                    send_to_all(addr_in_2str((struct sockaddr_in *)&client_addr) + " connected!");

                    sockfds.push_back({client_sockfd, POLLIN, 0});
                }
            }

            for (auto client_it = sockfds.begin() + 1; client_it != sockfds.end(); ++client_it)
            {
                if (client_it->revents & (POLLHUP | POLLERR | POLLNVAL))
                {
                    disconnect:
                    sockfds.erase(client_it);
                    send_to_all(addr_in_2str((struct sockaddr_in *)&client_addrs[client_it->fd].data) + " disconnected!");
                    client_addrs.erase(client_it->fd);
                    break;
                }
                else if (client_it->revents & POLLIN)
                {
                    char in_buf[65536] = {0}; // TODO: не ограничивать размер
                    ssize_t bytes_readed = recv(client_it->fd, in_buf, sizeof(in_buf) - 1, MSG_DONTWAIT);
                    if (bytes_readed < 0)
                    {
                        if (errno != EWOULDBLOCK)
                        {
                            std::cerr << "read(" << client_it->fd << ") error." << std::endl;
                        }
                        else
                        {
                            std::cerr << "recv(" << client_it->fd << ") => EWOULDBLOCK -- странно?" << std::endl;
                        }
                        errno = 0;
                    }
                    else if (bytes_readed == 0) // this is eof
                    {
                        goto disconnect;
                    }
                    else // TODO?: std::string_view
                    {
                        send_to_all(addr_in_2str((struct sockaddr_in *)&client_addrs[client_it->fd].data) + ": " + in_buf);
                    }
                }
            }
        }
//        close(client_sockfd);
    }
    else // client mode
    {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1)
        {
            std::cerr << "socket() error." << std::endl;
            exit(-5);
        }

        struct addrinfo *servinfo;
        const struct addrinfo hints = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
        };

        (std::cout << "Введите адрес сервера: ").flush();
        std::string addr;
        std::cin >> addr;
        (std::cout << "Введите порт сервера [0-65535]: ").flush();
        unsigned short port;
        std::cin >> port;
        int r = getaddrinfo(addr.data(), std::to_string(port).data(), &hints, &servinfo);
        if (r != 0)
        {
            std::cerr << "getaddrinfo() error " << r << '.' << std::endl;
            exit(-1);
        }
        if (!servinfo)
        {
            std::cerr << "addrinfo list is empty." << std::endl;
            exit(-2);
        }

        if (connect(sockfd, (struct sockaddr *)servinfo->ai_addr, sizeof(struct sockaddr_in)) != 0)
        {
            std::cerr << "connect() error." << std::endl;
            exit(-6);
        }

        freeaddrinfo(servinfo);


        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        (std::cout << '>').flush();
        std::thread{[&sockfd]
        {
            for (char in_buf[1024] = {0};;)
                if (read(sockfd, in_buf, 1023) > 0)
                    (std::cout << in_buf).flush();
                else
                    exit(0);
        }}.detach();

        for (;;)
        {
            std::string out_str;
            std::getline(std::cin, out_str);
            write(sockfd, out_str.data(), out_str.size() + 1);
        }

        close(sockfd);
    }

    return 0;
}
