#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <time.h>

#define PORT 67             // Puerto estándar del servidor DHCP
#define MAX_CLIENTS 10      // Número máximo de clientes (también el número de IPs disponibles)
#define IP_POOL_FILE "ip_pool.txt" // Archivo con las direcciones IP disponibles
#define LEASE_FILE "leases.txt"    // Archivo para registrar las asignaciones de IP
#define SUBNET_MASK "255.255.255.0" // Máscara de subred
#define DEFAULT_GATEWAY "192.168.1.1" // Puerta de enlace predeterminada
#define DNS_SERVER "8.8.8.8" // Servidor DNS
#define DOMAIN_NAME "example.local" // Dominio del cliente

// Estructura para un mensaje DHCP (simplificada para el ejemplo)
typedef struct {
    int message_type;         // Tipo de mensaje (e.g., DHCPDISCOVER, DHCPOFFER)
    char client_mac[18];      // Dirección MAC del cliente (formato: XX:XX:XX:XX:XX:XX)
    char requested_ip[16];    // Dirección IP solicitada por el cliente
    uint8_t options[312];     // Campo para las opciones DHCP
} dhcp_message;

// Declaraciones de las funciones para evitar advertencias de declaración implícita
void release_ip(const char* client_mac, const char* ip_address);
void load_leases();
void log_lease(const char* client_mac, const char* ip_address, time_t lease_time);

// Función para leer una dirección IP del archivo y marcarla como asignada
int assign_ip(char* ip_address) {
    FILE *file = fopen(IP_POOL_FILE, "r+");
    if (!file) {
        perror("No se pudo abrir el archivo de pool de IPs");
        return -1;
    }

    char line[16];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] != '#') {
            strcpy(ip_address, line);
            fseek(file, -strlen(line), SEEK_CUR);
            fprintf(file, "#%s", line); // Marcar la IP como asignada con un '#' al inicio
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return -1; // No hay IPs disponibles
}

// Función para construir las opciones DHCP
void build_dhcp_options(dhcp_message *response, const char* subnet_mask, const char* gateway, const char* dns, const char* domain, int lease_time) {
    uint8_t *options = response->options;
    int offset = 0;

    options[offset++] = 53; // Opción 53: Tipo de mensaje DHCP (DHCPOFFER o DHCPACK)
    options[offset++] = 1;  
    options[offset++] = response->message_type;

    options[offset++] = 1; // Opción 1: Máscara de subred
    options[offset++] = 4;  
    inet_pton(AF_INET, subnet_mask, &options[offset]);
    offset += 4;

    options[offset++] = 3; // Opción 3: Puerta de enlace predeterminada
    options[offset++] = 4;  
    inet_pton(AF_INET, gateway, &options[offset]);
    offset += 4;

    options[offset++] = 6; // Opción 6: Servidor DNS
    options[offset++] = 4;  
    inet_pton(AF_INET, dns, &options[offset]);
    offset += 4;

    options[offset++] = 15; // Opción 15: Nombre de dominio
    size_t domain_len = strlen(domain);
    options[offset++] = domain_len;
    memcpy(&options[offset], domain, domain_len);
    offset += domain_len;

    options[offset++] = 51; // Opción 51: Tiempo de concesión (lease time)
    options[offset++] = 4;  
    lease_time = htonl(lease_time);
    memcpy(&options[offset], &lease_time, 4);
    offset += 4;

    options[offset++] = 255; // Opción 255: Fin de las opciones
}

// Función para registrar una asignación de IP
void log_lease(const char* client_mac, const char* ip_address, time_t lease_time) {
    FILE *file = fopen(LEASE_FILE, "a");
    if (!file) {
        perror("No se pudo abrir el archivo de arrendamientos");
        return;
    }
    fprintf(file, "%s %s %ld\n", client_mac, ip_address, lease_time);
    fclose(file);
}

// Función para cargar las concesiones existentes desde el archivo de arrendamientos
void load_leases() {
    FILE *file = fopen(LEASE_FILE, "r");
    if (!file) {
        perror("No se pudo abrir el archivo de arrendamientos");
        return;
    }

    char mac[18];
    char ip[16];
    time_t lease_time;
    while (fscanf(file, "%s %s %ld", mac, ip, &lease_time) == 3) {
        printf("Cargando concesión: MAC: %s, IP: %s, Vence en: %ld\n", mac, ip, lease_time);
    }

    fclose(file);
}

// Función para liberar una dirección IP cuando el cliente envía un DHCPRELEASE
void release_ip(const char* client_mac, const char* ip_address) {
    FILE *file = fopen(IP_POOL_FILE, "r+");
    if (!file) {
        perror("No se pudo abrir el archivo de pool de IPs");
        return;
    }

    FILE *temp_file = fopen("temp_ip_pool.txt", "w");
    if (!temp_file) {
        perror("No se pudo crear el archivo temporal");
        fclose(file);
        return;
    }

    char line[16];
    int ip_released = 0;

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' && strstr(line, ip_address) != NULL) {
            fprintf(temp_file, "%s", line + 1); 
            ip_released = 1;
        } else {
            fprintf(temp_file, "%s", line); 
        }
    }

    fclose(file);
    fclose(temp_file);

    if (ip_released) {
        if (rename("temp_ip_pool.txt", IP_POOL_FILE) != 0) {
            perror("Error al actualizar el archivo de pool de IPs");
        } else {
            printf("IP %s liberada y marcada como disponible\n", ip_address);
        }
    } else {
        printf("No se encontró la IP %s para liberar\n", ip_address);
        remove("temp_ip_pool.txt");
    }
}

// Función para manejar los mensajes recibidos de los clientes o del relay agent
void handle_client_message(int sockfd, struct sockaddr_in *client_addr, dhcp_message *msg) {
    dhcp_message response;
    char assigned_ip[16];
    struct sockaddr_in response_addr = *client_addr;

    if (client_addr->sin_addr.s_addr != INADDR_ANY) {
        printf("Solicitud recibida desde un DHCP relay con la dirección IP %s\n", inet_ntoa(client_addr->sin_addr));
        response_addr.sin_addr = client_addr->sin_addr;  
    }

    response_addr.sin_port = htons(68);

    switch (msg->message_type) {
        case 1: 
            printf("Recibido DHCPDISCOVER de %s\n", msg->client_mac);
            if (assign_ip(assigned_ip) == 0) {
                response.message_type = 2;
                strcpy(response.client_mac, msg->client_mac);
                strcpy(response.requested_ip, assigned_ip);
                build_dhcp_options(&response, SUBNET_MASK, DEFAULT_GATEWAY, DNS_SERVER, DOMAIN_NAME, 3600);

                
                printf("Enviado DHCPOFFER de %s a %s (via %s)\n", assigned_ip, msg->client_mac, inet_ntoa(response_addr.sin_addr));
            } else {
                printf("No hay IPs disponibles para asignar\n");
            }
            break;

        case 3: 
            printf("Recibido DHCPREQUEST de %s para la IP %s\n", msg->client_mac, msg->requested_ip);
            response.message_type = 4;
            strcpy(response.client_mac, msg->client_mac);
            strcpy(response.requested_ip, msg->requested_ip);
            build_dhcp_options(&response, SUBNET_MASK, DEFAULT_GATEWAY, DNS_SERVER, DOMAIN_NAME, 3600);

            sendto(sockfd, &response, sizeof(response), 0, (struct sockaddr*)&response_addr, sizeof(response_addr));
            printf("Enviado DHCPACK para la IP %s a %s (via %s)\n", msg->requested_ip, msg->client_mac, inet_ntoa(response_addr.sin_addr));

            time_t lease_time = time(NULL) + 3600;
            log_lease(msg->client_mac, msg->requested_ip, lease_time);
            break;

        case 5: 
            printf("Recibido DHCPRELEASE de %s para la IP %s\n", msg->client_mac, msg->requested_ip);
            release_ip(msg->client_mac, msg->requested_ip);
            break;

        default:
            printf("Mensaje desconocido recibido\n");
            break;
    }
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    fd_set readfds;
    socklen_t addr_len = sizeof(client_addr);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("No se pudo crear el socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("No se pudo enlazar el socket");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Servidor DHCP iniciado y escuchando en el puerto %d...\n", PORT);

    load_leases();

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        int activity = select(sockfd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0 && errno != EINTR) {
            perror("Error en select()");
            break;
        }

        if (FD_ISSET(sockfd, &readfds)) {
            dhcp_message msg;
            memset(&msg, 0, sizeof(msg));

            if (recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&client_addr, &addr_len) < 0) {
                perror("Error al recibir mensaje");
                continue;
            }

            handle_client_message(sockfd, &client_addr, &msg);
        }
    }

    close(sockfd);
    return 0;
}
