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
    #include <sys/resource.h>
}

#include <iostream>
//#include <string>
#include <fstream>
#include <vector>
//#include <atomic>
//#include <future>
#include <thread>
//#include <mutex>
#include <map>
#include <regex>
//#include <coroutine>
#include <unordered_map>

//struct coro_t {
//    struct promise_type {
//        auto handle() {
//          return std::coroutine_handle<promise_type>::from_promise(*this);
//        }
//        auto get_return_object() {
//            return handle();
//        }
//        std::suspend_always initial_suspend() {
//            return {};
//        }
//        void return_void() {}
//        std::suspend_always final_suspend() noexcept {
//            return {};
//        }
//        void unhandled_exception() {}
//    };
//    std::coroutine_handle<promise_type> handle;
//};

//coro_t listen_coro() {
//    co_return;
//}

//#include <QCoreApplication>
//#include <QTimer>
//#include <QEvent>
//#include <QKeyEvent>


//void print_addrinfo(const struct addrinfo *info)
//{
//    std::cout << info->ai_flags << ':'
//              << info->ai_family << ':'
//              << info->ai_socktype << ':'
//              << info->ai_protocol << ':'
//              << info->ai_addrlen << ':'
//              << info->ai_addr->sa_family << ':'
//              << std::string(info->ai_addr->sa_data, 14) << ':'
//              << info->ai_canonname;
//}

//std::string ip2str(uint32_t ip)
//{
////    return std::to_string((ip      ) & 0xFF) + '.'
////         + std::to_string((ip >>  8) & 0xFF) + '.'
////         + std::to_string((ip >> 16) & 0xFF) + '.'
////         + std::to_string((ip >> 24)       );
//    return inet_ntoa()
//}

std::string addr_in_2str(const struct sockaddr_in *addr)
{
//    return ip2str(addr->sin_addr.s_addr) + ':' + std::to_string(addr->sin_port);
    return std::string(inet_ntoa(addr->sin_addr)) + ':' + std::to_string(addr->sin_port);
}

//std::mutex mtx;

//void f()
//{
//    std::cout << "pid = " << getpid() << std::endl;
//    std::cout << "Введите pid принимающей стороны: ";

//    pid_t target_pid;
//    std::cin >> target_pid;
//    std::cin.get();

//    mtx.unlock();

//    std::ofstream out(std::string("/proc/") + std::to_string(target_pid) + std::string("/fd/1"));

//    for (std::size_t i = 0; ; ++i)
//    {
//        out << i << std::endl;
//        std::this_thread::sleep_for(std::chrono::milliseconds(300));
//    }
//}

struct client_addr_t
{
    struct sockaddr data;
    socklen_t len;
};

int sockfd, INT_ONE = 1;
int main(int argc, char *argv[])
{
    rlimit rl{RLIM_INFINITY, RLIM_INFINITY};
    setrlimit(RLIMIT_STACK, &rl);

    signal(SIGPIPE, SIG_IGN); // throw в обработчике сигнала?
//    signal(SIGSTOP, SIG_IGN);
    signal(SIGINT, [](int sig){
//        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        exit(0);
    });
//    QCoreApplication a(argc, argv);


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




    std::cout << "Быть сервером? [y/n]: ";
    bool server_mode = std::cin.get() == 'y';

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

            // мб пригодится setsockopt SO_REUSEADDR          дадда
            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &INT_ONE, sizeof(INT_ONE)); // не отбрасывать возвращённое значение

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
        std::map<int, client_addr_t> client_addrs; // client_addrs[fd].len is unused?     sorted_map избыточен?
        std::unordered_map<int, std::string> client_in_bufs;

        auto send_to_all = [&sockfds](std::string out_str)
        {
            out_str = std::string("<!--<br>-->\n") + out_str + "<!--<br>\n>-->";
            (std::cout << out_str).flush();
            for (auto client_it = sockfds.begin() + 1; client_it != sockfds.end(); ++client_it)
            {
                if (write(client_it->fd, out_str.data(), out_str.size()) < 0)
                {
                    if (errno == EPIPE)
                    {
//                        to_remove_csfds.push_back(client_sockfd); // сообщать о выходе человека
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
//                std::cout << '>';
                std::string out_str;
                if (std::getline(std::cin, out_str).eof()) {
                    return;
                }
//                sockfds_mtx.lock(); // не работает
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
//                    send_to_all(addr_in_2str((struct sockaddr_in *)&client_addr) + " connected!");

                    // сейчас я даже на POST /messge отвечаю так
                    char out_buf[] = R"(HTTP/1.1 200 OK

                         <!DOCTYPE html>
                         <html lang="ru">
                         <head>
                             <meta charset="UTF-8">
                         </head>
                         <body>
                            <H1>Привет! Это САППОРТ!</H1>
                            <textarea name="message" placeholder="Пожалуйста, введите своё сообщение..."></textarea><br>
                            <button onclick="
                                let xhr = new XMLHttpRequest(), ta = document.getElementsByName('message')[0];
                                xhr.open('POST', '/message', true);
                                xhr.send(ta.value);
                                ta.value = '';
                            ">Отправить</button>
                            <H2>Сейчас также спрашивают:</H2>
                         <!-- </body>
                         </html> -->
                            <pre>
                    )";
                    if (write(client_sockfd, out_buf, sizeof(out_buf) - 1) == -1) {
                        std::cerr << "Error occurred while sending headers to new connection" << std::endl;
                    }

                    sockfds.push_back({client_sockfd, POLLIN, 0});
//                    close(client_sockfd);
                }
            }

            for (auto client_it = sockfds.begin() + 1; client_it != sockfds.end(); ++client_it)
            {
                bool must_break;
                int client_fd = client_it->fd;
                auto client_addr = client_addrs[client_fd].data;
                auto disconnect = [&] { // т.к. инвалидируются итераторы, после этого вызова нельзя продолжать итерации, т.е. нужен break
                    sockfds.erase(client_it); // инвалидируются итераторы!
//                    send_to_all(addr_in_2str((struct sockaddr_in *)&client_addrs[fd].data) + " disconnected!");
                    client_addrs.erase(client_fd);
                    must_break = true;
                };
                if (client_it->revents & (POLLHUP | POLLERR | POLLNVAL))
                {
                    disconnect();
                    break;
                }
                else if (client_it->revents & POLLIN)
                {
                    bool must_continue = false;
                    must_break = false;
                    for (char in_buf[16];;) {
                        ssize_t bytes_readed = recv(client_fd, in_buf, sizeof(in_buf), MSG_DONTWAIT); // т.к. я работаю с raw data, я никогда не должен полагаться на \0
                        if (bytes_readed > 0) {
                            // то добавить в буфер и считать ещё из сокета
                            client_in_bufs[client_fd] += {in_buf, in_buf + bytes_readed};
                        } else if (bytes_readed == 0) { // this is eof (т.к. у меня SOCK_STREAM (TCP))
                            disconnect();
                            // надо close(client_fd)?     видимо да, т.к. если нет eof, я могу вызвать close(), но во время вызова может стать eof, соответственно можно close() на eof'нутый сокет делать
                            break;
                        } else /* bytes_readed < 0 */ {
                            // error    or     ret_val = -1 and errno is EAGAIN or EWOULDBLOCK means that there are no data ready, try again later
                            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                                std::cerr << "read(" << client_fd << ") error. errno = " << strerror(errno) << std::endl;
                            } else {
                                // there are no data ready, try again later
                                // МБ ПОМОЖЕТ В ОТВЕТЕ ПОСЛАТЬ content-length: 0 и пустую строку? чтобы сюда не заходить выполнению
                                std::cerr << "уже считано " << std::regex_replace(client_in_bufs[client_fd], std::regex{"[^]*?\r\n\r\n"}, "", std::regex_constants::format_first_only).size() << std::endl;
                            }
                            errno = 0;

                            std::smatch splitted;
                            if (regex_match(client_in_bufs[client_fd], splitted, std::regex{"([^]*?\r\n)\r\n([^]*)"})) { // мб сильно сэкономит память если я буду не .str(2) делать, а только 1 захвал и .suffix()
                                auto headers = splitted.str(1);
//                                std::cout << headers << std::endl;
                                std::smatch m;
                                if (regex_search(headers, m, std::regex{"Content-Length: (\\d+)\r\n"})
                                  && splitted.str(2).size() >= std::stoull(m[1])) {
                                    disconnect();
                                    close(client_fd);
                                    break;
                                }
                            }

                            must_continue = true; // можно поменять на goto continue_2;
                            break;
                        }
                    }

                    if (must_continue) {
                        continue;
                    }

                    auto s = client_in_bufs[client_fd];
                    auto index = s.find("\r\n");
                    bool flag = false;
                    if (index != (size_t)-1) {
                        s = {s.begin(), s.begin() + index};
                        flag = s == "POST /message";
                    }
                    
                    // if (client_in_bufs[client_fd].starts_with("POST /message")) { // clang on androin fails here
                    if (flag) {
                        auto message = std::regex_replace(client_in_bufs[client_fd], std::regex{"[^]*?\r\n\r\n"}, "", std::regex_constants::format_first_only);
//                            std::cout << message;
                        static unsigned long long unique_number;
                        std::time_t cur_time = std::time(nullptr);
                        std::ofstream("messages/" + (std::ctime(&cur_time) + (" - " + std::to_string(unique_number++))), std::ios::binary) << message; // висячий \r\n (или \n?) не выводится?
                        send_to_all(addr_in_2str((struct sockaddr_in *)&client_addr) + ": " + message);
                    } else {
                        std::cout << client_in_bufs[client_fd] << std::endl;
                    }
                    client_in_bufs.erase(client_fd);
                    if (must_break) {
                        break;
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

//        const struct sockaddr_in dest_addr = {
//            .sin_family = AF_INET,
//            .sin_port = 20181,
//            .sin_addr = {127 | (0 << 8) | (0 << 16) | (1 << 24)}, // не тот endian же
//        };
        if (connect(sockfd, (struct sockaddr *)servinfo->ai_addr, sizeof(struct sockaddr_in)) != 0)
        {
            std::cerr << "connect() error." << std::endl;
            exit(-6);
        }

        freeaddrinfo(servinfo);








        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        (std::cout << '>').flush();
        std::thread{[]
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




//    QTimer testTimer;
//    testTimer.setInterval(1000 / 6);
//    testTimer.setSingleShot(false);

//    QObject::connect(&testTimer, &QTimer::timeout, []{
//        static int i = 0;
//        std::cout << i++ << std::endl;
//    });

//    testTimer.start();




//    mtx.lock();
//    std::thread t(f);

//    t.detach();

//    auto &&f = std::async(std::launch::async, [&a]{
//        mtx.lock();
//        std::getchar();
////        mtx.unlock(); need?
//        return a.exit(0);
//    });

//    (void)f;

    return 0; //a.exec();
}
