extern "C"
{
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <signal.h>
    #include <poll.h>
    #include <arpa/inet.h>
    #include <sys/resource.h>
}

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <thread>
//#include <mutex>
#include <map>
#include <regex>
#include <unordered_map>
#include "eth-lib/eth-lib.hpp"

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

int main(int argc, char *argv[])
{
    rlimit rl{RLIM_INFINITY, RLIM_INFINITY};
    setrlimit(RLIMIT_STACK, &rl);

    signal(SIGPIPE, SIG_IGN); // что будет если сделать throw в обработчике сигнала?
//    signal(SIGSTOP, SIG_IGN);
    signal(SIGINT, [](int sig){
//        shutdown(sockfd, SHUT_RDWR);
        exit(0);
    });


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
        (std::cout << "Какой порт будет у сервера? [0-65535]: ").flush();
        unsigned short port;
        std::cin >> port;
        listener_t listener(port);

        auto send_to_all = [&](std::string out_str) {
            std::cout << out_str << std::endl;
            listener.send_to_all("\n" + out_str);
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

        listener.run([](client_ref_t client) {
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
            if (!client.write(out_buf)) {
                std::cerr << "Error occurred while sending headers to new connection" << std::endl;
            }
        }, [&](client_ref_t client) {
            auto input_buf = client.input_buf();
            std::smatch splitted;
            if (regex_match(input_buf, splitted, std::regex{"([^]*?\r\n)\r\n([^]*)"})) { // мб сильно сэкономит память если я буду не .str(2) делать, а только 1 захвал и .suffix()
                auto headers = splitted.str(1);
//                                std::cout << headers << std::endl;
                std::smatch m;
                if (regex_search(headers, m, std::regex{"Content-Length: (\\d+)\r\n"})
                    && splitted.str(2).size() >= std::stoull(m[1])) {
                    auto addr = client.addr();
                    client.disconnect();
                    
                    // auto s = client_in_bufs[client_fd];
                    // auto index = s.find("\r\n");
                    // bool flag = false;
                    // if (index != (size_t)-1) {
                    //     s = {s.begin(), s.begin() + index};
                    //     flag = s == "POST /message";
                    // }
                    
                    // if (flag) {
                    if (input_buf.starts_with("POST /message")) { // clang on androin fails here and version above doesn't work on desktop?
                        auto message = std::regex_replace(input_buf, std::regex{"[^]*?\r\n\r\n"}, "", std::regex_constants::format_first_only);
    //                            std::cout << message;
                        static unsigned long long unique_number;
                        std::time_t cur_time = std::time(nullptr);
                        std::ofstream("messages/" + (std::ctime(&cur_time) + (" - " + std::to_string(unique_number++))), std::ios::binary) << message; // висячий \r\n (или \n?) не выводится?
                        send_to_all(addr + ": " + message);
                    } else {
                        std::cout << input_buf << std::endl;
                    }
                }
            }
        });
    }
    else // client mode
    {
        (std::cout << "Введите адрес сервера: ").flush();
        std::string addr;
        std::cin >> addr;
        (std::cout << "Введите порт сервера [0-65535]: ").flush();
        std::string port;
        std::cin >> port;

        client_t client(addr, port);

        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        std::thread{[&]
        {
            client.listen([](std::string chunk) {
                std::cout << '\n' << chunk << std::endl;
            });
        }}.detach();

        for (;;)
        {
            (std::cout << '>').flush();
            std::string out_str;
            std::getline(std::cin, out_str);
            client.write(out_str);
        }
    }

    return 0;
}
