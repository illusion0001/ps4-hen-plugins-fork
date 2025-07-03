/* Copyright (C) 2023 John Törnblom

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */

#include <stdint.h>
#include <stdio.h>
#include "../../common/plugin_common.h"
#include "../../common/notify.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

#define DISK_BUFFER 2048
#define DISK_PATH "/user/temp"
#define DISK_CHECK DISK_PATH "/k"
#define DISK_KLOG DISK_PATH "/klog"

static int serve_file_while_connected(const char* path, int server_fd)
{
    struct timeval timeout;
    size_t nb_connections;
    fd_set output_set;
    fd_set input_set;
    fd_set temp_set;
    int client_fd;
    int file_fd;
    int err = 0;
    char ch;

    if ((file_fd = open(path, O_RDONLY)) < 0)
    {
        perror("open");
        return -1;
    }

    FD_ZERO(&input_set);
    FD_ZERO(&output_set);

    FD_SET(server_fd, &input_set);
    FD_SET(file_fd, &input_set);

    timeout.tv_sec = 0;
    timeout.tv_usec = 1000 * 10;  // 10ms
    nb_connections = 0;

    do
    {
        temp_set = input_set;
        switch (select(FD_SETSIZE, &temp_set, NULL, NULL, &timeout))
        {
            case 0:
                continue;
            case -1:
                perror("select");
                close(file_fd);
                return -1;
        }

        // new connection
        if (FD_ISSET(server_fd, &temp_set))
        {
            if ((client_fd = accept(server_fd, NULL, NULL)) < 0)
            {
                perror("accept");
                err = -1;
                break;
            }
            FD_SET(client_fd, &output_set);
            nb_connections++;
        }

        // new data from file
        if (FD_ISSET(file_fd, &temp_set))
        {
            if (read(file_fd, &ch, 1) != 1)
            {
                perror("read");
                err = -1;
                break;
            }

            for (client_fd = 0; client_fd < FD_SETSIZE; client_fd++)
            {
                if (FD_ISSET(client_fd, &output_set))
                {
                    if (write(client_fd, &ch, 1) != 1)
                    {
                        FD_CLR(client_fd, &output_set);
                        close(client_fd);
                        nb_connections--;
                    }
                }
            }
        }
    } while (nb_connections > 0);

    for (client_fd = 0; client_fd < FD_SETSIZE; client_fd++)
    {
        if (FD_ISSET(client_fd, &output_set))
        {
            FD_CLR(client_fd, &output_set);
            close(client_fd);
        }
    }
    close(file_fd);

    return err;
}

static int serve_file(const char* path, uint16_t port, int notify_user)
{
    char ip[INET_ADDRSTRLEN];
    struct ifaddrs* ifaddr;
    struct sockaddr_in sin;
    int ifaddr_wait = 1;
    fd_set set;
    int sockfd;

    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        return -1;
    }

    // Enumerate all AF_INET IPs
    for (struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
        {
            continue;
        }

        if (ifa->ifa_addr->sa_family != AF_INET)
        {
            continue;
        }

        // skip localhost
        if (!strncmp("lo", ifa->ifa_name, 2))
        {
            continue;
        }

        struct sockaddr_in* in = (struct sockaddr_in*)ifa->ifa_addr;
        inet_ntop(AF_INET, &(in->sin_addr), ip, sizeof(ip));

        // skip interfaces without an ip
        if (!strncmp("0.", ip, 2))
        {
            continue;
        }
        if (notify_user)
        {
            Notify(TEX_ICON_SYSTEM,
                   "Serving %s\n"
                   "on %s:%d (%s)",
                   path,
                   ip,
                   port,
                   ifa->ifa_name);
        }
        ifaddr_wait = 0;
    }
    notify_user = 0;

    freeifaddrs(ifaddr);

    if (ifaddr_wait)
    {
        return 0;
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return -1;
    }

    int temp_1 = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &temp_1, sizeof(temp_1)) < 0)
    {
        perror("setsockopt");
        return -1;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&sin, sizeof(sin)) < 0)
    {
        perror("bind");
        return -1;
    }

    if (listen(sockfd, 5) < 0)
    {
        perror("listen");
        return -1;
    }

    while (1)
    {
        // wait for a connection
        FD_ZERO(&set);
        FD_SET(sockfd, &set);
        if (select(sockfd + 1, &set, NULL, NULL, NULL) < 0)
        {
            perror("select");
            return -1;
        }

        // someone wants to connect
        if (FD_ISSET(sockfd, &set))
        {
            if (serve_file_while_connected(path, sockfd) < 0)
            {
                close(sockfd);
                return -1;
            }
        }
    }

    return 0;
}

static void* klog_to_disk_thread(void* args)
{
    (void)args;

    char buffer[DISK_BUFFER];
    char filename[256];
    char date_str[32];
    time_t now;
    struct tm* tm_info;
    int klog_fd = -1;
    FILE* output_file = NULL;
    ssize_t bytes_read;
    struct stat st;

    while (1)
    {
        if (stat(DISK_CHECK, &st) == 0)
        {
            if (output_file)
            {
                fclose(output_file);
                output_file = NULL;
            }

            unlink(DISK_CHECK);
        }

        if (!output_file)
        {
            now = time(NULL);
            tm_info = localtime(&now);
            strftime(date_str, sizeof(date_str), "%Y%m%d_%H%M%S", tm_info);
            snprintf(filename, sizeof(filename), DISK_KLOG "_%s.txt", date_str);

            output_file = fopen(filename, "w");
            if (!output_file)
            {
                perror("fopen output file");
                sleep(1);
                continue;
            }

            printf("Started logging to: %s\n", filename);
        }

        if (klog_fd < 0)
        {
            klog_fd = open("/dev/klog", O_RDONLY | O_NONBLOCK);
            if (klog_fd < 0)
            {
                perror("open /dev/klog");
                sleep(1);
                continue;
            }
        }

        bytes_read = read(klog_fd, buffer, DISK_BUFFER - 1);
        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0';
            fprintf(output_file, "%s", buffer);
            fflush(output_file);
        }
        else if (bytes_read == 0)
        {
            usleep(10000);
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                usleep(10000);
            }
            else
            {
                perror("read /dev/klog");
                close(klog_fd);
                klog_fd = -1;
                sleep(1);
            }
        }
    }

    if (output_file)
    {
        fclose(output_file);
    }
    if (klog_fd >= 0)
    {
        close(klog_fd);
    }

    return NULL;
}

void* pthread_kserver(void* args)
{
    (void)args;
    uint16_t port = 3232;
    int notify_user = 1;
    pthread_t disk_logger_thread;

    printf("Socket server was compiled at %s %s\n", __DATE__, __TIME__);

    if (1 && pthread_create(&disk_logger_thread, NULL, klog_to_disk_thread, NULL) != 0)
    {
        perror("pthread_create disk logger");
        return NULL;
    }

    while (1)
    {
        serve_file("/dev/klog", port, notify_user);
        notify_user = 0;
        sleep(3);
    }
    return 0;
}
