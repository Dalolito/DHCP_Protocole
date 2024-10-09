#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

#define PORT 67             // Puerto estándar del servidor DHCP
#define MAX_CLIENTS 10      // Número máximo de clientes (también el número de IPs disponibles)
#define IP_POOL_FILE "ip_pool.txt" // Archivo con las direcciones IP disponibles

// Estructura para un mensaje DHCP (simplificada para el ejemplo)
typedef struct {
    int message_type;         // Tipo de mensaje (e.g., DHCPDISCOVER, DHCPOFFER)
    char client_mac[18];      // Dirección MAC del cliente (formato: XX:XX:XX:XX:XX:XX)
    char requested_ip[16];    // Dirección IP solicitada por el cliente
} dhcp_message;

// Función para leer una dirección IP del archivo y marcarla como asignada
int assign_ip(char* ip_address) {
    FILE *file = fopen(IP_POOL_FILE, "r+");
    if (!file) {
        perror("No se pudo abrir el archivo de pool de IPs");
        return -1;
    }

    char line[16];
    while (fgets(line, sizeof(line), file)) {
        // Verificar si la línea no está asignada (simplemente si no tiene un '#')
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

// Función para manejar los mensajes recibidos de los clientes
void handle_client_message(int sockfd, struct sockaddr_in *client_addr, dhcp_message *msg) {
    dhcp_message response;
    char assigned_ip[16];

    // Dependiendo del tipo de mensaje, generar una respuesta adecuada
    switch (msg->message_type) {
        case 1: // DHCPDISCOVER
            printf("Recibido DHCPDISCOVER de %s\n", msg->client_mac);
            if (assign_ip(assigned_ip) == 0) {
                response.message_type = 2; // DHCPOFFER
                strcpy(response.client_mac, msg->client_mac);
                strcpy(response.requested_ip, assigned_ip);
                sendto(sockfd, &response, sizeof(response), 0, (struct sockaddr*) client_addr, sizeof(*client_addr));
                printf("Enviado DHCPOFFER de %s a %s\n", assigned_ip, msg->client_mac);
            } else {
                printf("No hay IPs disponibles para asignar\n");
            }
            break;

        case 3: // DHCPREQUEST
            printf("Recibido DHCPREQUEST de %s para la IP %s\n", msg->client_mac, msg->requested_ip);
            response.message_type = 4; // DHCPACK
            strcpy(response.client_mac, msg->client_mac);
            strcpy(response.requested_ip, msg->requested_ip);
            sendto(sockfd, &response, sizeof(response), 0, (struct sockaddr*) client_addr, sizeof(*client_addr));
            printf("Enviado DHCPACK para la IP %s a %s\n", msg->requested_ip, msg->client_mac);
            break;

        case 5: // DHCPRELEASE
            printf("Recibido DHCPRELEASE de %s para la IP %s\n", msg->client_mac, msg->requested_ip);
            // Aquí podrías agregar la lógica para liberar la IP, volviendo a escribirla como disponible en el archivo
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

    // Crear el socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("No se pudo crear el socket");
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Enlazar el socket al puerto especificado
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("No se pudo enlazar el socket");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Servidor DHCP iniciado y escuchando en el puerto %d...\n", PORT);

    // Configurar el uso de select() para manejar múltiples clientes
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        int activity = select(sockfd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0 && errno != EINTR) {
            perror("Error en select()");
            break;
        }
        // Verificar si hay actividad en el socket principal (nueva solicitud de un cliente)
        if (FD_ISSET(sockfd, &readfds)) {
            dhcp_message msg;
            memset(&msg, 0, sizeof(msg));

            // Recibir el mensaje del cliente
            if (recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&client_addr, &addr_len) < 0) {
                perror("Error al recibir mensaje");
                continue;
            }

            // Manejar el mensaje del cliente
            handle_client_message(sockfd, &client_addr, &msg);
        }
    }

    // Cerrar el socket al finalizar el servidor
    close(sockfd);
    return 0;
}
