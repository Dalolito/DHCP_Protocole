#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVER_PORT 67           // Puerto donde escucha el servidor DHCP
#define CLIENT_PORT 68           // Puerto donde escucha el cliente DHCP
#define SERVER_IP "127.0.0.1"    // IP del servidor (usando localhost para pruebas)

// Estructura para un mensaje DHCP (simplificada para el ejemplo)
typedef struct {
    int message_type;         // Tipo de mensaje (1 = DHCPDISCOVER, 3 = DHCPREQUEST)
    char client_mac[18];      // Dirección MAC del cliente (formato: XX:XX:XX:XX:XX:XX)
    char requested_ip[16];    // Dirección IP solicitada por el cliente
} dhcp_message;

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(server_addr);
    dhcp_message msg, response;

    // Crear el socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("No se pudo crear el socket");
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección del cliente
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port = htons(CLIENT_PORT);

    // Enlazar el socket del cliente a su puerto (opcional)
    if (bind(sockfd, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        perror("No se pudo enlazar el socket del cliente");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección del servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Enviar mensaje DHCPDISCOVER al servidor
    msg.message_type = 1; // 1 = DHCPDISCOVER
    strcpy(msg.client_mac, "00:11:22:33:44:55");
    printf("Enviando DHCPDISCOVER al servidor...\n");

    if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&server_addr, addr_len) < 0) {
        perror("Error al enviar DHCPDISCOVER");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Esperar respuesta del servidor (DHCPOFFER)
    if (recvfrom(sockfd, &response, sizeof(response), 0, (struct sockaddr*)&server_addr, &addr_len) < 0) {
        perror("Error al recibir respuesta del servidor");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Recibido DHCPOFFER: %s\n", response.requested_ip);

    // Enviar mensaje DHCPREQUEST para solicitar la IP ofrecida
    msg.message_type = 3; // 3 = DHCPREQUEST
    strcpy(msg.requested_ip, response.requested_ip);
    printf("Enviando DHCPREQUEST al servidor para la IP %s...\n", msg.requested_ip);

    if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&server_addr, addr_len) < 0) {
        perror("Error al enviar DHCPREQUEST");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Esperar respuesta del servidor (DHCPACK)
    if (recvfrom(sockfd, &response, sizeof(response), 0, (struct sockaddr*)&server_addr, &addr_len) < 0) {
        perror("Error al recibir DHCPACK");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Recibido DHCPACK: IP asignada %s\n", response.requested_ip);

    // Cerrar el socket
    close(sockfd);
    return 0;
}
