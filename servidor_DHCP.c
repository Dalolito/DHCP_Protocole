#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#define PORT 67             // Puerto estándar del servidor DHCP
#define SUBNET_MASK "255.255.255.0"
#define DEFAULT_GATEWAY "192.168.1.1"
#define DNS_SERVER "8.8.8.8"
#define DOMAIN_NAME "example.local"
#define LEASE_TIME 120      // Tiempo de concesión (lease)
#define MAX_THREADS 5       // Número máximo de hilos activos

typedef struct {
    int message_type;
    char client_mac[18];
    char requested_ip[16];
    uint8_t options[312];
} dhcp_message;

typedef struct {
    unsigned int ip_addr;
    int is_assigned;
    time_t lease_expiration; // Control de expiración del lease
} ip_entry;

typedef struct {
    int sockfd;
    dhcp_message *msg;
    struct sockaddr_in client_addr;
} client_data;

ip_entry *ip_pool;
int pool_size = 0;
int active_threads = 0;          // Contador de hilos activos
pthread_mutex_t lock;            // Mutex para controlar el acceso a los recursos compartidos

unsigned int ip_to_int(const char *ip_str) {
    struct sockaddr_in sa;
    inet_pton(AF_INET, ip_str, &(sa.sin_addr));
    return ntohl(sa.sin_addr.s_addr);
}

void int_to_ip(unsigned int ip, char *ip_str) {
    struct in_addr addr;
    addr.s_addr = htonl(ip);
    inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN);
}

void init_ip_pool(const char *ip_start, const char *ip_end) {
    unsigned int start = ip_to_int(ip_start);
    unsigned int end = ip_to_int(ip_end);
    pool_size = end - start + 1;

    ip_pool = (ip_entry *)malloc(pool_size * sizeof(ip_entry));
    if (ip_pool == NULL) {
        perror("Error al asignar memoria para el pool de IPs");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < pool_size; i++) {
        ip_pool[i].ip_addr = start + i;
        ip_pool[i].is_assigned = 0;
    }
}

int assign_ip_dynamic(char *assigned_ip) {
    time_t current_time = time(NULL);

    for (int i = 0; i < pool_size; i++) {
        if (!ip_pool[i].is_assigned) {
            ip_pool[i].is_assigned = 1;
            ip_pool[i].lease_expiration = current_time + LEASE_TIME;
            int_to_ip(ip_pool[i].ip_addr, assigned_ip);
            return 0;
        }
    }
    return -1;
}

void release_ip_dynamic(const char *ip_str) {
    unsigned int ip = ip_to_int(ip_str);
    for (int i = 0; i < pool_size; i++) {
        if (ip_pool[i].ip_addr == ip) {
            ip_pool[i].is_assigned = 0;
            printf("IP %s liberada\n", ip_str);
            break;
        }
    }
}

void check_ip_leases() {
    time_t current_time = time(NULL);  // Obtener el tiempo actual
    char ip_str[INET_ADDRSTRLEN];

    for (int i = 0; i < pool_size; i++) {
        if (ip_pool[i].is_assigned && ip_pool[i].lease_expiration <= current_time) {
            int_to_ip(ip_pool[i].ip_addr, ip_str);
            printf("El tiempo de concesión de la IP %s ha expirado, liberando...\n", ip_str);
            ip_pool[i].is_assigned = 0;
        }
    }
}

void build_dhcp_options(dhcp_message *response, const char* subnet_mask, const char* gateway, const char* dns, const char* domain, int lease_time) {
    uint8_t *options = response->options;
    int offset = 0;

    options[offset++] = 53;
    options[offset++] = 1;
    options[offset++] = response->message_type;

    options[offset++] = 1;
    options[offset++] = 4;
    inet_pton(AF_INET, subnet_mask, &options[offset]);
    offset += 4;

    options[offset++] = 3;
    options[offset++] = 4;
    inet_pton(AF_INET, gateway, &options[offset]);
    offset += 4;

    options[offset++] = 6;
    options[offset++] = 4;
    inet_pton(AF_INET, dns, &options[offset]);
    offset += 4;

    size_t domain_len = strlen(domain);
    options[offset++] = 15;
    options[offset++] = domain_len;
    memcpy(&options[offset], domain, domain_len);
    offset += domain_len;

    options[offset++] = 51;
    options[offset++] = 4;
    lease_time = htonl(lease_time);
    memcpy(&options[offset], &lease_time, 4);
    offset += 4;

    options[offset++] = 255;
}

void* handle_client(void* arg) {
    client_data *data = (client_data*) arg;
    dhcp_message *msg = data->msg;
    int sockfd = data->sockfd;
    struct sockaddr_in client_addr = data->client_addr;

    dhcp_message response;
    char assigned_ip[16];

    switch (msg->message_type) {
        case 1: // DHCPDISCOVER
            printf("Recibido DHCPDISCOVER de %s\n", msg->client_mac);
            if (assign_ip_dynamic(assigned_ip) == 0) {
                printf("IP %s asignada a %s\n", assigned_ip, msg->client_mac);
                response.message_type = 2; // DHCPOFFER
                strcpy(response.client_mac, msg->client_mac);
                strcpy(response.requested_ip, assigned_ip);
                build_dhcp_options(&response, SUBNET_MASK, DEFAULT_GATEWAY, DNS_SERVER, DOMAIN_NAME, LEASE_TIME);
                sendto(sockfd, &response, sizeof(response), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
                printf("Enviado DHCPOFFER de %s a %s\n", assigned_ip, msg->client_mac);
            } else {
                printf("No hay IPs disponibles para asignar\n");
            }
            break;

        case 3: // DHCPREQUEST (para asignación inicial o renovación)
            printf("Recibido DHCPREQUEST de %s para la IP %s\n", msg->client_mac, msg->requested_ip);
            for (int i = 0; i < pool_size; i++) {
                if (ip_pool[i].is_assigned && strcmp(msg->requested_ip, inet_ntoa((struct in_addr){htonl(ip_pool[i].ip_addr)})) == 0) {
                    ip_pool[i].lease_expiration = time(NULL) + LEASE_TIME;
                    printf("Renovando IP %s para %s\n", msg->requested_ip, msg->client_mac);
                    break;
                }
            }
            response.message_type = 4; // DHCPACK
            strcpy(response.client_mac, msg->client_mac);
            strcpy(response.requested_ip, msg->requested_ip);
            build_dhcp_options(&response, SUBNET_MASK, DEFAULT_GATEWAY, DNS_SERVER, DOMAIN_NAME, LEASE_TIME);
            sendto(sockfd, &response, sizeof(response), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
            printf("Enviado DHCPACK para la IP %s a %s\n", msg->requested_ip, msg->client_mac);
            break;

        case 5: // DHCPRELEASE
            printf("Recibido DHCPRELEASE de %s para la IP %s\n", msg->client_mac, msg->requested_ip);
            release_ip_dynamic(msg->requested_ip);
            break;

        default:
            printf("Mensaje desconocido recibido\n");
            break;
    }

    pthread_mutex_lock(&lock);
    active_threads--;
    pthread_mutex_unlock(&lock);

    free(msg);
    free(data);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <IP de inicio> <IP de fin>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *ip_start = argv[1];
    const char *ip_end = argv[2];
    init_ip_pool(ip_start, ip_end);

    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    fd_set readfds;

    if (pthread_mutex_init(&lock, NULL) != 0) {
        perror("Mutex init failed");
        exit(EXIT_FAILURE);
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("No se pudo crear el socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket creado correctamente\n");

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("No se pudo enlazar el socket");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Socket enlazado al puerto %d\n", PORT);

    printf("Servidor DHCP iniciado y escuchando en el puerto %d...\n", PORT);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        int activity = select(sockfd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0 && errno != EINTR) {
            perror("Error en select()");
            break;
        }

        check_ip_leases();

        if (FD_ISSET(sockfd, &readfds)) {
            pthread_t thread;
            client_data *data = malloc(sizeof(client_data));

            data->msg = malloc(sizeof(dhcp_message));
            if (recvfrom(sockfd, data->msg, sizeof(dhcp_message), 0, (struct sockaddr*)&client_addr, &addr_len) < 0) {
                perror("Error al recibir mensaje");
                free(data->msg);
                free(data);
                continue;
            }

            data->sockfd = sockfd;
            data->client_addr = client_addr;

            pthread_mutex_lock(&lock);
            if (active_threads < MAX_THREADS) {
                active_threads++;
                pthread_mutex_unlock(&lock);

                if (pthread_create(&thread, NULL, handle_client, (void*)data) != 0) {
                    perror("Error al crear el hilo");
                    pthread_mutex_lock(&lock);
                    active_threads--;
                    pthread_mutex_unlock(&lock);
                    free(data->msg);
                    free(data);
                } else {
                    pthread_detach(thread); // No esperar a que el hilo termine
                }
            } else {
                pthread_mutex_unlock(&lock);
                printf("Máximo número de hilos alcanzado, esperando...\n");
                free(data->msg);
                free(data);
                sleep(1);
            }
        }
    }

    close(sockfd);
    pthread_mutex_destroy(&lock);
    free(ip_pool);
    return 0;
}
